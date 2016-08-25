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

#ifdef USE_WEBKIT
#include <webkit2/webkit2.h>
#endif

#include "gui/help.h"


static GtkWidget *window = NULL;
static bool visible;


static void destroy_help(GtkWidget *object, gpointer user_data)
{
	gtk_widget_destroy(window);
	window = NULL;
}


static gboolean key_press_event(GtkWidget *widget, GdkEventKey *event,
    gpointer data)
{
	switch (event->keyval) {
	case GDK_KEY_h:
	case GDK_KEY_Help:
	case GDK_KEY_q:
	case GDK_KEY_Escape:
		gtk_widget_hide(window);
		visible = 0;
		break;
	}
	return TRUE;
}


#ifdef USE_WEBKIT

static GtkWidget *help_content(void)
{
	GtkWidget *view;
	WebKitSettings *settings;

	view = webkit_web_view_new();

	settings = webkit_web_view_get_settings(WEBKIT_WEB_VIEW(view));
	webkit_settings_set_default_font_size(settings, 10);

	webkit_web_view_load_html(WEBKIT_WEB_VIEW(view),
#include "../help.inc"
	, NULL);

	return view;
}

#else /* USE_WEBKIT */

static GtkWidget *help_content(void)
{
	GtkWidget *scroll, *label;

	scroll = gtk_scrolled_window_new(NULL, NULL);
	label = gtk_label_new(NULL);

	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scroll),
            GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);

	gtk_label_set_line_wrap(GTK_LABEL(label), TRUE);
	gtk_label_set_markup(GTK_LABEL(label),
#include "../help.inc"
	    );

	gtk_container_add(GTK_CONTAINER(scroll), label);

	return scroll;
}

#endif /* !USE_WEBKIT */


static void new_help_window(void)
{
	GtkWidget *content;

	window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	content = help_content();

	gtk_container_add(GTK_CONTAINER(window), content);
	gtk_window_set_default_size(GTK_WINDOW(window), 480, 360);
	gtk_widget_show_all(window);

	gtk_widget_set_can_focus(content, TRUE);
	gtk_widget_add_events(content, GDK_KEY_PRESS_MASK);

	g_signal_connect(G_OBJECT(content), "key_press_event",
	    G_CALLBACK(key_press_event), NULL);
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
