/*
 * kicad/pl-render.c - RenderKiCad page layout
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
#include <string.h>

#include "misc/util.h"
#include "misc/diag.h"
#include "gfx/style.h"
#include "gfx/text.h"
#include "gfx/gfx.h"
#include "kicad/pl-common.h"
#include "kicad/pl.h"


/*
 * Eeschema works in mil
 * Page layouts are in mm
 */


static int mil(float mm)
{
	return mm / 25.4 * 1000;
}


static int coord(int v, int d, int o, int e)
{
	return d >= 0 ? o + v : e - v;
}


static void render_text(const struct pl_ctx *pl, const struct pl_obj *obj,
    int x, int y)
{
	struct text txt = {
		.s	= obj->s,
		.size	= mil(obj->ey ? obj->ey : pl->ty),
		.x	= x,
		.y	= y,
		.rot	= 0,
		.hor	= obj->hor,
		.vert	= obj->vert,
	};

	text_fig(&txt, COLOR_COMP_DWG, LAYER_COMP_DWG);
}


static void render_obj(const struct pl_ctx *pl, const struct pl_obj *obj,
    unsigned i, int w, int h)
{
	int xo = mil(pl->l);
	int yo = mil(pl->r);
	int xe = w - mil(pl->t);
	int ye = h - mil(pl->b);
	int x = mil(obj->x + i * obj->incrx);
	int y = mil(obj->y + i * obj->incry);
	int ex = mil(obj->ex + i * obj->incrx);
	int ey = mil(obj->ey + i * obj->incry);
	int ww = xe - xo;
	int hh = ye - yo;

	if (x < 0 || y < 0 || ex < 0 || ey < 0)
		return;
	if (x > ww || y > hh || ex > ww || ey > hh)
		return;

	x = coord(x, obj->dx, xo, xe);
	y = coord(y, obj->dy, yo, ye);
	ex = coord(ex, obj->edx, xo, xe);
	ey = coord(ey, obj->edy, yo, ye);

	switch (obj->type) {
	case pl_obj_rect:
		gfx_rect(x, y, ex, ey,
		    COLOR_COMP_DWG, COLOR_NONE, LAYER_COMP_DWG);
		break;
	case pl_obj_line: {
			int vx[] = { x, ex };
			int vy[] = { y, ey };

			gfx_poly(2, vx, vy,
			    COLOR_COMP_DWG, COLOR_NONE, LAYER_COMP_DWG);
		}
		break;
	case pl_obj_text:
		render_text(pl, obj, x, y);
		break;
	default:
		break;
	}
}


void pl_render(struct pl_ctx *pl, int w, int h)
{
	const struct pl_obj *obj;
	int i;

	for (obj = pl->objs; obj; obj = obj->next)
		for (i = 0; i != obj->repeat; i++)
			render_obj(pl, obj, i, w, h);
}
