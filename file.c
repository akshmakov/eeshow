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


bool file_cat(void *user, const char *line)
{
	printf("%s\n", line);
	return 1;
}


static void read_from_file(FILE *file,
    bool (*parse)(void *user, const char *line), void *user)
{
	char buf[1000];
	char *nl;

	while (fgets(buf, sizeof(buf), file)) {
		nl = strchr(buf, '\n');
		if (nl)
			*nl = 0;
		if (!parse(user, buf))
			break;
	}
}


void file_read(const char *name, bool (*parse)(void *user, const char *line),
    void *user)
{
	FILE *file;
	char *colon, *tmp;

	file = fopen(name, "r");
	if (file) {
		if (verbose)
			fprintf(stderr, "reading %s\n", name);
		read_from_file(file, parse, user);
		fclose(file);
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
	vcs_git_read(tmp, colon + 1, parse, user);
	free(tmp);
}
