/*
 * kicad/pl.h - KiCad page layout
 *
 * Written 2016 by Werner Almesberger
 * Copyright 2016 by Werner Almesberger
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */


#ifndef KICAD_PL_H
#define	KICAD_PL_H

#include "file/file.h"


struct pl_ctx;


void pl_render(struct pl_ctx *pl, int w, int h);

struct pl_ctx *pl_parse(struct file *file);
void pl_free(struct pl_ctx *pl);

#endif /* !KICAD_PL_H */
