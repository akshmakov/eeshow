/*
 * file/git-hist.h - Retrieve revision history from GIT repo
 *
 * Written 2016 by Werner Almesberger
 * Copyright 2016 by Werner Almesberger
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#ifndef FILE_GIT_HIST_H
#define	FILE_GIT_HIST_H

#include <stdbool.h>

#include <git2.h>


struct vcs_hist {
	struct git_commit *commit; /* NULL if uncommitted changes */

	unsigned branch;	/* branch index */

	const char **branches;	/* NULL if none */
	unsigned n_branches;

	struct vcs_hist **newer;
	unsigned n_newer;

	struct vcs_hist **older;
	unsigned n_older;

	struct vcs_hist *next;	/* no specific order */
	unsigned seen;		/* for traversal */
};

struct vcs_history;


bool vcs_git_try(const char *path);
struct vcs_history *vcs_git_history(const char *path);
struct vcs_hist *vcs_head(const struct vcs_history *history);

char *vcs_git_get_rev(struct vcs_hist *h);
const char *vcs_git_summary(struct vcs_hist *hist);
char *vcs_git_long_for_pango(struct vcs_hist *hist,
    char *(*formatter)(const char *fmt, ...));

void hist_iterate(struct vcs_history *history, struct vcs_hist *h, 
    void (*fn)(void *user, struct vcs_hist *h), void *user);
void dump_hist(struct vcs_history *history, struct vcs_hist *h);

#endif /* !FILE_GIT_HIST_H */
