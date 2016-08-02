/*
 * file.c - Open and read a file
 *
 * Written 2016 by Werner Almesberger
 * Copyright 2016 by Werner Almesberger
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "util.h"
#include "main.h"
#include "git-file.h"
#include "file.h"


bool file_cat(const struct file *file, void *user, const char *line)
{
	printf("%s\n", line);
	return 1;
}


void file_open(struct file *file, const char *name,
    const struct file *related)
{
	char *colon, *tmp;

	file->name = stralloc(name);
	file->lineno = 0;
	file->related = related;
	file->vcs = NULL;

	file->file = fopen(name, "r");
	if (file->file) {
		if (verbose)
			fprintf(stderr, "reading %s\n", name);
		return;
	}

	if (verbose)
		perror(name);

	colon = strchr(name, ':');
	if (!colon) {
		if (!verbose)
			perror(name);
		exit(1);
	}

	tmp = stralloc(name);
	tmp[colon - name] = 0;
	file->vcs = vcs_git_open(tmp, colon + 1);
	if (!file->vcs) {
		fprintf(stderr, "could not open %s:%s\n", tmp, colon + 1);
		exit(1);
	}
	free(tmp);
}


void file_read(struct file *file,
    bool (*parse)(const struct file *file, void *user, const char *line),
    void *user)
{
	char buf[1000];
	char *nl;

	if (file->vcs) {
		vcs_read(file->vcs, file, parse, user);
		return;
	}
	while (fgets(buf, sizeof(buf), file->file)) {
		nl = strchr(buf, '\n');
		if (nl)
			*nl = 0;
		file->lineno++;
		if (!parse(file, user, buf))
			break;
	}
}


void file_close(struct file *file)
{
	if (file->file)
		fclose(file->file);
	if (file->vcs)
		vcs_close(file->vcs);
	free((char *) file->name);
}
