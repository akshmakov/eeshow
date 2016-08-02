/*
 * sch-parse.c - Parse Eeschema .sch file
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
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <assert.h>

#include "util.h"
#include "dwg.h"
#include "file.h"
#include "lib.h"
#include "sch.h"


/* ----- (Global) Labels --------------------------------------------------- */


static enum dwg_shape do_decode_shape(const char *s)
{
	if (!strcmp(s, "UnSpc"))
		return dwg_unspec;
	if (!strcmp(s, "Input"))
		return dwg_in;
	if (!strcmp(s, "Output"))
		return dwg_out;
	if (!strcmp(s, "3State"))
		return dwg_tri;
	if (!strcmp(s, "BiDi"))
		return dwg_bidir;
	fprintf(stderr, "unknown shape: \"%s\"\n", s);
	exit(1);
}


static enum dwg_shape decode_shape(const char *s)
{
	enum dwg_shape res;

	res = do_decode_shape(s);
	free((void *) s);
	return res;
}


/* ----- Component fields -------------------------------------------------- */


void decode_alignment(struct text *txt, char hor, char vert)
{
	switch (hor) {
	case 'L':
		txt->hor = text_min;
		break;
	case 'C':
		txt->hor = text_mid;
		break;
	case 'R':
		txt->hor = text_max;
		break;
	default:
		assert(0);
	}

	switch (vert) {
	case 'B':
		txt->vert = text_min;
		break;
	case 'C':
		txt->vert = text_mid;
		break;
	case 'T':
		txt->vert = text_max;
		break;
	default:
		assert(0);
	}
}


static bool parse_field(struct sch_ctx *ctx, const char *line)
{
	struct sch_comp *comp = &ctx->obj.u.comp;
	int n;
	unsigned flags;
	char hv, hor, vert, italic, bold;
	struct comp_field *field;
	struct text *txt;

	field = alloc_type(struct comp_field);
	txt = &field->txt;

	if (sscanf(line, "F %d \"\" %c %d %d %u %u %c %c%c%c",
	    &n, &hv, &txt->x, &txt->y, &txt->size, &flags, &hor, &vert,
	    &italic, &bold) == 10) {
		free(field);
		return 1;
	}
	if (sscanf(line, "F %d \"%m[^\"]\" %c %d %d %u %u %c %c%c%c",
	    &n, &txt->s, &hv, &txt->x, &txt->y, &txt->size, &flags,
	    &hor, &vert, &italic, &bold) != 11)
		return 0;

	if (flags || !lib_field_visible(comp->comp, n)) {
		free(field);
		return 1;
	}

	if (n == 0 && comp->comp->units > 1) {
		int len = strlen(txt->s);
		char *s;

		s = realloc((void *) txt->s, len + 3);
		if (!s) {
			perror("realloc");
			exit(1);
		}
		if (comp->unit <= 26)
			sprintf(s + len, "%c", 'A' + comp->unit - 1);
		else
			sprintf(s + len, "%c%c",
			    'A' + (comp->unit - 1) / 26 - 1,
			    'A' + (comp->unit - 1) % 26);
		txt->s = s;
	}

	field->next = comp->fields;
	comp->fields = field;

	switch (hv) {
	case 'H':
		txt->rot = 0;
		break;
	case 'V':
		txt->rot = 90;
		break;
	default:
		assert(0);
	}

	decode_alignment(txt, hor, vert);

	// @@@ decode font

	return 1;
}


/* ----- Sheet field ------------------------------------------------------- */


static enum dwg_shape decode_form(char form)
{
	switch (form) {
	case 'O':
		return dwg_in;
	case 'I':
		return dwg_out;
	case 'B':
		/* fall through */
	case 'T':
		return dwg_bidir;
	case 'U':
		return dwg_unspec;
	default:
		fprintf(stderr, "unknown form: \"%c\"\n", form);
		exit(1);
	}
}


static int decode_side(char side)
{
	switch (side) {
	case 'L':
		return 2;	/* left */
	case 'B':
		return 1;	/* up */
	case 'R':
		return 0;	/* right */
	case 'T':
		return 3;	/* down */
	default:
		fprintf(stderr, "unknown side: \"%c\"\n", side);
		exit(1);
	}
}


static bool parse_hsheet_field(struct sch_ctx *ctx, const char *line)
{
	struct sch_sheet *sheet = &ctx->obj.u.sheet;
	char *s;
	char form, side;
	unsigned n, dim;
	struct sheet_field *field;

	if (sscanf(line, "F%d \"%m[^\"]\" %u", &n, &s, &dim) == 3) {
		switch (n) {
		case 0:
			sheet->sheet = s;
			sheet->sheet_dim = dim;
			return 1;
		case 1:
			sheet->file = s;
			sheet->file_dim = dim;
			return 1;
		default:
			assert(0);
		}
	}

	field = alloc_type(struct sheet_field);
	if (sscanf(line, "F%d \"%m[^\"]\" %c %c %d %d %u",
	    &n, &field->s, &form, &side, &field->x, &field->y, &field->dim)
	    != 7) {
		free(field);
		return 0;
	}
	assert(n >= 2);

	if (side == 'B' || side == 'T') {
		/*
		 * This is beautiful: since there is no indication for rotation
		 * on the hsheet, or the sheet or file fields, we need to look
		 * at whether the imported sheet pins go left or right (no
		 * rotation) or whether they go top or bottom (rotation).
		 *
		 * A sheet with no imported pins lacks these hints, and is
		 * therefore always assumed to be without rotation.
		 *
		 * Eeschema is careful to be consistent, and does not allow
		 * sheets with no imported pins to be rotated. Even better, it
		 * flips rotated sheets where the last imported pin is deleted
		 * back.
		 */
		sheet->rotated = 1;
	}
	field->shape = decode_form(form);
	field->side = decode_side(side);

	field->next = sheet->fields;
	sheet->fields = field;

	return 1;
}


/* ----- Schematics parser ------------------------------------------------- */


static void submit_obj(struct sch_ctx *ctx, enum sch_obj_type type)
{
	struct sch_obj *obj;

	obj = alloc_type(struct sch_obj);
	*obj = ctx->obj;
	obj->type = type;
	obj->next = NULL;

	*ctx->next_obj = obj;
	ctx->next_obj = &obj->next;
}


static struct sheet *new_sheet(struct sch_ctx *ctx)
{
	struct sheet *sheet;

	sheet = alloc_type(struct sheet);
	sheet->objs = NULL;
	sheet->next = NULL;

	ctx->next_obj = &sheet->objs;

	*ctx->next_sheet = sheet;
	ctx->next_sheet = &sheet->next;

	return sheet;
}


static bool parse_line(const struct file *file, void *user, const char *line);


static void recurse_sheet(struct sch_ctx *ctx, const struct file *related)
{
	struct sch_obj **saved_next_obj = ctx->next_obj;
	const char *name = ctx->obj.u.sheet.file;
	struct file file;

	/* @@@ clean this up: saved_next_obj is still pending */

	new_sheet(ctx);
	ctx->state = sch_descr;
	file_open(&file, name, related);
	file_read(&file, parse_line, ctx);
	file_close(&file);
	ctx->next_obj = saved_next_obj;
}


static bool parse_line(const struct file *file, void *user, const char *line)
{
	struct sch_ctx *ctx = user;
	struct sch_obj *obj = &ctx->obj;
	int n = 0;
	char *s;

	switch (ctx->state) {
	case sch_basic:
		if (sscanf(line, "$Comp%n", &n) == 0 && n) {
			ctx->state = sch_comp;
			obj->u.comp.fields = NULL;
			return 1;
		}
		if (sscanf(line, "$Sheet%n", &n) == 0 && n) {
			ctx->state = sch_sheet;
			obj->u.sheet.sheet = NULL;
			obj->u.sheet.file = NULL;
			obj->u.sheet.rotated = 0;
			obj->u.sheet.fields = NULL;
			return 1;
		}

		/* Text */

		struct sch_text *text = &obj->u.text;

		if (sscanf(line, "Text Notes %d %d %d %d",
		    &obj->x, &obj->y, &text->dir, &text->dim) == 4) {
			ctx->state = sch_text;
			obj->u.text.fn = dwg_text;
			return 1;
		}
		if (sscanf(line, "Text GLabel %d %d %d %d %ms",
		    &obj->x, &obj->y, &text->dir, &text->dim, &s) == 5) {
			ctx->state = sch_text;
			obj->u.text.fn = dwg_glabel;
			obj->u.text.shape = decode_shape(s);
			return 1;
		}
		if (sscanf(line, "Text HLabel %d %d %d %d %ms",
		    &obj->x, &obj->y, &text->dir, &text->dim, &s) == 5) {
			ctx->state = sch_text;
			obj->u.text.fn = dwg_hlabel;
			obj->u.text.shape = decode_shape(s);
			return 1;
		}
		if (sscanf(line, "Text Label %d %d %d %d",
		    &obj->x, &obj->y, &text->dir, &text->dim) == 4) {
			ctx->state = sch_text;
			obj->u.text.fn = dwg_label;
			return 1;
		}

		/* Connection */

		if (sscanf(line, "Connection ~ %d %d", &obj->x, &obj->y) == 2) {
			submit_obj(ctx, sch_obj_junction);
			return 1;
		}

		/* NoConn */

		if (sscanf(line, "NoConn ~ %d %d", &obj->x, &obj->y) == 2) {
			submit_obj(ctx, sch_obj_noconn);
			return 1;
		}

		/* Wire */

		if (sscanf(line, "Wire Wire Line%n", &n) == 0 && n) {
			ctx->state = sch_wire;
			obj->u.wire.fn = dwg_wire;
			return 1;
		}
		if (sscanf(line, "Wire Bus Line%n", &n) == 0 && n) {
			ctx->state = sch_wire;
			obj->u.wire.fn = dwg_bus;
			return 1;
		}
		if (sscanf(line, "Wire Notes Line%n", &n) == 0 && n) {
			ctx->state = sch_wire;
			obj->u.wire.fn = dwg_line;
			return 1;
		}

		/* Entry */

		/*
		 * Documentation mentions the following additional variants:
		 *
		 * - Entry Wire Line equivalent:
		 *   Wire Wire Bus
		 *   Entry Wire Bus
		 *
		 * - Entry Bus Bus equivalent:
		 *   Wire Bus Bus
		 */
		if (sscanf(line, "Entry Wire Line%n", &n) == 0 && n) {
			ctx->state = sch_wire;
			obj->u.wire.fn = dwg_wire;
			return 1;
		}
		if (sscanf(line, "Entry Bus Bus%n", &n) == 0 && n) {
			ctx->state = sch_wire;
			obj->u.wire.fn = dwg_bus;
			return 1;
		}

		/* EndSCHEMATC */

		if (sscanf(line, "$EndSCHEMATC%n", &n) == 0 && n)
			return 0;
		break;
	case sch_descr:
		if (sscanf(line, "$EndDescr%n", &n) || !n)
			return 1;
		ctx->state = sch_basic;
		return 1;
	case sch_comp:
		if (sscanf(line, "$EndComp%n", &n) == 0 && n) {
			ctx->state = sch_basic;
			submit_obj(ctx, sch_obj_comp);
			return 1;
		}
		if (sscanf(line, "L %ms", &s) == 1) {
			obj->u.comp.comp = lib_find(ctx->lib, s);
			free(s);
			return 1;
		}
		if (sscanf(line, "U %u", &obj->u.comp.unit) == 1)
			return 1;
		if (sscanf(line, "P %d %d", &obj->x, &obj->y) == 2)
			return 1;
		if (parse_field(ctx, line))
			return 1;
		if (sscanf(line, "AR %n", &n) == 0 && n)
			return 1; /* @@@ what is "AR" ? */
		n = sscanf(line, " %d %d %d %d",
		    obj->u.comp.m + 1, obj->u.comp.m + 2,
		    obj->u.comp.m + 4, obj->u.comp.m + 5);
		if (n == 3)
			return 1;
		if (n == 4) {
			obj->u.comp.m[0] = obj->x;
			obj->u.comp.m[3] = obj->y;
			return 1;
		}
		break;
	case sch_sheet:
		if (sscanf(line, "$EndSheet%n", &n) == 0 && n) {
			submit_obj(ctx, sch_obj_sheet);
			if (ctx->recurse)
				recurse_sheet(ctx, file);
			ctx->state = sch_basic;
			return 1;
		}
		if (sscanf(line, "S %d %d %u %u",
		    &obj->x, &obj->y, &obj->u.sheet.w, &obj->u.sheet.h) == 4)
			return 1;
		if (sscanf(line, "U %*x%n", &n) == 0 && n)
			return 1;
		if (parse_hsheet_field(ctx, line))
			return 1;
		break;
	case sch_text:
		ctx->state = sch_basic;
		{
			const char *from;
			char *to;

			s = alloc_size(strlen(line) + 1);
			from = line;
			to = s;
			while (*from) {
				if (from[0] != '\\' || from[1] != 'n') {
					*to++ = *from++;
					continue;
				}
				*to++ = '\n';
				from += 2;
			}
			*to = 0;
			obj->u.text.s = s;
			submit_obj(ctx, sch_obj_text);
		}
		return 1;
	case sch_wire:
		if (sscanf(line, "%d %d %d %d", &obj->x, &obj->y,
		    &obj->u.wire.ex, &obj->u.wire.ey) != 4)
			break;
		submit_obj(ctx, sch_obj_wire);
		ctx->state = sch_basic;
		return 1;
	default:
		abort();
	}
	fprintf(stderr, "%s:%u: cannot parse\n\"%s\"\n",
	    file->name, file->lineno, line);
	exit(1);
}


void sch_parse(struct sch_ctx *ctx, struct file *file, const struct lib *lib)
{
	ctx->lib = lib;
	file_read(file, parse_line, ctx);
}


void sch_init(struct sch_ctx *ctx, bool recurse)
{
	ctx->state = sch_descr;
	ctx->recurse = recurse;
	ctx->sheets = NULL;
	ctx->next_sheet = &ctx->sheets;
	new_sheet(ctx);
}
