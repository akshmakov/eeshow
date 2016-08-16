/*
 * text.h - FIG text object
 *
 * Written 2016 by Werner Almesberger
 * Copyright 2016 by Werner Almesberger
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */


#ifndef TEXT_H
#define TEXT_H


/* use constants of FIG text sub_type */

enum text_align {
	text_min = 0,	// left or bottom
	text_mid = 1,	// center
	text_max = 2,	// right or top
};

enum text_style {
	text_normal,
	text_italic,
};

struct text {
	const char *s;
	int size;
	int x, y;
	int rot;
	enum text_align hor;
	enum text_align vert;
};

void text_init(struct text *txt);
void text_free(struct text *txt);

void text_set(struct text *txt, const char *s);
void text_rot(struct text *txt, int deg);
void text_flip_x(struct text *txt);
enum text_align text_flip(enum text_align align);

void text_fig(const struct text *txt, int color, unsigned layer);

void text_rel(const struct text *txt, enum text_align xr, enum text_align yr,
    int dx, int dy, int *res_x, int *res_y);
void text_shift(struct text *txt, enum text_align xr, enum text_align yr,
    int dx, int dy);
int text_rel_x(const struct text *txt, enum text_align xr, enum text_align yr,
    int dx, int dy);
int text_rel_y(const struct text *txt, enum text_align xr, enum text_align yr,
    int dx, int dy);

#endif /* !TEXT_H */
