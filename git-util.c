/*
 * git-util.c - Git utility functions
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

#include <git2.h>

#include "git-util.h"


/*
 * Git documentation says that git_libgit2_init can be called more then once
 * but doesn't quite what happens then, e.g., whether references obtained
 * before an init (except for the first, of course) can still be used after
 * it. So we play it safe and initialize only once.
 */

void git_init_once(void)
{
	static bool initialized = 0;

	if (!initialized) {
		git_libgit2_init();
		initialized = 1;
	}
}
