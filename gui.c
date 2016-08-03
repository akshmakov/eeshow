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

#include <gtk/gtk.h>

#include "cro.h"
#include "gfx.h"
#include "sch.h"
#include "gui.h"


struct gui_ctx {
	const struct sch_ctx *sch;
	struct cro_ctx *gfx_ctx;
} gui_ctx;


static gboolean on_draw_event(GtkWidget *widget, cairo_t *cr,
    gpointer user_data)
{
	const struct gui_ctx *ctx = user_data;

	cro_canvas_draw(ctx->gfx_ctx, cr);

	return FALSE;
}


static void render(struct gui_ctx *ctx)
{
	char *argv[] = { "gui", NULL };

	gfx_init(&cro_canvas_ops, 1, argv);
	sch_render(ctx->sch->sheets);
	cro_canvas_end(gfx_ctx);
	ctx->gfx_ctx = gfx_ctx;
	// gfx_end();
}


int gui(struct sch_ctx *sch)
{
	GtkWidget *window;
	GtkWidget *da;
	struct gui_ctx ctx = {
		.sch = sch,
	};

	render(&ctx);

	window = gtk_window_new(GTK_WINDOW_TOPLEVEL);

	da = gtk_drawing_area_new();
	gtk_container_add(GTK_CONTAINER(window), da);

	g_signal_connect(G_OBJECT(da), "draw",
	    G_CALLBACK(on_draw_event), &ctx);
	g_signal_connect(window, "destroy",
	    G_CALLBACK(gtk_main_quit), NULL);

	gtk_window_set_position(GTK_WINDOW(window), GTK_WIN_POS_CENTER);
	gtk_window_set_default_size(GTK_WINDOW(window), 400, 90);
	gtk_window_set_title(GTK_WINDOW(window), "GTK window");

	gtk_widget_show_all(window);

	gtk_main();

	return 0;
}
