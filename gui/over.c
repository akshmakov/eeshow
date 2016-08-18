/*
 * gui/over.c - GUI: overlays
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
 *
 * Section "Description" in
 * https://developer.gnome.org/pango/stable/pango-Cairo-Rendering.html
 */

#include <stddef.h>
#include <stdarg.h>
#include <stdlib.h>
#include <stdio.h>
#include <math.h>

#include <cairo/cairo.h>
#include <pango/pangocairo.h>

#include "misc/util.h"
#include "gui/fmt-pango.h"
#include "gui/aoi.h"
#include "gui/style.h"
#include "gui/over.h"


struct overlay {
	const char *s;
	struct overlay_style style;

	struct aoi **aois;
	bool (*hover)(void *user, bool on);
	void (*click)(void *user);
	void *user;

	struct aoi *aoi;

	struct overlay *next, *prev;
};


/* ----- Drawing core ------------------------------------------------------ */


static void rrect(cairo_t *cr, double x, double y, double w, double h, int r)
{
	const double deg = M_PI / 180.0;

	cairo_new_path(cr);
	cairo_arc(cr, x + w - r, y + r, r, -90 * deg, 0);
	cairo_arc(cr, x + w - r, y + h - r, r, 0, 90 * deg);
	cairo_arc(cr, x + r, y + h - r, r, 90 * deg, 180 * deg);
	cairo_arc(cr, x + r, y + r, r, 180 * deg, 270 * deg);
	cairo_close_path(cr);
}


static unsigned overlay_draw(struct overlay *over, cairo_t *cr,
    unsigned x, unsigned y, int dx, int dy)
{
	const struct overlay_style *style = &over->style;
	const struct color *fg = &style->fg;
	const struct color *bg = &style->bg;
	const struct color *frame = &style->frame;
	unsigned ink_w, ink_h;	/* effectively used text area size */
	unsigned w, h;		/* box size */
	int tx, ty;		/* text start position */
	double center;

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

	ink_h = ink_h > style->hmin ? ink_h : style->hmin;
	ink_h = !style->hmax || ink_h < style->hmax ? ink_h : style->hmax;
	h = ink_h + 2 * style->pad;

	if (dx < 0)
		x -= w;
	if (dy < 0)
		y -= h;

	tx = x - ink_rect.x / PANGO_SCALE + style->pad;
	ty = y - ink_rect.y / PANGO_SCALE + style->pad;

	center = style->width / 2.0;
	rrect(cr, x - center, y - center, w + style->width, h + style->width,
	    style->radius);

	cairo_set_source_rgba(cr, bg->r, bg->g, bg->b, bg->alpha);
	cairo_fill_preserve(cr);
	cairo_set_source_rgba(cr, frame->r, frame->g, frame->b, frame->alpha);
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

	cairo_set_source_rgba(cr, fg->r, fg->g, fg->b, fg->alpha);
	cairo_move_to(cr, tx, ty);

	pango_cairo_update_layout(cr, layout);
	pango_cairo_show_layout(cr, layout);
	cairo_reset_clip(cr);
	g_object_unref(layout);

	if (over->hover || over->click) {
		struct aoi aoi_cfg = {
			.x	= x,
			.y	= y,
			.w	= w,
			.h	= h,
			.hover	= over->hover,
			.click	= over->click,
			.user	= over->user,
		};

		if (over->aoi)
			aoi_update(over->aoi, &aoi_cfg);
		else
			over->aoi = aoi_add(over->aois, &aoi_cfg);
	}

	return h;
}


/* ----- Drawing interfaces ------------------------------------------------ */


void overlay_draw_all_d(struct overlay *overlays, cairo_t *cr,
    unsigned x, unsigned y, int dx, int dy)
{
	struct overlay *over = overlays;
	unsigned h;

	if (dy < 0)
		while (over && over->next)
			over = over->next;
	while (over) {
		h = overlay_draw(over, cr, x, y, dx, dy);
		y += dy * (h + over->style.skip);
		if (dy >= 0)
			over = over->next;
		else
			over = over->prev;
		
	}
}


void overlay_draw_all(struct overlay *overlays, cairo_t *cr, int x, int y)
{
	int dx = 1;
	int dy = 1;

	if (x < 0 || y < 0) {
		double x1, y1, x2, y2;
		int sw, sh;

		cairo_clip_extents(cr, &x1, &y1, &x2, &y2);
		sw = x2 - x1;
		sh = y2 - y1;
		if (x < 0) {
			x = sw + x;
			dx = -1;
		}
		if (y < 0) {
			y = sh + y;
			dy = -1;
		}
	}

	overlay_draw_all_d(overlays, cr, x, y, dx, dy);
}


/* ----- Sizing ------------------------------------------------------------ */


void overlay_size(const struct overlay *over, PangoContext *pango_context,
    int *w, int *h)
{
	const struct overlay_style *style = &over->style;
	PangoLayout *layout;
	PangoFontDescription *desc;
	PangoRectangle ink_rect;
	unsigned ink_w, ink_h;	/* effectively used text area size */

	/*
	 * Note that we need the caller to provide the Cairo context, because
	 * the font size changes subtly even between image (which we could
	 * create locally) and screen (which is better left to the outside
	 * world).
	 */

	desc = pango_font_description_from_string(style->font);
	layout = pango_layout_new(pango_context);
	pango_layout_set_font_description(layout, desc);
	pango_layout_set_markup(layout, over->s, -1);
	pango_font_description_free(desc);

	pango_layout_get_extents(layout, &ink_rect, NULL);
	g_object_unref(layout);

	ink_w = ink_rect.width / PANGO_SCALE;
	ink_h = ink_rect.height / PANGO_SCALE;

	ink_w = ink_w > style->wmin ? ink_w : style->wmin;
	ink_w = !style->wmax || ink_w < style->wmax ? ink_w : style->wmax;

	ink_h = ink_h > style->hmin ? ink_h : style->hmin;
	ink_h = !style->hmax || ink_h < style->hmax ? ink_h : style->hmax;

	if (w)
		*w = ink_w + 2 * over->style.pad;
	if (h)
		*h = ink_h + 2 * over->style.pad;
}


void overlay_size_all(const struct overlay *overlays,
    PangoContext *pango_context, bool dx, bool dy, int *w, int *h)
{
	const struct overlay *over;
	int w1, h1;

	if (w)
		*w = 0;
	if (h)
		*h = 0;

	for (over = overlays; over; over = over->next) {
		int skip = over == overlays ? 0 : over->style.skip;

		overlay_size(over, pango_context, &w1, &h1);
		if (w) {
			if (dx)
				*w += w1 + skip;
			else
				*w = *w > w1 ? *w : w1;
		}
		if (h) {
			if (dy)
				*h += h1 + skip;
			else
				*h = *h > h1 ? *h : h1;
		}
	}
}


/* ----- Creation ---------------------------------------------------------- */


struct overlay *overlay_add(struct overlay **overlays, struct aoi **aois,
    bool (*hover)(void *user, bool on), void (*click)(void *user), void *user)
{
	struct overlay *over, *prev;
	struct overlay **anchor;

	over = alloc_type(struct overlay);
	over->s = NULL;
	over->style = overlay_style_default;

	over->aois = aois;
	over->hover = hover;
	over->click = click;
	over->user = user;
	over->aoi = NULL;

	prev = NULL;
	for (anchor = overlays; *anchor; anchor = &(*anchor)->next)
		prev = *anchor;
	over->next = NULL;
	over->prev = prev;
	*anchor = over;

	return over;
}


/* ----- Configuration ----------------------------------------------------- */


void overlay_style(struct overlay *over, const struct overlay_style *style)
{
	over->style = *style;
}


void overlay_text_raw(struct overlay *over, const char *s)
{
	free((char *) over->s);
	over->s = stralloc(s);
}


void overlay_text(struct overlay *over, const char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	overlay_text_raw(over, vfmt_pango(fmt, ap));
	va_end(ap);
}


/* ----- Removal ----------------------------------------------------------- */


static void overlay_free(struct overlay *over)
{
	if (over->aoi)
		aoi_remove(over->aois, over->aoi);
	free((void *) over->s);
	free(over);
}


void overlay_remove(struct overlay **overlays, struct overlay *over)
{
	if (over->next)
		over->next->prev = over->prev;
	if (over->prev)
		over->prev->next = over->next;
	else
		*overlays = over->next;
	overlay_free(over);
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
