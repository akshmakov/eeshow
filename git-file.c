/*
 * git-file.c - Open and read a file from git version control system
 *
 * Written 2016 by Werner Almesberger
 * Copyright 2016 by Werner Almesberger
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#define	_GNU_SOURCE	/* for get_current_dir_name */
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>

#include <git2.h>

#include "util.h"
#include "main.h"
#include "file.h"
#include "git-util.h"
#include "git-file.h"


struct vcs_git {
	const char *name;
	const char *revision;
	const struct vcs_git *related;

	git_repository *repo;
	git_tree *tree;

	const void *data;
	unsigned size;
};


static git_repository *select_repo(const char *path)
{
	git_repository *repo = NULL;
	char *tmp = stralloc(path);
	char *slash;

	/*
	 * If we can't find a repo, this may be due to the file or directory
	 * the path points to not existing in the currently checked-out tree.
	 * So we trim off elements until we find a repository.
	 */
	while (1) {
		if (verbose > 2)
			fprintf(stderr, "trying \"%s\"\n", tmp);
		if (!git_repository_open_ext(&repo, *tmp ? tmp : "/",
		    GIT_REPOSITORY_OPEN_CROSS_FS, NULL))
			break;
		slash = strrchr(tmp, '/');
		if (!slash)
			break;
		*slash = 0;
	}
	free(tmp);
	return repo;
}


static git_tree *pick_revision(git_repository *repo, const char *revision)
{
	git_commit *commit;
	git_object *obj;
	git_tree *tree;

	if (git_revparse_single(&obj, repo, revision)) {
		const git_error *e = giterr_last();

		fprintf(stderr, "%s: %s\n",
		    git_repository_path(repo), e->message);
		exit(1);
	}

	if (git_object_type(obj) != GIT_OBJ_COMMIT) {
		fprintf(stderr, "%s: not a commit\n", revision);
		exit(1);
	}
	commit = (git_commit *) obj;

	if (git_commit_tree(&tree, commit)) {
		const git_error *e = giterr_last();

		fprintf(stderr, "%s: %s\n", revision, e->message);
		exit(1);
	}

	return tree;
}


static char *canonical_path_into_repo(const char *repo_dir, const char *path)
{
	struct stat repo_st, path_st;
	char *tmp, *tmp2, *slash, *tail, *real;
	char *to;
	const char *end, *from;

	/* identify inode of repo root */

	if (stat(repo_dir, &repo_st) < 0) {
		perror(repo_dir);
		exit(1);
	}
	if (!S_ISDIR(repo_st.st_mode)) {
		fprintf(stderr, "%s: not a directory\n", repo_dir);
		exit(1);
	}

	/* convert relative paths to absolute */

	if (*path == '/') {
		tmp = stralloc(path);
	} else {
		char *cwd = get_current_dir_name();

		tmp = alloc_size(strlen(cwd) + 1 + strlen(path) + 1);
		sprintf(tmp, "%s/%s", cwd, path);
		free(cwd);
	}

	/* remove trailing / */

	slash = strrchr(tmp, '/');
	if (slash && slash != tmp && !slash[1])
		*slash = 0;

	/*
	 * If path does point to inexistent object, separate into the part that
	 * is valid on the current system and the tail containing dead things.
	 */
	end = tail = strchr(tmp, 0);

	while (1) {
		if (verbose > 2)
			fprintf(stderr, "probing \"%s\" tail \"%s\"\n",
			    tmp, tail);
		if (stat(tmp, &path_st) == 0)
			break;
		if (!tmp[1]) {
			fprintf(stderr, "%s: cannot resolve\n", path);
			exit(1);
		}
		slash = strrchr(tmp, '/');
		if (tail != end)
			tail[-1] = '/';
		tail = slash + 1;
		*slash = 0;
	}

	/* remove . and .. from tail */

	if (verbose > 2)
		fprintf(stderr, "input tail \"%s\"\n", tail);
	from = to = tail;
	while (1) {
		if (!strncmp(from, "./", 2)) {
			from += 2;
			continue;
		}
		if (!strcmp(from, "."))
			break;
		if (strncmp(from, "../", 3) && strcmp(from, "..")) {
			while (*from) {
				*to++ = *from++;
				if (from[-1] == '/')
					break;
			}
			if (!*from)
				break;
		}
		if (to == tail) {
			/*
			 * We have something like this:
			 * /home/repo/dead/../../foo
			 */
			fprintf(stderr, "%s: can't climb out of dead path\n",
			    path);
			exit(1);
		}

		/*
		 * We have something like
		 * "foo/" -> ""
		 * or
		 * "foo/bar/" -> "foo/"
		 * where "to" points to the end.
		 */
		to--;
		while (to != tail && to[-1] != '/')
			to--;
	}
	*to = 0;
	if (verbose > 2)
		fprintf(stderr, "output tail \"%s\"\n", tail);

	/* resolve all symlinks */

	real = realpath(tmp, NULL);
	if (verbose > 2)
		fprintf(stderr, "realpath(\"%s\") = \"%s\"\n", tmp, real);

	/* append tail */

	if (*tail) {
		tmp2 = alloc_size(strlen(real) + 1 + strlen(tail) + 1);
		sprintf(tmp2, "%s/%s", real, tail);
		free(real);
	} else {
		tmp2 = real;
	}
	free(tmp);
	tmp = tmp2;

	if (verbose > 1)
		fprintf(stderr, "full object path \"%s\"\n", tmp);

	/* find which part of our path is inside the repo */

	end = tail = strchr(tmp, 0);
	while (1) {
		if (verbose > 2)
			fprintf(stderr, "trying \"%s\" tail \"%s\"\n",
			    tmp, tail);

		if (stat(tmp, &path_st) == 0 &&
		    path_st.st_dev == repo_st.st_dev &&
		    path_st.st_ino == repo_st.st_ino)
			break;

		/* "this cannot happen" */
		if (tail == tmp) {
			fprintf(stderr,
			    "divergent paths:\nrepo \"%s\"\nobject \"%s\"\n",
			    repo_dir, tmp);
			exit(1);
		}

		slash = strrchr(tmp, '/');
		if (tail != end)
			tail[-1] = '/';
		tail = slash + 1;
		*slash = 0;
	}

	if (verbose > 1)
		fprintf(stderr, "path in repo \"%s\"\n", tail);

	tmp2 = stralloc(tail);
	free(tmp);
	return tmp2;
}


static git_tree_entry *find_file(git_repository *repo, git_tree *tree,
    const char *path)
{
	git_tree_entry *entry;
	char *repo_path = stralloc(git_repository_path(repo));
	char *slash, *canon_path;
	int len;

	/* remove trailing / from repo_path */
	slash = strrchr(repo_path, '/');
	if (slash && slash != repo_path && !slash[1])
		*slash = 0;

	len = strlen(repo_path);
	if (len >= 5 && !strcmp(repo_path + len - 5, "/.git"))
		repo_path[len == 5 ? 1 : len - 5] = 0;

	if (verbose > 1)
		fprintf(stderr, "repo dir \"%s\"\n", repo_path);

	canon_path = canonical_path_into_repo(repo_path, path);

	if (git_tree_entry_bypath(&entry, tree, canon_path)) {
		const git_error *e = giterr_last();

		fprintf(stderr, "%s: %s\n", path, e->message);
		exit(1);
	}
	free(canon_path);

	return entry;
}


static const void *get_data(git_repository *repo, git_tree_entry *entry,
    unsigned *size)
{
	git_object *obj;
	git_blob *blob;

	if (git_tree_entry_type(entry) != GIT_OBJ_BLOB) {
		fprintf(stderr, "entry is not a blob\n");
		exit(1);
	}
	if (git_tree_entry_to_object(&obj, repo, entry)) {
		const git_error *e = giterr_last();

		fprintf(stderr, "%s\n", e->message);
		exit(1);
	}

	if (verbose > 2) {
		git_buf buf = { 0 };

		if (git_object_short_id(&buf, obj))  {
			const git_error *e = giterr_last();

			fprintf(stderr, "%s\n", e->message);
			exit(1);
		}
		fprintf(stderr, "object %s\n", buf.ptr);
		git_buf_free(&buf);
	}
	blob = (git_blob *) obj;
	*size = git_blob_rawsize(blob);
	return git_blob_rawcontent(blob);
}


static bool send_line(const char *s, unsigned len,
    bool (*parse)(const struct file *file, void *user, const char *line),
    void *user, const struct file *file)
{
	char *tmp = alloc_size(len + 1);
	bool res;

	memcpy(tmp, s, len);
	tmp[len] = 0;
	res = parse(file, user, tmp);
	free(tmp);
	return res;
}


static void access_file_data(struct vcs_git *vcs_git, const char *name)
{
	git_tree_entry *entry;

	entry = find_file(vcs_git->repo, vcs_git->tree, name);
	if (verbose)
		fprintf(stderr, "reading %s\n", name);

	vcs_git->data = get_data(vcs_git->repo, entry, &vcs_git->size);
}


static bool related_same_repo(struct vcs_git *vcs_git)
{
	const struct vcs_git *related = vcs_git->related;

	vcs_git->repo = related->repo;
	vcs_git->tree = related->tree;

	access_file_data(vcs_git, vcs_git->name);

	return 1;
}


static bool related_other_repo(struct vcs_git *vcs_git)
{
	/* @@@ find revision <= date of revision in related */
	fprintf(stderr, "warning: related_other_repo is not yet implemented\n");
	return 0;
}


static bool related_only_repo(struct vcs_git *vcs_git)
{
	const struct vcs_git *related = vcs_git->related;
	char *tmp;

	if (verbose > 1)
		fprintf(stderr, "trying graft \"%s\" \"%s\"\n",
		    related->name, vcs_git->name);
	tmp = file_graft_relative(related->name, vcs_git->name);
	if (!tmp)
		return 0;

	vcs_git->repo = related->repo;
	vcs_git->tree = related->tree;

	access_file_data(vcs_git, tmp);

	free((char *) vcs_git->name);
	vcs_git->name = tmp;

	return 1;
}


static bool try_related(struct vcs_git *vcs_git)
{
	if (!vcs_git->related)
		return 0;
	if (vcs_git->revision)
		return 0;

	vcs_git->repo = select_repo(vcs_git->name);
	if (vcs_git->repo) {
		if (!strcmp(git_repository_path(vcs_git->related->repo),
		    git_repository_path(vcs_git->repo)))
			return related_same_repo(vcs_git);
		else
			return related_other_repo(vcs_git);
	}

	return related_only_repo(vcs_git);
}


struct vcs_git *vcs_git_open(const char *revision, const char *name,
    const struct vcs_git *related)
{
	struct vcs_git *vcs_git = alloc_type(struct vcs_git);

	git_init_once();

	vcs_git->name = stralloc(name);
	vcs_git->revision = revision ? stralloc(revision) : NULL;
	vcs_git->related = related;

	if (try_related(vcs_git))
		return vcs_git;

	vcs_git->repo = select_repo(name);
	if (!vcs_git->repo) {
		fprintf(stderr, "%s: not found\n", name);
		exit(1);
	}
	if (verbose > 1)
		fprintf(stderr, "using repository %s\n",
		    git_repository_path(vcs_git->repo));

	if (!revision)
		revision = "HEAD";
	vcs_git->tree = pick_revision(vcs_git->repo, revision);

	access_file_data(vcs_git, name);

	return vcs_git;
}


bool vcs_git_read(void *ctx, struct file *file,
    bool (*parse)(const struct file *file, void *user, const char *line),
    void *user)
{
	const struct vcs_git *vcs_git = ctx;
	const char *end = vcs_git->data + vcs_git->size;
	const char *p = vcs_git->data;
	const char *nl;

	while (p != end) {
		nl = memchr(p, '\n', end - p);
		file->lineno++;
		if (!nl)
			return send_line(p, end - p, parse, user, file);
		if (!send_line(p, nl - p, parse, user, file))
			return 0;
		p = nl + 1;
	}
	return 1;
}


void vcs_git_close(void *ctx)
{
	struct vcs_git *vcs_git = ctx;

	free((char *) vcs_git->name);
	free((char *) vcs_git->revision);
	free(vcs_git);
}
