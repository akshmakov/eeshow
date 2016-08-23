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

#define	_GNU_SOURCE	/* for asprintf */
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>

#include "misc/util.h"
#include "misc/diag.h"
#include "gfx/style.h"
#include "gfx/text.h"
#include "gfx/gfx.h"
#include "kicad/sch.h"
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


static char *expand(const struct pl_ctx *pl, const char *s,
    const struct sheet *sheets, const struct sheet *sheet)
{
	const struct sheet *sch;
	char *res = NULL;
	const char *p;
	unsigned size = 0;
	unsigned len;
	char *x;
	unsigned n;

	while (1) {
		p = strchr(s, '%');
		if (!p)
			break;
		switch (p[1]) {
		case '%':
			x = "%";
			break;
		case 'C':
			x = "%C";	// comment #n
			break;
		case 'D':
			x = "%D";	// date
			break;
		case 'F':
			x = "%F";	// file name
			break;
		case 'K':
			x = "%K";	// KiCad version
			break;
		case 'N':
			n = 0;
			for (sch = sheets; sch; sch = sch->next)
				n++;
			if (asprintf(&x, "%u", n)) {}
			break;
		case 'P':
			x = "%P";	// sheet path
			break;
		case 'R':
			x = "%R";	// revision
			break;
		case 'S':
			n = 1;
			for (sch = sheets; sch != sheet;
			    sch = sch->next)
				n++;
			if (asprintf(&x, "%u", n)) {}
			break;
		case 'T':
			x = (char *) sheet->title;
			break;
		case 'Y':
			x = "%Y";	// company name
			break;
		case 'Z':
			x = "%Z";	// paper format
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
    int x, int y, int inc,
    const struct sheet *sheets, const struct sheet *sheet)
{
	char *s = expand(pl, obj->s, sheets, sheet);
	struct text txt = {
		.size	= mil(obj->ey ? obj->ey : pl->ty),
		.x	= x,
		.y	= y,
		.rot	= 0,
		.hor	= obj->hor,
		.vert	= obj->vert,
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
	text_fig(&txt, COLOR_COMP_DWG, LAYER_COMP_DWG);
	free(s);
}


static void render_obj(const struct pl_ctx *pl, const struct pl_obj *obj,
    unsigned inc,
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
		render_text(pl, obj, x, y, inc, sheets, sheet);
		break;
	default:
		break;
	}
}


void pl_render(struct pl_ctx *pl, const struct sheet *sheets,
    const struct sheet *sheet)
{
	const struct pl_obj *obj;
	int i;

	for (obj = pl->objs; obj; obj = obj->next)
		for (i = 0; i != obj->repeat; i++)
			render_obj(pl, obj, i, sheets, sheet);
}
