/*
 * delta.c - Find differences in .sch files
 *
 * Written 2016 by Werner Almesberger
 * Copyright 2016 by Werner Almesberger
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */


#include <stddef.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include "util.h"
#include "text.h"
#include "sch.h"
#include "delta.h"


/*
 * @@@ we should clone components and compare them, too
 *
 * if two components or sub-sheets mismatch in their field lists, we should
 * remove matching items from the lists.
 *
 * @@@@ line(A, B) == line(B, A), etc.
 */

static struct sch_obj *objs_clone(const struct sch_obj *objs)
{
	struct sch_obj *new_objs = NULL;
	struct sch_obj **next = &new_objs;
	const struct sch_obj *obj;

	for (obj = objs; obj; obj = obj->next) {
		*next = alloc_type(struct sch_obj);
		**next = *obj;
		next = &(*next)->next;
	}
	*next = NULL;
	return new_objs;
}


static void add_obj(struct sheet *sch, struct sch_obj *obj)
{
	*sch->next_obj = obj;
	sch->next_obj = &obj->next;
	obj->next = NULL;
}


static bool comp_fields_eq(const struct comp_field *a,
    const struct comp_field *b)
{
	while (a && b) {
		if (a->txt.x != b->txt.x || a->txt.y != b->txt.y)
			return 0;
		if (a->txt.size != b->txt.size)
			return 0;
		if (a->txt.rot != b->txt.rot)
			return 0;
		if (a->txt.hor != b->txt.hor || a->txt.vert != b->txt.vert)
			return 0;
		if (strcmp(a->txt.s, b->txt.s))
			return 0;
		a = a->next;
		b = b->next;
	}
	return a == b;
}


static bool sheet_fields_eq(const struct sheet_field *a,
    const struct sheet_field *b)
{
	const struct sheet_field *ta = a;
	const struct sheet_field *tb = b;

	for (ta = a; ta; ta = ta->next) {
		for (tb = b; tb; tb = tb->next) {
			if (ta->x != tb->x || ta->y != tb->y)
				continue;
			if (ta->dim != tb->dim)
				continue;
			if (ta->dim != tb->dim)
				continue;
			if (ta->shape != tb->shape)
				continue;
			if (strcmp(ta->s, tb->s))
				continue;
			goto match;
		}
		return 0;
match:
		continue;
	}

	while (a && b) {
		a = a->next;
		b = b->next;
	}
	return a == b;
}


/*
 * @@@ idea: if we send all strings through a "unique" function, we can
 * memcmp things like "struct sch_text" without having to go through the
 * fields individually.
 */

static bool obj_eq(const struct sch_obj *a, const struct sch_obj *b)
{
	if (a->type != b->type)
		return 0;
	if (a->x != b->x || a->y != b->y)
		return 0;
	switch (a->type) {
	case sch_obj_wire:
		if (a->u.wire.fn != b->u.wire.fn)
			return 0;
		if (a->x != b->x || a->y != b->y)
			return 0;
		if (a->u.wire.ex != b->u.wire.ex ||
		    a->u.wire.ey != b->u.wire.ey)
			return 0;
		return 1;
	case sch_obj_junction:
		return 1;	
	case sch_obj_noconn:
		return 1;	
	case sch_obj_text:
		if (a->u.text.fn != b->u.text.fn)
			return 0;
		if (a->u.text.dir != b->u.text.dir)
			return 0;
		if (a->u.text.dim != b->u.text.dim)
			return 0;
		if (a->u.text.shape != b->u.text.shape)
			return 0;
		if (strcmp(a->u.text.s, b->u.text.s))
			return 0;
		return 1;
	case sch_obj_comp:
		if (a->u.comp.comp != b->u.comp.comp)
			return 0;
		if (a->u.comp.unit != b->u.comp.unit)
			return 0;
		if (memcmp(a->u.comp.m, b->u.comp.m, sizeof(a->u.comp.m)))
			return 0;
		return comp_fields_eq(a->u.comp.fields, b->u.comp.fields);
	case sch_obj_sheet:
		if (a->u.sheet.w != b->u.sheet.w)
			return 0;
		if (a->u.sheet.h != b->u.sheet.h)
			return 0;
		if (a->u.sheet.name_dim != b->u.sheet.name_dim)
			return 0;
		if (a->u.sheet.file_dim != b->u.sheet.file_dim)
			return 0;
		if (a->u.sheet.rotated != b->u.sheet.rotated)
			return 0;
		if (strcmp(a->u.sheet.name, b->u.sheet.name))
			return 0;
		if (strcmp(a->u.sheet.file, b->u.sheet.file))
			return 0;
		return sheet_fields_eq(a->u.sheet.fields, b->u.sheet.fields);
	default:
		abort();
	}
}


static void free_obj(struct sch_obj *obj)
{
	/* there may be more to free once we get into cloning components */
	free(obj);
}


static void init_res(struct sheet *res)
{
	res->title = NULL;
        res->objs = NULL;
        res->next_obj = &res->objs;
        res->parent = NULL;
        res->next = NULL;
}


void delta(const struct sheet *a, const struct sheet *b,
    struct sheet *res_a, struct sheet *res_b, struct sheet *res_ab)
{
	struct sch_obj *objs_a, *objs_b;
	struct sch_obj *next_a;
	struct sch_obj **obj_b;

	init_res(res_a);
	init_res(res_b);
	init_res(res_ab);

	if (!strcmp(a->title, b->title)) {
		res_ab->title = a->title;
	} else {
		res_a->title = a->title;
		res_b->title = b->title;
	}

	objs_a = objs_clone(a->objs);
	objs_b = objs_clone(b->objs);

	while (objs_a) {
		next_a = objs_a->next;
		for (obj_b = &objs_b; *obj_b; obj_b = &(*obj_b)->next)
			if (obj_eq(objs_a, *obj_b)) {
				struct sch_obj *tmp = *obj_b;

				add_obj(res_ab, objs_a);
				*obj_b = tmp->next;
				free_obj(tmp);
				goto found;
			}
		add_obj(res_a, objs_a);
found:
		objs_a = next_a;
	}
	res_b->objs = objs_b;
}


void delta_free(struct sheet *d)
{
	struct sch_obj *next;

	while (d->objs) {
		next = d->objs->next;
		free_obj(d->objs);
		d->objs = next;
	}
}
