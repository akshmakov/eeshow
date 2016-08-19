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
#include "gfx/cro.h"
#include "gfx/gfx.h"
#include "kicad/sch.h"
#include "kicad/delta.h"
#include "gfx/diff.h"
#include "gui/aoi.h"
#include "gui/over.h"
#include "gui/common.h"


#define	VCS_OVERLAYS_X		5
#define	VCS_OVERLAYS_Y		5

#define	SHEET_OVERLAYS_X	-10
#define	SHEET_OVERLAYS_Y	10

#define GLABEL_HIGHLIGHT_PAD	6


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


static void highlight_glabel(const struct gui_ctx *ctx, cairo_t *cr,
    int x, int y, float f)
{
	const struct sch_obj *obj;

	if (!ctx->glabel)
		return;

	for (obj = ctx->curr_sheet->sch->objs; obj; obj = obj->next) {
		const struct dwg_bbox *bbox = &obj->u.text.bbox;

		if (obj->type != sch_obj_glabel)
			continue;
		if (strcmp(obj->u.text.s, ctx->glabel))
			continue;

		cairo_rectangle(cr,
		    cx(bbox->x, x, f) - GLABEL_HIGHLIGHT_PAD,
		    cy(bbox->y, y, f) - GLABEL_HIGHLIGHT_PAD,
		    cd(bbox->w, f) + 2 * GLABEL_HIGHLIGHT_PAD,
		    cd(bbox->h, f) + 2 * GLABEL_HIGHLIGHT_PAD);
		cairo_set_source_rgb(cr, 1, 0.8, 1);
		cairo_fill(cr);
	}
}


/* ----- Draw to screen ---------------------------------------------------- */


static void hack(const struct gui_ctx *ctx, cairo_t *cr)
{
	const struct gui_sheet *new = ctx->curr_sheet;
	const struct gui_sheet *old = find_corresponding_sheet(
	    ctx->old_hist->sheets, ctx->new_hist->sheets, ctx->curr_sheet);

	diff_to_canvas(cr, ctx->x, ctx->y, 1.0 / (1 << ctx->zoom),
	    old->gfx_ctx, new->gfx_ctx);
}


static gboolean on_draw_event(GtkWidget *widget, cairo_t *cr,
    gpointer user_data)
{
	const struct gui_ctx *ctx = user_data;
	const struct gui_sheet *sheet = ctx->curr_sheet;
	GtkAllocation alloc;
	float f = 1.0 / (1 << ctx->zoom);
	int x, y;

	gtk_widget_get_allocation(ctx->da, &alloc);
	x = -(sheet->xmin + ctx->x) * f + alloc.width / 2;
	y = -(sheet->ymin + ctx->y) * f + alloc.height / 2;

	cro_canvas_prepare(cr);
	if (!ctx->old_hist) {
		highlight_glabel(ctx, cr, x, y, f);
		cro_canvas_draw(sheet->gfx_ctx, cr, x, y, f);
	} else {
#if 0
		/* @@@ fix geometry later */
		cro_canvas_draw(ctx->delta_ab.gfx_ctx, cr, x, y, f);
		cro_canvas_draw(ctx->delta_a.gfx_ctx, cr, x, y, f);
		cro_canvas_draw(ctx->delta_b.gfx_ctx, cr, x, y, f);
#endif
		hack(ctx, cr);
	}

	overlay_draw_all(ctx->sheet_overlays, cr,
	    SHEET_OVERLAYS_X, SHEET_OVERLAYS_Y);
	overlay_draw_all_d(ctx->hist_overlays, cr,
	    VCS_OVERLAYS_X,
	    VCS_OVERLAYS_Y + (ctx->showing_history ? ctx->hist_y_offset : 0),
	    0, 1);
	overlay_draw_all_d(ctx->pop_underlays, cr, ctx->pop_x, ctx->pop_y,
	    ctx->pop_dx, ctx->pop_dy);
	overlay_draw_all_d(ctx->pop_overlays, cr,
	    ctx->pop_x + ctx->pop_dx * GLABEL_STACK_PADDING,
	    ctx->pop_y + ctx->pop_dy * GLABEL_STACK_PADDING,
	    ctx->pop_dx, ctx->pop_dy);

	return FALSE;
}


/* ----- Pre-rendering ----------------------------------------------------- */


void render_sheet(struct gui_sheet *sheet)
{
	char *argv[] = { "gui", NULL };

	gfx_init(&cro_canvas_ops, 1, argv);
	sch_render(sheet->sch);
	cro_canvas_end(gfx_ctx,
	    &sheet->w, &sheet->h, &sheet->xmin, &sheet->ymin);
	sheet->gfx_ctx = gfx_ctx;
	sheet->rendered = 1;
	// gfx_end();
}


void render_delta(struct gui_ctx *ctx)
{
#if 0
	/* @@@ needs updating for curr/last vs. new/old */
	struct sheet *sch_a, *sch_b, *sch_ab;
	const struct gui_sheet *a = ctx->curr_sheet;
	const struct gui_sheet *b = find_corresponding_sheet(
	    ctx->last_hist->sheets, ctx->curr_hist->sheets, ctx->curr_sheet);

	sch_a = alloc_type(struct sheet);
	sch_b = alloc_type(struct sheet);
	sch_ab = alloc_type(struct sheet);

	delta(a->sch, b->sch, sch_a, sch_b, sch_ab);
	ctx->delta_a.sch = sch_a,
	ctx->delta_b.sch = sch_b,
	ctx->delta_ab.sch = sch_ab,

	render_sheet(&ctx->delta_a);
	render_sheet(&ctx->delta_b);
	render_sheet(&ctx->delta_ab);

	cro_color_override(ctx->delta_ab.gfx_ctx, COLOR_LIGHT_GREY);
	cro_color_override(ctx->delta_a.gfx_ctx, COLOR_RED);
	cro_color_override(ctx->delta_b.gfx_ctx, COLOR_GREEN2);

	// @@@ clean up when leaving sheet
#endif
	struct gui_sheet *b = find_corresponding_sheet(
	    ctx->old_hist->sheets, ctx->new_hist->sheets, ctx->curr_sheet);

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
