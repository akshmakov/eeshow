/*
 * gui/gui.h - GUI for eeshow
 *
 * Written 2016 by Werner Almesberger
 * Copyright 2016 by Werner Almesberger
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#ifndef GUI_GUI_H
#define	GUI_GUI_H

#include <stdbool.h>

/*
 * Note: this isn't (argc, argv) ! args stars right with the first file name
 * and there is no NULL at the end.
 */

int gui(unsigned n_args, char **args, bool recurse, int limit,
    struct pl_ctx *pl);

#endif /* !GUI_GUI_H */
