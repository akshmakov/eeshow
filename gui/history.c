/*
 * gui/history.c - Revision history (navigation)
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
#include <pango/pangocairo.h>

#include "misc/util.h"
#include "misc/diag.h"
#include "file/git-hist.h"
#include "gui/fmt-pango.h"
#include "gui/style.h"
#include "gui/aoi.h"
#include "gui/over.h"
#include "gui/input.h"
#include "gui/common.h"


static void hide_history(struct gui_ctx *ctx)
{
	input_pop();

	ctx->mode = showing_sheet;
	do_revision_overlays(ctx);
	redraw(ctx);
}


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
	case sel_split:
		style.frame = COLOR(FRAME_SEL_ONLY);
		break;
	case sel_old:
		style.frame = COLOR(FRAME_SEL_OLD);
		break;
	case sel_new:
		style.frame = COLOR(FRAME_SEL_NEW);
		break;
	default:
		BUG("invalid mode %d", ctx->selecting);
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


/*
 * One difficulty with resizing (enlarging, in this case) list items on hover
 * is that, if we only change the size but not the position, hovering towards
 * the next item will cause the previous item to shrink and thus move the next
 * item up - possibly even above the mouse pointer. This can be confusing.
 *
 * We could adjust the mouse pointer, but manipulating the pointer position is
 * not universally popular.
 *
 * Instead, we move the list such that the bottom edge of the item we're
 * leaving remains stationary. Thus the list moves down when mousing over items
 * from the top towards the bottom.
 *
 * To prevent this movement from being overly pronounced, we try to predict the
 * direction in which an item will be left (i.e., in the same direction from
 * which it was entered), and compensate for the likely list movement on
 * departure on entry.
 *
 * These heuristics can still sometimes fail, but by and large, they produce
 * the desired result without introducing too much list movement.
 */

static bool hover_history(void *user, bool on, int dx, int dy)
{
	struct gui_hist *h = user;
	struct gui_ctx *ctx = h->ctx;
	char *s;
	int before, after;

	if (dy)
		overlay_size(h->over, gtk_widget_get_pango_context(ctx->da),
		    NULL, &before);
	if (on) {
		s = vcs_git_long_for_pango(h->vcs_hist, fmt_pango);
		overlay_text_raw(h->over, s);
		free(s);
	} else {
		overlay_text(h->over, "<small>%s</small>",
		    vcs_git_summary(h->vcs_hist));
	}
	set_history_style(h, on);
	if (dy)
		overlay_size(h->over, gtk_widget_get_pango_context(ctx->da),
		    NULL, &after);

	if (dy < 0 && on)
		ctx->hist_y_offset -= after - before;
	if (dy > 0 && !on)
		ctx->hist_y_offset -= after - before;

	redraw(ctx);
	return 1;
}


static void click_history(void *user)
{
	struct gui_hist *h = user;
	struct gui_ctx *ctx = h->ctx;
	struct gui_sheet *sheet, *old_sheet;

	hide_history(ctx);

	if (!h->sheets)
		return;

	sheet = find_corresponding_sheet(h->sheets,
	    ctx->new_hist->sheets, ctx->curr_sheet);
	old_sheet = find_corresponding_sheet(
	    ctx->old_hist ? ctx->old_hist->sheets : ctx->new_hist->sheets,
	    ctx->new_hist->sheets, ctx->curr_sheet);

	switch (ctx->selecting) {
	case sel_only:
		ctx->new_hist = h;
		break;
	case sel_split:
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
		BUG("invalid mode %d", ctx->selecting);
	}

	ctx->diff_mode = diff_delta;

	if (ctx->old_hist) {
		if (ctx->new_hist->age > ctx->old_hist->age) {
			swap(ctx->new_hist, ctx->old_hist);
			if (ctx->selecting == sel_old) {
				go_to_sheet(ctx, sheet);
			} else {
				go_to_sheet(ctx, old_sheet);
				render_delta(ctx);
			}
		} else {
			if (ctx->selecting != sel_old)
				go_to_sheet(ctx, sheet);
			else
				render_delta(ctx);
		}
	} else {
		go_to_sheet(ctx, sheet);
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


/* ----- Input ------------------------------------------------------------- */


static bool history_click(void *user, int x, int y)
{
	struct gui_ctx *ctx = user;

	if (aoi_click(&ctx->aois, x, y))
		return 1;
	hide_history(ctx);
	return 1;
}


static bool history_hover_update(void *user, int x, int y)
{
	struct gui_ctx *ctx = user;

	return aoi_hover(&ctx->aois, x, y);
}


static void history_drag_move(void *user, int dx, int dy)
{
	struct gui_ctx *ctx = user;

	ctx->hist_y_offset += dy;
	redraw(ctx);
}


static void history_key(void *user, int x, int y, int keyval)
{
	struct gui_ctx *ctx = user;

	switch (keyval) {
	case GDK_KEY_Escape:
		hide_history(ctx);
		break;
	case GDK_KEY_q:
		gtk_main_quit();
	}
}


static const struct input_ops history_input_ops = {
	.click		= history_click,
	.hover_begin	= history_hover_update,
	.hover_update	= history_hover_update,
	.hover_click	= history_click,
	.drag_begin	= input_accept,
	.drag_move	= history_drag_move,
	.key		= history_key,
};


/* ----- Invocation -------------------------------------------------------- */


void show_history(struct gui_ctx *ctx, enum selecting sel)
{
	struct gui_hist *h = ctx->hist;

	input_push(&history_input_ops, ctx);

	ctx->mode = showing_history;
	ctx->hist_y_offset = 0;
	ctx->selecting = sel;
	overlay_remove_all(&ctx->hist_overlays);
	for (h = ctx->hist; h; h = h->next) {
		h = skip_history(ctx, h);
		h->over = overlay_add(&ctx->hist_overlays, &ctx->aois,
		    hover_history, click_history, h);
		set_history_style(h, 0);
		hover_history(h, 0, 0, 0);
	}
	redraw(ctx);
}
