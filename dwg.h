/*
 * dwg.h - Complex drawing functions
 *
 * Written 2016 by Werner Almesberger
 * Copyright 2016 by Werner Almesberger
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */


#ifndef DWG_H
#define	DWG_H

#include "fig.h"


enum dwg_shape {
	dwg_unspec,	// UnSpc
	dwg_in,		// Input
	dwg_out,	// Output
	dwg_tri,	// 3State
	dwg_bidir,	// Bidirectional
};

struct dwg_bbox {
	int x, y;
	int w, h;
};


void dwg_label(int x, int y, const char *s, int dir, int dim,
    enum dwg_shape shape, struct dwg_bbox *bbox);
void dwg_hlabel(int x, int y, const char *s, int dir, int dim,
    enum dwg_shape shape, struct dwg_bbox *bbox);
void dwg_glabel(int x, int y, const char *s, int dir, int dim,
    enum dwg_shape shape, struct dwg_bbox *bbox);
void dwg_text(int x, int y, const char *s, int dir, int dim,
    enum dwg_shape shape, struct dwg_bbox *bbox);

void dwg_junction(int x, int y);
void dwg_noconn(int x, int y);

void dwg_line(int sx, int sy, int ex, int ey);

void dwg_wire(int sx, int sy, int ex, int ey);
void dwg_bus(int sx, int sy, int ex, int ey);

#endif /* !DWG_H */
