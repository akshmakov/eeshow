/*
 * kicad/lib-render.c - Render component from library
 *
 * Written 2016 by Werner Almesberger
 * Copyright 2016 by Werner Almesberger
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */


#include <stdlib.h>
#include <stdio.h>
#include <assert.h>

#include "misc/util.h"
#include "misc/diag.h"
#include "gfx/misc.h"
#include "gfx/style.h"
#include "gfx/gfx.h"
#include "gfx/text.h"
#include "kicad/sch.h"
#include "kicad/lib.h"


/* ----- Drawing ----------------------------------------------------------- */


static void draw_poly(const struct lib_poly *poly, const int m[6])
{
	int n = poly->points;
	int x[n];
	int y[n];
	int i;

	for (i = 0; i != n; i++) {
		x[i] = mx(poly->x[i], poly->y[i], m);
		y[i] = my(poly->x[i], poly->y[i], m);
	}

	gfx_poly(n, x, y, COLOR_COMP_DWG, COLOR_NONE, LAYER_COMP_DWG);

	switch (poly->fill) {
	case 'N':
		break;
	case 'F':
		gfx_poly(n, x, y, COLOR_NONE, COLOR_COMP_DWG,
		    LAYER_COMP_DWG_BG);
		break;
	case 'f':
		gfx_poly(n, x, y, COLOR_NONE, COLOR_COMP_DWG_BG,
		    LAYER_COMP_DWG_BG);
		break;
	default:
		abort();
	}
}


static void draw_rect(const struct lib_rect *rect, const int m[6])
{
	int sx = mx(rect->sx, rect->sy, m);
	int sy = my(rect->sx, rect->sy, m);
	int ex = mx(rect->ex, rect->ey, m);
	int ey = my(rect->ex, rect->ey, m);

	gfx_rect(sx, sy, ex, ey, COLOR_COMP_DWG, COLOR_NONE, LAYER_COMP_DWG);

	switch (rect->fill) {
	case 'N':
		break;
	case 'F':
		gfx_rect(sx, sy, ex, ey, COLOR_NONE, COLOR_COMP_DWG,
		    LAYER_COMP_DWG_BG);
		break;
	case 'f':
		gfx_rect(sx, sy, ex, ey, COLOR_NONE, COLOR_COMP_DWG_BG,
		    LAYER_COMP_DWG_BG);
		break;
	default:
		abort();
	}
}


static void draw_circ(const struct lib_circ *circ, const int m[6])
{
	int x = mx(circ->x, circ->y, m);
	int y = my(circ->x, circ->y, m);
	int r = circ->r;

	gfx_circ(x, y, r, COLOR_COMP_DWG, COLOR_NONE, LAYER_COMP_DWG);

	switch (circ->fill) {
	case 'N':
		break;
	case 'F':
		gfx_circ(x, y, r, COLOR_NONE, COLOR_COMP_DWG,
		    LAYER_COMP_DWG_BG);
		break;
	case 'f':
		gfx_circ(x, y, r, COLOR_NONE, COLOR_COMP_DWG_BG,
		    LAYER_COMP_DWG_BG);
		break;
	default:
		abort();
	}
}


static void draw_arc(const struct lib_arc *arc, const int m[6])
{
	int a = matrix_to_angle(m);
	int x = mx(arc->x, arc->y, m);
	int y = my(arc->x, arc->y, m);
	int sa = angle_add(arc->start_a, a);
	int ea = angle_add(arc->end_a, a);

	if (matrix_is_mirrored(m)) {
		sa = 180 - sa;
		ea = 180 - ea;
		while (ea < sa)
			ea += 360;
		while (ea - sa > 360)
			ea -= 360;
		if (ea - sa >= 180) {
			swap(sa, ea);
			sa += 360;
		}
	}

	/*
	 * cr_arc (and maybe others) close the arc if filling, so we supply a
	 * foreground color as well. Other objects are closed and don't need
	 * need a foreground color when filling.
	 */
	switch (arc->fill) {
	case 'N':
		break;
	case 'F':
		gfx_arc(x, y, arc->r, sa, ea,
		    COLOR_COMP_DWG, COLOR_COMP_DWG, LAYER_COMP_DWG_BG);
		break;
	case 'f':
		gfx_arc(x, y, arc->r, sa, ea,
		    COLOR_COMP_DWG_BG, COLOR_COMP_DWG_BG, LAYER_COMP_DWG_BG);
		break;
	default:
		assert(0);
	}

	gfx_arc(x, y, arc->r, sa, ea,
	    COLOR_COMP_DWG, COLOR_NONE, LAYER_COMP_DWG);
}


static void draw_pin_name(const struct comp *comp, const struct lib_pin *pin,
    const int m[6], int dx, int dy, int rot, enum text_align hor)
{
	int ox, oy, sx, sy;

	if (!strcmp(pin->name, "~"))
		return;

	if (comp->name_offset) {
		ox = dx * (pin->length + comp->name_offset);
		oy = dy * (pin->length + comp->name_offset);
		sx = sy = 0;
	} else {
		ox = dx * pin->length / 2;
		oy = dy * pin->length / 2;
		sx = mxr(-dy * PIN_NUM_OFFSET, dx * PIN_NUM_OFFSET, m);
		sy = myr(-dy * PIN_NUM_OFFSET, dx * PIN_NUM_OFFSET, m);
		if (sx > 0)
			sx = -sx;
		if (sy > 0)
			sy = -sy;
	}

	struct text txt = {
		.s = pin->name,
		.x = mx(pin->x + ox, pin->y + oy, m) + sx,
		.y = my(pin->x + ox, pin->y + oy, m) + sy,
		.size = pin->name_size,
		.rot = rot,
		.hor = comp->name_offset ? hor : text_mid,
		.vert = comp->name_offset ? text_mid : text_min,
	};

	text_rot(&txt, matrix_to_angle(m));
	if (matrix_is_mirrored(m)) {
		if ((txt.rot % 180) == 0)
			txt.hor = text_flip(txt.hor);
		else
			txt.vert = text_flip(txt.vert);
	}

	switch (txt.rot) {
	case 180:
	case 270:
		text_flip_x(&txt);
		break;
	default:
		break;
	}

	text_fig(&txt, COLOR_PIN_NAME, LAYER_PIN_NAME);
}


static void draw_pin_num(const struct comp *comp, const struct lib_pin *pin,
    const int m[6], int dx, int dy, int rot, enum text_align hor)
{
	int ox, oy, sx, sy;

	ox = dx * pin->length / 2;
	oy = dy * pin->length / 2;

	sx = mxr(-dy * PIN_NUM_OFFSET, dx * PIN_NUM_OFFSET, m);
	sy = myr(-dy * PIN_NUM_OFFSET, dx * PIN_NUM_OFFSET, m);
	if (sx > 0)
		sx = -sx;
	if (sy > 0)
		sy = -sy;

	if (!comp->name_offset) {
		sx = -sx;
		sy = -sy;
	}

	struct text txt = {
		.s = pin->number,
		.x = mx(pin->x + ox, pin->y + oy, m) + sx,
		.y = my(pin->x + ox, pin->y + oy, m) + sy,
		.size = pin->number_size,
		.rot = rot,
		.hor = text_mid,
		.vert = comp->name_offset ? text_min : text_max,
	};

	text_rot(&txt, matrix_to_angle(m) % 180);
	if (matrix_is_mirrored(m)) {
		switch (txt.rot) {
		case 0:
			txt.hor = text_flip(txt.hor);
			break;
		case 90:
			break;
		case 180:
			txt.hor = text_flip(txt.hor);
			break;
		case 270:
			break;
		}
	}

	switch (txt.rot) {
	case 180:
	case 270:
		text_flip_x(&txt);
		break;
	default:
		break;
	}

	text_fig(&txt, COLOR_PIN_NUMBER, LAYER_PIN_NUMBER);
}


static void transform_poly(unsigned n, int *vx, int *vy, const int m[6])
{
	unsigned i;
	int x, y;

	for (i = 0; i != n; i++) {
		x = mx(*vx, *vy, m);
		y = my(*vx, *vy, m);
		*vx++ = x;
		*vy++ = y;
	}
}


static void draw_pin_line(const struct lib_pin *pin, enum pin_shape shape,
    int dx, int dy, const int m[6])
{
	int len = pin->length;
	int x[4], y[4];
	int ex, ey;
	if ((shape & pin_inverted) || (shape & pin_falling_edge))
		len = pin->length - 2 * PIN_R;

	
	x[0] = pin->x;
	y[0] = pin->y;
	x[1] = pin->x + dx * len;
	y[1] = pin->y + dy * len;
	transform_poly(2, x, y, m);

	gfx_poly(2, x, y, COLOR_COMP_DWG, COLOR_NONE, LAYER_COMP_DWG);

	if (shape & pin_inverted) {
		x[0] = pin->x + dx * (len + PIN_R);
		y[0] = pin->y + dy * (len + PIN_R);
		transform_poly(1, x, y, m);
		gfx_circ(x[0], y[0], PIN_R,
		    COLOR_COMP_DWG, COLOR_NONE, LAYER_COMP_DWG);
	}

	ex = pin->x + dx * pin->length;
	ey = pin->y + dy * pin->length;

	if (shape & pin_clock) {
		x[0] = ex - dy * PIN_R;
		y[0] = ey - dx * PIN_R;
		x[1] = ex + dx * 2 * PIN_R;
		y[1] = ey + dy * 2 * PIN_R;
		x[2] = ex + dy * PIN_R;
		y[2] = ey + dx * PIN_R;
		x[3] = x[0];
		y[3] = y[0];
		transform_poly(4, x, y, m);
		gfx_poly(4, x, y, COLOR_COMP_DWG, COLOR_NONE, LAYER_COMP_DWG);
	}

	if (shape & pin_input_low) {
		x[0] = ex;
		y[0] = ey;
		x[1] = ex - (dx - dy) * 2 * PIN_R;
		y[1] = ey - (dy - dx) * 2 * PIN_R;
		x[2] = ex - dx * 2 * PIN_R;
		y[2] = ey - dy * 2 * PIN_R;
		transform_poly(3, x, y, m);
		gfx_poly(3, x, y, COLOR_COMP_DWG, COLOR_NONE, LAYER_COMP_DWG);
	}

	if (shape & pin_output_low) {
		x[0] = ex + dy * 2 * PIN_R;
		y[0] = ey + dx * 2 * PIN_R;
		x[1] = ex - dx * 2 * PIN_R;
		y[1] = ey - dy * 2 * PIN_R;
		transform_poly(2, x, y, m);
		gfx_poly(2, x, y, COLOR_COMP_DWG, COLOR_NONE, LAYER_COMP_DWG);
	}

	if (shape & pin_falling_edge) {
		x[0] = ex - dy * PIN_R;
		y[0] = ey - dx * PIN_R;
		x[1] = ex - dx * 2 * PIN_R;
		y[1] = ey - dy * 2 * PIN_R;
		x[2] = ex + dy * PIN_R;
		y[2] = ey + dx * PIN_R;
		transform_poly(3, x, y, m);
		gfx_poly(3, x, y, COLOR_COMP_DWG, COLOR_NONE, LAYER_COMP_DWG);
	}

	if (shape & pin_non_logic) {
		x[0] = ex - PIN_R;
		y[0] = ey - PIN_R;
		x[1] = ex + PIN_R;
		y[1] = ey + PIN_R;
		transform_poly(2, x, y, m);
		gfx_poly(2, x, y, COLOR_COMP_DWG, COLOR_NONE, LAYER_COMP_DWG);
		swap(x[0], x[1]);
		gfx_poly(2, x, y, COLOR_COMP_DWG, COLOR_NONE, LAYER_COMP_DWG);
	}
}


static void draw_pin(const struct comp *comp, const struct lib_pin *pin,
    const int m[6])
{
	int dx = 0, dy = 0;
	int rot;
	enum text_align hor;
	enum pin_shape shape = pin->shape & ~pin_invisible;

	if (pin->shape & pin_invisible)
		return;

	switch (pin->orient) {
	case 'U':
		dy = 1;
		rot = 90;
		hor = text_min;
		break;
	case 'D':
		dy = -1;
		rot = 90;
		hor = text_max;
		break;
	case 'R':
		dx = 1;
		rot = 0;
		hor = text_min;
		break;
	case 'L':
		dx = -1;
		rot = 0;
		hor = text_max;
		break;
	default:
		abort();
	}

	draw_pin_line(pin, shape, dx, dy, m);

	if (comp->show_pin_name)
		draw_pin_name(comp, pin, m, dx, dy, rot, hor);

	if (comp->show_pin_num)
		draw_pin_num(comp, pin, m, dx, dy, rot, hor);
}


static void draw_text(const struct lib_text *text, const int m[6])
{
	struct text txt = {
		.s = text->s,
		.size = text->dim,
		.x = mx(text->x, text->y, m),
		.y = my(text->x, text->y, m),
		.rot = angle_add(text->orient / 10, matrix_to_angle(m)),
	};

	decode_alignment(&txt, text->hor_align, text->vert_align);

	switch (txt.rot) {
	case 180:
	case 270:
		/* @@@ consolidate this with text_flip_x */
		txt.rot = angle_add(txt.rot, 180);
		txt.hor = text_flip(txt.hor);
		txt.vert = text_flip(txt.vert);
//		text_flip_x(&txt);
		break;
	default:
		break;
	}

	if (matrix_is_mirrored(m))
		switch (txt.rot) {
		case 0:
		case 180:
			txt.hor = text_flip(txt.hor);
			break;
		case 90:
		case 270:
			txt.vert = text_flip(txt.vert);
			break;
		default:
			abort();
		}

	text_fig(&txt, COLOR_COMP_DWG, WIDTH_COMP_DWG);
}


static void draw(const struct comp *comp, const struct lib_obj *obj,
    const int m[6])
{
	switch (obj->type) {
	case lib_obj_poly:
		draw_poly(&obj->u.poly, m);
		break;
	case lib_obj_rect:
		draw_rect(&obj->u.rect, m);
		break;
	case lib_obj_circ:
		draw_circ(&obj->u.circ, m);
		break;
	case lib_obj_arc:
		draw_arc(&obj->u.arc, m);
		break;
	case lib_obj_text:
		draw_text(&obj->u.text, m);
		break;
	case lib_obj_pin:
		draw_pin(comp, &obj->u.pin, m);
		break;
	default:
		abort();
	}
}


const struct comp *lib_find(const struct lib *lib, const char *name)
{
	const struct comp *comp;
	const struct comp_alias *alias;

	for (comp = lib->comps; comp; comp = comp->next) {
		if (!strcmp(comp->name, name))
			return comp;
		for (alias = comp->aliases; alias; alias = alias->next)
			if (!strcmp(alias->name, name))
				return comp;
	}
	error("\"%s\" not found", name);
	return NULL;
}


bool lib_field_visible(const struct comp *comp, int n)
{
	return (comp->visible >> n) & 1;
}


static void missing_component(const int m[4])
{
	int sx = mx(0, 0, m);
	int sy = my(0, 0, m);
	int ex = mx(MISSING_WIDTH, MISSING_HEIGHT, m);
	int ey = my(MISSING_WIDTH, MISSING_HEIGHT, m);

	gfx_rect(sx, sy, ex, ey, COLOR_MISSING_FG, COLOR_MISSING_BG,
	    LAYER_COMP_DWG);
}


void lib_render(const struct comp *comp, unsigned unit, unsigned convert,
    const int m[4])
{
	const struct lib_obj *obj;

	if (!comp) {
		missing_component(m);
		return;
	}
	if (!unit)
		unit = 1;
	for (obj = comp->objs; obj; obj = obj->next) {
		if (obj->unit && obj->unit != unit)
			continue;
		if (obj->convert && obj->convert != convert)
			continue;
		draw(comp, obj, m);
	}
}
