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


static int cx(int x, int dx, int xo, int xe)
{
	return dx >= 0 ? xo + x : xe - x;
}


static int cy(int y, int dy, int yo, int ye)
{
	return dy >= 0 ? yo + y : ye - y;
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

	switch (obj->type) {
	case pl_obj_rect:
		gfx_rect(
		    cx(x, obj->dx, xo, xe), cy(y, obj->dy, yo, ye),
	    	    cx(ex, obj->edx, xo, xe), cy(ey, obj->edy, yo, ye),
		    COLOR_COMP_DWG, COLOR_NONE, LAYER_COMP_DWG);
		break;
	case pl_obj_line: {
			int vx[] = {
			    cx(x, obj->dx, xo, xe),
			    cx(ex, obj->edx, xo, xe)
			};
			int vy[] = {
			    cy(y, obj->dy, yo, ye),
			    cy(ey, obj->edy, yo, ye)
			};

			gfx_poly(2, vx, vy,
			    COLOR_COMP_DWG, COLOR_NONE, LAYER_COMP_DWG);
		}
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
