/*
 * gfx.h - Generate graphical output for Eeschema items
 *
 * Written 2016 by Werner Almesberger
 * Copyright 2016 by Werner Almesberger
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */


#ifndef GFX_H
#define	GFX_H

#include <stdbool.h>

#include "text.h"


struct gfx_ops {
	const char *name;
	void (*line)(void *ctx, int sx, int sy, int ex, int ey,
	    int color, unsigned layer);
	void (*rect)(void *ctx, int sx, int sy, int ex, int ey,
	    int color, int fill_color, unsigned layer);
	void (*poly)(void *ctx,
	    int points, const int x[points], const int y[points],
	    int color, int fill_color, unsigned layer);
	void (*circ)(void *ctx, int x, int y, int r,
	    int color, int fill_color, unsigned layer);
	void (*arc)(void *ctx, int x, int y, int r, int sa, int ea,
	    int color, int fill_color, unsigned layer);
	void (*text)(void *ctx, int x, int y, const char *s, unsigned size,
	    enum text_align align, int rot, unsigned color, unsigned layer);
	void (*tag)(void *ctx,  const char *s,
	    int points, const int x[points], const int y[points]);
	unsigned (*text_width)(void *ctx, const char *s, unsigned size);
	void *(*init)(int argc, char *const *argv);
	void (*new_sheet)(void *ctx);
	void (*end)(void *ctx);
};


extern void *gfx_ctx;


/* wrappers */

void gfx_line(int sx, int sy, int ex, int ey, int color, unsigned layer);
void gfx_rect(int sx, int sy, int ex, int ey,
    int color, int fill_color, unsigned layer);
void gfx_poly(int points, const int x[points], const int y[points],
    int color, int fill_color, unsigned layer);
void gfx_circ(int x, int y, int r, int color, int fill_color, unsigned layer);
void gfx_arc(int x, int y, int r, int sa, int ea,
    int color, int fill_color, unsigned layer);
void gfx_text(int x, int y, const char *s, unsigned size,
    enum text_align align, int rot, unsigned color, unsigned layer);
void gfx_tag(const char *s,
    unsigned points, const int x[points], int const y[points]);
unsigned gfx_text_width(const char *s, unsigned size);

/* inititalization and termination */

void gfx_init(const struct gfx_ops *ops, int argc, char *const *argv);
void gfx_new_sheet(void);
bool gfx_multi_sheet(void);
void gfx_end(void);

#endif /* !GFX_H */
