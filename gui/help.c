/*
 * gui/help.c - Online help
 *
 * Written 2016 by Werner Almesberger
 * Copyright 2016 by Werner Almesberger
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include <stdbool.h>

#include <gtk/gtk.h>
#include <webkit2/webkit2.h>

#include "gui/help.h"


static GtkWidget *window = NULL;
static bool visible;


static void destroy_help(GtkWidget *object, gpointer user_data)
{
	gtk_widget_destroy(window);
	window = NULL;
}


static void new_help_window(void)
{
	GtkWidget *view;
	WebKitSettings *settings;

	window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	view = webkit_web_view_new();

	settings = webkit_web_view_get_settings(WEBKIT_WEB_VIEW(view));
	webkit_settings_set_default_font_size(settings, 10);

	gtk_container_add(GTK_CONTAINER(window), view);
	gtk_window_set_default_size(GTK_WINDOW(window), 480, 360);
	gtk_widget_show_all(window);

	webkit_web_view_load_html(WEBKIT_WEB_VIEW(view),
#include "../help.inc"
	, NULL);

	g_signal_connect(window, "destroy", G_CALLBACK(destroy_help), NULL);

	gtk_widget_show_all(window);
}


void help(void)
{
	if (!window) {
		new_help_window();
		visible = 1;
	} else {
		visible = !visible;
		if (visible)
			gtk_widget_show(window);
		else
			gtk_widget_hide(window);
	}
}
