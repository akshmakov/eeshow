/*
 * gfx/diff.c - Schematics difference
 *
 * Written 2016 by Werner Almesberger
 * Copyright 2016 by Werner Almesberger
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */


#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <assert.h>

#include <cairo/cairo.h>

#include "misc/util.h"
#include "misc/diag.h"
#include "main.h"
#include "gfx/cro.h"
#include "file/file.h"
#include "kicad/sch.h"
#include "kicad/lib.h"
#include "gfx/record.h"
#include "gfx/gfx.h"
#include "gfx/diff.h"


#define	DEFAULT_FRAME_RADIUS	30

#define	FADE_SHIFT	3
#define	FADE_MASK	((0xff >> FADE_SHIFT) * (0x010101))
#define	FADE_OFFSET	(~FADE_MASK & 0xffffff)

#define	MASK 0xffffff

/* steal from schhist/ppmdiff.c */

#define	ONLY_OLD	0xff5050
#define	ONLY_NEW	0x00c000
#define	BOTH		0x707070

#define	AREA_FILL	0xffd0f0


struct diff {
	struct gfx *gfx;
	uint32_t *new_img;
	int w, h, stride;
	const char *output_name;
	int frame_radius;
	struct area *areas;
};


/* ----- Wrappers ---------------------------------------------------------- */


static void diff_line(void *ctx, int sx, int sy, int ex, int ey,
    int color, unsigned layer)
{
	const struct diff *diff = ctx;

	gfx_line(diff->gfx, sx, sy, ex, ey, color, layer);
}


static void diff_poly(void *ctx,
    int points, const int x[points], const int y[points],
    int color, int fill_color, unsigned layer)
{
	const struct diff *diff = ctx;

	gfx_poly(diff->gfx, points, x, y, color, fill_color, layer);
}


static void diff_circ(void *ctx, int x, int y, int r,
    int color, int fill_color, unsigned layer)
{
	const struct diff *diff = ctx;

	gfx_circ(diff->gfx, x, y, r, color, fill_color, layer);
}


static void diff_arc(void *ctx, int x, int y, int r, int sa, int ea,
    int color, int fill_color, unsigned layer)
{
	const struct diff *diff = ctx;

	gfx_arc(diff->gfx, x, y, r, sa, ea, color, fill_color, layer);
}


static void diff_text(void *ctx, int x, int y, const char *s, unsigned size,
    enum text_align align, int rot, unsigned color, unsigned layer)
{
	const struct diff *diff = ctx;

	gfx_text(diff->gfx, x, y, s, size, align, rot, color, layer);
}


static unsigned diff_text_width(void *ctx, const char *s, unsigned size)
{
	const struct diff *diff = ctx;

	return gfx_text_width(diff->gfx, s, size);
}


/* ----- Initialization ---------------------------------------------------- */


static void *diff_init(void)
{
	struct diff *diff;

	diff = alloc_type(struct diff);
	diff->areas = NULL;

	diff->output_name = NULL;
	diff->frame_radius = DEFAULT_FRAME_RADIUS;
	diff->gfx = NULL;

	return diff;
}


static bool diff_args(void *ctx, int argc, char *const *argv)
{
	struct diff *diff = ctx;
	char c;
	int arg;
	struct sch_ctx new_sch;
	struct file sch_file;
	struct lib new_lib;

	sch_init(&new_sch, 0);
	lib_init(&new_lib);

	while ((c = getopt(argc, argv, "o:s:")) != EOF)
		switch (c) {
		case 'o':
			diff->output_name = optarg;
			break;
		case 's':
			/* for cro_png */
			break;
		default:
			usage(*argv);
		}

	if (argc - optind < 1)
		usage(*argv);

	if (!file_open(&sch_file, argv[argc - 1], NULL)) 
		goto fail_open;
	for (arg = optind; arg != argc - 1; arg++)
		if (!lib_parse(&new_lib, argv[arg], &sch_file))
			goto fail_parse;
	if (!sch_parse(&new_sch, &sch_file, &new_lib, NULL))
		goto fail_parse;
	file_close(&sch_file);

	diff->gfx = gfx_init(&cro_img_ops);
	if (!gfx_args(diff->gfx, argc, argv))
		goto fail_open;
	sch_render(new_sch.sheets, diff->gfx);
	diff->new_img = cro_img_end(gfx_user(diff->gfx),
	    &diff->w, &diff->h, &diff->stride);

	diff->gfx = gfx_init(&cro_img_ops);
	if (!gfx_args(diff->gfx, argc, argv))
		goto fail_open;
	//diff->gfx = cro_img_ops.init(argc, argv);

	return 1;

fail_parse:
	file_close(&sch_file);
fail_open:
	sch_free(&new_sch);
	lib_free(&new_lib);
	return 0;
}


/* ----- Area highlighting ------------------------------------------------- */


void add_area(struct area **areas, int xa, int ya, int xb, int yb,
    uint32_t color)
{
	struct area *area;

	area = alloc_type(struct area);

	area->xa = xa;
	area->ya = ya;
	area->xb = xb;
	area->yb = yb;
	area->color = color;

	area->next = *areas;
	*areas = area;
}


static void mark_area(struct diff *diff, int x, int y)
{
	struct area *area;
	int xa = x - diff->frame_radius;
	int ya = y - diff->frame_radius;
	int xb = x + diff->frame_radius;
	int yb = y + diff->frame_radius;

	for (area = diff->areas; area; area = area->next)
		if (x >= area->xa && x <= area->xb &&
		    y >= area->ya && y <= area->yb) {
			if (area->xa > xa)
				area->xa = xa;
			if (area->xb < xb)
				area->xb = xb;
			if (area->ya > ya)
				area->ya = ya;
			if (area->yb < yb)
				area->yb = yb;
			return;
		}

	add_area(&diff->areas, xa, ya, xb, yb, AREA_FILL);
}


static void complement_box(struct diff *diff, uint32_t *a,
    int xa, int ya, int xb, int yb, uint32_t color)
{
	int sx, sy, ex, ey;
	uint32_t *p;
	int x, y;

	sx = xa > 0 ? xa : 0;
	ex = xb < diff->w ? xb : diff->w;
	sy = ya > 0 ? ya : 0;
	ey = yb < diff->h ? yb : diff->h;

	if (sx >= ex || sy >= ey)
		return;

	for (y = sy; y != ey; y++) {
		p = a + y * (diff->stride >> 2);
		for (x = sx; x != ex; x++)
			if ((p[x] & MASK) == MASK)
				p[x] = color;
	}
}


static void show_areas(struct diff *diff, uint32_t *a)
{
	const struct area *area;

	for (area = diff->areas; area; area = area->next)
		complement_box(diff, a, area->xa, area->ya, area->xb, area->yb,
		    area->color);
}


void free_areas(struct area **areas)
{
	struct area *next;

	while (*areas) {
		next = (*areas)->next;
		free(*areas);
		*areas = next;
	}
}


/* ----- Termination ------------------------------------------------------- */


static void differences(struct diff *diff, uint32_t *a, const uint32_t *b)
{
	unsigned skip = diff->w * 4 - diff->stride;
	int x, y;

	for (y = 0; y != diff->h; y++) {
		for (x = 0; x != diff->w; x++) {
			if (!((*a ^ *b) & MASK)) {
				*a = ((*a >> FADE_SHIFT) & FADE_MASK) |
				    FADE_OFFSET;
			} else {
				mark_area(diff, x, y);
				*a = (*a & MASK) == MASK ? ONLY_NEW :
				    (*b & MASK) == MASK ? ONLY_OLD : BOTH;
			}
			a++;
			b++;
		}
		a += skip;
		b += skip;
	}
}


static void diff_end(void *ctx)
{
	struct diff *diff = ctx;
	uint32_t *old_img;
	int w, h, stride;

	assert(diff->gfx);
	old_img = cro_img_end(gfx_user(diff->gfx), &w, &h, &stride);
	if (diff->w != w || diff->h != h)
		fatal("%d x %d vs. %d x %d image\n", w, h, diff->w, diff->h);

	differences(diff, old_img, diff->new_img);
	show_areas(diff, old_img);
	free_areas(&diff->areas);

	cro_img_write(gfx_user(diff->gfx), diff->output_name);
}


/* ----- Diff to canvas ---------------------------------------------------- */


static void merge_coord(int pos_a, int pos_b, int dim_a, int dim_b,
    int *pos_res, int *res_dim)
{
	if (pos_a < pos_b) {
		*pos_res = pos_a;
		dim_b += pos_b - pos_a;
	} else {
		*pos_res = pos_b;
		dim_a += pos_a - pos_b;
	}
	*res_dim = dim_a > dim_b ? dim_a : dim_b;
}


void diff_to_canvas(cairo_t *cr, int cx, int cy, float scale,
    struct cro_ctx *old, struct cro_ctx *old_extra,
    struct cro_ctx *new, struct cro_ctx *new_extra,
    const struct area *areas)
{
	int old_xmin, old_ymin, old_w, old_h;
	int new_xmin, new_ymin, new_w, new_h;
	int xmin, ymin, w, h, stride;
	uint32_t *img_old, *img_new;
	double x1, y1, x2, y2;
	int sw, sh;
	cairo_t *old_cr;
	cairo_surface_t *s;

	cairo_clip_extents(cr, &x1, &y1, &x2, &y2);
	sw = x2 - x1;
	sh = y2 - y1;

	/* @@@ baeh ! */
	record_bbox((const struct record *) old,
	    &old_xmin, &old_ymin, &old_w, &old_h);
	record_bbox((const struct record *) new,
	    &new_xmin, &new_ymin, &new_w, &new_h);

	merge_coord(old_xmin, new_xmin, old_w, new_w, &xmin, &w);
	merge_coord(old_ymin, new_ymin, old_h, new_h, &ymin, &h);

	img_old = cro_img(old, old_extra,
	    sw / 2.0 - (cx + xmin) * scale,
	    sh / 2.0 - (cy + ymin) * scale,
	    sw, sh,
	    scale, &old_cr, &stride);
	img_new = cro_img(new, new_extra,
	    sw / 2.0 - (cx + xmin) * scale,
	    sh / 2.0 - (cy + ymin) * scale,
	    sw, sh,
	    scale, NULL, NULL);

	struct diff diff = {
		.w		= sw,
		.h		= sh,
		.stride		= stride,
		.frame_radius	= DEFAULT_FRAME_RADIUS,
		.areas		= NULL,
	};

	s = cairo_get_target(old_cr);
	cairo_surface_flush(s);
	differences(&diff, img_old, img_new);
	show_areas(&diff, img_old);
	free_areas(&diff.areas);
	if (areas) {
		diff.areas = (struct area *) areas;
		show_areas(&diff, img_old);
	}
	cairo_surface_mark_dirty(s);

	cairo_set_source_surface(cr, s, 0, 0);
	cairo_paint(cr);

	cairo_surface_destroy(s);
	cairo_destroy(old_cr);
	free(img_old);
	free(img_new);
}


/* ----- Operations -------------------------------------------------------- */


const struct gfx_ops diff_ops = {
	.name		= "diff",
	.line		= diff_line,
	.poly		= diff_poly,
	.circ		= diff_circ,
	.arc		= diff_arc,
	.text		= diff_text,
	.text_width	= diff_text_width,
	.init		= diff_init,
	.args		= diff_args,
	.end		= diff_end,
};
