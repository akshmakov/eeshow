/*
 * gui/progress.c - Progress bar
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

#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>

#include <cairo/cairo.h>
#include <gtk/gtk.h>

#include "gui/common.h"


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


void setup_progress_bar(struct gui_ctx *ctx, GtkWidget *window)
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


void progress_update(struct gui_ctx *ctx)
{
	unsigned mask = (1 << ctx->progress_scale) - 1;

	ctx->progress++;
	if ((ctx->progress & mask) != mask)
		return;

	redraw(ctx);
	gtk_main_iteration_do(0);
}
