/*
 * gui/common.h - Common data structures and declarations
 *
 * Written 2016 by Werner Almesberger
 * Copyright 2016 by Werner Almesberger
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#ifndef GUI_COMMON_H
#define	GUI_COMMON_H

#include <stdbool.h>

#include <gtk/gtk.h>

#include "gfx/cro.h"
#include "file/git-hist.h"
#include "kicad/lib.h"
#include "kicad/sch.h"
#include "gui/aoi.h"
#include "gui/over.h"


struct gui_ctx;
struct gui_hist;

struct gui_sheet {
	const struct sheet *sch;
	struct gui_ctx *ctx;	/* back link */
	struct gui_hist *hist;	/* back link */
	struct cro_ctx *gfx_ctx;

	int w, h;		/* in eeschema coordinates */
	int xmin, ymin;

	bool rendered;		/* 0 if we still have to render it */

	struct overlay *over;	/* current overlay */
	struct aoi *aois;	/* areas of interest; in schematics coord  */

	struct gui_sheet *next;
};

struct gui_hist {
	struct gui_ctx *ctx;	/* back link */
	struct hist *vcs_hist;	/* NULL if not from repo */
	struct overlay *over;	/* current overlay */
	struct gui_sheet *sheets; /* NULL if failed */
	unsigned age;		/* 0-based; uncommitted or HEAD = 0 */

	struct pl_ctx *pl;	/* NULL if none or failed */

	/* caching support */
	void **oids;		/* file object IDs */
	unsigned libs_open;
	struct sch_ctx sch_ctx;
	struct lib lib;		/* combined library */
	bool identical;		/* identical with previous entry */

	struct gui_hist *next;
};

struct gui_ctx {
	GtkWidget *da;

	float scale;		/* pixels = eeschema * scale */
	int x, y;		/* center, in eeschema coordinates */

	struct gui_hist *hist;	/* revision history; NULL if none */
	struct hist *vcs_hist;	/* underlying VCS data; NULL if none */

	bool showing_history;
	enum selecting {
		sel_only,	/* select the only revision we show */
		sel_split,	/* select revision to compare with */
		sel_new,	/* select the new revision */
		sel_old,	/* select the old revision */
	} selecting;

	struct overlay *sheet_overlays;
	struct overlay *hist_overlays;

	struct overlay *pop_overlays; /* pop-up dialogs */
	struct overlay *pop_underlays;
	int pop_x, pop_y;
	int pop_dx, pop_dy;
	const void *pop_origin;	/* item that created this pop-up */
	const char *glabel;	/* currenly highlighted glabel, or NULL */

	struct aoi *aois;	/* areas of interest; in canvas coord  */

	struct gui_sheet delta_a;
	struct gui_sheet delta_b;
	struct gui_sheet delta_ab;

	struct gui_sheet *curr_sheet;
				/* current sheet, always on new_hist */

	enum diff_mode {
		diff_new,	/* show only new sheet */
		diff_old,	/* show only old sheet */
		diff_delta,	/* show delta */
	} diff_mode;
	struct gui_hist *new_hist;
	struct gui_hist *old_hist; /* NULL if not comparing */

	int hist_y_offset;	/* history list y offset */

	/* progress bar */
	int hist_size;		/* total number of revisions */
	unsigned progress;	/* progress counter */
	unsigned progress_scale;/* right-shift by this value */
};


#define	GLABEL_STACK_PADDING	5


/* progress.c */

void setup_progress_bar(struct gui_ctx *ctx, GtkWidget *window);
void progress_update(struct gui_ctx *ctx);

/* render.c */

extern bool use_delta;
extern bool show_extra;


void redraw(const struct gui_ctx *ctx);
void render_sheet(struct gui_sheet *sheet);
void render_delta(struct gui_ctx *ctx);
void render_setup(struct gui_ctx *ctx);

/* glabel.c */

void dehover_glabel(struct gui_ctx *ctx);
void add_glabel_aoi(struct gui_sheet *sheet, const struct sch_obj *obj);

/* sheet.c */

void go_to_sheet(struct gui_ctx *ctx, struct gui_sheet *sheet);
void do_revision_overlays(struct gui_ctx *ctx);
void sheet_setup(struct gui_ctx *ctx);

/* history.c */

void show_history(struct gui_ctx *ctx, enum selecting sel);

/* gui.c */

void unrender_all(struct gui_ctx *ctx);

struct gui_sheet *find_corresponding_sheet(struct gui_sheet *pick_from,
     struct gui_sheet *ref_in, const struct gui_sheet *ref);
void mark_aois(struct gui_ctx *ctx, struct gui_sheet *sheet);

#endif /* !GUI_COMMON_H */
