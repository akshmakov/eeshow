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
	redraw(ctx);
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

	GtkAllocation alloc;
	struct overlay_style style = {
		.font	= BOLD_FONT,
		.wmin	= 100,
		.wmax	= 100,
		.radius	= 0,
		.pad	= 4,
		.skip	= -4,
		.fg	= { 0.0, 0.0, 0.0, 1.0 },
		.bg	= { 1.0, 0.8, 0.4, 0.8 },
		.frame	= { 1.0, 1.0, 1.0, 1.0 }, /* debugging */
		.width	= 0,
	};
	int sx, sy, ex, ey, mx, my;
	unsigned n = 0;
	struct gui_sheet *sheet;
	const struct sch_obj *obj;
	struct overlay *over;

	aoi_dehover();
	overlay_remove_all(&ctx->pop_overlays);
	for (sheet = ctx->new_hist->sheets; sheet; sheet = sheet->next) {
		n++;
		if (sheet == curr_sheet)
			continue;
		for (obj = sheet->sch->objs; obj; obj = obj->next) {
			if (obj->type != sch_obj_glabel)
				continue;
			if (strcmp(obj->u.text.s, aoi_ctx->obj->u.text.s))
				continue;
			over = overlay_add(&ctx->pop_overlays,
			    &ctx->aois, NULL, glabel_dest_click, sheet);
			overlay_text(over, "%d %s", n, sheet->sch->title);
			overlay_style(over, &style);
			break;
		}
	}

	eeschema_coord(ctx,
	    bbox->x - curr_sheet->xmin, bbox->y - curr_sheet->ymin,
	    &sx, &sy);
	eeschema_coord(ctx, bbox->x + bbox->w - curr_sheet->xmin,
	    bbox->y + bbox->h - curr_sheet->ymin, &ex, &ey);

	gtk_widget_get_allocation(ctx->da, &alloc);
	mx = (sx + ex) / 2;
	my = (sy + ey) / 2;
	ctx->pop_x = mx < alloc.width / 2 ?
	    sx - CHEAT : -(alloc.width - ex) + CHEAT;
	ctx->pop_y = my < alloc.height / 2 ?
	    sy - CHEAT : -(alloc.height - ey) + CHEAT;

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
