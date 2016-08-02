/*
 * gfx.c - Generate graphical output for Eeschema items
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

#include "style.h"
#include "text.h"
#include "gfx.h"


void *gfx_ctx;

static const struct gfx_ops *gfx_ops;


void gfx_line(int sx, int sy, int ex, int ey, int color, unsigned layer)
{
	if (gfx_ops->line) {
		gfx_ops->line(gfx_ctx, sx, sy, ex, ey, color, layer);
		return;
	}

	int vx[] = { sx, ex };
	int vy[] = { sy, ey };

	gfx_poly(2, vx, vy, color, COLOR_NONE, layer);
}


void gfx_rect(int sx, int sy, int ex, int ey,
    int color, int fill_color, unsigned layer)
{
	if (gfx_ops->rect) {
		gfx_ops->rect(gfx_ctx, sx, sy, ex, ey,
		    color, fill_color, layer);
		return;
	}

	int vx[] = { sx, ex, ex, sx, sx };
	int vy[] = { sy, sy, ey, ey, sy };

	gfx_poly(5, vx, vy, color, fill_color, layer);
}


void gfx_poly(int points, const int x[points], const int y[points],
    int color, int fill_color, unsigned layer)
{
	gfx_ops->poly(gfx_ctx, points, x, y, color, fill_color, layer);
}


void gfx_circ(int x, int y, int r, int color, int fill_color, unsigned layer)
{
	gfx_ops->circ(gfx_ctx, x, y, r, color, fill_color, layer);
}


void gfx_arc(int x, int y, int r, int sa, int ea,
    int color, int fill_color, unsigned layer)
{
	gfx_ops->arc(gfx_ctx, x, y, r, sa, ea, color, fill_color, layer);
}


void gfx_text(int x, int y, const char *s, unsigned size,
    enum text_align align, int rot, unsigned color, unsigned layer)
{
	gfx_ops->text(gfx_ctx, x, y, s, size, align, rot, color, layer);
}


void gfx_tag(const char *s,
    unsigned points, const int x[points], int const y[points])
{
	if (gfx_ops->tag)
		gfx_ops->tag(gfx_ctx, s, points, x, y);
}


unsigned gfx_text_width(const char *s, unsigned size)
{
	return gfx_ops->text_width(gfx_ctx, s, size);
}


void gfx_init(const struct gfx_ops *ops, int argc, char *const *argv)
{
	gfx_ctx = ops->init(argc, argv);
	gfx_ops = ops;
}


void gfx_new_sheet(void)
{
	if (gfx_ops->new_sheet)
		gfx_ops->new_sheet(gfx_ctx);
}


bool gfx_multi_sheet(void)
{
	return !!gfx_ops->new_sheet;
}

void gfx_end(void)
{
	if (gfx_ops->end)
		gfx_ops->end(gfx_ctx);
}
