/*
 * gui-over.h - GUI: overlays
 *
 * Written 2016 by Werner Almesberger
 * Copyright 2016 by Werner Almesberger
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#ifndef GUI_OVER_H
#define	GUI_OVER_H

#include <stdbool.h>
#include <stdint.h>

#include <cairo/cairo.h>

#include "gui-aoi.h"


struct overlay_style {
	const char *font;
	unsigned wmin, wmax;
	unsigned radius;
	unsigned pad;	/* in x and y direction; adjust for radius ! */
	unsigned skip;	/* should be list-specific */
	double fg[4];
	double bg[4];
	double frame[4];
	double width;
};

struct overlay;


extern struct overlay_style overlay_style_default;
extern struct overlay_style overlay_style_dense;
extern struct overlay_style overlay_style_dense_selected;

struct overlay *overlay_draw(struct overlay *over, cairo_t *cr, int *x, int *y);
void overlay_draw_all(struct overlay *overlays, cairo_t *cr, int x, int y);
struct overlay *overlay_add(struct overlay **overlays, struct aoi **aois,
    bool (*hover)(void *user, bool on), void (*click)(void *user), void *user);
void overlay_text_raw(struct overlay *over, const char *s);
void overlay_text(struct overlay *over, const char *fmt, ...);
void overlay_style(struct overlay *over, const struct overlay_style *style);
void overlay_remove(struct overlay **overlays, struct overlay *over);
void overlay_remove_all(struct overlay **overlays);

#endif /* !GUI_OVER_H */
