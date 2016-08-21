/*
 * gfx/pdftoc.c - PDF writer with TOC generation
 *
 * Written 2016 by Werner Almesberger
 * Copyright 2016 by Werner Almesberger
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

/*
 * Strongly influenced by https://neo900.org/git?p=misc;a=tree;f=schtoc
 *
 * PDF Reference:
 * http://www.adobe.com/content/dam/Adobe/en/devnet/acrobat/pdfs/pdf_reference_1-7.pdf
 */


#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>

#include "misc/util.h"
#include "misc/diag.h"
#include "gfx/pdftoc.h"


struct title {
	char *s;
	struct title *next;
};

struct object {
	int gen;
	unsigned pos;
	bool is_page;
};

struct pdftoc {
	FILE *file;

	enum state {
		idle,	/* between objects */
		object,	/* inside an object */
		catalog,/* inside the catalog object */
		xref,	/* stopped at xref */
		trailer,/* going through the trailer */
	} state;

	struct title *titles;
	struct title **next_title;
	unsigned n_titles;

	char *buf;
	unsigned left;		/* bytes left in buffer */
	unsigned offset;	/* offset into buffer */
	unsigned pos;		/* position in file */

	struct object *objs;	/* object array */
	struct object *curr_obj;
	int top;		/* highest object number; -1 if no objects */

	int root;		/* catalog dict */
	int info;		/* information dict, 0 if absent */
};


static bool begins(const char *s, const char *pfx)
{
	return !strncmp(s, pfx, strlen(pfx));
}


struct pdftoc *pdftoc_begin(const char *file)
{
	struct pdftoc *ctx;

	ctx = alloc_type(struct pdftoc);
	if (file) {
		ctx->file = fopen(file, "w");
		if (!ctx->file)
			diag_pfatal(file);
	} else {
		ctx->file = stdout;
	}

	ctx->state = idle;

	ctx->titles = NULL;
	ctx->next_title = &ctx->titles;
	ctx->n_titles = 0;

	ctx->buf = NULL;
	ctx->left = 0;
	ctx->offset = 0;
	ctx->pos = 0;

	ctx->objs = NULL;
	ctx->top = -1;

	ctx->root = 0;
	ctx->info = 0;

	return ctx;
}


static void add_object(struct pdftoc *ctx, int id, int gen, unsigned pos)
{
	struct object *obj;

	if (id > ctx->top) {
		ctx->objs = realloc(ctx->objs,
		    (id + 1) * sizeof(struct object));
		memset(ctx->objs + ctx->top + 1 , 0,
		    (id - ctx->top) * sizeof(struct object));
		ctx->top = id;
	}

	obj = ctx->objs + id;
	ctx->curr_obj = obj;
	obj->gen = gen;
	obj->pos = pos;
	obj->is_page = 0;
}


static bool parse_object(struct pdftoc *ctx, const char *s)
{
	int id, gen;
	int n = 0;

	if (sscanf(s, "%d %d obj%n", &id, &gen, &n) != 2 || !n)
		return 0;
	add_object(ctx, id, gen, ctx->pos);
	return 1;
}


static void line(struct pdftoc *ctx, const char *s)
{

	switch (ctx->state) {
	case idle:
		if (parse_object(ctx, s)) {
			ctx->state = object;
			break;
		}
		if (begins(s, "xref")) {
			ctx->state = xref;
			break;
		}
		break;
	case object:
		if (begins(s, "endobj")) {
			ctx->state = idle;
			break;
		}
		if (begins(s, "<< /Type /Page")) {
			ctx->curr_obj->is_page = 1;
			break;
		}
		if (begins(s, "<< /Type /Catalog")) {
			ctx->state = catalog;
			break;
		}
		break;
	case catalog:
		if (begins(s, ">>")) {
			ctx->state = object;
			ctx->pos += fprintf(ctx->file,
			    "   /Outlines %u 0 R\n",
			    ctx->top + 1);
			break;
		}
		break;
	case xref:
		abort();
	case trailer:
		if (sscanf(s, "   /Root %d 0 R", &ctx->root) == 1)
			break;
		if (sscanf(s, "   /Info %d 0 R", &ctx->info) == 1)
			break;
		break;
	default:
		abort();
	}
}


static void parse_buffer(struct pdftoc *ctx, bool do_write)
{
	unsigned size, wrote;
	char *nl;

	while (ctx->state != xref) {
		nl = memchr(ctx->buf + ctx->offset, '\n', ctx->left);
		if (!nl)
			break;
		*nl = 0;
		size = nl - (ctx->buf + ctx->offset);
		line(ctx, ctx->buf + ctx->offset);
		*nl = '\n';
		if (ctx->state == xref)
			break;
		if (do_write) {
			wrote = fwrite(ctx->buf + ctx->offset, 1, size + 1,
			    ctx->file);
			if (wrote != size + 1)
				diag_pfatal("fwrite");
			ctx->pos += size + 1;
		}
		ctx->offset += size + 1;
		ctx->left -= size + 1;
	}
}


bool pdftoc_write(struct pdftoc *ctx, const void *data, unsigned length)
{
	char *buf;

	buf = alloc_size(ctx->left + length + 1);
	memcpy(buf, ctx->buf + ctx->offset, ctx->left);
	memcpy(buf + ctx->left, data, length);
	ctx->offset = 0;
	ctx->left += length;
	free(ctx->buf);
	ctx->buf = buf;

	parse_buffer(ctx, 1);

	return 1;
}


void pdftoc_title(struct pdftoc *ctx, const char *title)
{
	struct title *t;

	t = alloc_type(struct title);
	t->s = stralloc(title);
	*ctx->next_title = t;
	t->next = NULL;
	ctx->next_title = &t->next;
	ctx->n_titles++;
}


static void write_trailer(struct pdftoc *ctx)
{
	unsigned n = ctx->top + 1;
	const struct object *obj = ctx->objs;
	const struct object *end = ctx->objs + ctx->top + 1;
	const struct title *t;
	unsigned outline, tail;

	/* Outline root */

	outline = n;
	add_object(ctx, n, 0, ctx->pos);
	tail = fprintf(ctx->file,
	    "%u 0 obj\n<<\n"
	    "   /Count %u\n"
	    "   /First %u 0 R\n"
	    "   /Last %u 0 R\n"
	    ">>\nendobj\n",
	    n, ctx->n_titles, n + 1, n + ctx->n_titles);

	/* Outline items */

	n++;
	for (t = ctx->titles; t; t = t->next) {
		while (!obj->is_page) {
			assert(obj != end);
			obj++;
		}
		add_object(ctx, n, 0, ctx->pos + tail);
		tail += fprintf(ctx->file,
		    "%u 0 obj\n<<\n"
		    "   /Title (%s)\n"
		    "   /Parent %u 0 R\n",
		    n, t->s, outline);
		if (t != ctx->titles)
			tail += fprintf(ctx->file,
			    "   /Prev %u 0 R\n", n - 1);
		if (t->next)
			tail += fprintf(ctx->file,
			    "   /Next %u 0 R\n", n + 1);
		tail += fprintf(ctx->file,
		    "   /Dest [%u %u R /Fit]\n"
		    ">>\nendobj\n",
		    (unsigned) (obj - ctx->objs), obj->gen);
		n++;
		obj++;
	}

	/* xref table */

	fprintf(ctx->file, "xref\n0 %u\n", n);
	for (obj = ctx->objs; obj != ctx->objs + ctx->top + 1; obj++)
		fprintf(ctx->file,
		    "%010u %05u %c \n",
		    obj->pos, obj->pos ? 0 : 65535, obj->pos ? 'n' : 'f');

	fprintf(ctx->file,
	    "trailer\n"
	    "<< /Size %u\n"
	    "   /Root %u 0 R\n",
	    n, ctx->root);
	if (ctx->info)
		fprintf(ctx->file, "   /Info %u 0 R\n", ctx->info);
	fprintf(ctx->file, ">>\nstartxref\n%u\n%%%%EOF\n", ctx->pos + tail);
}


void pdftoc_end(struct pdftoc *ctx)
{
	struct title *next;

	assert(ctx->state == xref);
	ctx->state = trailer;
	parse_buffer(ctx, 0);
	if (ctx->left) {
		fatal("%u bytes left in buffer at end\n", ctx->left);
		exit(1);
	}

	write_trailer(ctx);

	if (fclose(ctx->file) < 0)
		diag_pfatal("fclose");

	while (ctx->titles) {
		next = ctx->titles->next;
		free(ctx->titles->s);
		free(ctx->titles);
		ctx->titles = next;
	}
	free(ctx->buf);
	free(ctx);
}