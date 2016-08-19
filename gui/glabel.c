/*
 * gui/glabel.c - Global label pop-up
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

#include <gtk/gtk.h>

#include "misc/util.h"
#include "kicad/dwg.h"
#include "gui/style.h"
#include "gui/input.h"
#include "gui/aoi.h"
#include "gui/over.h"
#include "gui/common.h"


/* small offset to hide rounding errors */
#define	CHEAT	1


struct glabel_aoi_ctx {
	const struct gui_sheet *sheet;
	const struct sch_obj *obj;
	struct dwg_bbox bbox;
	struct overlay *over;
};


#define	GLABEL_W	100


/* ----- Tools ------------------------------------------------------------- */


static void eeschema_coord(const struct gui_ctx *ctx,
    int x, int y, int *rx, int *ry)
{
	GtkAllocation alloc;

	gtk_widget_get_allocation(ctx->da, &alloc);
	*rx = ((x - ctx->x) >> ctx->zoom) + alloc.width / 2;
	*ry = ((y - ctx->y) >> ctx->zoom) + alloc.height / 2;
}


/* ----- AoIs -------------------------------------------------------------- */


static void glabel_dest_click(void *user)
{
	struct gui_sheet *sheet = user;

	go_to_sheet(sheet->ctx, sheet);
}


void dehover_glabel(struct gui_ctx *ctx)
{
	overlay_remove_all(&ctx->pop_overlays);
	overlay_remove_all(&ctx->pop_underlays);
	ctx->pop_origin = NULL;
	redraw(ctx);
}


static void add_dest_header(struct gui_ctx *ctx, const char *label)
{
	struct overlay_style style = {
		.font	= BOLD_FONT,
		.wmin	= GLABEL_W,
		.wmax	= GLABEL_W,
		.radius	= 0,
		.pad	= 0,
		.skip	= 6,
		.fg	= { 0.5, 0.0, 0.0, 1.0 },
		.bg	= { 0.0, 0.0, 0.0, 0.0 },
		.frame	= { 1.0, 1.0, 1.0, 1.0 }, /* debugging */
		.width	= 0,
	};
	struct overlay *over;

	over = overlay_add(&ctx->pop_overlays, NULL, NULL, NULL, NULL);
	overlay_text(over, "%s", label);
	overlay_style(over, &style);
}


static void add_dest_overlay(struct gui_ctx *ctx, const char *label,
    struct gui_sheet *sheet, unsigned n)
{
	struct overlay_style style = {
		.font	= BOLD_FONT,
		.wmin	= GLABEL_W,
		.wmax	= GLABEL_W,
		.radius	= 0,
		.pad	= 0,
		.skip	= 4,
		.fg	= { 0.0, 0.0, 0.0, 1.0 },
		.bg	= { 0.0, 0.0, 0.0, 0.0 },
		.frame	= { 1.0, 1.0, 1.0, 1.0 }, /* debugging */
		.width	= 0,
	};
	const struct sch_obj *obj;
	struct overlay *over;

	if (sheet == ctx->curr_sheet)
		style.fg = RGBA(0.5, 0.5, 0.5, 1.0);

	for (obj = sheet->sch->objs; obj; obj = obj->next) {
		if (obj->type != sch_obj_glabel)
			continue;
		if (strcmp(obj->u.text.s, label))
			continue;
		over = overlay_add(&ctx->pop_overlays,
		    &ctx->aois, NULL, glabel_dest_click, sheet);
		overlay_text(over, "%d %s", n,
		    sheet->sch->title ? sheet->sch->title : "(unnamed)");
		overlay_style(over, &style);
		break;
	}
}


static bool pop_hover(void *user, bool on)
{
	struct gui_ctx *ctx = user;

	if (!on)
		dehover_glabel(ctx);
	return 1;
}


static void add_dest_frame(struct gui_ctx *ctx)
{
	int w, h;

	overlay_size_all(ctx->pop_overlays,
	    gtk_widget_get_pango_context(ctx->da), 0, 1, &w, &h);

	struct overlay_style style = {
		.font	= BOLD_FONT,
		.wmin	= w,
		.hmin	= h,
		.radius	= 0,
		.pad	= GLABEL_STACK_PADDING,
		.skip	= 0,
		.fg	= { 0.0, 0.0, 0.0, 1.0 },
		.bg	= { 0.9, 0.9, 0.3, 0.8 },
		.frame	= { 0.0, 0.0, 0.0, 1.0 }, /* debugging */
		.width	= 1,
	};
	struct overlay *over;

	over = overlay_add(&ctx->pop_underlays, &ctx->aois,
	    pop_hover, NULL, ctx);
	overlay_text_raw(over, "");
	overlay_style(over, &style);

	/*
	 * This makes it all work. When we receive a click while hovering, it
	 * goes to the hovering overlay if that overlay accepts clicks.
	 * However, if the overlay accepting the click is different, we first
	 * de-hover.
	 *
	 * Now, in the case of the frame overlay, dehovering would destroy the
	 * destination overlays right before trying to deliver the click.
	 *
	 * We solve this by declaring the frame overlay to be "related" to the
	 * destination overlays. This suppresses dehovering.
	 */
	overlay_set_related_all(ctx->pop_overlays, over);
}


static bool hover_glabel(void *user, bool on)
{
	struct glabel_aoi_ctx *aoi_ctx = user;
	struct gui_ctx *ctx = aoi_ctx->sheet->ctx;
	const struct gui_sheet *curr_sheet = ctx->curr_sheet;
	const struct dwg_bbox *bbox = &aoi_ctx->bbox;

	if (!on) {
		dehover_glabel(ctx);
		return 1;
	}
	if (ctx->pop_underlays) {
		if (ctx->pop_origin == aoi_ctx)
			return 0;
		dehover_glabel(ctx);
	}

	GtkAllocation alloc;
	int sx, sy, ex, ey, mx, my;
	unsigned n = 0;
	struct gui_sheet *sheet;

	ctx->glabel = aoi_ctx->obj->u.text.s;
	ctx->pop_origin = aoi_ctx;

	aoi_dehover();
	overlay_remove_all(&ctx->pop_overlays);
	overlay_remove_all(&ctx->pop_underlays);

	add_dest_header(ctx, aoi_ctx->obj->u.text.s);
	for (sheet = ctx->new_hist->sheets; sheet; sheet = sheet->next)
		add_dest_overlay(ctx, aoi_ctx->obj->u.text.s, sheet, ++n);
	add_dest_frame(ctx);

	eeschema_coord(ctx,
	    bbox->x - curr_sheet->xmin, bbox->y - curr_sheet->ymin,
	    &sx, &sy);
	eeschema_coord(ctx, bbox->x + bbox->w - curr_sheet->xmin,
	    bbox->y + bbox->h - curr_sheet->ymin, &ex, &ey);

	gtk_widget_get_allocation(ctx->da, &alloc);
	mx = (sx + ex) / 2;
	my = (sy + ey) / 2;
	if (mx < alloc.width / 2) {
		ctx->pop_x = sx - CHEAT;
		ctx->pop_dx = 1;
	} else {
		ctx->pop_x = ex + CHEAT;
		ctx->pop_dx = -1;
	}
	if (my < alloc.height / 2) {
		ctx->pop_y = sy - CHEAT;
		ctx->pop_dy = 1;
	} else {
		ctx->pop_y = ey + CHEAT;
		ctx->pop_dy = -1;
	}

	/*
	 * @@@ The idea is to get input to trigger hovering over the pop-up.
	 * However, this doesn't work because the overlay has not been drawn
	 * yet and therefore has not created its AoI. We therefore only get a
	 * chance to begin hovering at the next motion update, which may
	 * already be outside the pop-up.
	 *
	 * Probably the only way to fix this is by making overlay_add do the
	 * layout calculation and create the AoI immediately.
	 *
	 * Another problem occurs as deep zoom levels, when the label is larger
	 * than the pop-up. Then we can trigger pop-up creation from a location
	 * that will be outside the pop-up.
	 *
	 * We could fix this by aligning the pop-up with the mouse position
	 * instead the box, either in general, or in this specific case. Not
	 * sure if it's worth the trouble, though.
	 *
	 * Both issues result in a "hanging" pop-up because AoI (and input)
	 * don't even know we're hovering. The pop-up can be cleared by
	 * - hovering into it,
	 * - hovering over some other glabel, or
	 * - clicking.
	 */
	input_update();
	redraw(ctx);
	return 0;
}


void add_glabel_aoi(struct gui_sheet *sheet, const struct sch_obj *obj)
{
	const struct dwg_bbox *bbox = &obj->u.text.bbox;
	struct glabel_aoi_ctx *aoi_ctx = alloc_type(struct glabel_aoi_ctx);

	struct aoi cfg = {
		.x	= bbox->x,
		.y	= bbox->y,
		.w	= bbox->w,
		.h	= bbox->h,
		.hover	= hover_glabel,
		.user	= aoi_ctx,
	};

	aoi_ctx->sheet = sheet;
	aoi_ctx->obj = obj;
	aoi_ctx->bbox = *bbox;

	aoi_add(&sheet->aois, &cfg);
}
