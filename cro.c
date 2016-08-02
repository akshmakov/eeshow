/*
 * cro.c - Cairo graphics back-end
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
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <math.h>

#include <cairo/cairo.h>
#include <cairo/cairo-pdf.h>

#include "util.h"
#include "style.h"
#include "text.h"
#include "gfx.h"
#include "record.h"
#include "main.h"
#include "cro.h"


/*
 * FIG works with 1/1200 in
 * KiCad works with mil
 * 1 point = 1/72 in
 */

#define	DEFAULT_SCALE	(72.0 / 1200)


struct cro_ctx {
	struct record record;	/* must be first */

	int xo, yo;
	float scale;

	cairo_t *cr;
	cairo_surface_t *s;

	struct record *sheets;	/* for PDF */
	unsigned n_sheets;

	const char *output_name;
};


static inline int cd(struct cro_ctx *cc, int x)
{
	return x * cc->scale;
}


static inline int cx(struct cro_ctx *cc, int x)
{
	return cc->xo + x * cc->scale;
}


static inline int xc(struct cro_ctx *cc, int x)
{
	return (x - cc->xo) / cc->scale;
}


static inline int cy(struct cro_ctx *cc, int y)
{
	return cc->yo + y * cc->scale;
}


static inline float pt(struct cro_ctx *cc, int x)
{
	return cd(cc, x) * 72 * 1.5 / 1200.0;
}


static void set_color(cairo_t *cr, int color)
{
	uint32_t c;

	if (color < 0)
		return;
	c = color_rgb[color];
	cairo_set_source_rgb(cr, (c >> 16) / 255.0, ((c >> 8) & 255) / 255.0,
	    (c & 255) / 255.0);
}


static void paint(cairo_t *cr, int color, int fill_color)
{
	if (fill_color != COLOR_NONE) {
		set_color(cr, fill_color);
		if (color == COLOR_NONE)
			cairo_fill(cr);
		else
			cairo_fill_preserve(cr);
	}
	if (color != COLOR_NONE) {
		set_color(cr, color);
		cairo_stroke(cr);
	}
}


/* ----- General items ----------------------------------------------------- */


static void cr_line(void *ctx, int sx, int sy, int ex, int ey,
    int color, unsigned layer)
{
	struct cro_ctx *cc = ctx;
	static const double dashes[] = { 4, 2 };

	cairo_new_path(cc->cr);
	cairo_move_to(cc->cr, cx(cc, sx), cy(cc, sy));
	cairo_line_to(cc->cr, cx(cc, ex), cy(cc, ey));
	cairo_set_dash(cc->cr, dashes, ARRAY_ELEMENTS(dashes), 0);
	paint(cc->cr, color, COLOR_NONE);
	cairo_set_dash(cc->cr, NULL, 0, 0);
}


static void cr_poly(void *ctx,
    int points, const int x[points], const int y[points],
    int color, int fill_color, unsigned layer)
{
	struct cro_ctx *cc = ctx;
	bool closed;
	int i;

	if (points < 2)
		return;
	closed = x[0] == x[points - 1] && y[0] == y[points - 1];

	cairo_new_path(cc->cr);
	cairo_move_to(cc->cr, cx(cc, x[0]), cy(cc, y[0]));

	for (i = 1; i != points - closed; i++)
		cairo_line_to(cc->cr, cx(cc, x[i]), cy(cc, y[i]));
	if (closed)
		cairo_close_path(cc->cr);

	paint(cc->cr, color, fill_color);
}


static void cr_circ(void *ctx, int x, int y, int r,
    int color, int fill_color, unsigned layer)
{
	struct cro_ctx *cc = ctx;

	cairo_new_path(cc->cr);
	cairo_arc(cc->cr, cx(cc, x), cy(cc, y), cd(cc, r), 0, 2 * M_PI);
	paint(cc->cr, color, fill_color);
}


static void cr_arc(void *ctx, int x, int y, int r, int sa, int ea,
    int color, int fill_color, unsigned layer)
{
	struct cro_ctx *cc = ctx;

	cairo_new_path(cc->cr);
	cairo_arc(cc->cr, cx(cc, x), cy(cc, y), cd(cc, r),
	    -ea / 180.0 * M_PI, -sa / 180.0 * M_PI);
	paint(cc->cr, color, fill_color);
}


#define	TEXT_STRETCH	1.3


static void cr_text(void *ctx, int x, int y, const char *s, unsigned size,
    enum text_align align, int rot, unsigned color, unsigned layer)
{
	struct cro_ctx *cc = ctx;
	cairo_text_extents_t ext;
	cairo_matrix_t m;

	cairo_set_font_size(cc->cr, cd(cc, size) * TEXT_STRETCH);
	cairo_text_extents(cc->cr, s, &ext);

	set_color(cc->cr, color);

	cairo_move_to(cc->cr, cx(cc, x), cy(cc, y));

	cairo_get_matrix(cc->cr, &m);
	cairo_rotate(cc->cr, -rot / 180.0 * M_PI);

	switch (align) {
	case text_min:
		break;
	case text_mid:
		cairo_rel_move_to(cc->cr, -ext.width / 2.0, 0);
		break;
	case text_max:
		cairo_rel_move_to(cc->cr, -ext.width, 0);
		break;
	default:
		abort();
	}

	cairo_show_text(cc->cr, s);
	cairo_set_matrix(cc->cr, &m);
}


static unsigned cr_text_width(void *ctx, const char *s, unsigned size)
{
	struct cro_ctx *cc = ctx;
	cairo_text_extents_t ext;

	cairo_set_font_size(cc->cr, cx(cc, size) * TEXT_STRETCH);
	cairo_text_extents(cc->cr, s, &ext);
	return xc(cc, ext.width) * 1.05; /* @@@ Cairo seems to underestimate */
}


/* ----- Initializatio and termination ------------------------------------- */


static const struct gfx_ops real_cro_ops = {
	.name		= "cairo",
	.line		= cr_line,
	.poly		= cr_poly,
	.circ		= cr_circ,
	.arc		= cr_arc,
	.text		= cr_text,
	.text_width	= cr_text_width,
};


static struct cro_ctx *init_common(int argc, char *const *argv)
{
	struct cro_ctx *cc;
	char c;

	cc = alloc_type(struct cro_ctx);
	cc->xo = cc->yo = 0;
	cc->scale = DEFAULT_SCALE;

	cc->sheets = NULL;
	cc->n_sheets = 0;

	cc->output_name = NULL;
	while ((c = getopt(argc, argv, "o:s:")) != EOF)
		switch (c) {
		case 'o':
			cc->output_name = optarg;
			break;
		case 's':
			cc->scale = atof(optarg) * DEFAULT_SCALE;
			break;
		default:
			usage(*argv);
		}

	record_init(&cc->record, &real_cro_ops, cc);

	return cc;
}


static void *cr_png_init(int argc, char *const *argv)
{
	struct cro_ctx *cc;

	cc = init_common(argc, argv);

	/* cr_text_width needs *something* to work with */

	cc->s = cairo_image_surface_create(CAIRO_FORMAT_RGB24, 16, 16);
	cc->cr = cairo_create(cc->s);

	return cc;
}


static void *cr_pdf_init(int argc, char *const *argv)
{
	struct cro_ctx *cc;

	cc = init_common(argc, argv);

	/* cr_text_width needs *something* to work with */

	cc->s = cairo_pdf_surface_create(NULL, 16, 16);
	cc->cr = cairo_create(cc->s);

	return cc;
}


static void end_common(struct cro_ctx *cc, int *w, int *h)
{
	int x, y;

	cairo_surface_destroy(cc->s);
	cairo_destroy(cc->cr);

	record_bbox(&cc->record, &x, &y, w, h);

//	fprintf(stderr, "%dx%d%+d%+d\n", *w, *h, x, y);
	cc->xo = -cd(cc, x);
	cc->yo = -cd(cc, y);
	*w = cd(cc, *w);
	*h = cd(cc, *h);
//	fprintf(stderr, "%dx%d%+d%+d\n", *w, *h, x, y);
}


static void cr_png_end(void *ctx)
{
	struct cro_ctx *cc = ctx;
	int w, h;

	end_common(cc, &w, &h);

	cc->s = cairo_image_surface_create(CAIRO_FORMAT_RGB24, w, h);
	cc->cr = cairo_create(cc->s);

	set_color(cc->cr, COLOR_WHITE);
	cairo_paint(cc->cr);

	cairo_select_font_face(cc->cr, "Helvetica", CAIRO_FONT_SLANT_NORMAL,
	    CAIRO_FONT_WEIGHT_BOLD);
	cairo_set_line_width(cc->cr, 2);

	record_replay(&cc->record);
	record_destroy(&cc->record);

	if (cc->output_name)
		cairo_surface_write_to_png(cc->s, cc->output_name);
}


static void cr_pdf_new_sheet(void *ctx)
{
	struct cro_ctx *cc = ctx;

	cc->n_sheets++;
	cc->sheets = realloc(cc->sheets, sizeof(struct record) * cc->n_sheets);
	if (!cc->sheets) {
		perror("realloc");
		exit(1);
	}
	cc->sheets[cc->n_sheets - 1] = cc->record;
	record_wipe(&cc->record);
}


static void cr_pdf_end(void *ctx)
{
	struct cro_ctx *cc = ctx;
	int w, h;
	unsigned i;

	end_common(cc, &w, &h);

	cc->s = cairo_pdf_surface_create(cc->output_name, w, h);
	cc->cr = cairo_create(cc->s);

	cairo_select_font_face(cc->cr, "Helvetica", CAIRO_FONT_SLANT_NORMAL,
	    CAIRO_FONT_WEIGHT_BOLD);
	cairo_set_line_width(cc->cr, 2);

	for (i = 0; i != cc->n_sheets; i++) {
		set_color(cc->cr, COLOR_WHITE);
		cairo_paint(cc->cr);

		record_replay(cc->sheets + i);
		record_destroy(cc->sheets + i);

		cairo_show_page(cc->cr);
	}

	record_replay(&cc->record);
	record_destroy(&cc->record);

	cairo_show_page(cc->cr);

	cairo_surface_destroy(cc->s);
	cairo_destroy(cc->cr);
}


uint32_t *cro_img_end(void *ctx, int *w, int *h, int *stride)
{
	struct cro_ctx *cc = ctx;
	uint32_t *data;

	end_common(cc, w, h);

	*stride = cairo_format_stride_for_width(CAIRO_FORMAT_RGB24, *w);
	data = alloc_size(*stride * *h);

	cc->s = cairo_image_surface_create_for_data((unsigned char  *) data,
	    CAIRO_FORMAT_RGB24, *w, *h, *stride);
	cc->cr = cairo_create(cc->s);

	set_color(cc->cr, COLOR_WHITE);
	cairo_paint(cc->cr);

	cairo_select_font_face(cc->cr, "Helvetica", CAIRO_FONT_SLANT_NORMAL,
	    CAIRO_FONT_WEIGHT_BOLD);
	cairo_set_line_width(cc->cr, 2);

	record_replay(&cc->record);
	record_destroy(&cc->record);

	return data;
}


void cro_img_write(void *ctx, const char *name)
{
	struct cro_ctx *cc = ctx;

	cairo_surface_write_to_png(cc->s, name);
}


/* ----- Operations -------------------------------------------------------- */


const struct gfx_ops cro_png_ops = {
	.name		= "png",
	.line		= record_line,
	.poly		= record_poly,
	.circ		= record_circ,
	.arc		= record_arc,
	.text		= record_text,
	.text_width	= cr_text_width,
	.init		= cr_png_init,
	.end		= cr_png_end,
};

const struct gfx_ops cro_pdf_ops = {
	.name		= "pdf",
	.line		= record_line,
	.poly		= record_poly,
	.circ		= record_circ,
	.arc		= record_arc,
	.text		= record_text,
	.text_width	= cr_text_width,
	.init		= cr_pdf_init,
	.new_sheet	= cr_pdf_new_sheet,
	.end		= cr_pdf_end,
};
