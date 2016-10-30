/*
 * gui/view.c - Text view window
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

#include "misc/util.h"
#include "gui/view.h"


struct view {
	GtkWidget *window;
	GtkTextBuffer *buffer;
	bool visible;
	bool destroyed;
	void (*key_press)(void *user, struct view *view, guint keyval);
	void *user;
};


static void destroy_view(GtkWidget *object, gpointer user_data)
{
	struct view *view = user_data;

	gtk_widget_destroy(view->window);
	view->destroyed = 1;
}


static gboolean key_press_event(GtkWidget *widget, GdkEventKey *event,
    gpointer data)
{
	struct view *view = data;

	view->key_press(view->user, view, event->keyval);
	return TRUE;
}


struct view *view_open(
    void (*key_press)(void *user, struct view *view, guint keyval), void *user)
{
	struct view *view;
	GtkWidget *scroll, *tv;

	view = alloc_type(struct view);
	view->visible = 1;
	view->destroyed = 0;
	view->key_press = key_press;
	view->user = user;

	view->window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	scroll = gtk_scrolled_window_new(NULL, NULL);

	view->buffer = gtk_text_buffer_new(NULL);
	tv = gtk_text_view_new_with_buffer(view->buffer);

	gtk_text_view_set_left_margin(GTK_TEXT_VIEW(tv), 5);
	gtk_text_view_set_right_margin(GTK_TEXT_VIEW(tv), 5);
//	gtk_text_view_set_top_margin(GTK_TEXT_VIEW(tv), 5);
//	gtk_text_view_set_bottom_margin(GTK_TEXT_VIEW(tv), 5);

	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scroll),
	    GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);

	gtk_text_view_set_editable(GTK_TEXT_VIEW(tv), FALSE);
	gtk_text_view_set_cursor_visible(GTK_TEXT_VIEW(tv), FALSE);
	gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(tv), TRUE);

	gtk_container_add(GTK_CONTAINER(scroll), tv);

	gtk_container_add(GTK_CONTAINER(view->window), scroll);
	gtk_window_set_default_size(GTK_WINDOW(view->window), 480, 360);
	gtk_widget_show_all(view->window);

	gtk_widget_set_can_focus(scroll, TRUE);
	gtk_widget_add_events(scroll, GDK_KEY_PRESS_MASK);

	g_signal_connect(G_OBJECT(tv), "key_press_event",
	    G_CALLBACK(key_press_event), view);
	g_signal_connect(view->window, "destroy", G_CALLBACK(destroy_view),
	    view);

	gtk_widget_show_all(view->window);

	return view;
}


bool view_visible(struct view *view, bool visible)
{
	if (view->destroyed)
		return 0;
	if (view->visible == visible)
		return 1;
	if (visible)
		gtk_widget_show(view->window);
	else
		gtk_widget_hide(view->window);
	view->visible = visible;
	return 1;
}


bool view_visible_toggle(struct view *view)
{
	return view_visible(view, !view->visible);
}


void view_close(struct view *view)
{
	if (!view->destroyed)
		gtk_widget_destroy(view->window);
	free(view);
}


bool view_update(struct view *view, const char *s, bool markup)
{
	if (view->destroyed)
		return 0;
	if (markup) {
		GtkTextIter iter;

		gtk_text_buffer_set_text(GTK_TEXT_BUFFER(view->buffer), "", 0);
		gtk_text_buffer_get_iter_at_offset(view->buffer, &iter, 0);
		gtk_text_buffer_insert_markup(GTK_TEXT_BUFFER(view->buffer),
		    &iter, s, -1);
	} else {
		gtk_text_buffer_set_text(GTK_TEXT_BUFFER(view->buffer), s, -1);
	}
	return 1;
}
