/*
 * gui/gui.c - GUI for eeshow
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
 * Resources:
 *
 * http://zetcode.com/gfx/cairo/cairobackends/
 * https://developer.gnome.org/gtk3/stable/gtk-migrating-2-to-3.html
 */

#define	_GNU_SOURCE	/* for asprintf */
#include <stddef.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <gtk/gtk.h>

#include "version.h"
#include "misc/util.h"
#include "misc/diag.h"
#include "file/git-hist.h"
#include "kicad/pl.h"
#include "kicad/lib.h"
#include "kicad/sch.h"
#include "kicad/delta.h"
#include "gui/aoi.h"
#include "gui/input.h"
#include "gui/common.h"
#include "gui/icons.h"
#include "gui/gui.h"


/* ----- Helper functions -------------------------------------------------- */


struct gui_sheet *find_corresponding_sheet(struct gui_sheet *pick_from,
     struct gui_sheet *ref_in, const struct gui_sheet *ref)
{
	struct gui_sheet *sheet, *plan_b;
	const char *title = ref->sch->title;

	/* plan A: try to find sheet with same name */

	if (title)
		for (sheet = pick_from; sheet; sheet = sheet->next)
			if (sheet->sch->title &&
			    !strcmp(title, sheet->sch->title))
				return sheet;

	/* plan B: use sheet in same position in sheet sequence */

	plan_b = ref_in;
	for (sheet = pick_from; sheet; sheet = sheet->next) {
		if (plan_b == ref)
			return sheet;
		plan_b = plan_b->next;
	}

	/* plan C: just go to the top */
	return pick_from;
}


/* ----- AoIs -------------------------------------------------------------- */


struct sheet_aoi_ctx {
	struct gui_ctx *gui_ctx;
	const struct sch_obj *obj;
};


static void select_subsheet(void *user)
{
	const struct sheet_aoi_ctx *aoi_ctx = user;
	struct gui_ctx *ctx = aoi_ctx->gui_ctx;
	const struct sch_obj *obj = aoi_ctx->obj;
	struct gui_sheet *sheet;

	if (!obj->u.sheet.sheet)
		return;

	if (!ctx->old_hist || ctx->diff_mode != diff_old) {
		for (sheet = ctx->new_hist->sheets; sheet; sheet = sheet->next)
			if (sheet->sch == obj->u.sheet.sheet) {
				go_to_sheet(ctx, sheet);
				return;
			}
		abort();
	}

	for (sheet = ctx->old_hist->sheets; sheet; sheet = sheet->next)
		if (sheet->sch == obj->u.sheet.sheet)
			goto found;
	abort();

found:
	sheet = find_corresponding_sheet(ctx->new_hist->sheets,
                    ctx->old_hist->sheets, sheet);
	go_to_sheet(ctx, sheet);
}


static void add_sheet_aoi(struct gui_ctx *ctx, struct gui_sheet *parent,
    const struct sch_obj *obj)
{
	struct sheet_aoi_ctx *aoi_ctx = alloc_type(struct sheet_aoi_ctx);

	aoi_ctx->gui_ctx = ctx;
	aoi_ctx->obj = obj;

	struct aoi aoi = {
		.x	= obj->x,
		.y	= obj->y,
		.w	= obj->u.sheet.w,
		.h	= obj->u.sheet.h,
		.click	= select_subsheet,
		.user	= aoi_ctx,
	};

	aoi_add(&parent->aois, &aoi);
}


/* ----- Load revisions ---------------------------------------------------- */


void mark_aois(struct gui_ctx *ctx, struct gui_sheet *sheet)
{
	const struct sch_obj *obj;

	sheet->aois = NULL;
	for (obj = sheet->sch->objs; obj; obj = obj->next)
		switch (obj->type) {
		case sch_obj_sheet:
			add_sheet_aoi(ctx, sheet, obj);
			break;
		case sch_obj_glabel:
			add_glabel_aoi(sheet, obj);
		default:
			break;
		}
}


static struct gui_sheet *get_sheets(struct gui_ctx *ctx,
    const struct sheet *sheets)
{
	const struct sheet *sheet;
	struct gui_sheet *gui_sheets = NULL;
	struct gui_sheet **next = &gui_sheets;
	struct gui_sheet *new;

	for (sheet = sheets; sheet; sheet = sheet->next) {
		new = alloc_type(struct gui_sheet);
		new->sch = sheet;
		new->ctx = ctx;
		new->rendered = 0;

		*next = new;
		next = &new->next;
	}
	*next = NULL;
	return gui_sheets;
}


/*
 * Library caching:
 *
 * We reuse previous components if all libraries are identical
 *
 * Future optimizations:
 * - don't parse into single list of components, so that we can share
 *   libraries that are the same, even if there are others that have changed.
 * - maybe put components into tree, so that they can be replaced individually
 *   (this would also help to identify sheets that don't need parsing)
 *
 * Sheet caching:
 *
 * We reuse previous sheets if
 * - all libraries are identical (whether a given sheet uses them or not),
 * - they have no sub-sheets, and
 * - the objects IDs (hashes) are identical.
 *
 * Note that we only compare with the immediately preceding (newer) revision,
 * so branches and merges can disrupt caching.
 *
 * Possible optimizations:
 * - if we record which child sheets a sheet has, we could also clone it,
 *   without having to parse it. However, this is somewhat complex and may
 *   not save all that much time.
 * - we could record what libraries a sheet uses, and parse only if one of
 *   these has changed (benefits scenarios with many library files),
 * - we could record what components a sheet uses, and parse only if one of
 *   these has changed (benefits scenarios with few big libraries),
 * - we could postpone library lookups to render time.
 * - we could record IDs globally, which would help to avoid tripping over
 *   branches and merges.
 */

static const struct sheet *parse_files(struct gui_hist *hist,
    int n_args, char **args, bool recurse, struct gui_hist *prev)
{
	char *rev = NULL;
	struct file sch_file;
	struct file lib_files[n_args - 1];
	int libs_open, i;
	bool libs_cached = 0;
	bool ok;

	if (hist->vcs_hist && hist->vcs_hist->commit)
		rev = vcs_git_get_rev(hist->vcs_hist);

	sch_init(&hist->sch_ctx, recurse);
	ok = file_open_revision(&sch_file, rev, args[n_args - 1], NULL);

	if (rev)
		free(rev);
	if (!ok) {
		sch_free(&hist->sch_ctx);
		return NULL;
	}

	lib_init(&hist->lib);
	for (libs_open = 0; libs_open != n_args - 1; libs_open++)
		if (!file_open(lib_files + libs_open, args[libs_open],
		    &sch_file))
			goto fail;

	if (hist->vcs_hist) {
		hist->oids = alloc_type_n(void *, libs_open);
		hist->libs_open = libs_open;
		for (i = 0; i != libs_open; i++)
			hist->oids[i] = file_oid(lib_files + i);
		if (prev && prev->vcs_hist && prev->libs_open == libs_open) {
			for (i = 0; i != libs_open; i++)
				if (!file_oid_eq(hist->oids[i], prev->oids[i]))
					break;
			if (i == libs_open) {
				hist->lib.comps = prev->lib.comps;
				libs_cached = 1;
			}
		}
	}

	if (!libs_cached)
		for (i = 0; i != libs_open; i++)
			if (!lib_parse_file(&hist->lib, lib_files +i))
				goto fail;

	if (!sch_parse(&hist->sch_ctx, &sch_file, &hist->lib,
	    libs_cached ? &prev->sch_ctx : NULL))
		goto fail;

	for (i = 0; i != libs_open; i++)
		file_close(lib_files + i);
	file_close(&sch_file);

	if (prev && sheet_eq(prev->sch_ctx.sheets, hist->sch_ctx.sheets))
		prev->identical = 1;

	/*
	 * @@@ we have a major memory leak for the component library.
	 * We should record parsed schematics and libraries separately, so
	 * that we can clean them up, without having to rely on the history,
	 * with - when sharing unchanged item - possibly many duplicate
	 * pointers.
	 */
	return hist->sch_ctx.sheets;

fail:
	while (libs_open--)
		file_close(lib_files + libs_open);
	sch_free(&hist->sch_ctx);
	lib_free(&hist->lib);
	file_close(&sch_file);
	return NULL;
}


struct add_hist_ctx {
	struct gui_ctx *ctx;
	int n_args;
	char **args;
	bool recurse;
	unsigned limit;
};


static void add_hist(void *user, struct hist *h)
{
	struct add_hist_ctx *ahc = user;
	struct gui_ctx *ctx = ahc->ctx;
	struct gui_hist **anchor, *hist, *prev;
	const struct sheet *sch;
	unsigned age = 0;

	if (!ahc->limit)
		return;
	if (ahc->limit > 0)
		ahc->limit--;

	prev = NULL;
	for (anchor = &ctx->hist; *anchor; anchor = &(*anchor)->next) {
		prev = *anchor;
		age++;
	}

	hist = alloc_type(struct gui_hist);
	hist->ctx = ctx;
	hist->vcs_hist = h;
	hist->libs_open = 0;
	hist->identical = 0;
	sch = parse_files(hist, ahc->n_args, ahc->args, ahc->recurse, prev);
	hist->sheets = sch ? get_sheets(ctx, sch) : NULL;
	hist->age = age;

	hist->next = NULL;
	*anchor = hist;

	if (ctx->hist_size)
		progress_update(ctx);
}


static void get_revisions(struct gui_ctx *ctx,
    int n_args, char **args, bool recurse, int limit)
{
	struct add_hist_ctx add_hist_ctx = {
		.ctx		= ctx,
		.n_args		= n_args,
		.args		= args,
		.recurse	= recurse,
		.limit		= limit ? limit < 0 ? -limit : limit : -1,
	};

	if (ctx->vcs_hist)
		hist_iterate(ctx->vcs_hist, add_hist, &add_hist_ctx);
	else
		add_hist(&add_hist_ctx, NULL);
}


/* ----- Retrieve and count history ---------------------------------------- */


static void count_history(void *user, struct hist *h)
{
	struct gui_ctx *ctx = user;

	ctx->hist_size++;
}


static void get_history(struct gui_ctx *ctx, const char *sch_name, int limit)
{
	if (!vcs_git_try(sch_name)) {
		ctx->vcs_hist = NULL;
		return;
	} 
	
	ctx->vcs_hist = vcs_git_hist(sch_name);
	if (limit)
		ctx->hist_size = limit > 0 ? limit : -limit;
	else
		hist_iterate(ctx->vcs_hist, count_history, ctx);
}


/* ----- Initialization ---------------------------------------------------- */


int gui(unsigned n_args, char **args, bool recurse, int limit,
    struct pl_ctx *pl)
{
	GtkWidget *window;
	char *title;
	struct gui_ctx ctx = {
		.zoom		= 4,	/* scale by 1 / 16 */
		.pl		= pl, // @@@
		.hist		= NULL,
		.vcs_hist	= NULL,
		.showing_history= 0,
		.sheet_overlays	= NULL,
		.hist_overlays	= NULL,
		.pop_overlays	= NULL,
		.pop_underlays	= NULL,
		.pop_origin	= NULL,
		.glabel		= NULL,
		.aois		= NULL,
		.diff_mode	= diff_delta,
		.old_hist	= NULL,
		.hist_y_offset 	= 0,
		.hist_size	= 0,
	};

	window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	ctx.da = gtk_drawing_area_new();
	gtk_container_add(GTK_CONTAINER(window), ctx.da);

	gtk_window_set_default_size(GTK_WINDOW(window), 640, 480);
	if (asprintf(&title, "eeshow (rev %s)", version)) {};
	gtk_window_set_title(GTK_WINDOW(window), title);

	gtk_widget_set_events(ctx.da,
	    GDK_EXPOSE | GDK_ENTER_NOTIFY_MASK | GDK_LEAVE_NOTIFY_MASK);

	input_setup(ctx.da);

	gtk_widget_show_all(window);

	get_history(&ctx, args[n_args - 1], limit);
	if (ctx.hist_size)
		setup_progress_bar(&ctx, window);

	get_revisions(&ctx, n_args, args, recurse, limit);
	for (ctx.new_hist = ctx.hist; ctx.new_hist && !ctx.new_hist->sheets;
	    ctx.new_hist = ctx.new_hist->next);
	if (!ctx.new_hist)
		fatal("no valid sheets\n");

	g_signal_connect(window, "destroy",
	    G_CALLBACK(gtk_main_quit), NULL);

	icons_init();
	sheet_setup(&ctx);
	render_setup(&ctx);

//	gtk_window_set_position(GTK_WINDOW(window), GTK_WIN_POS_CENTER);

	go_to_sheet(&ctx, ctx.new_hist->sheets);
	gtk_widget_show_all(window);

	/* for performance testing, use -N-depth */
	if (limit >= 0)
		gtk_main();

	return 0;
}
