/*
 * gui/comp.c - Component pop-up
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
#include "gui/pop.h"
#include "gui/common.h"


struct comp_aoi_ctx {
	struct gui * gui;
	const struct sch_obj *obj;
	struct dwg_bbox bbox;
	struct overlay *over;
};


#define	COMP_W	100


/* ----- Bounding box ------------------------------------------------------ */


static void bbox_add(int x, int y, int *xa, int *ya, int *xb, int *yb,
    bool *first)
{
	if (x < *xa || *first)
		*xa = x;
	if (x > *xb || *first)
		*xb = x;
	if (y < *ya || *first)
		*ya = y;
	if (y > *yb || *first)
		*yb = y;
	*first = 0;
}


static void bbox_add_circ(int x, int y, int r,
    int *xa, int *ya, int *xb, int *yb, bool *first)
{
	bbox_add(x - r, y - r, xa, ya, xb, yb, first);
	bbox_add(x + r, y + r, xa, ya, xb, yb, first);
}


static struct dwg_bbox get_bbox(const struct sch_obj *sch_obj)
{
	const struct lib_obj *obj;
	int xa = 0, ya = 0, xb = 0, yb = 0;
	bool first = 1;
	int i;

	for (obj = sch_obj->u.comp.comp->objs; obj; obj = obj->next)
		switch (obj->type) {
		case lib_obj_poly:
			for (i = 0; i != obj->u.poly.points; i++)
				bbox_add(obj->u.poly.x[i], obj->u.poly.y[i],
				    &xa, &ya, &xb, &yb, &first);
			break;
		case lib_obj_rect:
			bbox_add(obj->u.rect.sx, obj->u.rect.sy,
			    &xa, &ya, &xb, &yb, &first);
			bbox_add(obj->u.rect.ex, obj->u.rect.ey,
			    &xa, &ya, &xb, &yb, &first);
			break;
		case lib_obj_circ:
			bbox_add_circ(obj->u.circ.x, obj->u.circ.y,
			    obj->u.circ.r, &xa, &ya, &xb, &yb, &first);
			break;
		case lib_obj_arc:
			bbox_add_circ(obj->u.arc.x, obj->u.arc.y,
			    obj->u.arc.r, &xa, &ya, &xb, &yb, &first);
			break;
		/* @@@ consider pins, too ? */
		default:
			break;
		}

	struct dwg_bbox bbox = {
		.x	= xa + sch_obj->x,
		.y	= ya + sch_obj->y,
		.w	= xb - xa + 1,
		.h	= yb - ya + 1,
	};
	return bbox;
}


/* ----- AoIs -------------------------------------------------------------- */


static void comp_click(void *user)
{
	const char *doc = user;

	fprintf(stderr, "%s\n", doc);
}


static bool hover_comp(void *user, bool on, int dx, int dy)
{
	struct comp_aoi_ctx *aoi_ctx = user;
	struct gui *gui = aoi_ctx->gui;
	const struct comp_field *f;
	const char *ref = NULL;
	const char *doc = NULL;

	if (!on) {
		dehover_pop(gui);
		return 1;
	}
	if (gui->pop_underlays) {
		if (gui->pop_origin == aoi_ctx)
			return 0;
		dehover_pop(gui);
	}

	gui->pop_origin = aoi_ctx;

	aoi_dehover();
	overlay_remove_all(&gui->pop_overlays);
	overlay_remove_all(&gui->pop_underlays);

	for (f = aoi_ctx->obj->u.comp.fields; f; f = f->next)
		switch (f->n) {
		case 0:
			ref = f->txt.s;
			break;
		case 3:
			doc = f->txt.s;
			break;
		default:
			break;
		}

	add_pop_header(gui, COMP_W, ref ? ref : "???");
	if (doc)
		add_pop_item(gui, comp_click, (void *) doc, COMP_W, 0, "Doc");
	add_pop_frame(gui);

	place_pop(gui, &aoi_ctx->bbox);

	redraw(gui);
	return 0;
}


void add_comp_aoi(struct gui_sheet *sheet, const struct sch_obj *obj)
{
	const struct dwg_bbox bbox = get_bbox(obj);
	struct comp_aoi_ctx *aoi_ctx = alloc_type(struct comp_aoi_ctx);
	const struct comp_field *f;

	struct aoi cfg = {
		.x	= bbox.x,
		.y	= bbox.y,
		.w	= bbox.w,
		.h	= bbox.h,
		.hover	= hover_comp,
		.user	= aoi_ctx,
	};

	/* ignore power symbols */
	for (f = obj->u.comp.fields; f; f = f->next)
		if (!f->n && *f->txt.s == '#')
			return;

	aoi_ctx->gui = sheet->gui;
	aoi_ctx->obj = obj;
	aoi_ctx->bbox = bbox;

	aoi_add(&sheet->aois, &cfg);
}
