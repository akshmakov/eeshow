/*
 * gui/gui.c - GUI for eeshow
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

#define	_GNU_SOURCE	/* for asprintf */
#include <stddef.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>

#include <cairo/cairo.h>
#include <gtk/gtk.h>

#include "misc/util.h"
#include "misc/diag.h"
#include "gfx/style.h"
#include "gfx/cro.h"
#include "gfx/gfx.h"
#include "file/git-hist.h"
#include "kicad/lib.h"
#include "kicad/sch.h"
#include "kicad/delta.h"
#include "gfx/diff.h"
#include "kicad/dwg.h"
#include "gui/fmt-pango.h"
#include "gui/aoi.h"
#include "gui/style.h"
#include "gui/over.h"
#include "gui/input.h"
#include "gui/common.h"
#include "gui/gui.h"


/* ----- Helper functions -------------------------------------------------- */


static void redraw(const struct gui_ctx *ctx)
{
	gtk_widget_queue_draw(ctx->da);
}


static struct gui_sheet *find_corresponding_sheet(struct gui_sheet *pick_from,
     struct gui_sheet *ref_in, const struct gui_sheet *ref)
{
	struct gui_sheet *sheet, *plan_b;
	const char *title = ref->sch->title;

	/* plan A: try to find sheet with same name */

	if (title)
		for (sheet = pick_from; sheet; sheet = sheet->next)
			if (sheet->sch->title &&
			    !strcmp(title, sheet->sch->title))
				return sheet;

	/* plan B: use sheet in same position in sheet sequence */

	plan_b = ref_in;
	for (sheet = pick_from; sheet; sheet = sheet->next) {
		if (plan_b == ref)
			return sheet;
		plan_b = plan_b->next;
	}

	/* plan C: just go to the top */
	return pick_from;
}


/* ----- Rendering --------------------------------------------------------- */


#define	VCS_OVERLAYS_X		5
#define	VCS_OVERLAYS_Y		5

#define	SHEET_OVERLAYS_X	-10
#define	SHEET_OVERLAYS_Y	10


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
	overlay_draw_all(ctx->pop_overlays, cr, ctx->pop_x, ctx->pop_y);

	return FALSE;
}


static void render_sheet(struct gui_sheet *sheet)
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


/* @@@ not nice to have this so far out */
static void mark_aois(struct gui_ctx *ctx, struct gui_sheet *sheet);


static void render_delta(struct gui_ctx *ctx)
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


static void eeschema_coord(const struct gui_ctx *ctx,
    int x, int y, int *rx, int *ry)
{
	GtkAllocation alloc;

	gtk_widget_get_allocation(ctx->da, &alloc);
	*rx = ((x - ctx->x) >> ctx->zoom) + alloc.width / 2;
	*ry = ((y - ctx->y) >> ctx->zoom) + alloc.height / 2;
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


/* ----- Need this for jumping around -------------------------------------- */


static void go_to_sheet(struct gui_ctx *ctx, struct gui_sheet *sheet);
static bool go_up_sheet(struct gui_ctx *ctx);


/* ----- Revision history -------------------------------------------------- */


static void do_revision_overlays(struct gui_ctx *ctx);


static void hide_history(struct gui_ctx *ctx)
{
	input_pop();

	ctx->showing_history = 0;
	do_revision_overlays(ctx);
	redraw(ctx);
}


#define	RGBA(r, g, b, a)	((struct color) { (r), (g), (b), (a) })
#define	COLOR(color)		RGBA(color)


static void set_history_style(struct gui_hist *h, bool current)
{
	struct gui_ctx *ctx = h->ctx;
	struct overlay_style style = overlay_style_dense;
	const struct gui_hist *new = ctx->new_hist;
	const struct gui_hist *old = ctx->old_hist;

	/* this is in addition to showing detailed content */
	if (current)
		style.width++;

	switch (ctx->selecting) {
	case sel_only:
		style.frame = COLOR(FRAME_SEL_ONLY);
		break;
	case sel_old:
		style.frame = COLOR(FRAME_SEL_OLD);
		break;
	case sel_new:
		style.frame = COLOR(FRAME_SEL_NEW);
		break;
	default:
		abort();
	}

	if (ctx->new_hist == h || ctx->old_hist == h) {
		style.width++;
		style.font = BOLD_FONT;
	}
	if (ctx->old_hist) {
		if (h == new)
			style.bg = COLOR(BG_NEW);
		if (h == old)
			style.bg = COLOR(BG_OLD);
	}

	if (h->identical)
		style.fg = RGBA(0.5, 0.5, 0.5, 1);
	if (!h->sheets)
		style.fg = RGBA(0.7, 0.0, 0.0, 1);

	overlay_style(h->over, &style);
}


static bool hover_history(void *user, bool on)
{
	struct gui_hist *h = user;
	struct gui_ctx *ctx = h->ctx;
	char *s;

	if (on) {
		s = vcs_git_long_for_pango(h->vcs_hist, fmt_pango);
		overlay_text_raw(h->over, s);
		free(s);
	} else {
		overlay_text(h->over, "<small>%s</small>",
		    vcs_git_summary(h->vcs_hist));
	}
	set_history_style(h, on);
	redraw(ctx);
	return 1;
}


static void click_history(void *user)
{
	struct gui_hist *h = user;
	struct gui_ctx *ctx = h->ctx;
	struct gui_sheet *sheet;

	hide_history(ctx);

	if (!h->sheets)
		return;

	sheet = find_corresponding_sheet(h->sheets,
	    ctx->new_hist->sheets, ctx->curr_sheet);

	switch (ctx->selecting) {
	case sel_only:
		ctx->old_hist = ctx->new_hist;
		ctx->new_hist = h;
		break;
	case sel_new:
		ctx->new_hist = h;
		break;
	case sel_old:
		ctx->old_hist = h;
		break;
	default:
		abort();
	}

	if (ctx->new_hist->age > ctx->old_hist->age) {
		swap(ctx->new_hist, ctx->old_hist);
		if (ctx->selecting == sel_old)
			go_to_sheet(ctx, sheet);
		else
			render_delta(ctx);
	} else {
		if (ctx->selecting != sel_old)
			go_to_sheet(ctx, sheet);
		else
			render_delta(ctx);
	}

	if (ctx->old_hist == ctx->new_hist)
		ctx->old_hist = NULL;

	do_revision_overlays(ctx);
	redraw(ctx);
}


static void ignore_click(void *user)
{
}


static struct gui_hist *skip_history(struct gui_ctx *ctx, struct gui_hist *h)
{
	struct overlay_style style = overlay_style_dense;
	unsigned n;

	/* don't skip the first entry */
	if (h == ctx->hist)
		return h;

	/* need at least two entries */
	if (!h->identical || !h->next || !h->next->identical)
		return h;

	/* don't skip the last entry */
	for (n = 0; h->next && h->identical; h = h->next)
		n++;

	h->over = overlay_add(&ctx->hist_overlays, &ctx->aois,
	    NULL, ignore_click, h);
	overlay_text(h->over, "<small>%u commits without changes</small>", n);

	style.width = 0;
	style.pad = 0;
	style.bg = RGBA(1.0, 1.0, 1.0, 0.8);
	overlay_style(h->over, &style);

	return h;
}


static const struct input_ops history_input_ops;


static void show_history(struct gui_ctx *ctx, enum selecting sel)
{
	struct gui_hist *h = ctx->hist;

	input_push(&history_input_ops, ctx);

	ctx->showing_history = 1;
	ctx->hist_y_offset = 0;
	ctx->selecting = sel;
	overlay_remove_all(&ctx->hist_overlays);
	for (h = ctx->hist; h; h = h->next) {
		h = skip_history(ctx, h);
		h->over = overlay_add(&ctx->hist_overlays, &ctx->aois,
		    hover_history, click_history, h);
		hover_history(h, 0);
		set_history_style(h, 0);
	}
	redraw(ctx);
}


static void show_history_cb(void *user)
{
	struct gui_hist *h = user;
	struct gui_ctx *ctx = h->ctx;
	enum selecting sel = sel_only;

	if (ctx->old_hist)
		sel = h == ctx->new_hist ? sel_new : sel_old;
	show_history(ctx, sel);
}


/* ----- Navigate sheets --------------------------------------------------- */


/* @@@ find a better place for this forward declaration */
static void mark_aois(struct gui_ctx *ctx, struct gui_sheet *sheet);


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


static void revision_overlays_diff(struct gui_ctx *ctx)
{
	struct gui_hist *new = ctx->new_hist;
	struct gui_hist *old = ctx->old_hist;

	new->over = overlay_add(&ctx->hist_overlays, &ctx->aois,
	    show_history_details, show_history_cb, new);
	overlay_style(new->over, &overlay_style_diff_new);
	show_history_details(new, 0);

	old->over = overlay_add(&ctx->hist_overlays, &ctx->aois,
	    show_history_details, show_history_cb, old);
	overlay_style(old->over, &overlay_style_diff_old);
	show_history_details(old, 0);
}


static void do_revision_overlays(struct gui_ctx *ctx)
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
	}
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


static void go_to_sheet(struct gui_ctx *ctx, struct gui_sheet *sheet)
{
	aoi_dehover();
	overlay_remove_all(&ctx->pop_overlays);
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


/* ----- Input: sheet ------------------------------------------------------ */


static bool sheet_click(void *user, int x, int y)
{
	struct gui_ctx *ctx = user;
	const struct gui_sheet *curr_sheet = ctx->curr_sheet;
	int ex, ey;

	canvas_coord(ctx, x, y, &ex, &ey);

	if (aoi_click(ctx->aois, x, y))
		return 1;
	if (aoi_click(curr_sheet->aois,
	    ex + curr_sheet->xmin, ey + curr_sheet->ymin))
		return 1;

	if (ctx->showing_history)
		hide_history(ctx);
	overlay_remove_all(&ctx->pop_overlays);
	redraw(ctx);
	return 1;
}


static bool sheet_hover_update(void *user, int x, int y)
{
	struct gui_ctx *ctx = user;
	const struct gui_sheet *curr_sheet = ctx->curr_sheet;
	int ex, ey;

	canvas_coord(ctx, x, y, &ex, &ey);

	if (aoi_hover(ctx->aois, x, y))
		return 1;
	return aoi_hover(curr_sheet->aois,
	    ex + curr_sheet->xmin, ey + curr_sheet->ymin);
}


static void sheet_hover_end(void *user)
{
}


static void dehover_glabel(struct gui_ctx *ctx);


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


static void sheet_scroll(void *user, int x, int y, int dy)
{
	struct gui_ctx *ctx = user;
	int ex, ey;

	canvas_coord(ctx, x, y, &ex, &ey);
	if (dy < 0)
		zoom_in(ctx, ex, ey);
	else
		zoom_out(ctx, ex, ey);
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
		if (sheet != ctx->new_hist->sheets)
			go_to_sheet(ctx, ctx->new_hist->sheets);
		break;
	case GDK_KEY_BackSpace:
	case GDK_KEY_Delete:
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
	case GDK_KEY_q:
		gtk_main_quit();
	}
}


static const struct input_ops sheet_input_ops = {
	.click		= sheet_click,
	.hover_begin	= sheet_hover_update,
	.hover_update	= sheet_hover_update,
	.hover_click	= sheet_click,
	.hover_end	= sheet_hover_end,
	.scroll		= sheet_scroll,
	.drag_begin	= sheet_drag_begin,
	.drag_move	= sheet_drag_move,
	.key		= sheet_key,
};


/* ----- Input: history ---------------------------------------------------- */


static void history_drag_move(void *user, int dx, int dy)
{
	struct gui_ctx *ctx = user;

	ctx->hist_y_offset += dy;
	redraw(ctx);
}


/* @@@ under construction */


static const struct input_ops history_input_ops = {
	.click		= sheet_click,
	.hover_begin	= sheet_hover_update,
	.hover_update	= sheet_hover_update,
	.hover_click	= sheet_click,
	.hover_end	= sheet_hover_end,
	.scroll		= sheet_scroll,
	.drag_begin	= input_accept,
	.drag_move	= history_drag_move,
	.key		= sheet_key,
};


/* ----- Event handlers ---------------------------------------------------- */


static void size_allocate_event(GtkWidget *widget, GdkRectangle *allocation,
    gpointer data)
{
	struct gui_ctx *ctx = data;

	zoom_to_extents(ctx);
}


/* ----- AoI callbacks ----------------------------------------------------- */


struct sheet_aoi_ctx {
	struct gui_ctx *gui_ctx;
	const struct sch_obj *obj;
};


static void select_subsheet(void *user)
{
	const struct sheet_aoi_ctx *aoi_ctx = user;
	struct gui_ctx *ctx = aoi_ctx->gui_ctx;
	const struct sch_obj *obj = aoi_ctx->obj;
	struct gui_sheet *sheet;

	if (!obj->u.sheet.sheet)
		return;
	for (sheet = ctx->new_hist->sheets; sheet; sheet = sheet->next)
		if (sheet->sch == obj->u.sheet.sheet) {
			go_to_sheet(ctx, sheet);
			return;
		}
	abort();
}


struct glabel_aoi_ctx {
	const struct gui_sheet *sheet;
	const struct sch_obj *obj;
	struct dwg_bbox bbox;
	struct overlay *over;
};


/* small offset to hide rounding errors */
#define	CHEAT	1


static void glabel_dest_click(void *user)
{
	struct gui_sheet *sheet = user;

	go_to_sheet(sheet->ctx, sheet);
}


static void dehover_glabel(struct gui_ctx *ctx)
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


/* ----- Progress bar ------------------------------------------------------ */


#define	PROGRESS_BAR_HEIGHT	10


static void progress_draw_event(GtkWidget *widget, cairo_t *cr,
    gpointer user_data)
{
	GtkAllocation alloc;
	struct gui_ctx *ctx = user_data;
	unsigned w, x;

	x = ctx->progress >> ctx->progress_scale;
	if (!x) {
		/* @@@ needed ? Gtk seems to always clear the the surface. */
		cairo_set_source_rgb(cr, 1, 1, 1);
		cairo_paint(cr);
	}

	gtk_widget_get_allocation(ctx->da, &alloc);
	w = ctx->hist_size >> ctx->progress_scale;

	cairo_save(cr);
	cairo_translate(cr,
	    (alloc.width - w) / 2, (alloc.height - PROGRESS_BAR_HEIGHT) / 2);

	cairo_set_source_rgb(cr, 0, 0.7, 0);
	cairo_set_line_width(cr, 0);
	cairo_rectangle(cr, 0, 0, x, PROGRESS_BAR_HEIGHT);
	cairo_fill(cr);

	cairo_set_source_rgb(cr, 0, 0, 0);
	cairo_set_line_width(cr, 2);
	cairo_rectangle(cr, 0, 0, w, PROGRESS_BAR_HEIGHT);
	cairo_stroke(cr);

	cairo_restore(cr);
}


static void setup_progress_bar(struct gui_ctx *ctx, GtkWidget *window)
{
	GtkAllocation alloc;

	gtk_widget_get_allocation(ctx->da, &alloc);

	ctx->progress_scale = 0;
	while ((ctx->hist_size >> ctx->progress_scale) > alloc.width)
		ctx->progress_scale++;
	ctx->progress = 0;

	g_signal_connect(G_OBJECT(ctx->da), "draw",
	    G_CALLBACK(progress_draw_event), ctx);

	redraw(ctx);
	gtk_main_iteration_do(0);
}


static void progress_update(struct gui_ctx *ctx)
{
	unsigned mask = (1 << ctx->progress_scale) - 1;

	ctx->progress++;
	if ((ctx->progress & mask) != mask)
		return;

	redraw(ctx);
	gtk_main_iteration_do(0);
}


/* ----- Initialization ---------------------------------------------------- */


static void add_sheet_aoi(struct gui_ctx *ctx, struct gui_sheet *parent,
    const struct sch_obj *obj)
{
	struct sheet_aoi_ctx *aoi_ctx = alloc_type(struct sheet_aoi_ctx);

	aoi_ctx->gui_ctx = ctx;
	aoi_ctx->obj = obj;

	struct aoi aoi = {
		.x	= obj->x,
		.y	= obj->y,
		.w	= obj->u.sheet.w,
		.h	= obj->u.sheet.h,
		.click	= select_subsheet,
		.user	= aoi_ctx,
	};

	aoi_add(&parent->aois, &aoi);
}


static void add_glabel_aoi(struct gui_sheet *sheet, const struct sch_obj *obj)
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


static void mark_aois(struct gui_ctx *ctx, struct gui_sheet *sheet)
{
	const struct sch_obj *obj;

	sheet->aois = NULL;
	for (obj = sheet->sch->objs; obj; obj = obj->next)
		switch (obj->type) {
		case sch_obj_sheet:
			add_sheet_aoi(ctx, sheet, obj);
			break;
		case sch_obj_glabel:
			add_glabel_aoi(sheet, obj);
		default:
			break;
		}
}


static struct gui_sheet *get_sheets(struct gui_ctx *ctx,
    const struct sheet *sheets)
{
	const struct sheet *sheet;
	struct gui_sheet *gui_sheets = NULL;
	struct gui_sheet **next = &gui_sheets;
	struct gui_sheet *new;

	for (sheet = sheets; sheet; sheet = sheet->next) {
		new = alloc_type(struct gui_sheet);
		new->sch = sheet;
		new->ctx = ctx;
		new->rendered = 0;

		*next = new;
		next = &new->next;
	}
	*next = NULL;
	return gui_sheets;
}


/*
 * Library caching:
 *
 * We reuse previous components if all libraries are identical
 *
 * Future optimizations:
 * - don't parse into single list of components, so that we can share
 *   libraries that are the same, even if there are others that have changed.
 * - maybe put components into tree, so that they can be replaced individually
 *   (this would also help to identify sheets that don't need parsing)
 *
 * Sheet caching:
 *
 * We reuse previous sheets if
 * - all libraries are identical (whether a given sheet uses them or not),
 * - they have no sub-sheets, and
 * - the objects IDs (hashes) are identical.
 *
 * Note that we only compare with the immediately preceding (newer) revision,
 * so branches and merges can disrupt caching.
 *
 * Possible optimizations:
 * - if we record which child sheets a sheet has, we could also clone it,
 *   without having to parse it. However, this is somewhat complex and may
 *   not save all that much time.
 * - we could record what libraries a sheet uses, and parse only if one of
 *   these has changed (benefits scenarios with many library files),
 * - we could record what components a sheet uses, and parse only if one of
 *   these has changed (benefits scenarios with few big libraries),
 * - we could postpone library lookups to render time.
 * - we could record IDs globally, which would help to avoid tripping over
 *   branches and merges.
 */

static const struct sheet *parse_files(struct gui_hist *hist,
    int n_args, char **args, bool recurse, struct gui_hist *prev)
{
	char *rev = NULL;
	struct file sch_file;
	struct file lib_files[n_args - 1];
	int libs_open, i;
	bool libs_cached = 0;
	bool ok;

	if (hist->vcs_hist && hist->vcs_hist->commit)
		rev = vcs_git_get_rev(hist->vcs_hist);

	sch_init(&hist->sch_ctx, recurse);
	ok = file_open_revision(&sch_file, rev, args[n_args - 1], NULL);

	if (rev)
		free(rev);
	if (!ok) {
		sch_free(&hist->sch_ctx);
		return NULL;
	}

	lib_init(&hist->lib);
	for (libs_open = 0; libs_open != n_args - 1; libs_open++)
		if (!file_open(lib_files + libs_open, args[libs_open],
		    &sch_file))
			goto fail;

	if (hist->vcs_hist) {
		hist->oids = alloc_type_n(void *, libs_open);
		hist->libs_open = libs_open;
		for (i = 0; i != libs_open; i++)
			hist->oids[i] = file_oid(lib_files + i);
		if (prev && prev->vcs_hist && prev->libs_open == libs_open) {
			for (i = 0; i != libs_open; i++)
				if (!file_oid_eq(hist->oids[i], prev->oids[i]))
					break;
			if (i == libs_open) {
				hist->lib.comps = prev->lib.comps;
				libs_cached = 1;
			}
		}
	}

	if (!libs_cached)
		for (i = 0; i != libs_open; i++)
			if (!lib_parse_file(&hist->lib, lib_files +i))
				goto fail;

	if (!sch_parse(&hist->sch_ctx, &sch_file, &hist->lib,
	    libs_cached ? &prev->sch_ctx : NULL))
		goto fail;

	for (i = 0; i != libs_open; i++)
		file_close(lib_files + i);
	file_close(&sch_file);

	if (prev && sheet_eq(prev->sch_ctx.sheets, hist->sch_ctx.sheets))
		prev->identical = 1;

	/*
	 * @@@ we have a major memory leak for the component library.
	 * We should record parsed schematics and libraries separately, so
	 * that we can clean them up, without having to rely on the history,
	 * with - when sharing unchanged item - possibly many duplicate
	 * pointers.
	 */
	return hist->sch_ctx.sheets;

fail:
	while (libs_open--)
		file_close(lib_files + libs_open);
	sch_free(&hist->sch_ctx);
	lib_free(&hist->lib);
	file_close(&sch_file);
	return NULL;
}


struct add_hist_ctx {
	struct gui_ctx *ctx;
	int n_args;
	char **args;
	bool recurse;
	unsigned limit;
};


static void add_hist(void *user, struct hist *h)
{
	struct add_hist_ctx *ahc = user;
	struct gui_ctx *ctx = ahc->ctx;
	struct gui_hist **anchor, *hist, *prev;
	const struct sheet *sch;
	unsigned age = 0;

	if (!ahc->limit)
		return;
	if (ahc->limit > 0)
		ahc->limit--;

	prev = NULL;
	for (anchor = &ctx->hist; *anchor; anchor = &(*anchor)->next) {
		prev = *anchor;
		age++;
	}

	hist = alloc_type(struct gui_hist);
	hist->ctx = ctx;
	hist->vcs_hist = h;
	hist->identical = 0;
	sch = parse_files(hist, ahc->n_args, ahc->args, ahc->recurse, prev);
	hist->sheets = sch ? get_sheets(ctx, sch) : NULL;
	hist->age = age;

	hist->next = NULL;
	*anchor = hist;

	if (ctx->hist_size)
		progress_update(ctx);
}


static void count_history(void *user, struct hist *h)
{
	struct gui_ctx *ctx = user;

	ctx->hist_size++;
}


static void get_history(struct gui_ctx *ctx, const char *sch_name, int limit)
{
	if (!vcs_git_try(sch_name)) {
		ctx->vcs_hist = NULL;
		return;
	} 
	
	ctx->vcs_hist = vcs_git_hist(sch_name);
	if (limit)
		ctx->hist_size = limit > 0 ? limit : -limit;
	else
		hist_iterate(ctx->vcs_hist, count_history, ctx);
}


static void get_revisions(struct gui_ctx *ctx,
    int n_args, char **args, bool recurse, int limit)
{
	struct add_hist_ctx add_hist_ctx = {
		.ctx		= ctx,
		.n_args		= n_args,
		.args		= args,
		.recurse	= recurse,
		.limit		= limit ? limit < 0 ? -limit : limit : -1,
	};

	if (ctx->vcs_hist)
		hist_iterate(ctx->vcs_hist, add_hist, &add_hist_ctx);
	else
		add_hist(&add_hist_ctx, NULL);
}


int gui(unsigned n_args, char **args, bool recurse, int limit)
{
	GtkWidget *window;
	struct gui_ctx ctx = {
		.zoom		= 4,	/* scale by 1 / 16 */
		.hist		= NULL,
		.vcs_hist	= NULL,
		.showing_history= 0,
		.sheet_overlays	= NULL,
		.hist_overlays	= NULL,
		.pop_overlays	= NULL,
		.aois		= NULL,
		.old_hist	= NULL,
		.hist_y_offset 	= 0,
		.hist_size	= 0,
	};

	window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	ctx.da = gtk_drawing_area_new();
	gtk_container_add(GTK_CONTAINER(window), ctx.da);

	gtk_window_set_default_size(GTK_WINDOW(window), 640, 480);
	gtk_window_set_title(GTK_WINDOW(window), "eeshow");

	gtk_widget_set_events(ctx.da,
	    GDK_EXPOSE | GDK_ENTER_NOTIFY_MASK | GDK_LEAVE_NOTIFY_MASK);

	input_setup(ctx.da);

	gtk_widget_show_all(window);

	get_history(&ctx, args[n_args - 1], limit);
	if (ctx.hist_size)
		setup_progress_bar(&ctx, window);

	get_revisions(&ctx, n_args, args, recurse, limit);
	for (ctx.new_hist = ctx.hist; ctx.new_hist && !ctx.new_hist->sheets;
	    ctx.new_hist = ctx.new_hist->next);
	if (!ctx.new_hist)
		fatal("no valid sheets\n");

	g_signal_connect(G_OBJECT(ctx.da), "draw",
	    G_CALLBACK(on_draw_event), &ctx);
	g_signal_connect(G_OBJECT(ctx.da), "size_allocate",
	    G_CALLBACK(size_allocate_event), &ctx);

	g_signal_connect(window, "destroy",
	    G_CALLBACK(gtk_main_quit), NULL);

//	gtk_window_set_position(GTK_WINDOW(window), GTK_WIN_POS_CENTER);

	go_to_sheet(&ctx, ctx.new_hist->sheets);
	gtk_widget_show_all(window);

	input_push(&sheet_input_ops, &ctx);

	/* for performance testing, use -N-depth */
	if (limit >= 0)
		gtk_main();

	return 0;
}
