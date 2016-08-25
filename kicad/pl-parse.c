/*
 * kicad/pl-parse.c - KiCad page layout parser
 *
 * Written 2016 by Werner Almesberger
 * Copyright 2016 by Werner Almesberger
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */


#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include "misc/util.h"
#include "misc/diag.h"
#include "gfx/text.h"
#include "file/file.h"
#include "kicad/sexpr.h"
#include "kicad/pl-common.h"
#include "kicad/pl.h"


static bool get_coord(const struct expr *e,
    float *x, float *y, int *dx, int *dy)
{
	unsigned n = 0;

	*dx = -1;
	*dy = -1;
	for (; e; e = e->next) {
		if (e->e) {
			warning("coord: ignoring list");
			continue;
		}

		if (!strcmp(e->s, "ltcorner")) {
			*dx = *dy = 1;
		} else if (!strcmp(e->s, "lbcorner")) {
			*dx = 1;
			*dy = -1;
		} else if (!strcmp(e->s, "rtcorner")) {
			*dx = -1;
			*dy = 1;
		} else if (!strcmp(e->s, "rbcorner")) {
			*dx = *dy = -1;
		} else {
			char *end;
			float f = strtof(e->s, &end);

			if (*end) {
				error("coord: not a number \"%s\"", e->s);
				return 0;
			}
			if (n++)
				*y = f;
			else
				*x = f;
		}
	}

	switch (n) {
	case 0:
	case 1:
		error("coord: not enough coordinates");
		return 0;
	case 2:
		return 1;
	default:
		error("coord: too many coordinates");
		return 0;
	}
}


static bool get_size(const struct expr *e, float *x, float *y)
{
	unsigned n = 0;

	for (; e; e = e->next) {
		char *end;
		float f;

		if (e->e) {
			warning("size: ignoring list");
			continue;
		}

		f = strtof(e->s, &end);
		if (*end) {
			error("size: not a number \"%s\"", e->s);
			return 0;
		}
		if (n++)
			*y = f;
		else
			*x = f;
	}

	switch (n) {
	case 0:
	case 1:
		error("size: not enough coordinates");
		return 0;
	case 2:
		return 1;
	default:
		error("size: too many coordinates");
		return 0;
	}
}


static bool get_float(const struct expr *e, float *f)
{
	for (; e; e = e->next)
		if (e->s) {
			*f = atof(e->s);	// @@@ error checking
			return 1;
		}
	error("no number found");
	return 0;
}



static bool get_int(const struct expr *e, int *n)
{
	for (; e; e = e->next)
		if (e->s) {
			*n = atoi(e->s);	// @@@ error checking
			return 1;
		}
	error("no number foundn");
	return 0;
}


static bool process_setup(struct pl_ctx *pl, const struct expr *e)
{
	const char *s;
	const struct expr *next;

	for (; e; e = e->next) {
		if (!e->e) {
			warning("setup: ignoring non-list");
			continue;
		}

		s = e->e->s;
		next = e->e->next;

		if (!strcmp(s, "comment"))
			continue;

		if (!strcmp(s, "textsize")) {
			if (!get_size(next, &pl->tx, &pl->ty))
				return 0;
		} else if (!strcmp(s, "linewidth")) {
			// meh
		} else if (!strcmp(s, "textlinewidth")) {
			// meh
		} else if (!strcmp(s, "left_margin")) {
			if (!get_float(next, &pl->l))
				return 0;
		} else if (!strcmp(s, "right_margin")) {
			if (!get_float(next, &pl->r))
				return 0;
		} else if (!strcmp(s, "top_margin")) {
			if (!get_float(next, &pl->t))
				return 0;
		} else if (!strcmp(s, "bottom_margin")) {
			if (!get_float(next, &pl->b))
				return 0;
		} else {
			warning("setup: ignoring \"%s\"", s);
		}
	}
	return 1;
}


static bool process_font(struct pl_obj *obj, const struct expr *e)
{
	const char *s;
	const struct expr *next;

	for (; e; e = e->next) {
		if (e->s) {
			if (!strcmp(e->s, "bold"))
				obj->font |= font_bold;
			else if (!strcmp(e->s, "italic"))
				obj->font |= font_italic;
			else
				warning("font: ignoring \"%s\"", e->s);
			continue;
		}

		if (!e->e) {
			warning("font: ignoring empty list");
			continue;
		}
		s = e->e->s;
		next = e->e->next;

		if (!strcmp(s, "comment"))
			continue;

		if (!strcmp(s, "size")) {
			if (!get_size(next, &obj->ex, &obj->ey))
				return 0;
		} else {
			warning("font: ignoring \"%s\"", s);
		}
	}
	return 1;
}


static bool process_justify(struct pl_obj *obj, const struct expr *e)
{
	for (; e; e = e->next) {
		if (e->e) {
			warning("justify: ignoring list");
			continue;
		}

		if (!strcmp(e->s, "center"))
			obj->hor = obj->vert = text_mid;
		else if (!strcmp(e->s, "left"))
			obj->hor = text_min;
		else if (!strcmp(e->s, "right"))
			obj->hor = text_max;
		else if (!strcmp(e->s, "top"))
			obj->vert = text_max;
		else if (!strcmp(e->s, "bottom"))
			obj->vert = text_min;
		else
			warning("justify: ignoring \"%s\"", e->s);
	}
	return 1;
}


static bool process_obj(struct pl_ctx *pl, const struct expr *e,
    enum pl_obj_type type)
{
	struct pl_obj *obj;
	const char *s;
	const struct expr *next;

	obj = alloc_type(struct pl_obj);
	obj->type = type;
	obj->s = NULL;
	obj->repeat = 1;
	obj->x = obj->y = obj->ex = obj->ey = 0;
	obj->dx = obj->dy = obj->edx = obj->edy = 0;
	obj->incrx = 0;
	obj->incry = 0;
	obj->incrlabel = 0;
	obj->font = 0;
	obj->hor = text_min;
	obj->vert = text_mid;

	for (; e; e = e->next) {
		if (e->s) {
			if (obj->s) {
				error("pl_obj: multiple strings");
				return 0;
			}
			obj->s = stralloc(e->s);
			continue;
		}
		if (!e->e) {
			warning("pl_obj: ignoring empty list");
			continue;
		}

		s = e->e->s;
		next = e->e->next;

		if (!strcmp(s, "comment"))
			continue;
		if (!strcmp(s, "name"))
			continue;

		if (!strcmp(s, "linewidth")) {
			// meh
		} else if (!strcmp(s, "start") || !strcmp(s, "pos")) {
			if (!get_coord(next, &obj->x, &obj->y,
			    &obj->dx, &obj->dy))
				return 0;
		} else if (!strcmp(s, "end")) {
			if (!get_coord(next, &obj->ex, &obj->ey,
			    &obj->edx, &obj->edy))
				return 0;
		} else if (!strcmp(s, "repeat")) {
			if (!get_int(next, &obj->repeat))
				return 0;
		} else if (!strcmp(s, "incrx")) {
			if (!get_float(next, &obj->incrx))
				return 0;
		} else if (!strcmp(s, "incry")) {
			if (!get_float(next, &obj->incry))
				return 0;
		} else if (!strcmp(s, "incrlabel")) {
			if (!get_int(next, &obj->incrlabel))
				return 0;
		} else if (!strcmp(s, "font")) {
			if (!process_font(obj, next))
				return 0;
		} else if (!strcmp(s, "justify")) {
			if (!process_justify(obj, next))
				return 0;
		} else
			warning("pl_obj: ignoring \"%s\"", s);
	}

	obj->next = pl->objs;
	pl->objs = obj;

	return 1;
}


static bool process_layout(struct pl_ctx *pl, const struct expr *e)
{
	const char *s;
	const struct expr *next;

	for (; e; e = e->next) {
		if (!e->e) {
			warning("layout: ignoring non-list");
			continue;
		}

		s = e->e->s;
		next = e->e->next;

		if (!strcmp(s, "comment"))
			continue;
		if (!strcmp(s, "setup")) {
			if (!process_setup(pl, next))
				return 0;
		} else if (!strcmp(s, "rect")) {
			if (!process_obj(pl, next, pl_obj_rect))
				return 0;
		} else if (!strcmp(s, "line")) {
			if (!process_obj(pl, next, pl_obj_line))
				return 0;
		} else if (!strcmp(s, "tbtext")) {
			if (!process_obj(pl, next, pl_obj_text))
				return 0;
		} else {
			warning("layout: ignoring \"%s\"", s);
		}
	}
	return 1;
}


static bool process(struct pl_ctx *p, const struct expr *e)
{
	while (e) {
		if (e->e && e->e->s && !strcmp(e->e->s, "page_layout") &&
		    e->e->next && e->e->next->e)
			return process_layout(p, e->e->next);
		e = e->next;
	}
	error("no layout information found");
	return 0;
}


static bool pl_parse_line(const struct file *file,
    void *user, const char *line)
{
	struct pl_ctx *pl = user;

	return sexpr_parse(pl->sexpr_ctx, line);
}


struct pl_ctx *pl_parse(struct file *file)
{
	struct pl_ctx *pl;
	struct expr *e = NULL;

	pl = alloc_type(struct pl_ctx);
	pl->sexpr_ctx = sexpr_new();
	pl->l = pl->r = pl->t = pl->b = 0;
	pl->tx = pl->ty = 0;
	pl->objs = NULL;

	if (!file_read(file, pl_parse_line, pl)) {
		sexpr_abort(pl->sexpr_ctx);
		goto fail;
	}
	if (!sexpr_finish(pl->sexpr_ctx, &e))
		goto fail;
	if (!process(pl, e))
		goto fail;
	free_expr(e);
	return pl;

fail:
	free_expr(e);
	free(pl);
	return 0;
}


void pl_free(struct pl_ctx *pl)
{
	struct pl_obj *next;

	while (pl->objs) {
		next = pl->objs->next;
		free((void *) pl->objs->s);
		free(pl->objs);
		pl->objs = next;
	}
	free(pl);
}
