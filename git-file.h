/*
 * git-file.h - Open and read a file from git version control system
 *
 * Written 2016 by Werner Almesberger
 * Copyright 2016 by Werner Almesberger
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#ifndef GIT_FILE_H
#define	GIT_FILE_H

#include <stdbool.h>


void vcs_git_read(const char *revision, const char *name,
    bool (*parse)(void *user, const char *line), void *user);

#endif /* !GIT_FILE_H */
