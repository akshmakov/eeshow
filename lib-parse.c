/*
 * lib.c - Parse Eeschema .lib file
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

#include "util.h"
#include "text.h"
#include "file.h"
#include "lib.h"


/* ----- Text -------------------------------------------------------------- */


static enum text_style decode_style(const char *s)
{
	if (!strcmp(s, "Normal"))
		return text_normal;
	assert(0);
}


/* ----- Polygons ---------------------------------------------------------- */


static bool parse_poly(struct lib_poly *poly, const char *line, int points)
{
	int i, n;

	poly->points = points;
	poly->x = alloc_size(sizeof(int) * points);
	poly->y = alloc_size(sizeof(int) * points);
	for (i = 0; i != points; i++) {
		if (sscanf(line, "%d %d %n",
		    poly->x + i, poly->y + i, &n) != 2)
			return 0;
		line += n;
	}
	if (sscanf(line, "%c", &poly->fill) != 1)
		return 0;
	return 1;
}


/* ----- Definitions ------------------------------------------------------- */


static bool parse_def(struct lib *lib, const char *line)
{
	char *s;
	char draw_num, draw_name;
	unsigned name_offset;
	unsigned units;

	if (sscanf(line, "DEF %ms %*s %*d %u %c %c %u",
	    &s, &name_offset, &draw_num, &draw_name, &units) != 5)
		return 0;

	lib->curr_comp = alloc_type(struct comp);
	if (*s == '~') {
		char *tmp = alloc_size(strlen(s) + 1);

		/* we can't just s++, since that would break freeing */
		strcpy(tmp, s + 1);
		free(s);
		s = tmp;
	}
	lib->curr_comp->name = s;
	lib->curr_comp->units = units;

	lib->curr_comp->visible = 0;
	lib->curr_comp->show_pin_name = draw_name == 'Y';
	lib->curr_comp->show_pin_num = draw_num == 'Y';
	lib->curr_comp->name_offset = name_offset;

	lib->curr_comp->objs = NULL;
	lib->next_obj = &lib->curr_comp->objs;

	lib->curr_comp->next = NULL;
	*lib->next_comp = lib->curr_comp;
	lib->next_comp = &lib->curr_comp->next;

	return 1;
}


/* ----- Arcs -------------------------------------------------------------- */


static bool parse_arc(struct lib_obj *obj, const char *line)
{
	struct lib_arc *arc = &obj->u.arc;
	int a1, a2;

	if (sscanf(line, "A %d %d %d %d %d %u %u %u %c",
	    &arc->x, &arc->y, &arc->r, &a1, &a2, &obj->unit, &obj->convert,
	    &arc->thick, &arc->fill) != 9)
		return 0;

	/*
	 * KiCad arcs can be clockwise or counter-clockwise. They must always
	 * be smaller than 180 degrees.
	 */

	while (a1 < 0)
		a1 += 3600;
	while (a2 < 0)
		a2 += 3600;
	a1 %= 3600;
	a2 %= 3600;
	if (a2 < a1)
		a2 += 3600;
	assert(a2 - a1 != 1800);
	if (a2 - a1 > 1800)
		swap(a1, a2);

	arc->start_a = (a1 % 3600) / 10;
	arc->end_a = (a2 % 3600) / 10;

	return 1;
}


/* ----- Library parser ---------------------------------------------------- */


static bool lib_parse_line(const struct file *file,
    void *user, const char *line)
{
	struct lib *lib = user;
	int n = 0;
	unsigned points;
	struct lib_obj *obj;
	char *style;
	unsigned zero1, zero2;
	char vis;

	switch (lib->state) {
	case lib_skip:
		if (parse_def(lib, line)) {
			lib->state = lib_def;
			return 1;
		}
		return 1;
	case lib_def:
		if (sscanf(line, "DRAW%n", &n) == 0 && n) {
			lib->state = lib_draw;
			return 1;
		}
		if (sscanf(line, "F%d \"\" %*d %*d %*d %*c %c", &n, &vis) == 2
		    || sscanf(line, "F%d \"%*[^\"]\" %*d %*d %*d %*c %c",
		    &n, &vis) == 2) {
			if (vis == 'V')
				lib->curr_comp->visible |= 1 << n;
			return 1;
		}
		/* @@@ explicitly ignore FPLIST */
		return 1;
	case lib_draw:
		if (sscanf(line, "ENDDRAW%n", &n) == 0 && n) {
			lib->state = lib_skip;
			return 1;
		}

		obj = alloc_type(struct lib_obj);
		obj->next = NULL;
		*lib->next_obj = obj;
		lib->next_obj = &obj->next;

		if (sscanf(line, "P %u %u %u %u %n",
		    &points, &obj->unit, &obj->convert, &obj->u.poly.thick,
		    &n) == 4) {
			obj->type = lib_obj_poly;
			if (parse_poly(&obj->u.poly, line + n, points))
				return 1;
			break;
		}
		if (sscanf(line, "S %d %d %d %d %u %u %d %c",
		    &obj->u.rect.sx, &obj->u.rect.sy, &obj->u.rect.ex,
		    &obj->u.rect.ey, &obj->unit, &obj->convert,
		    &obj->u.rect.thick, &obj->u.rect.fill) == 8) {
			obj->type = lib_obj_rect;
			return 1;
		}
		if (sscanf(line, "C %d %d %d %u %u %d %c",
		    &obj->u.circ.x, &obj->u.circ.y, &obj->u.circ.r,
		    &obj->unit, &obj->convert, &obj->u.circ.thick,
		    &obj->u.circ.fill) == 7) {
			obj->type = lib_obj_circ;
			return 1;
		}
		if (parse_arc(obj, line)) {
			obj->type = lib_obj_arc;
			return 1;
		}
		n = sscanf(line,
		    "T %d %d %d %d %u %u %u \"%m[^\"]\" %ms %u %c %c",
		    &obj->u.text.orient, &obj->u.text.x, &obj->u.text.y,
		    &obj->u.text.dim, &zero1, &obj->unit, &obj->convert,
		    &obj->u.text.s, &style, &zero2,
		    &obj->u.text.hor_align, &obj->u.text.vert_align);
		if (n != 12) {
			n = sscanf(line,
			    "T %d %d %d %d %u %u %u %ms %ms %u %c %c",
			    &obj->u.text.orient, &obj->u.text.x, &obj->u.text.y,
			    &obj->u.text.dim, &zero1, &obj->unit, &obj->convert,
			    &obj->u.text.s, &style, &zero2,
			    &obj->u.text.hor_align, &obj->u.text.vert_align);
			while (n == 12) {
				char *tilde;

				tilde = strchr(obj->u.text.s, '~');
				if (!tilde)
					break;
				*tilde = ' ';
			}
		}
		/*
		 * zero2 seems to be the font style: 0 = normal, 1 = bold ?
		 */
		if (n == 12) {
			if (zero1) {
				fprintf(stderr, "%u: only understand 0 x x\n"
				    "\"%s\"\n", file->lineno, line);
				exit(1);
			}
			obj->u.text.style = decode_style(style);
			obj->type = lib_obj_text;
			return 1;
		}
		if (sscanf(line, "X %ms %ms %d %d %d %c %d %d %u %u %c",
		    &obj->u.pin.name, &obj->u.pin.number,
		    &obj->u.pin.x, &obj->u.pin.y, &obj->u.pin.length,
		    &obj->u.pin.orient,
		    &obj->u.pin.number_size, &obj->u.pin.name_size,
		    &obj->unit, &obj->convert, &obj->u.pin.etype) == 11) {
			obj->type = lib_obj_pin;
			return 1;
		}
		break;
	default:
		abort();
	}
	fprintf(stderr, "%u: cannot parse\n\"%s\"\n", file->lineno, line);
	exit(1);
}


bool lib_parse_file(struct lib *lib, struct file *file)
{
	lib->state = lib_skip;
	return file_read(file, lib_parse_line, lib);
}



bool lib_parse(struct lib *lib, const char *name, const struct file *related)
{
	struct file file;
	bool res;

	if (!file_open(&file, name, related))
		return 0;
	res = lib_parse_file(lib, &file);
	file_close(&file);
	return res;
}


void lib_init(struct lib *lib)
{
	lib->comps = NULL;
	lib->next_comp = &lib->comps;
}


static void free_objs(struct lib_obj *objs)
{
	struct lib_obj *next;

	while (objs) {
		next = objs->next;
		switch (objs->type) {
		case lib_obj_text:
			free((char *) objs->u.text.s);
			break;
		case lib_obj_pin:
			free((char *) objs->u.pin.name);
			free((char *) objs->u.pin.number);
			break;
		default:
			break;
		}
		free(objs);
		objs = next;
	}
}


void lib_free(struct lib *lib)
{
	struct comp *comp, *next;

	for (comp = lib->comps; comp; comp = next) {
		next = comp->next;
		free((char *) comp->name);
		free_objs(comp->objs);
		free(comp);
	}
}
