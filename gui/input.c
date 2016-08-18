/*
 * gui/input.c - Input operations
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
#include <math.h>
#include <assert.h>

#include <gtk/gtk.h>

#include "misc/util.h"
#include "gui/input.h"


#define	DRAG_RADIUS	5


static struct input {
	const struct input_ops *ops;
	void *user;

	enum state {
		input_normal,
		input_clicking,
		input_ignoring,	/* click rejected by moving the cursor */
		input_hovering,
		input_dragging,
	} state;

	struct input *next;
} *sp = NULL;

static int curr_x, curr_y;		/* last mouse position */
static int clicked_x, clicked_y;	/* button down position */


/* ----- Mouse button ------------------------------------------------------ */


static gboolean motion_notify_event(GtkWidget *widget, GdkEventMotion *event,
    gpointer data)
{
	const struct input *old_sp = sp;

	curr_x = event->x;
	curr_y = event->y;

	if (!sp)
		return TRUE;

	switch (sp->state) {
	case input_normal:
		if (sp->ops->hover_begin &&
		    sp->ops->hover_begin(sp->user, event->x, event->y))
			sp->state = input_hovering;
		assert(sp == old_sp);
		break;
	case input_clicking:
		if (hypot(event->x - clicked_x, event->y - clicked_y) <
		    DRAG_RADIUS)
			break;
		if (sp->ops->drag_begin &&
		    sp->ops->drag_begin(sp->user, clicked_x, clicked_y))
			sp->state = input_dragging;
		else
			sp->state = input_ignoring;
		assert(sp == old_sp);
		break;
	case input_ignoring:
		break;
	case input_hovering:
		if (!sp->ops->hover_update)
			break;

		/* Caution: hover_update may switch input layers */
		if (sp->ops->hover_update(sp->user, event->x, event->y) &&
		    sp == old_sp) {
			sp->state = input_normal;
			if (sp->ops->hover_end)
				sp->ops->hover_end(sp->user);
		}
		break;
	case input_dragging:
		if (sp->ops->drag_move)
		    sp->ops->drag_move(sp->user,
		        event->x - clicked_x, event->y - clicked_y);
		clicked_x = event->x;
		clicked_y = event->y;
		break;
	default:
		abort();
	}
	return TRUE;
}


static gboolean button_press_event(GtkWidget *widget, GdkEventButton *event,
    gpointer data)
{
	const struct input *old_sp = sp;

	if (event->button != 1)
		return TRUE;

	switch (sp->state) {
	case input_normal:
		sp->state = input_clicking;
		clicked_x = event->x;
		clicked_y = event->y;
		break;
	case input_clicking:
	case input_ignoring:
	case input_dragging:
		/* ignore double-click */
		break;
	case input_hovering:
		if (sp->ops->hover_click &&
		    sp->ops->hover_click(sp->user, event->x, event->y) &&
		    sp == old_sp) {
			sp->state = input_ignoring;
			if (sp->ops->hover_end)
				sp->ops->hover_end(sp->user);
		}
		break;
	default:
		abort();
	}

	return TRUE;
}


static gboolean button_release_event(GtkWidget *widget, GdkEventButton *event,
    gpointer data)
{
	if (event->button != 1)
		return TRUE;

	switch (sp->state) {
	case input_normal:
		abort();
	case input_clicking:
		sp->state = input_normal;
		if (sp->ops->click)
			sp->ops->click(sp->user, clicked_x, clicked_y);
		break;
	case input_ignoring:
		sp->state = input_normal;
		break;
	case input_dragging:
		sp->state = input_normal;
		if (sp->ops->drag_end)
			sp->ops->drag_end(sp->user);
		break;
	case input_hovering:
		break;
	default:
		abort();
	}

	return TRUE;
}


/* ----- Scroll wheel ------------------------------------------------------ */


static gboolean scroll_event(GtkWidget *widget, GdkEventScroll *event,
    gpointer data)
{
	if (!sp || !sp->ops->scroll)
		return TRUE;
	switch (event->direction) {
	case GDK_SCROLL_UP:
		sp->ops->scroll(sp->user, event->x, event->y, -1);
		break;
	case GDK_SCROLL_DOWN:
		sp->ops->scroll(sp->user, event->x, event->y, 1);
		break;
	default:
		/* ignore */;
	}
	return TRUE;
}


/* ----- Keys -------------------------------------------------------------- */


static gboolean key_press_event(GtkWidget *widget, GdkEventKey *event,
    gpointer data)
{
	if (sp && sp->ops->key)
		sp->ops->key(sp->user, curr_x, curr_y, event->keyval);
	return TRUE;
}


/* ----- Covenience function for hover_begin and drag_begin ---------------- */


bool input_accept(void *user, int x, int y)
{
	return 1;
}


/* ----- Adding/removing interaction layers -------------------------------- */


static void cleanup(void)
{
	if (!sp)
		return;

	switch (sp->state) {
	case input_hovering:
		if (sp->ops->hover_end)
			sp->ops->hover_end(sp->user);
		break;
	case input_dragging:
		if (sp->ops->drag_end)
			sp->ops->drag_end(sp->user);
		break;
	default:
		;
	}
}


void input_push(const struct input_ops *ops, void *user)
{
	struct input *new;

	cleanup();

	new = alloc_type(struct input);
	new->ops = ops;
	new->user = user;
	new->state = input_normal;
	new->next = sp;
	sp = new;
}


void input_pop(void)
{
	struct input *next = sp->next;

	cleanup();
	free(sp);
	sp = next;
}


/* ----- Initialization ---------------------------------------------------- */


void input_setup(GtkWidget *da)
{
	gtk_widget_set_can_focus(da, TRUE);

	gtk_widget_add_events(da,
	    GDK_KEY_PRESS_MASK |
	    GDK_BUTTON_PRESS_MASK | GDK_BUTTON_RELEASE_MASK |
	    GDK_SCROLL_MASK |
	    GDK_POINTER_MOTION_MASK);

	g_signal_connect(G_OBJECT(da), "key_press_event",
	    G_CALLBACK(key_press_event), NULL);
	g_signal_connect(G_OBJECT(da), "motion_notify_event",
	    G_CALLBACK(motion_notify_event), NULL);
	g_signal_connect(G_OBJECT(da), "button_press_event",
	    G_CALLBACK(button_press_event), NULL);
	g_signal_connect(G_OBJECT(da), "button_release_event",
	    G_CALLBACK(button_release_event), NULL);
	g_signal_connect(G_OBJECT(da), "scroll_event",
	    G_CALLBACK(scroll_event), NULL);
}
