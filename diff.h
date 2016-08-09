/*
 * diff.h - Schematics difference
 *
 * Written 2016 by Werner Almesberger
 * Copyright 2016 by Werner Almesberger
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */


#ifndef DIFF_H
#define	DIFF_H

#include <cairo/cairo.h>

#include "gfx.h"
#include "cro.h"


extern const struct gfx_ops diff_ops;


void diff_to_canvas(cairo_t *cr, int cx, int cy, float scale, 
    struct cro_ctx *old, struct cro_ctx *new);

#endif /* !DIFF_H */
