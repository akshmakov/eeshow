/*
 * record.h - Record graphics operations by layers and replay
 *
 * Written 2016 by Werner Almesberger
 * Copyright 2016 by Werner Almesberger
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */


#ifndef RECORD_H
#define	RECORD_H

#include "gfx.h"


struct record_obj;

struct record_layer {
	unsigned layer;
	struct record_obj *objs;
	struct record_obj **next_obj;
	struct record_layer *next;
};

struct record {
	const struct gfx_ops *ops;
	void *user;
	int xmin, xmax;
	int ymin, ymax;
	struct record_layer *layers;
};


void record_line(void *ctx, int sx, int sy, int ex, int ey,
    int color, unsigned layer);
void record_rect(void *ctx, int sx, int sy, int ex, int ey,
    int color, int fill_color, unsigned layer);
void record_poly(void *ctx,
    int points, const int x[points], const int y[points],
    int color, int fill_color, unsigned layer);
void record_circ(void *ctx, int x, int y, int r,
    int color, int fill_color, unsigned layer);
void record_arc(void *ctx, int x, int y, int r, int sa, int ea,
    int color, int fill_color, unsigned layer);
void record_text(void *ctx, int x, int y, const char *s, unsigned size,
    enum text_align align, int rot, unsigned color, unsigned layer);

void record_init(struct record *rec, const struct gfx_ops *ops, void *user);
void record_wipe(struct record *rec);
void record_replay(const struct record *rec);
void record_bbox(const struct record *rec, int *x, int *y, int *w, int *h);
void record_destroy(struct record *rec);

#endif /* !RECORD_H */
