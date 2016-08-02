/*
 * main.h - Convert Eeschema schematics to FIG
 *
 * Written 2016 by Werner Almesberger
 * Copyright 2016 by Werner Almesberger
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#ifndef MAIN_H
#define	MAIN_H

#include <stdbool.h>


/*
 * 0: no progress indications
 * 1: reasonable progress indications
 * 2: verbose output
 * > 2: go wild !
 */

extern int verbose;


void usage(const char *name);

#endif /* !MAIN_H */
