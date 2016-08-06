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

#include <cairo/cairo.h>

#include "gui-aoi.h"


struct overlay;


struct overlay *overlay_draw(struct overlay *over, cairo_t *cr, int *x, int *y);
void overlay_draw_all(struct overlay *overlays, cairo_t *cr);
struct overlay *overlay_add(struct overlay **overlays, struct aoi **aois,
    bool (*hover)(void *user, bool on), void (*click)(void *user), void *user);
void overlay_text(struct overlay *over, const char *fmt, ...);
void overlay_remove(struct overlay **overlays, struct overlay *over);
void overlay_remove_all(struct overlay **overlays);

#endif /* !GUI_OVER_H */
