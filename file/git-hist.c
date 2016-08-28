/*
 * file/git-hist.c - Retrieve revision history from GIT repo
 *
 * Written 2016 by Werner Almesberger
 * Copyright 2016 by Werner Almesberger
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#define _GNU_SOURCE
#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>	/* for vcs_long_for_pango */
#include <alloca.h>

#include <git2.h>

#include "misc/util.h"
#include "misc/diag.h"
#include "file/git-util.h"
#include "file/git-file.h"
#include "file/git-hist.h"


/*
 * @@@ we assume to have a single head. That isn't necessarily true, since
 * each open branch has its own head. Getting this right is for further study.
 */


struct branch {
	const char *name;
	git_commit *commit;
	
};


struct vcs_history {
	struct vcs_hist *head;
	git_repository *repo;
	struct branch *branches;
	unsigned n_branches;
	struct vcs_hist *history;	/* any order */
};


/* ----- History retrieval ------------------------------------------------- */


static struct vcs_hist *new_commit(struct vcs_history *history, unsigned branch)
{
	struct vcs_hist *h;

	h = alloc_type(struct vcs_hist);
	h->commit = NULL;
	h->branch = branch;
	h->branches = NULL;
	h->n_branches = 0;
	h->newer = NULL;
	h->n_newer = 0;
	h->older = NULL;
	h->n_older = 0;
	h->next = history->history;
	history->history = h;
	return h;
}


static struct branch *get_branches(git_repository *repo, unsigned *res_n)
{
	git_branch_iterator *iter;
	git_reference *ref;
	git_branch_t type;
	git_object *obj;
	struct branch *res = NULL;
	unsigned n = 0;

	if (git_branch_iterator_new(&iter, repo, GIT_BRANCH_ALL))
		pfatal_git("git_branch_iterator");
	while (!git_branch_next(&ref, &type, iter)) {
		/*
		 * @@@ is it okay to just ignore symbolic references ?
		 * E.g., remotes/origin/HEAD -> origin/master
		 */
		if (git_reference_type(ref) != GIT_REF_OID)
			continue;
		if (git_reference_peel(&obj, ref, GIT_OBJ_COMMIT))
			pfatal_git("git_reference_peel");

		res = realloc_type_n(res, struct branch, n + 1);
		if (git_branch_name(&res[n].name, ref))
			pfatal_git("git_branch_name");
		res[n].commit = (git_commit *) obj;
		n++;
	}
	git_branch_iterator_free(iter);
	*res_n = n;
	return res;
}


static const char **matching_branches(struct vcs_history *history,
    const struct git_commit *commit, unsigned *res_n)
{
	unsigned i, n = 0;
	const char **res;

	for (i = 0; i != history->n_branches; i++)
		if (history->branches[i].commit == commit)
			n++;
	res = alloc_type_n(const char *, n);
	n = 0;
	for (i = 0; i != history->n_branches; i++)
		if (history->branches[i].commit == commit)
			res[n++] = history->branches[i].name;
	*res_n = n;
	return res;
}


static void uplink(struct vcs_hist *down, struct vcs_hist *up)
{
	down->newer = realloc_type_n(down->newer, struct vcs_hist *,
	    down->n_newer + 1);
	down->newer[down->n_newer++] = up;
}


static struct vcs_hist *find_commit(struct vcs_history *history,
    const git_commit *commit)
{
	struct vcs_hist *h;

	/*
	 * @@@ should probably use
	 * git_oid_equal(git_object_id(a), git_object_id(b))
	 */

	for (h = history->history; h; h = h->next)
		if (h->commit == commit)
			break;
	return h;
}


static void recurse(struct vcs_history *history, struct vcs_hist *h,
    unsigned n_branches)
{
	unsigned n, i;

	n = git_commit_parentcount(h->commit);
	if (verbose > 2)
		progress(3, "commit %p: %u + %u", h->commit, n_branches, n);

	n_branches--;

	h->older = alloc_type_n(struct vcs_hist *, n);
	h->n_older = n;

	for (i = 0; i != n; i++) {
		git_commit *commit;
		struct vcs_hist *found = NULL;

		if (git_commit_parent(&commit, h->commit, i))
			pfatal_git("git_commit_parent");
		found = find_commit(history, commit);
		if (found) {
			uplink(found, h);
			h->older[i] = found;
		} else {
			struct vcs_hist *new;

			new = new_commit(history, n_branches);
			new->commit = commit;
			new->branches = matching_branches(history, commit,
			    &new->n_branches);
			h->older[i] = new;
			n_branches++;
			uplink(new, h);
			recurse(history, new, n_branches);
		}
	}
}


bool vcs_git_try(const char *path)
{
	git_repository *repo;

	git_init_once();

	if (git_repository_open_ext(&repo, path,
	    GIT_REPOSITORY_OPEN_CROSS_FS, NULL))
		return 0;
	return !git_repository_is_empty(repo);
}


struct vcs_history *vcs_git_history(const char *path)
{
	struct vcs_history *history;
	struct vcs_hist *head, *dirty;
	git_oid oid;

	history = alloc_type(struct vcs_history);
	history->history = NULL;

	head = new_commit(history, 0);

	git_init_once();

	if (git_repository_open_ext(&history->repo, path,
	    GIT_REPOSITORY_OPEN_CROSS_FS, NULL))
		pfatal_git(path);

	history->branches = get_branches(history->repo, &history->n_branches);

	if (git_reference_name_to_id(&oid, history->repo, "HEAD"))
		pfatal_git(git_repository_path(history->repo));

	if (git_commit_lookup(&head->commit, history->repo, &oid))
		pfatal_git(git_repository_path(history->repo));

	head->branches = matching_branches(history, head->commit,
	    &head->n_branches);

	recurse(history, head, 1);

	if (!git_repo_is_dirty(history->repo)) {
		history->head = head;
		return history;
	}

	dirty = new_commit(history, 0);
	dirty->older = alloc_type(struct vcs_hist *);
	dirty->older[0] = head;
	dirty->n_older = 1;
	uplink(head, dirty);

	history->head = dirty;
	return history;
}


/* ----- Get the head ------------------------------------------------------ */


struct vcs_hist *vcs_head(const struct vcs_history *history)
{
	return history->head;
}


/* ----- Get information about commit -------------------------------------- */


char *vcs_git_get_rev(struct vcs_hist *h)
{
	const git_oid *oid = git_commit_id(h->commit);
	char *s = alloc_size(GIT_OID_HEXSZ + 1);

	return git_oid_tostr(s, GIT_OID_HEXSZ + 1, oid);
}


const char *vcs_git_summary(struct vcs_hist *h)
{
	const char *summary;

	if (!h->commit)
		return "Uncommitted changes";
	summary = git_commit_summary(h->commit);
	if (summary)
		return summary;

	pfatal_git("git_commit_summary");
}


char *vcs_git_summary_for_pango(struct vcs_hist *h,
    char *(*formatter)(const char *fmt, ...))
{
	const char *summary;

	if (!h->commit)
		return formatter("<small>Uncommitted changes</small>");
	summary = git_commit_summary(h->commit);
	if (!summary)
		pfatal_git("git_commit_summary");

	if (h->n_branches)
		return formatter(
		    "<small><span background=\"#00e00080\"><b>%s</b>%s</span>"
		    " %s</small>",
		    h->branches[0], h->n_branches > 1 ? "+" : "", summary);
	else
		return formatter("<small>%s</small>", summary);
}


/*
 * @@@ This one is a bit inconvenient. It depends both on the information the
 * VCS provides, some of which is fairly generic, but some may not be, and
 * the very specific constraints imposed by the markup format of Pango. 
 */

char *vcs_git_long_for_pango(struct vcs_hist *h,
    char *(*formatter)(const char *fmt, ...))
{
	git_buf buf = { 0 };
	time_t commit_time;
	const git_signature *sig;
	char *s;

	if (!h->commit)
		return stralloc("Uncommitted changes");
	if (git_object_short_id(&buf, (git_object *) h->commit))
		goto fail;
	commit_time = git_commit_time(h->commit);
	sig = git_commit_committer(h->commit);

	if (h->n_branches) {
		unsigned i;
		unsigned len = 1;

		for (i = 0; i != h->n_branches; i++)
			len += strlen(h->branches[i]) + 1;

		char b[len + 1];

		*b = ' ';
		len = 1;
		for (i = 0; i != h->n_branches; i++) {
			strcpy(b + len, h->branches[i]);
			len += strlen(h->branches[i]);
			b[len++] = ' ';
		}
		b[len] = 0;

		s = formatter(
		    "<span background=\"#00e00080\"><b>%s</b></span>\n"
		    "<b>%s</b> %s%s &lt;%s&gt;<small>\n%s</small>",
		    b, buf.ptr, ctime(&commit_time), sig->name, sig->email,
		    git_commit_summary(h->commit));
	} else {
		s = formatter("<b>%s</b> %s%s &lt;%s&gt;<small>\n%s</small>",
		    buf.ptr, ctime(&commit_time), sig->name, sig->email,
		    git_commit_summary(h->commit));
	}
	git_buf_free(&buf);
	return s;

fail:
	pfatal_git("vcs_git_long_for_pango");
}


/* ----- Iteration --------------------------------------------------------- */


/*
 * We use the "seen" counter to make sure we only show a commit after all newer
 * commits have been shown. We could accomplish the same by reordering the
 * h->older array of all ancestors each time we find a branch, but this works
 * just as well, has only the small disadvantage that we're modifying the
 * history entries during traversal,  and is simpler.
 */

static void hist_iterate_recurse(struct vcs_hist *h,
    void (*fn)(void *user, struct vcs_hist *h), void *user)
{
	unsigned i;

	fn(user, h);
	for (i = 0; i != h->n_older; i++)
		if (++h->older[i]->seen == h->older[i]->n_newer)
			hist_iterate_recurse(h->older[i], fn, user);
}


void hist_iterate(struct vcs_history *history, struct vcs_hist *hist,
    void (*fn)(void *user, struct vcs_hist *h), void *user)
{
	struct vcs_hist *h;

	for (h = history->history; h; h = h->next)
		h->seen = 0;
	hist_iterate_recurse(hist, fn, user);
}


/* ----- Textual dump (mainly for debugging) ------------------------------- */


// http://stackoverflow.com/questions/12132862/how-do-i-get-the-name-of-the-current-branch-in-libgit2

static void dump_one(void *user, struct vcs_hist *h)
{
	git_buf buf = { 0 };
	unsigned i;

	if (!h->commit) {
		printf("dirty\n");
		return;
	}

	if (git_object_short_id(&buf, (git_object *) h->commit))
		pfatal_git("git_object_short_id");
	printf("%*s%s  ", 2 * h->branch, "", buf.ptr);
	git_buf_free(&buf);


	if (h->n_branches) {
		printf("[");
		for (i = 0; i != h->n_branches; i++)
			printf("%s%s", i ? " " : "", h->branches[i]);
		printf("] ");
	}

	printf("%s\n", vcs_git_summary(h));
}


void dump_hist(struct vcs_history *history, struct vcs_hist *h)
{
	hist_iterate(history, h, dump_one, NULL);
}
