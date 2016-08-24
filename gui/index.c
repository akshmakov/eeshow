/*
 * gui/index.c - Sheet index
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
#include <assert.h>

#include <gtk/gtk.h>

#include "gfx/record.h"
#include "kicad/delta.h"
#include "gui/aoi.h"
#include "gui/style.h"
#include "gui/over.h"
#include "gui/input.h"
#include "gui/help.h"
#include "gui/common.h"


#define	SHEET_MAX_W	200
#define	SHEET_MAX_NAME	300
#define	SHEET_ASPECT	1.4146	/* width / height */
#define	SHEET_PAD	3
#define	SHEET_GAP	12	/* not counting the padding ! */
#define	INDEX_MARGIN	10	/* margin on each side */


/* @@@ clean all this up and move into gui_ctx */
static unsigned thumb_rows, thumb_cols;
static unsigned thumb_w, thumb_h;
static struct overlay *name_over = NULL;
static const struct gui_sheet *curr_sheet = NULL;


/* ----- Tools ------------------------------------------------------------- */


static void thumbnail_pos(const struct gui_ctx *ctx, GtkAllocation *alloc,
    unsigned n, int *ix, int *iy)
{
	*ix = alloc->width / 2 + (thumb_w + SHEET_GAP) *
	    (n % thumb_cols - (thumb_cols - 1) / 2.0);
	*iy = alloc->height / 2 + (thumb_h + SHEET_GAP) *
	    (n / thumb_cols - (thumb_rows - 1) / 2.0);
}


/* ----- Drawing ----------------------------------------------------------- */


static struct gui_sheet *sheets(const struct gui_ctx *ctx)
{
	if (ctx->old_hist && ctx->diff_mode == diff_old)
		return ctx->old_hist->sheets;
	else
		return ctx->new_hist->sheets;
}


void index_draw_event(const struct gui_ctx *ctx, cairo_t *cr)
{
	GtkAllocation alloc;
	const struct gui_sheet *sheet;
	unsigned n = 0;
	int ix, iy, x, y;
	int w, h;
	int named = -1;

	gtk_widget_get_allocation(ctx->da, &alloc);

	cairo_set_source_rgba(cr, 1, 1, 1, 0.7);
	cairo_paint(cr);

	n = 0;
	for (sheet = sheets(ctx); sheet; sheet = sheet->next) {
		thumbnail_pos(sheet->ctx, &alloc, n, &ix, &iy);
		x = ix - thumb_w / 2 - SHEET_PAD;
		y = iy - thumb_h / 2 - SHEET_PAD;

		overlay_draw(sheet->thumb_over, cr, x, y, 1, 1);
		if (name_over && curr_sheet == sheet)
			named = n;
		n++;
	}

	if (named == -1)
		return;

	thumbnail_pos(curr_sheet->ctx, &alloc, named, &ix, &iy);
	overlay_size(name_over,
	    gtk_widget_get_pango_context(curr_sheet->ctx->da), &w, &h);
	x = ix - w / 2;
	if (x < INDEX_MARGIN)
		x = INDEX_MARGIN;
	if (x + w >= alloc.width - INDEX_MARGIN)
		x = alloc.width - w - INDEX_MARGIN;
	overlay_draw(name_over, cr, x, iy - h / 2, 1, 1);
}


/* ----- Thumbnail actions ------------------------------------------------- */


static void close_index(struct gui_ctx *ctx)
{
	overlay_remove_all(&ctx->thumb_overlays);
	name_over = NULL;
	ctx->mode = showing_sheet;
	input_pop();
	redraw(ctx);
}


static void thumb_click(void *user)
{
	struct gui_sheet *sheet = user;
	struct gui_ctx *ctx = sheet->ctx;

	go_to_sheet(ctx, sheet);
	close_index(ctx);
}


static void thumb_set_style(struct gui_sheet *sheet, bool selected)
{
	struct gui_ctx *ctx = sheet->ctx;
	struct overlay_style style = overlay_style_dense;
	const struct gui_sheet *old;

	style.radius = 3;
	style.pad = SHEET_PAD;
	style.bg = RGBA(1, 1, 1, 0.8);

	if (selected) {
		style.width = 2;
		style.frame = RGBA(0, 0, 0, 1);
		style.bg = RGBA(1, 1, 1, 1);
	}

	if (ctx->old_hist && ctx->diff_mode == diff_delta) {
		old = find_corresponding_sheet(ctx->old_hist->sheets,
		    ctx->new_hist->sheets, sheet);
		if (!sheet_eq(sheet->sch, old->sch))
			style.bg = RGBA(1.0, 1.0, 0, 1);
	}

	overlay_style(sheet->thumb_over, &style);
}


static bool thumb_hover(void *user, bool on, int dx, int dy)
{
	struct gui_sheet *sheet = user;
	struct gui_ctx *ctx = sheet->ctx;
	struct overlay_style style = overlay_style_default;

	if (on) {

		thumb_set_style(sheet, 1);
		name_over = overlay_add(&ctx->thumb_overlays, &ctx->aois,
		    NULL, NULL, NULL);
		if (sheet->sch && sheet->sch->title)
			overlay_text(name_over, "%s", sheet->sch->title);
		else
			overlay_text(name_over, "???");
		style.font = BOLD_FONT_LARGE;
		style.width = 0;
		style.wmax = SHEET_MAX_NAME;
		overlay_style(name_over, &style);
		curr_sheet = sheet;
	} else {
		thumb_set_style(sheet, 0);
		overlay_remove(&ctx->thumb_overlays, name_over);
		name_over = NULL;
	}
	redraw(ctx);
	return 1;
}


/* ----- Rendering to cache ------------------------------------------------ */


static bool best_ratio(const struct gui_ctx *ctx)
{
	GtkAllocation alloc;
	const struct gui_sheet *sheet;
	unsigned n = 0;
	unsigned r, c;
	float size, best_size = 0;
	int aw, ah;	/* available size */
	int w, h;

	gtk_widget_get_allocation(ctx->da, &alloc);

	for (sheet = sheets(ctx); sheet; sheet = sheet->next)
		n++;
	assert(n);

	for (r = 1; r <= n; r++) {
		c = (n + r - 1) / r;
		aw = alloc.width - (c - 1) * SHEET_GAP - 2 * INDEX_MARGIN;
		ah = alloc.height - (r - 1) * SHEET_GAP - 2 * INDEX_MARGIN;
		if (aw < 0 || ah < 0)
			continue;
		w = aw / c;
		h = ah / r;
		if (w > SHEET_MAX_W)
			w = SHEET_MAX_W;
		if (h * SHEET_ASPECT > w)
			 h = w / SHEET_ASPECT;
		if (w / SHEET_ASPECT > h)
			w = h * SHEET_ASPECT;
		if (!w || !h)
			continue;
		size = ((c - 1) * (w + SHEET_GAP) + w) *
		    ((r - 1) * (h + SHEET_GAP) + h);
		if (size > best_size) {
			best_size = size;
			thumb_cols = c;
			thumb_rows = r;
			thumb_w = w;
			thumb_h = h;
		}
	}

	return best_size;
}


static void index_render_sheet(const struct gui_ctx *ctx,
    struct gui_sheet *sheet)
{
	int xmin, ymin, w, h;
	float fw, fh, f;
	int xo, yo;

	if (!sheet->gfx_ctx_thumb) {
		char *argv[] = { "index", NULL };

		gfx_init(&cro_canvas_ops, 1, argv);
		sch_render(sheet->sch);
		cro_canvas_end(gfx_ctx, NULL, NULL, NULL, NULL);
       		sheet->gfx_ctx_thumb = gfx_ctx;
	}

	if (sheet->thumb_surf &&
	    sheet->thumb_w == thumb_w && sheet->thumb_h == thumb_h)
		return;

	if (sheet->thumb_surf) {
		// @@@ free data
		cairo_surface_destroy(sheet->thumb_surf);
		sheet->thumb_surf = NULL;
	}

	record_bbox((const struct record *) sheet->gfx_ctx_thumb,
	    &xmin, &ymin, &w, &h);
	if (!w || !h)
		return;

	fw = (float) thumb_w / w;
	fh = (float) thumb_h / h;
	f = fw < fh ? fw : fh;

	xo = -(xmin + w / 2) * f + thumb_w / 2;
	yo = -(ymin + h / 2) * f + thumb_h / 2;
	cro_img(sheet->gfx_ctx_thumb, NULL, xo, yo, thumb_w, thumb_h,  f,
	    NULL, NULL);

	sheet->thumb_surf = cro_img_surface(sheet->gfx_ctx_thumb);
	sheet->thumb_w = thumb_w;
	sheet->thumb_h = thumb_h;
}


static void index_add_overlay(struct gui_ctx *ctx, struct gui_sheet *sheet)
{
	sheet->thumb_over = overlay_add(&ctx->thumb_overlays, &ctx->aois,
	    thumb_hover, thumb_click, sheet);
	overlay_icon(sheet->thumb_over, sheet->thumb_surf);
	thumb_set_style(sheet, 0);
}


static void index_render_sheets(struct gui_ctx *ctx)
{
	struct gui_sheet *sheet;

	for (sheet = sheets(ctx); sheet; sheet = sheet->next) {
		index_render_sheet(ctx, sheet);
		index_add_overlay(ctx, sheet);
	}
}


/* ----- Input ------------------------------------------------------------- */


static bool index_hover_update(void *user, int x, int y)
{
	struct gui_ctx *ctx = user;

	if (aoi_hover(&ctx->aois, x, y))
		return 1;
	return 0;
}


static bool index_click(void *user, int x, int y)
{
	struct gui_ctx *ctx = user;

	if (aoi_click(&ctx->aois, x, y))
		return 1;
	close_index(ctx);
	return 1;
}


static void index_key(void *user, int x, int y, int keyval)
{
	struct gui_ctx *ctx = user;

	switch (keyval) {
	case GDK_KEY_Escape:
		ctx->mode = showing_sheet;
		input_pop();
		redraw(ctx);
		break;

	case GDK_KEY_h:
		help();
		break;

	case GDK_KEY_q:
		gtk_main_quit();
	}
}


static const struct input_ops index_input_ops = {
	.click		= index_click,
	.hover_begin	= index_hover_update,
	.hover_update	= index_hover_update,
	.hover_click	= index_click,
	.key		= index_key,
};


/* ----- Resizing ---------------------------------------------------------- */


void index_resize(struct gui_ctx *ctx)
{
	overlay_remove_all(&ctx->thumb_overlays);
	name_over = NULL;
	if (best_ratio(ctx))
		index_render_sheets(ctx);
	else
		close_index(ctx);
	redraw(ctx);
}


/* ----- Initialization ---------------------------------------------------- */


void show_index(struct gui_ctx *ctx)
{
	input_push(&index_input_ops, ctx);
	ctx->mode = showing_index;
	index_resize(ctx);
}
