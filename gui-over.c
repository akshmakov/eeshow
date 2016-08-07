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
#include <stdarg.h>
#include <stdlib.h>
#include <stdio.h>
#include <math.h>

#include <cairo/cairo.h>
#include <pango/pangocairo.h>

#include "util.h"
#include "fmt-pango.h"
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
	const struct overlay_style *style;

	struct aoi **aois;
	bool (*hover)(void *user, bool on);
	void (*click)(void *user);
	void *user;

	const struct aoi *aoi;

	struct overlay *next;
};


#define	BG_DEFAULT	{ 0.8, 0.9, 1.0, 0.8 }
#define	FRAME_DEFAULT	{ 0.5, 0.5, 1.0, 0.7 }
#define	FRAME_SELECTED	{ 0.0, 0.0, 1.0, 0.8 }
#define	FG_DEFAULT	{ 0.0, 0.0, 0.0, 1.0 }


struct overlay_style overlay_style_default = {
	.font	= "Helvetica 10",
	.radius	= 6,
	.pad	= 8,
	.skip	= 8,
	.bg	= BG_DEFAULT,
	.frame	= FRAME_DEFAULT,
	.fg	= FG_DEFAULT,
	.width	= 2,
}, overlay_style_dense = {
	.font	= "Helvetiva 10",
	.radius	= 3,
	.pad	= 4,
	.skip	= 5,
	.bg	= BG_DEFAULT,
	.frame	= FRAME_DEFAULT,
	.fg	= FG_DEFAULT,
	.width	= 1,
}, overlay_style_dense_selected = {
	.font	= "Helvetica Bold 10",
	.radius	= 3,
	.pad	= 4,
	.skip	= 5,
	.bg	= BG_DEFAULT,
	.frame	= FRAME_SELECTED,
	.fg	= FG_DEFAULT,
	.width	= 1,
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
	const struct overlay_style *style = over->style;
	const double *fg = style->fg;
	const double *bg = style->bg;
	const double *frame = style->frame;
	unsigned ink_w, ink_h, w, h;
	int tx, ty;

	PangoLayout *layout;
	PangoFontDescription *desc;
	PangoRectangle ink_rect;

	desc = pango_font_description_from_string(style->font);
	layout = pango_cairo_create_layout(cr);
	pango_layout_set_font_description(layout, desc);
	pango_layout_set_markup(layout, over->s, -1);
	pango_font_description_free(desc);

	pango_layout_get_extents(layout, &ink_rect, NULL);
#if 0
fprintf(stderr, "%d + %d  %d + %d\n",
    ink_rect.x / PANGO_SCALE, ink_rect.width / PANGO_SCALE,
    ink_rect.y / PANGO_SCALE, ink_rect.height / PANGO_SCALE);
#endif
	ink_w = ink_rect.width / PANGO_SCALE;
	ink_h = ink_rect.height / PANGO_SCALE;

	ink_w = ink_w > style->wmin ? ink_w : style->wmin;
	ink_w = !style->wmax || ink_w < style->wmax ? ink_w : style->wmax;
	w = ink_w + 2 * style->pad;
	h = ink_h + 2 * style->pad;

	tx = *x - ink_rect.x / PANGO_SCALE + style->pad;
	ty = *y - ink_rect.y / PANGO_SCALE + style->pad;

	rrect(cr, *x, *y, w, h, style->radius);

	cairo_set_source_rgba(cr, bg[0], bg[1], bg[2], bg[3]);
	cairo_fill_preserve(cr);
	cairo_set_source_rgba(cr, frame[0], frame[1], frame[2], frame[3]);
	cairo_set_line_width(cr, style->width);
	cairo_stroke(cr);

	if (style->wmax) {
		cairo_new_path(cr);
#if 0
fprintf(stderr, "%u(%d) %u %.60s\n", ty, ink_rect.y / PANGO_SCALE, ink_h, over->s);
#endif
/*
 * @@@ for some mysterious reason, we get
 * ink_h = ink_rect.height / PANGO_SCALE = 5
 * instead of 2 if using overlay_style_dense_selected. Strangely, changing
 * overlay_style_dense_selected such that it becomes more like
 * overlay_style_dense has no effect.
 *
 * This causes the text to be cut vertically, roughly in the middle. We hack
 * around this problem by growind the clipping area vertically. This works,
 * since we're currently only concerned about horizontal clipping anyway.
 */

		cairo_rectangle(cr, tx, ty, ink_w, ink_h + 20);
		cairo_clip(cr);
	}

	cairo_set_source_rgba(cr, fg[0], fg[1], fg[2], fg[3]);
	cairo_move_to(cr, tx, ty);

	pango_cairo_update_layout(cr, layout);
	pango_cairo_show_layout(cr, layout);
	cairo_reset_clip(cr);
	g_object_unref(layout);

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

	*y += h + style->skip;

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


struct overlay *overlay_add(struct overlay **overlays, struct aoi **aois,
    bool (*hover)(void *user, bool on), void (*click)(void *user), void *user)
{
	struct overlay *over;
	struct overlay **anchor;

	over = alloc_type(struct overlay);
	over->s = NULL;
	over->style = &overlay_style_default;

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


void overlay_style(struct overlay *over, const struct overlay_style *style)
{
	over->style = style;
}


void overlay_text(struct overlay *over, const char *fmt, ...)
{
	va_list ap;

	free((char *) over->s);
	va_start(ap, fmt);
	over->s = vfmt_pango(fmt, ap);
	va_end(ap);
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
