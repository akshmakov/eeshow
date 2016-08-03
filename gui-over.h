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

#include <cairo/cairo.h>


struct overlay;


void overlay_draw_all(const struct overlay *overlays, cairo_t *cr);
struct overlay *overlay_add(struct overlay **overlays, const char *s);
void overlay_remove(struct overlay **overlays, struct overlay *over);
void overlay_remove_all(struct overlay **overlays);

#endif /* !GUI_OVER_H */
