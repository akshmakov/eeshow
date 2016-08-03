/*
 * gui.c - GUI for eeshow
 *
 * Written 2016 by Werner Almesberger
 * Copyright 2016 by Werner Almesberger
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

/*
 * Resources:
 *
 * http://zetcode.com/gfx/cairo/cairobackends/
 * https://developer.gnome.org/gtk3/stable/gtk-migrating-2-to-3.html
 */

#include <stddef.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <math.h>

#include <cairo/cairo.h>
#include <gtk/gtk.h>

#include "util.h"
#include "cro.h"
#include "gfx.h"
#include "sch.h"
#include "gui.h"


struct gui_ctx;

struct aoi {
	int x, y, w, h;		/* activation box, eeschema coordinates */

	bool (*hover)(struct gui_ctx *ctx, void *user, bool on);
	void (*click)(struct gui_ctx *ctx, void *user);
	void *user;

	struct aoi *next;
};

struct gui_sheet {
	const const struct sheet *sch;
	struct cro_ctx *gfx_ctx;

	int w, h;
	int xmin, ymin;

	struct aoi *aois;	/* areas of interest */

	struct gui_sheet *prev;	/* previous in stack */

	struct gui_sheet *next;
};

struct overlay;

struct gui_ctx {
	GtkWidget *da;

	int curr_x;		/* last on-screen mouse position */
	int curr_y;

	unsigned zoom;		/* scale by 1.0 / (1 << zoom) */
	int x, y;		/* center, in eeschema coordinates */

	bool panning;
	int pan_x, pan_y;

	const struct aoi *aoi_hovering;	/* hovering over this aoi */

	struct overlay *overlays;

	struct gui_sheet *curr_sheet;
				/* current sheet */
	struct gui_sheet *sheets;
};


/* ----- Helper functions -------------------------------------------------- */


static void redraw(const struct gui_ctx *ctx)
{
	gtk_widget_queue_draw(ctx->da);
}


/* ----- Area of intereest ------------------------------------------------- */


static void aoi_add(struct gui_sheet *sheet, const struct aoi *aoi)
{
	struct aoi *new;

	new = alloc_type(struct aoi);
	*new = *aoi;
	new->next = sheet->aois;
	sheet->aois = new;
}


static void aoi_hover(struct gui_ctx *ctx, int x, int y)
{
	const struct gui_sheet *sheet = ctx->curr_sheet;
	const struct aoi *aoi = ctx->aoi_hovering;

	if (aoi) {
		if (x >= aoi->x && x < aoi->x + aoi->w &&
		    y >= aoi->y && y < aoi->y + aoi->h)
			return;
		aoi->hover(ctx, aoi->user, 0);
		ctx->aoi_hovering = NULL;
	}

	for (aoi = sheet->aois; aoi; aoi = aoi->next)
		if (x >= aoi->x && x < aoi->x + aoi->w &&
		    y >= aoi->y && y < aoi->y + aoi->h)
			break;
	if (aoi && aoi->hover && aoi->hover(ctx, aoi->user, 1))
		ctx->aoi_hovering = aoi;
}


static void aoi_click(struct gui_ctx *ctx, int x, int y)
{
	const struct gui_sheet *sheet = ctx->curr_sheet;
	const struct aoi *aoi = ctx->aoi_hovering;

	x += sheet->xmin;
	y += sheet->ymin;

	if (aoi) {
		aoi->hover(ctx, aoi->user, 0);
		ctx->aoi_hovering = NULL;
	}
	for (aoi = sheet->aois; aoi; aoi = aoi->next)
		if (x >= aoi->x && x < aoi->x + aoi->w &&
		    y >= aoi->y && y < aoi->y + aoi->h)
			break;
	if (aoi && aoi->click)
		aoi->click(ctx, aoi->user);
}


/* ----- Overlays ---------------------------------------------------------- */


struct overlay {
	const char *s;
	struct overlay *next;
};


#define	OVER_FONT_SIZE	16
#define	OVER_BORDER	8
#define	OVER_RADIUS	6
#define	OVER_SEP	8
#define	OVER_X0		10
#define	OVER_Y0		10


static void rrect(cairo_t *cr, int x, int y, int w, int h, int r)
{
	const double deg = M_PI / 180.0;

	// https://www.cairographics.org/samples/rounded_rectangle/

	cairo_new_path(cr);
	cairo_arc(cr, x + w - r, y + r, r, -90 * deg, 0);
	cairo_arc(cr, x + w - r, y + h - r, r, 0, 90 * deg);
	cairo_arc(cr, x + r, y + h - r, r, 90 * deg, 180 * deg);
	cairo_arc(cr, x + r, y + r, r, 180 * deg, 270 * deg);
	cairo_close_path(cr);
}


static void overlay_draw(const struct overlay *over, cairo_t *cr,
    int *x, int *y)
{
	cairo_text_extents_t ext;

	cairo_set_font_size(cr, OVER_FONT_SIZE);
	cairo_text_extents(cr, over->s, &ext);

	rrect(cr, *x, *y,
	    ext.width + 2 * OVER_BORDER, ext.height + 2 * OVER_BORDER,
	    OVER_RADIUS);

	cairo_set_source_rgba(cr, 0.8, 0.9, 1, 0.8);
	cairo_fill_preserve(cr);
	cairo_set_source_rgba(cr, 0.5, 0.5, 1, 0.7);
	cairo_set_line_width(cr, 2);
	cairo_stroke(cr);

	cairo_set_source_rgb(cr, 0, 0, 0);
	cairo_move_to(cr, *x + OVER_BORDER, *y + OVER_BORDER + ext.height);
	cairo_show_text(cr, over->s);

	*y += ext.height + OVER_SEP;
}


static void overlay_draw_all(const struct gui_ctx *ctx, cairo_t *cr)
{
	const struct overlay *over;
	int x = OVER_X0;
	int y = OVER_Y0;

	for (over = ctx->overlays; over; over = over->next)
		overlay_draw(over, cr, &x, &y);
}


static struct overlay *overlay_add(struct gui_ctx *ctx, const char *s)
{
	struct overlay *over;
	struct overlay **anchor;
	
	over = alloc_type(struct overlay);
	over->s = stralloc(s);

	for (anchor = &ctx->overlays; *anchor; anchor = &(*anchor)->next);
	over->next = NULL;
	*anchor = over;

	return over;
}


static void overlay_free(struct overlay *over)
{
	free((void *) over->s);
	free(over);
}


static void overlay_remove(struct gui_ctx *ctx, struct overlay *over)
{
	struct overlay **anchor;

	for (anchor = &ctx->overlays; *anchor; anchor = &(*anchor)->next)
		if (*anchor == over) {
			*anchor = over->next;
			overlay_free(over);
			redraw(ctx);
			return;
		}
	abort();
}


static void overlay_remove_all(struct gui_ctx *ctx)
{
	struct overlay *next;

	while (ctx->overlays) {
		next = ctx->overlays->next;
		overlay_free(ctx->overlays);
		ctx->overlays = next;
	}
	redraw(ctx);
}


/* ----- Rendering --------------------------------------------------------- */


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
	cro_canvas_draw(sheet->gfx_ctx, cr, x, y, f);

	overlay_draw_all(ctx, cr);

	return FALSE;
}


static void render(struct gui_ctx *ctx, struct gui_sheet *sheet)
{
	char *argv[] = { "gui", NULL };

	gfx_init(&cro_canvas_ops, 1, argv);
	sch_render(sheet->sch);
	cro_canvas_end(gfx_ctx,
	    &sheet->w, &sheet->h, &sheet->xmin, &sheet->ymin);
	sheet->gfx_ctx = gfx_ctx;

	ctx->x = sheet->w >> 1;
	ctx->y = sheet->h >> 1;
	// gfx_end();
}


/* ----- Tools ------------------------------------------------------------- */


static void canvas_coord(const struct gui_ctx *ctx,
    int ex, int ey, int *x, int *y)
{
	GtkAllocation alloc;
	int sx, sy;

	gtk_widget_get_allocation(ctx->da, &alloc);
	sx = ex - alloc.width / 2;
	sy = ey - alloc.height / 2;
	*x = (sx << ctx->zoom) + ctx->x;
	*y = (sy << ctx->zoom) + ctx->y;
}


/* ----- Panning ----------------------------------------------------------- */


static void pan_begin(struct gui_ctx *ctx, int x, int y)
{
	if (ctx->panning)
		return;
	ctx->panning = 1;
	ctx->pan_x = x;
	ctx->pan_y = y;
}


static void pan_update(struct gui_ctx *ctx, int x, int y)
{
	if (!ctx->panning)
		return;
	
	ctx->x -= (x - ctx->pan_x) << ctx->zoom;
	ctx->y -= (y - ctx->pan_y) << ctx->zoom;
	ctx->pan_x = x;
	ctx->pan_y = y;

	redraw(ctx);
}


static void pan_end(struct gui_ctx *ctx, int x, int y)
{
	pan_update(ctx, x, y);
	ctx->panning = 0;
}


/* ----- Zoom -------------------------------------------------------------- */



static void zoom_in(struct gui_ctx *ctx, int x, int y)
{
	if (ctx->zoom == 0)
		return;
	ctx->zoom--;
	ctx->x = (ctx->x + x) / 2;
	ctx->y = (ctx->y + y) / 2;
	redraw(ctx);
}


static void zoom_out(struct gui_ctx *ctx, int x, int y)
{
	if (ctx->curr_sheet->w >> ctx->zoom <= 16)
		return;
	ctx->zoom++;
	ctx->x = 2 * ctx->x - x;
	ctx->y = 2 * ctx->y - y;
	redraw(ctx);
}


static void zoom_to_extents(struct gui_ctx *ctx)
{
	const struct gui_sheet *sheet = ctx->curr_sheet;
	GtkAllocation alloc;

	ctx->x = sheet->w / 2;
	ctx->y = sheet->h / 2;

	gtk_widget_get_allocation(ctx->da, &alloc);
	ctx->zoom = 0;
	while (sheet->w >> ctx->zoom > alloc.width ||
	    sheet->h >> ctx->zoom > alloc.height)
		ctx->zoom++;

	redraw(ctx);
}


/* ----- Event handlers ---------------------------------------------------- */


static gboolean motion_notify_event(GtkWidget *widget, GdkEventMotion *event,
    gpointer data)
{
	struct gui_ctx *ctx = data;
	int x, y;

	ctx->curr_x = event->x;
	ctx->curr_y = event->y;

	canvas_coord(ctx, event->x, event->y, &x, &y);

	aoi_hover(ctx, x, y);
	pan_update(ctx, event->x, event->y);

	return TRUE;
}


static gboolean button_press_event(GtkWidget *widget, GdkEventButton *event,
    gpointer data)
{
	struct gui_ctx *ctx = data;
	int x, y;

	canvas_coord(ctx, event->x, event->y, &x, &y);

	switch (event->button) {
	case 1:
		aoi_click(ctx, x, y);
		break;
	case 2:
		pan_begin(ctx, event->x, event->y);
		break;
	case 3:
		break;
	}
	return TRUE;
}


static gboolean button_release_event(GtkWidget *widget, GdkEventButton *event,
    gpointer data)
{
	struct gui_ctx *ctx = data;
	int x, y;

	canvas_coord(ctx, event->x, event->y, &x, &y);

	switch (event->button) {
	case 1:
		break;
	case 2:
		pan_end(ctx, event->x, event->y);
		break;
	case 3:
		break;
	}
	return TRUE;
}


static void set_sheet(struct gui_ctx *ctx, struct gui_sheet *sheet)
{
	if (sheet->prev == sheet)
		sheet->prev = NULL;
	ctx->curr_sheet = sheet;
	overlay_remove_all(ctx);
	zoom_to_extents(ctx);
}


static gboolean key_press_event(GtkWidget *widget, GdkEventKey *event,
    gpointer data)
{
	struct gui_ctx *ctx = data;
	struct gui_sheet *sheet = ctx->curr_sheet;
	int x, y;

	canvas_coord(ctx, ctx->curr_x, ctx->curr_y, &x, &y);

	switch (event->keyval) {
	case '+':
	case '=':
		zoom_in(ctx, x, y);
		break;
	case '-':
		zoom_out(ctx, x, y);
		break;
	case '*':
		zoom_to_extents(ctx);
		break;
	case GDK_KEY_Home:
		if (sheet != ctx->sheets)
			set_sheet(ctx, ctx->sheets);
		break;
	case GDK_KEY_BackSpace:
	case GDK_KEY_Delete:
		if (sheet->prev)
			set_sheet(ctx, sheet->prev);
		break;
	case GDK_KEY_q:
		gtk_main_quit();
	}
	return TRUE;
}


static gboolean scroll_event(GtkWidget *widget, GdkEventScroll *event,
    gpointer data)
{
	struct gui_ctx *ctx = data;
	int x, y;

	canvas_coord(ctx, event->x, event->y, &x, &y);
	switch (event->direction) {
	case GDK_SCROLL_UP:
		zoom_in(ctx, x, y);
		break;
	case GDK_SCROLL_DOWN:
		zoom_out(ctx, x, y);
		break;
	default:
		/* ignore */;
	}
	return TRUE;
}


static void size_allocate_event(GtkWidget *widget, GdkRectangle *allocation,
    gpointer data)
{
	struct gui_ctx *ctx = data;

	zoom_to_extents(ctx);
}


/* ----- AoI callbacks ----------------------------------------------------- */


static void select_subsheet(struct gui_ctx *ctx, void *user)
{
	const struct sch_obj *obj = user;
	struct gui_sheet *sheet;

	for (sheet = ctx->sheets; sheet; sheet = sheet->next)
		if (sheet->sch == obj->u.sheet.sheet) {
			sheet->prev = ctx->curr_sheet;
			set_sheet(ctx, sheet);
			overlay_add(ctx, obj->u.sheet.name);
			return;
		}
	abort();
}


/* ----- Initialization ---------------------------------------------------- */


static void mark_aois(struct gui_sheet *sheet)
{
	const struct sch_obj *obj;

	sheet->aois = NULL;
	for (obj = sheet->sch->objs; obj; obj = obj->next)
		switch (obj->type) {
		case sch_obj_sheet: {
				struct aoi aoi = {
					.x	= obj->x,
					.y	= obj->y,
					.w	= obj->u.sheet.w,
					.h	= obj->u.sheet.h,
					.click	= select_subsheet,
					.user	= (void *) obj,
				};

				aoi_add(sheet, &aoi);
			}
			break;
		default:
			break;
		}
}


static void get_sheets(struct gui_ctx *ctx, const struct sheet *sheets)
{
	const struct sheet *sheet;
	struct gui_sheet **next = &ctx->sheets;
	struct gui_sheet *gui_sheet;

	for (sheet = sheets; sheet; sheet = sheet->next) {
		gui_sheet = alloc_type(struct gui_sheet);
		gui_sheet->sch = sheet;
		gui_sheet->prev = NULL;

		render(ctx, gui_sheet);
		mark_aois(gui_sheet);

		*next = gui_sheet;
		next = &gui_sheet->next;
	}
	*next = NULL;
	ctx->curr_sheet = ctx->sheets;
}


int gui(const struct sheet *sheets)
{
	GtkWidget *window;
	struct gui_ctx ctx = {
		.zoom		= 4,	/* scale by 1 / 16 */
		.panning	= 0,
		.overlays	= NULL,
	};

	get_sheets(&ctx, sheets);

	window = gtk_window_new(GTK_WINDOW_TOPLEVEL);

	ctx.da = gtk_drawing_area_new();
	gtk_container_add(GTK_CONTAINER(window), ctx.da);

	g_signal_connect(G_OBJECT(ctx.da), "draw",
	    G_CALLBACK(on_draw_event), &ctx);
	g_signal_connect(G_OBJECT(ctx.da), "motion_notify_event",
	    G_CALLBACK(motion_notify_event), &ctx);
	g_signal_connect(G_OBJECT(ctx.da), "button_press_event",
	    G_CALLBACK(button_press_event), &ctx);
	g_signal_connect(G_OBJECT(ctx.da), "button_release_event",
	    G_CALLBACK(button_release_event), &ctx);
	g_signal_connect(G_OBJECT(ctx.da), "scroll_event",
	    G_CALLBACK(scroll_event), &ctx);
	g_signal_connect(G_OBJECT(ctx.da), "key_press_event",
	    G_CALLBACK(key_press_event), &ctx);
	g_signal_connect(G_OBJECT(ctx.da), "size_allocate",
	    G_CALLBACK(size_allocate_event), &ctx);

	g_signal_connect(window, "destroy",
	    G_CALLBACK(gtk_main_quit), NULL);

	gtk_widget_set_can_focus(ctx.da, TRUE);

	gtk_widget_set_events(ctx.da,
	    GDK_EXPOSE | GDK_ENTER_NOTIFY_MASK | GDK_LEAVE_NOTIFY_MASK |
	    GDK_KEY_PRESS_MASK |
	    GDK_BUTTON_PRESS_MASK | GDK_BUTTON_RELEASE_MASK |
	    GDK_SCROLL_MASK |
	    GDK_POINTER_MOTION_MASK);

//	gtk_window_set_position(GTK_WINDOW(window), GTK_WIN_POS_CENTER);
//	gtk_window_set_default_size(GTK_WINDOW(window), 400, 90);
	gtk_window_set_title(GTK_WINDOW(window), "eeshow");

	gtk_widget_show_all(window);
	zoom_to_extents(&ctx);

	gtk_main();

	return 0;
}
