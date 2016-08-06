/*
 * gui-over.c - GUI: overlays
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
 * https://www.cairographics.org/samples/rounded_rectangle/
 */

#include <stddef.h>
#include <stdlib.h>
#include <math.h>

#include <cairo/cairo.h>
#include <pango/pangocairo.h>

#include "util.h"
#include "gui-aoi.h"
#include "gui-over.h"


#define	OVER_FONT_SIZE	16
#define	OVER_BORDER	8
#define	OVER_RADIUS	6
#define	OVER_SEP	8
#define	OVER_X0		10
#define	OVER_Y0		10


struct overlay {
	const char *s;

	struct aoi **aois;
	bool (*hover)(void *user, bool on);
	void (*click)(void *user);
	void *user;

	const struct aoi *aoi;

	struct overlay *next;
};


static void rrect(cairo_t *cr, int x, int y, int w, int h, int r)
{
	const double deg = M_PI / 180.0;

	cairo_new_path(cr);
	cairo_arc(cr, x + w - r, y + r, r, -90 * deg, 0);
	cairo_arc(cr, x + w - r, y + h - r, r, 0, 90 * deg);
	cairo_arc(cr, x + r, y + h - r, r, 90 * deg, 180 * deg);
	cairo_arc(cr, x + r, y + r, r, 180 * deg, 270 * deg);
	cairo_close_path(cr);
}


struct overlay *overlay_draw(struct overlay *over, cairo_t *cr, int *x, int *y)
{
	int w, h;

	PangoLayout *layout;
	PangoFontDescription *desc;
	PangoRectangle ink_rect;

	layout = pango_cairo_create_layout(cr);
	pango_layout_set_text(layout, over->s, -1);
	desc = pango_font_description_from_string("Helvetica Bold 12");
	pango_layout_set_font_description(layout, desc);
	pango_font_description_free(desc);

	pango_layout_get_extents(layout, &ink_rect, NULL);
	w = ink_rect.width / PANGO_SCALE + 2 * OVER_BORDER;
	h = ink_rect.height / PANGO_SCALE + 2 * OVER_BORDER;

	rrect(cr, *x, *y, w, h, OVER_RADIUS);

	cairo_set_source_rgba(cr, 0.8, 0.9, 1, 0.8);
	cairo_fill_preserve(cr);
	cairo_set_source_rgba(cr, 0.5, 0.5, 1, 0.7);
	cairo_set_line_width(cr, 2);
	cairo_stroke(cr);

	cairo_set_source_rgb(cr, 0, 0, 0);
	cairo_move_to(cr, *x - ink_rect.x / PANGO_SCALE + OVER_BORDER,
	    *y - ink_rect.y / PANGO_SCALE + OVER_BORDER + 0*ink_rect.height);

	pango_cairo_update_layout(cr, layout);
	pango_cairo_show_layout(cr, layout);

	if (over->hover || over->click) {
		struct aoi aoi = {
			.x	= *x,
			.y	= *y,
			.w	= w,
			.h	= h,
			.hover	= over->hover,
			.click	= over->click,
			.user	= over->user,
		};

		if (over->aoi)
			aoi_remove(over->aois, over->aoi);
		over->aoi = aoi_add(over->aois, &aoi);
	}

	*y += h + OVER_SEP;

	return over->next;
}


void overlay_draw_all(struct overlay *overlays, cairo_t *cr)
{
	struct overlay *over;
	int x = OVER_X0;
	int y = OVER_Y0;

	for (over = overlays; over; over = over->next)
		overlay_draw(over, cr, &x, &y);
}


struct overlay *overlay_add(struct overlay **overlays, const char *s,
    struct aoi **aois,
    bool (*hover)(void *user, bool on), void (*click)(void *user), void *user)
{
	struct overlay *over;
	struct overlay **anchor;

	over = alloc_type(struct overlay);
	over->s = stralloc(s);

	over->aois = aois;
	over->hover = hover;
	over->click = click;
	over->user = user;
	over->aoi = NULL;

	for (anchor = overlays; *anchor; anchor = &(*anchor)->next);
	over->next = NULL;
	*anchor = over;

	return over;
}


static void overlay_free(struct overlay *over)
{
	if (over->aoi)
		aoi_remove(over->aois, over->aoi);
	free((void *) over->s);
	free(over);
}


void overlay_remove(struct overlay **overlays, struct overlay *over)
{
	struct overlay **anchor;

	for (anchor = overlays; *anchor; anchor = &(*anchor)->next)
		if (*anchor == over) {
			*anchor = over->next;
			overlay_free(over);
			return;
		}
	abort();
}


void overlay_remove_all(struct overlay **overlays)
{
	struct overlay *next;

	while (*overlays) {
		next = (*overlays)->next;
		overlay_free(*overlays);
		*overlays = next;
	}
}
