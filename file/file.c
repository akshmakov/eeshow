/*
 * file/file.c - Open and read a file
 *
 * Written 2016 by Werner Almesberger
 * Copyright 2016 by Werner Almesberger
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include <stddef.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "misc/util.h"
#include "misc/diag.h"
#include "file/git-file.h"
#include "file/file.h"


void *file_oid(const struct file *file)
{
	if (!file->vcs)
		return NULL;
	return vcs_git_get_oid(file->vcs);
}


bool file_oid_eq(const void *a, const void *b)
{
	/*
	 * If both a and b are NULL, we don't have revision data and thus
	 * can't tell if they're identical.
	 */
	return a && b && vcs_git_oid_eq(a, b);
}


bool file_cat(const struct file *file, void *user, const char *line)
{
	printf("%s\n", line);
	return 1;
}


char *file_graft_relative(const char *base, const char *name)
{
	const char *slash;
	char *res;
	unsigned len;

	if (*name == '/')
		return NULL;

	slash = strrchr(base, '/');
	if (!slash)
		return NULL;

	len = slash + 1 - base;
	res = alloc_size(len + strlen(name) + 1);
	memcpy(res, base, len);
	strcpy(res + len, name);

	return res;
}


static bool try_related(struct file *file)
{
	char *tmp;

	if (!file->related)
		return 0;

	tmp = file_graft_relative(file->related->name, file->name);
	if (!tmp)
		return NULL;

	if (*file->name == '/')
		return 0;

	file->file = fopen(tmp, "r");
	if (!file->file) {
		free(tmp);
		return 0;
	}

	progress(1, "reading %s\n", tmp);

	free((char *) file->name);
	file->name = tmp;
	return 1;
}


/*
 * @@@ logic isn't quite complete yet. It should go something like this:
 *
 * - if there is no related item,
 *   - try file,
 *   - if there is a colon, try VCS,
 *   - give up
 * - if there is a related item,
 *   - if it is a VCS,
 *     - try the revision matching or predating (if different repo) that of the
 *       related repo,
 (     - fall through and try as if it was a file
 *   - try opening as file. If this fails,
 *     - if the name of the file to open is absolute, give up
 *     - try `dirname related`/file_ope_open
 *     - give up
 *
 * @@@ should we see if non-VCS file is in a VCS ? E.g.,
 * /home/my-libs/foo.lib 1234:/home/my-project/foo.sch
 * or maybe use : as indictor for VCS, i.e.,
 * :/home/my-libs/foo.lib ...
 *
 * @@@ explicit revision should always win over related.
 */

static void *open_vcs(struct file *file)
{
	char *colon;

	colon = strchr(file->name, ':');
	if (colon) {
		char *tmp;

		tmp = stralloc(file->name);
		tmp[colon - file->name] = 0;
		file->vcs = vcs_git_open(tmp, colon + 1,
		    file->related ? file->related->vcs : NULL);
		if (file->vcs) {
			free(tmp);
			return file->vcs;
		}
		progress(2, "could not open %s:%s\n", tmp, colon + 1);
		return NULL;
	} else {
		file->vcs = vcs_git_open(NULL, file->name,
		    file->related ? file->related->vcs : NULL);
		if (file->vcs)
			return file->vcs;
		progress(2, "could not open %s\n", file->name);
		return NULL;
	}
}


static void file_init(struct file *file, const char *name,
    const struct file *related)
{
	file->name = stralloc(name);
	file->lineno = 0;
	file->related = related;
	file->file = NULL;
	file->vcs = NULL;
}


bool file_open(struct file *file, const char *name, const struct file *related)
{
	file_init(file, name, related);

	if (related && related->vcs) {
		file->vcs = open_vcs(file);
		if (file->vcs)
			return 1;
	}

	file->file = fopen(name, "r");
	if (file->file) {
		progress(1, "reading %s\n", name);
		return 1;
	}

	if (try_related(file))
		return 1;

	if (verbose)
		diag_perror(name);

	if (!strchr(name, ':')) {
		if (!verbose)
			diag_perror(name);
		goto fail;
	}

	file->vcs = open_vcs(file);
	if (file->vcs)
		return 1;

	error("could not open %s", name);
fail:
	free((char *) file->name);
	return 0;
}


bool file_open_revision(struct file *file, const char *rev, const char *name,
    const struct file *related)
{
	if (!rev)
		return file_open(file, name, related);

	file_init(file, name, related);
	file->vcs = vcs_git_open(rev, name, related ? related->vcs : NULL);
	if (file->vcs)
		return 1;
	progress(2, "could not open %s at %s\n", name, rev);
	return 0;
}


bool file_read(struct file *file,
    bool (*parse)(const struct file *file, void *user, const char *line),
    void *user)
{
	static char *buf = NULL;
	static size_t n = 0;
	char *nl;

	if (file->vcs)
		return vcs_read(file->vcs, file, parse, user);
	while (getline(&buf, &n, file->file) > 0) {
		nl = strchr(buf, '\n');
		if (nl)
			*nl = 0;
		file->lineno++;
		if (!parse(file, user, buf))
			return 0;
	}
	return 1;
}


void file_close(struct file *file)
{
	if (file->file)
		fclose(file->file);
	if (file->vcs)
		vcs_close(file->vcs);
	free((char *) file->name);
}
