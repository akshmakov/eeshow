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
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <math.h>

#include "misc/util.h"
#include "misc/diag.h"
#include "gfx/style.h"
#include "gfx/text.h"
#include "gfx/gfx.h"
#include "kicad/sch.h"
#include "kicad/pl-common.h"
#include "kicad/pl.h"


/*
 * For uses where we take libraries from a .pro but don't want the page layout,
 * too. This is especially important in non-interactive diff mode.
 */

bool suppress_page_layout = 0;


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


static char *expand(const struct pl_ctx *pl, const char *s,
    const struct sheet *sheets, const struct sheet *sheet)
{
	const struct sheet *sch;
	char *res = NULL;
	const char *p;
	unsigned size = 0;
	unsigned len;
	char *x;
	bool do_free;
	unsigned n;

	while (1) {
		do_free = 0;
		p = strchr(s, '%');
		if (!p)
			break;
		switch (p[1]) {
		case '%':
			x = "%";
			break;
		case 'C':		// comment #n
			x = "%C";
			break;
		case 'D':		// date
			x = "%D";
			break;
		case 'F':		// file name
			x = sheet->file ? (char *) sheet->file : "";
			break;
		case 'K':		// KiCad version
			x = "%K";
			break;
		case 'N':		// number of sheets
			n = 0;
			for (sch = sheets; sch; sch = sch->next)
				n++;
			alloc_printf(&x, "%u", n);
			do_free = 1;
			break;
		case 'P':		// sheet path
			x = sheet->path ? (char *) sheet->path : "";
			break;
		case 'R':		// revision
			x = "%R";
			break;
		case 'S':		// sheet number
			n = 1;
			for (sch = sheets; sch != sheet;
			    sch = sch->next)
				n++;
			alloc_printf(&x, "%u", n);
			do_free = 1;
			break;
		case 'T':		// title
			x = sheet->title ? (char *) sheet->title : "";
			break;
		case 'Y':		// company name
			x = "%Y";
			break;
		case 'Z':		// paper format
			x = "%Z";
			break;
		default:
			x = "???";
			break;
		}
		len = strlen(x);
		res = realloc_size(res, size + p - s + len);
		memcpy(res + size, s, p - s);
		size += p - s;
		s = p + 2;
		memcpy(res + size, x, len);
		size += len;
		if (do_free)
			free(x);
	}

	len = strlen(s);
	res = realloc_size(res, size + len + 1);
	memcpy(res + size, s, len + 1);
	return res;
}


static char *increment(char *s, int inc, const char *range)
{
	char *t;
	unsigned len = strlen(s);
	int base, n;

	t = realloc_size(s, len + 2);
	t[len + 1] = 0;

	base = range[1] - range[0] + 1;
	n = t[len - 1] - range[0] + inc;
	t[len - 1] = n / base + range[0];
	t[len] = n % base + range[0];
	return t;
}


static void render_text(const struct pl_ctx *pl, const struct pl_obj *obj,
    struct gfx *gfx, int x, int y, int inc,
    const struct sheet *sheets, const struct sheet *sheet)
{
	char *s = expand(pl, obj->s, sheets, sheet);
	struct text txt = {
		.size	= mil(obj->ey ? obj->ey : pl->ty),
		.x	= x,
		.y	= y,
		.rot	= obj->rotate,
		.hor	= obj->hor,
		.vert	= obj->vert,
		.style	= text_normal,	// @@@
	};

	if (inc && *s) {
		char *end = strchr(s, 0) - 1;
		const char *range = NULL;

		if (isdigit(*end))
			range = "09";
		else if (isupper(*end))
			range = "AZ";
		else if (islower(*end))
			range = "az";
		if (range) {
			 if (*end + inc <= range[1])
				*end += inc;
			else
				s = increment(s, inc, range);
		}
	}
	txt.s = s;
	text_show(&txt, gfx, COLOR_COMP_DWG, LAYER_COMP_DWG);
	free(s);
}


static void render_poly(const struct pl_obj *obj, const struct pl_poly *poly,
    struct gfx *gfx, int x, int y)
{
	double a = obj->rotate / 180.0 * M_PI;
	const struct pl_point *p;
	unsigned n = 0;
	int px, py;

	for (p = poly->points; p; p = p->next)
		n++;

	int vx[n];
	int vy[n];

	n = 0;
	for (p = poly->points; p; p = p->next) {
		px = mil(p->x);
		py = mil(p->y);
		vx[n] = x + cos(a) * px + sin(a) * py;
		vy[n] = y + cos(a) * py - sin(a) * px;
		n++;
	}
	gfx_poly(gfx, n, vx, vy,
	    COLOR_COMP_DWG, COLOR_COMP_DWG, LAYER_COMP_DWG);
}


static void render_polys(const struct pl_obj *obj, struct gfx *gfx,
    int x, int y)
{
	const struct pl_poly *poly;

	for (poly = obj->poly; poly; poly = poly->next)
		render_poly(obj, poly, gfx, x, y);
}


static void render_obj(const struct pl_ctx *pl, const struct pl_obj *obj,
    struct gfx *gfx, unsigned inc,
    const struct sheet *sheets, const struct sheet *sheet)
{
	int w = sheet->w;
	int h = sheet->h;
	int xo = mil(pl->l);
	int yo = mil(pl->r);
	int xe = w - mil(pl->t);
	int ye = h - mil(pl->b);
	int x = mil(obj->x + inc * obj->incrx);
	int y = mil(obj->y + inc * obj->incry);
	int ex = mil(obj->ex + inc * obj->incrx);
	int ey = mil(obj->ey + inc * obj->incry);
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
		gfx_rect(gfx, x, y, ex, ey,
		    COLOR_COMP_DWG, COLOR_NONE, LAYER_COMP_DWG);
		break;
	case pl_obj_line: {
			int vx[] = { x, ex };
			int vy[] = { y, ey };

			gfx_poly(gfx, 2, vx, vy,
			    COLOR_COMP_DWG, COLOR_NONE, LAYER_COMP_DWG);
		}
		break;
	case pl_obj_text:
		render_text(pl, obj, gfx, x, y, inc, sheets, sheet);
		break;
	case pl_obj_poly:
		render_polys(obj, gfx, x, y);
		break;
	default:
		break;
	}
}


void pl_render(struct pl_ctx *pl, struct gfx *gfx, const struct sheet *sheets,
    const struct sheet *sheet)
{
	const struct pl_obj *obj;
	int i;

	if (suppress_page_layout)
		return;
	for (obj = pl->objs; obj; obj = obj->next)
		for (i = 0; i != obj->repeat; i++)
			if (obj->pc == pc_none ||
			    (obj->pc == pc_only_one) == (sheets == sheet))
				render_obj(pl, obj, gfx, i, sheets, sheet);
}
