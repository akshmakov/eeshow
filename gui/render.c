/*
 * gui/render.c - Render schematics and GUI elements
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
#include <string.h>

#include <cairo/cairo.h>
#include <gtk/gtk.h>

#include "misc/util.h"
#include "misc/diag.h"
#include "gfx/style.h"
#include "gfx/cro.h"
#include "gfx/gfx.h"
#include "kicad/pl.h"
#include "kicad/sch.h"
#include "kicad/delta.h"
#include "gfx/diff.h"
#include "gfx/diff.h"
#include "gui/aoi.h"
#include "gui/over.h"
#include "gui/timer.h"
#include "gui/common.h"


#define	VCS_OVERLAYS_X		5
#define	VCS_OVERLAYS_Y		5

#define	SHEET_OVERLAYS_X	-10
#define	SHEET_OVERLAYS_Y	10

#define GLABEL_HIGHLIGHT_PAD	6


bool use_delta = 0;
bool show_extra = 0;


/* ----- Helper functions -------------------------------------------------- */


void redraw(const struct gui_ctx *ctx)
{
	gtk_widget_queue_draw(ctx->da);
}


/* ----- Highlight glabel -------------------------------------------------- */


/*
 * cd, cx, cy are simplified versions of what cro.c uses. Since we don't
 * support glabel highlighting in diff mode, we don't need the xe and ye offset
 * components.
 */

static inline int cd(int x, float scale)
{
	return x * scale;
}


static inline int cx(int x, int xo, float scale)
{
	return xo + x * scale;
}


static inline int cy(int y, int yo, float scale)
{
	return yo + y * scale;
}


static void highlight_glabel(const struct gui_ctx *ctx,
    const struct gui_sheet *sheet,  cairo_t *cr, int xo, int yo, float f)
{
	const struct sch_obj *obj;

	if (!ctx->glabel)
		return;

	cairo_set_source_rgb(cr, 1, 0.8, 1);
	for (obj = sheet->sch->objs; obj; obj = obj->next) {
		const struct dwg_bbox *bbox = &obj->u.text.bbox;

		if (obj->type != sch_obj_glabel)
			continue;
		if (strcmp(obj->u.text.s, ctx->glabel))
			continue;

		cairo_rectangle(cr,
		    cx(bbox->x, xo, f) - GLABEL_HIGHLIGHT_PAD,
		    cy(bbox->y, yo, f) - GLABEL_HIGHLIGHT_PAD,
		    cd(bbox->w, f) + 2 * GLABEL_HIGHLIGHT_PAD,
		    cd(bbox->h, f) + 2 * GLABEL_HIGHLIGHT_PAD);
		cairo_fill(cr);
	}
}


/* ----- Draw to screen ---------------------------------------------------- */


/*
 * @@@ the highlighting of sub-sheets possibly containing changes is very
 * unreliable since sheet_eq (from delta) responds to a lot of purely
 * imaginary changes. However, this will be a good way to exercise and improve
 * delta.
 */

static struct area *changed_sheets(const struct gui_ctx *ctx,
    int xo, int yo, float f)
{
	const struct gui_sheet *new = ctx->curr_sheet;
	const struct sch_obj *obj;
	struct area *areas = NULL;

	for (obj = new->sch->objs; obj; obj = obj->next) {
		const struct gui_sheet *new_sub;
		const struct gui_sheet *old_sub;

		if (obj->type != sch_obj_sheet)
			continue;
		if (!obj->u.sheet.sheet)
			continue;

		for (new_sub = ctx->new_hist->sheets;
		    new_sub && new_sub->sch != obj->u.sheet.sheet;
		    new_sub = new_sub->next);
		if (!new_sub)
			continue;
		old_sub = find_corresponding_sheet(ctx->old_hist->sheets,
		    ctx->new_hist->sheets, new_sub);

		if (!sheet_eq(new_sub->sch, old_sub->sch))
			add_area(&areas, cx(obj->x, xo, f), cy(obj->y, yo, f),
		    	    cx(obj->x + obj->u.sheet.w, xo, f),
			    cy(obj->y + obj->u.sheet.h, yo, f), 0xffff00);
	}
	return areas;
}


static void hack(const struct gui_ctx *ctx, cairo_t *cr,
    int xo, int yo, float f)
{
	const struct gui_sheet *new = ctx->curr_sheet;
	const struct gui_sheet *old = find_corresponding_sheet(
	    ctx->old_hist->sheets, ctx->new_hist->sheets, ctx->curr_sheet);
	struct area *areas = NULL;

	areas = changed_sheets(ctx, xo, yo, f);
	diff_to_canvas(cr, ctx->x, ctx->y, ctx->scale,
	    gfx_user(old->gfx), show_extra ? gfx_user(old->gfx_extra) : NULL,
	    gfx_user(new->gfx), show_extra ? gfx_user(new->gfx_extra) : NULL,
	    areas);
	free_areas(&areas);
}


static gboolean on_draw_event(GtkWidget *widget, cairo_t *cr,
    gpointer user_data)
{
	struct gui_ctx *ctx = user_data;
	const struct gui_sheet *sheet = ctx->curr_sheet;
	GtkAllocation alloc;
	float f = ctx->scale;
	int x, y;

	gtk_widget_get_allocation(ctx->da, &alloc);
	x = -(sheet->xmin + ctx->x) * f + alloc.width / 2;
	y = -(sheet->ymin + ctx->y) * f + alloc.height / 2;

	cro_canvas_prepare(cr);
	if (!ctx->old_hist || ctx->diff_mode == diff_new) {
		highlight_glabel(ctx, sheet, cr, x, y, f);
		if (show_extra)
			cro_canvas_draw(gfx_user(sheet->gfx_extra),
			    cr, x, y, f);
		cro_canvas_draw(gfx_user(sheet->gfx), cr, x, y, f);
	} else if (ctx->diff_mode == diff_old) {
		sheet = find_corresponding_sheet(ctx->old_hist->sheets,
		    ctx->new_hist->sheets, ctx->curr_sheet);
		highlight_glabel(ctx, sheet, cr, x, y, f);
		if (show_extra)
			cro_canvas_draw(gfx_user(sheet->gfx_extra),
			    cr, x, y, f);
		cro_canvas_draw(gfx_user(sheet->gfx), cr, x, y, f);
	} else if (use_delta) {
		struct area *areas = changed_sheets(ctx, x, y, f);
		const struct area *area;

		cairo_set_source_rgb(cr, 1, 1, 0);
		for (area = areas; area; area = area->next) {
			cairo_rectangle(cr, area->xa, area->ya,
			    area->xb - area->xa, area->yb - area->ya);
			cairo_fill(cr);
		}
		free_areas(&areas);

		/* @@@ fix geometry later */
		if (show_extra) {
			cro_canvas_draw(gfx_user(ctx->delta_ab.gfx_extra),
			    cr, x, y, f);
			cro_canvas_draw(gfx_user(ctx->delta_a.gfx_extra),
			    cr, x, y, f);
			cro_canvas_draw(gfx_user(ctx->delta_b.gfx_extra),
			    cr, x, y, f);
		}
		cro_canvas_draw(gfx_user(ctx->delta_ab.gfx), cr, x, y, f);
		cro_canvas_draw(gfx_user(ctx->delta_a.gfx), cr, x, y, f);
		cro_canvas_draw(gfx_user(ctx->delta_b.gfx), cr, x, y, f);
	} else {
		hack(ctx, cr, x, y, f);
	}

	overlay_draw_all(ctx->sheet_overlays, cr,
	    SHEET_OVERLAYS_X, SHEET_OVERLAYS_Y);
	overlay_draw_all_d(ctx->hist_overlays, cr,
	    VCS_OVERLAYS_X,
	    VCS_OVERLAYS_Y +
	    (ctx->mode == showing_history ? ctx->hist_y_offset : 0),
	    0, 1);
	overlay_draw_all_d(ctx->pop_underlays, cr, ctx->pop_x, ctx->pop_y,
	    ctx->pop_dx, ctx->pop_dy);
	overlay_draw_all_d(ctx->pop_overlays, cr,
	    ctx->pop_x + ctx->pop_dx * GLABEL_STACK_PADDING,
	    ctx->pop_y + ctx->pop_dy * GLABEL_STACK_PADDING,
	    ctx->pop_dx, ctx->pop_dy);

	if (ctx->mode == showing_index)
		index_draw_event(ctx, cr);

	timer_show(cr);

	return FALSE;
}


/* ----- Pre-rendering ----------------------------------------------------- */


void render_sheet(struct gui_sheet *sheet)
{
	sheet->gfx = gfx_init(&cro_canvas_ops);
	if (sheet->hist && sheet->hist->pl) /* @@@ no pl_render for delta */
		pl_render(sheet->hist->pl, sheet->gfx,
		    sheet->hist->sch_ctx.sheets, sheet->sch);
	sch_render(sheet->sch, sheet->gfx);
	cro_canvas_end(gfx_user(sheet->gfx),
	    &sheet->w, &sheet->h, &sheet->xmin, &sheet->ymin);

	sheet->gfx_extra = gfx_init(&cro_canvas_ops);
	sch_render_extra(sheet->sch, sheet->gfx_extra);
	cro_canvas_end(gfx_user(sheet->gfx_extra), NULL, NULL, NULL, NULL);

	sheet->rendered = 1;
	// gfx_end();
}


void render_delta(struct gui_ctx *ctx)
{
#if 1
	/* @@@ needs updating for curr/last vs. new/old */
	struct sheet *sch_a, *sch_b, *sch_ab;
	struct gui_sheet *a = ctx->curr_sheet;
	struct gui_sheet *b = find_corresponding_sheet(
	    ctx->old_hist->sheets, ctx->new_hist->sheets, ctx->curr_sheet);

	sch_a = alloc_type(struct sheet);
	sch_b = alloc_type(struct sheet);
	sch_ab = alloc_type(struct sheet);

	delta(a->sch, b->sch, sch_a, sch_b, sch_ab);
	ctx->delta_a.sch = sch_a,
	ctx->delta_b.sch = sch_b,
	ctx->delta_ab.sch = sch_ab,

	ctx->delta_a.ctx = ctx->delta_b.ctx = ctx->delta_ab.ctx = NULL;
	ctx->delta_a.hist = ctx->delta_b.hist = ctx->delta_ab.hist = NULL;

	render_sheet(&ctx->delta_a);
	render_sheet(&ctx->delta_b);
	render_sheet(&ctx->delta_ab);

	cro_color_override(gfx_user(ctx->delta_ab.gfx), COLOR_LIGHT_GREY);
	cro_color_override(gfx_user(ctx->delta_b.gfx), COLOR_RED);
	cro_color_override(gfx_user(ctx->delta_a.gfx), COLOR_GREEN2);

	cro_color_override(gfx_user(ctx->delta_ab.gfx_extra), COLOR_LIGHT_GREY);
	cro_color_override(gfx_user(ctx->delta_b.gfx_extra), COLOR_RED);
	cro_color_override(gfx_user(ctx->delta_a.gfx_extra), COLOR_GREEN2);

	// @@@ clean up when leaving sheet
#endif

	if (!b->rendered) {
		render_sheet(b);
		mark_aois(ctx, b);
	}
}


/* ----- Setup ------------------------------------------------------------- */


void render_setup(struct gui_ctx *ctx)
{
	g_signal_connect(G_OBJECT(ctx->da), "draw",
	    G_CALLBACK(on_draw_event), ctx);
}
