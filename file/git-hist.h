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


struct hist {
	struct git_commit *commit; /* NULL if uncommitted changes */

	unsigned branch;	/* branch index */

	struct hist **newer;
	unsigned n_newer;

	struct hist **older;
	unsigned n_older;

	struct hist *next;	/* no specific order */
	unsigned seen;		/* for traversal */
};


bool vcs_git_try(const char *path);
struct hist *vcs_git_hist(const char *path);
char *vcs_git_get_rev(struct hist *h);
const char *vcs_git_summary(struct hist *hist);
char *vcs_git_long_for_pango(struct hist *hist,
    char *(*formatter)(const char *fmt, ...));
void hist_iterate(struct hist *h, 
    void (*fn)(void *user, struct hist *h), void *user);
void dump_hist(struct hist *h);

#endif /* !FILE_GIT_HIST_H */
