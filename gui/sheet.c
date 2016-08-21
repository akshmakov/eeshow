/*
 * gui/sheet.c - Sheet navigation
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
#include <stdlib.h>

#include <gtk/gtk.h>

#include "file/git-hist.h"
#include "kicad/sch.h"
#include "kicad/delta.h"
#include "gui/fmt-pango.h"
#include "gui/style.h"
#include "gui/aoi.h"
#include "gui/over.h"
#include "gui/input.h"
#include "gui/help.h"
#include "gui/icons.h"
#include "gui/common.h"


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


/* ----- Zoom -------------------------------------------------------------- */


static bool zoom_in(struct gui_ctx *ctx, int x, int y)
{
	if (ctx->zoom == 0)
		return 0;
	ctx->zoom--;
	ctx->x = (ctx->x + x) / 2;
	ctx->y = (ctx->y + y) / 2;
	redraw(ctx);
	return 1;
}


static bool zoom_out(struct gui_ctx *ctx, int x, int y)
{
	if (ctx->curr_sheet->w >> ctx->zoom <= 16)
		return 0;
	ctx->zoom++;
	ctx->x = 2 * ctx->x - x;
	ctx->y = 2 * ctx->y - y;
	redraw(ctx);
	return 1;
}


static void curr_sheet_size(struct gui_ctx *ctx, int *w, int *h)
{
	const struct gui_sheet *sheet = ctx->curr_sheet;
	int ax1, ay1, bx1, by1;

	if (!ctx->old_hist) {
		*w = sheet->w;
		*h = sheet->h;
	} else {
		const struct gui_sheet *old =
		    find_corresponding_sheet(ctx->old_hist->sheets,
		    ctx->new_hist->sheets, sheet);

		/*
		 * We're only interested in differences here, so no need for
		 * the usual "-1" in x1 = x0 + w - 1
		 */
		ax1 = sheet->xmin + sheet->w;
		ay1 = sheet->ymin + sheet->h;
		bx1 = old->xmin + old->w;
		by1 = old->ymin + old->h;
		*w = (ax1 > bx1 ? ax1 : bx1) -
		    (sheet->xmin < old->xmin ? sheet->xmin : old->xmin);
		*h = (ay1 > by1 ? ay1 : by1) -
		    (sheet->ymin < old->ymin ? sheet->ymin : old->ymin);
	}
}


static void zoom_to_extents(struct gui_ctx *ctx)
{
	GtkAllocation alloc;
	int w, h;

	curr_sheet_size(ctx, &w, &h);
	ctx->x = w / 2;
	ctx->y = h / 2;

	gtk_widget_get_allocation(ctx->da, &alloc);
	ctx->zoom = 0;
	while (w >> ctx->zoom > alloc.width || h >> ctx->zoom > alloc.height)
		ctx->zoom++;

	redraw(ctx);
}


/* ----- Revision selection overlays --------------------------------------- */


static bool show_history_details(void *user, bool on)
{
	struct gui_hist *h = user;
	struct gui_ctx *ctx = h->ctx;
	char *s;

	if (on) {
		s = vcs_git_long_for_pango(h->vcs_hist, fmt_pango);
		overlay_text_raw(h->over, s);
		free(s);
	} else {
		overlay_text(h->over, "%.40s", vcs_git_summary(h->vcs_hist));
	}
	redraw(ctx);
	return 1;
}


static void set_diff_mode(struct gui_ctx *ctx, enum diff_mode mode)
{
	ctx->diff_mode = mode;
	do_revision_overlays(ctx);
	redraw(ctx);
}


static void show_history_cb(void *user)
{
	struct gui_hist *h = user;
	struct gui_ctx *ctx = h->ctx;
	enum selecting sel = sel_only;

	if (ctx->old_hist) {
		if (h == ctx->new_hist && ctx->diff_mode != diff_new) {
			set_diff_mode(ctx, diff_new);
			return;
		}
		if (h == ctx->old_hist && ctx->diff_mode != diff_old) {
			set_diff_mode(ctx, diff_old);
			return;
		}
		sel = h == ctx->new_hist ? sel_new : sel_old;
	}
	show_history(ctx, sel);
}


static void show_diff_cb(void *user)
{
	struct gui_ctx *ctx = user;

	if (ctx->old_hist)
		set_diff_mode(ctx,
		    ctx->diff_mode == diff_delta ? diff_new : diff_delta);
	else
		show_history(ctx, sel_split);
}


static void toggle_old_new(struct gui_ctx *ctx)
{
	set_diff_mode(ctx, ctx->diff_mode == diff_new ? diff_old : diff_new);
}


static void add_delta(struct gui_ctx *ctx)
{
	struct overlay *over;
	struct overlay_style style;

	over = overlay_add(&ctx->hist_overlays, &ctx->aois,
	    NULL, show_diff_cb, ctx);
	style = overlay_style_default;
	if (ctx->old_hist && ctx->diff_mode == diff_delta)
		style.frame = RGBA(0, 0, 0, 1);
	overlay_style(over, &style);
	if (use_delta)
		overlay_icon(over, icon_delta);
	else
		overlay_icon(over, icon_diff);
}


static void revision_overlays_diff(struct gui_ctx *ctx)
{
	struct gui_hist *new = ctx->new_hist;
	struct gui_hist *old = ctx->old_hist;
	struct overlay_style style;

	new->over = overlay_add(&ctx->hist_overlays, &ctx->aois,
	    show_history_details, show_history_cb, new);
	style = overlay_style_diff_new;
	if (ctx->diff_mode == diff_new)
		style.frame = RGBA(0, 0, 0, 1);
	overlay_style(new->over, &style);
	show_history_details(new, 0);

	add_delta(ctx);

	old->over = overlay_add(&ctx->hist_overlays, &ctx->aois,
	    show_history_details, show_history_cb, old);
	style = overlay_style_diff_old;
	if (ctx->diff_mode == diff_old)
		style.frame = RGBA(0, 0, 0, 1);
	overlay_style(old->over, &style);
	show_history_details(old, 0);
}


void do_revision_overlays(struct gui_ctx *ctx)
{
	overlay_remove_all(&ctx->hist_overlays);

	if (ctx->old_hist) {
		revision_overlays_diff(ctx);
	} else {
		ctx->new_hist->over = overlay_add(&ctx->hist_overlays,
		    &ctx->aois, show_history_details, show_history_cb,
		    ctx->new_hist);
		overlay_style(ctx->new_hist->over, &overlay_style_default);
		show_history_details(ctx->new_hist, 0);

		add_delta(ctx);
	}
}


/* ----- Sheet selection overlays ------------------------------------------ */


static void close_subsheet(void *user)
{
	struct gui_sheet *sheet = user;
	struct gui_ctx *ctx = sheet->ctx;

	go_to_sheet(ctx, sheet);
}


static bool hover_sheet(void *user, bool on)
{
	struct gui_sheet *sheet = user;
	struct gui_ctx *ctx = sheet->ctx;
	const char *title = sheet->sch->title;

	if (!title)
		title = "(unnamed)";
	if (on) {
		const struct gui_sheet *s;
		int n = 0, this = -1;

		for (s = ctx->new_hist->sheets; s; s = s->next) {
			n++;
			if (s == sheet)
				this = n;
		}
		overlay_text(sheet->over, "<b>%s</b>\n<big>%d / %d</big>",
		    title, this, n);
	} else {
		overlay_text(sheet->over, "<b>%s</b>", title);
	}
	redraw(ctx);
	return 1;
}


static struct gui_sheet *find_parent_sheet(struct gui_sheet *sheets,
    const struct gui_sheet *ref)
{
	struct gui_sheet *parent;
	const struct sch_obj *obj;

	for (parent = sheets; parent; parent = parent->next)
		for (obj = parent->sch->objs; obj; obj = obj->next)
			if (obj->type == sch_obj_sheet &&
			    obj->u.sheet.sheet == ref->sch)
				return parent;
	return NULL;
}


static void sheet_selector_recurse(struct gui_ctx *ctx, struct gui_sheet *sheet)
{
	struct gui_sheet *parent;

	parent = find_parent_sheet(ctx->new_hist->sheets, sheet);
	if (parent)
		sheet_selector_recurse(ctx, parent);
	sheet->over = overlay_add(&ctx->sheet_overlays, &ctx->aois,
	    hover_sheet, close_subsheet, sheet);
	hover_sheet(sheet, 0);
}


static void do_sheet_overlays(struct gui_ctx *ctx)
{
	overlay_remove_all(&ctx->sheet_overlays);
	sheet_selector_recurse(ctx, ctx->curr_sheet);
}


/* ----- Navigate sheets --------------------------------------------------- */


void go_to_sheet(struct gui_ctx *ctx, struct gui_sheet *sheet)
{
	aoi_dehover();
	overlay_remove_all(&ctx->pop_overlays);
	overlay_remove_all(&ctx->pop_underlays);
	if (!sheet->rendered) {
		render_sheet(sheet);
		mark_aois(ctx, sheet);
	}
	ctx->curr_sheet = sheet;
	if (ctx->old_hist)
		render_delta(ctx);
	if (ctx->vcs_hist)
		do_revision_overlays(ctx);
	do_sheet_overlays(ctx);
	zoom_to_extents(ctx);
}


static bool go_up_sheet(struct gui_ctx *ctx)
{
	struct gui_sheet *parent;

	parent = find_parent_sheet(ctx->new_hist->sheets, ctx->curr_sheet);
	if (!parent)
		return 0;
	go_to_sheet(ctx, parent);
	return 1;
}


static bool go_prev_sheet(struct gui_ctx *ctx)
{
	struct gui_sheet *sheet;

	for (sheet = ctx->new_hist->sheets; sheet; sheet = sheet->next)
		if (sheet->next && sheet->next == ctx->curr_sheet) {
			go_to_sheet(ctx, sheet);
			return 1;
		}
	return 0;
}


static bool go_next_sheet(struct gui_ctx *ctx)
{
	if (!ctx->curr_sheet->next)
		return 0;
	go_to_sheet(ctx, ctx->curr_sheet->next);
	return 1;
}


/* ----- Input ------------------------------------------------------------- */


static bool sheet_click(void *user, int x, int y)
{
	struct gui_ctx *ctx = user;
	const struct gui_sheet *curr_sheet = ctx->curr_sheet;
	int ex, ey;

	canvas_coord(ctx, x, y, &ex, &ey);

	if (ctx->old_hist && ctx->diff_mode == diff_old)
		curr_sheet = find_corresponding_sheet(ctx->old_hist->sheets,
		    ctx->new_hist->sheets, ctx->curr_sheet);

	if (aoi_click(&ctx->aois, x, y))
		return 1;
	if (aoi_click(&curr_sheet->aois,
	    ex + curr_sheet->xmin, ey + curr_sheet->ymin))
		return 1;

	overlay_remove_all(&ctx->pop_overlays);
	overlay_remove_all(&ctx->pop_underlays);
	redraw(ctx);
	return 1;
}


static bool sheet_hover_update(void *user, int x, int y)
{
	struct gui_ctx *ctx = user;
	const struct gui_sheet *curr_sheet = ctx->curr_sheet;
	int ex, ey;

	canvas_coord(ctx, x, y, &ex, &ey);

	if (ctx->old_hist && ctx->diff_mode == diff_old)
		curr_sheet = find_corresponding_sheet(ctx->old_hist->sheets,
		    ctx->new_hist->sheets, ctx->curr_sheet);

	if (aoi_hover(&ctx->aois, x, y))
		return 1;
	return aoi_hover(&curr_sheet->aois,
	    ex + curr_sheet->xmin, ey + curr_sheet->ymin);
}


static bool sheet_drag_begin(void *user, int x, int y)
{
	dehover_glabel(user);
	return 1;
}


static void sheet_drag_move(void *user, int dx, int dy)
{
	struct gui_ctx *ctx = user;

	ctx->x -= dx << ctx->zoom;
	ctx->y -= dy << ctx->zoom;
	redraw(ctx);
}


static void sheet_drag_end(void *user)
{
	input_update();
}


static void sheet_scroll(void *user, int x, int y, int dy)
{
	struct gui_ctx *ctx = user;
	int ex, ey;


	canvas_coord(ctx, x, y, &ex, &ey);
	if (dy < 0) {
		if (!zoom_in(ctx, ex, ey))
			return;
	} else {
		if (!zoom_out(ctx, ex, ey))
			return;
	}
	dehover_glabel(ctx);
	input_update();
}


static void sheet_key(void *user, int x, int y, int keyval)
{
	struct gui_ctx *ctx = user;
	struct gui_sheet *sheet = ctx->curr_sheet;
	int ex, ey;

	canvas_coord(ctx, x, y, &ex, &ey);

	switch (keyval) {
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
	case GDK_KEY_KP_Home:
		if (sheet != ctx->new_hist->sheets)
			go_to_sheet(ctx, ctx->new_hist->sheets);
		break;
	case GDK_KEY_BackSpace:
	case GDK_KEY_Delete:
	case GDK_KEY_KP_Delete:
		go_up_sheet(ctx);
		break;
	case GDK_KEY_Page_Up:
	case GDK_KEY_KP_Page_Up:
		go_prev_sheet(ctx);
		break;
	case GDK_KEY_Page_Down:
	case GDK_KEY_KP_Page_Down:
		go_next_sheet(ctx);
		break;
	case GDK_KEY_Up:
	case GDK_KEY_KP_Up:
		show_history(ctx, sel_new);
		break;
	case GDK_KEY_Down:
	case GDK_KEY_KP_Down:
		show_history(ctx, sel_old);
		break;
	case GDK_KEY_Tab:
	case GDK_KEY_KP_Tab:
		toggle_old_new(ctx);
		break;

	case GDK_KEY_Escape:
		dehover_glabel(user);
		ctx->glabel = NULL;
		redraw(ctx);
		break;

	case GDK_KEY_a:
		use_delta = !use_delta;
		do_revision_overlays(ctx);
		redraw(ctx);
		break;

	case GDK_KEY_n:
		ctx->diff_mode = diff_new;
		redraw(ctx);
		break;
	case GDK_KEY_o:
		ctx->diff_mode = diff_old;
		redraw(ctx);
		break;
	case GDK_KEY_d:
		ctx->diff_mode = diff_delta;
		redraw(ctx);
		break;

	case GDK_KEY_h:
		help();
		break;

	case GDK_KEY_q:
		gtk_main_quit();
	}
}


static const struct input_ops sheet_input_ops = {
	.click		= sheet_click,
	.hover_begin	= sheet_hover_update,
	.hover_update	= sheet_hover_update,
	.hover_click	= sheet_click,
	.scroll		= sheet_scroll,
	.drag_begin	= sheet_drag_begin,
	.drag_move	= sheet_drag_move,
	.drag_end	= sheet_drag_end,
	.key		= sheet_key,
};


/* ----- Event handlers ---------------------------------------------------- */


static void size_allocate_event(GtkWidget *widget, GdkRectangle *allocation,
    gpointer data)
{
	struct gui_ctx *ctx = data;

	zoom_to_extents(ctx);
}


/* ----- Initialization ---------------------------------------------------- */


void sheet_setup(struct gui_ctx *ctx)
{
	g_signal_connect(G_OBJECT(ctx->da), "size_allocate",
	    G_CALLBACK(size_allocate_event), ctx);
	input_push(&sheet_input_ops, ctx);
}
