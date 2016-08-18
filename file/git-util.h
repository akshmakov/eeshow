/*
 * file/git-util.h - Git utility functions
 *
 * Written 2016 by Werner Almesberger
 * Copyright 2016 by Werner Almesberger
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#ifndef FILE_GIT_UTIL_H
#define	FILE_GIT_UTIL_H

#include <stdbool.h>

#include <git2.h>


bool git_repo_is_dirty(git_repository *repo);
void git_init_once(void);

#endif /* !FILE_GIT_UTIL_H */
