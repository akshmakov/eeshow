/*
 * main/eediff.c - Show differences between eeschema schematics
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
#include <stdio.h>
#include <unistd.h>
#include <string.h>

#include <cairo/cairo.h>

#include "misc/util.h"
#include "misc/diag.h"
#include "gfx/diff.h"
#include "gfx/gfx.h"
#include "file/file.h"
#include "kicad/ext.h"
#include "kicad/pl.h"
#include "kicad/lib.h"
#include "kicad/sch.h"
#include "kicad/pro.h"
#include "version.h"
#include "main/common.h"
#include "main.h"


void usage(const char *name)
{
	fprintf(stderr,
"usage: %s [-1] [-e] [-v ...] kicad_file ...\n"
"       %*s-- driver_spec\n"
"       %s -V\n"
"       %s gdb ...\n"
"\n"
"  kicad_file  [rev:]file.ext\n"
"    ext       .pro, .lib, .sch, or .kicad_wks\n"
"    rev       git revision\n"
"\n"
"  -1    show only one sheet - do not recurse into sub-sheets\n"
"  -e    show extra information (e.g., pin types)\n"
"  -v    increase verbosity of diagnostic output\n"
"  -E shell_command ...\n"
"        execute the specified shell command when the GUI is ready.\n"
"        Sets EESHOW_WINDOW_ID to the X11 window ID.\n"
"  -V    print revision (version) number and exit\n"
"  gdb   run eeshow under gdb\n"
"\n"
"Diff driver spec:\n"
"  diff [-o output.png] [-s scale] [file.lib ...] file.sch\n"
"\n"
"  see PNG\n"
    , name, (int) strlen(name) + 1, "", name, name);
	exit(1);
}


#define	OPTIONS	"1evL:OPV"


int main(int argc, char **argv)
{
	struct lib lib;
	struct sch_ctx sch_ctx;
	struct file pro_file, sch_file;
	bool extra = 0;
	bool one_sheet = 0;
	struct pl_ctx *pl = NULL;
	char c;
	int  dashdash;
	unsigned i;
	struct file_names file_names;
	struct file_names *fn = &file_names;
	int gfx_argc;
	char **gfx_argv;
	struct gfx *gfx;
	int retval;

	run_under_gdb(argc, argv);

	for (dashdash = 1; dashdash != argc; dashdash++)
		if (!strcmp(argv[dashdash], "--"))
			break;
	if (dashdash == argc)
		usage(*argv);

	while ((c = getopt(dashdash, argv, OPTIONS)) != EOF)
		switch (c) {
		case '1':
			one_sheet = 1;
			break;
		case 'e':
			extra = 1;
			break;
		case 'v':
			verbose++;
			break;
		case 'L':
			suppress_page_layout = 1;
			break;
		case 'O':
			disable_overline = 1;
			break;
		case 'P':
			use_pango = 1;
			break;
		case 'V':
			fprintf(stderr, "%s %sZ\n", version, build_date);
			return 1;
		default:
			usage(*argv);
		}

	if (dashdash - optind < 1)
		usage(*argv);

	classify_files(&file_names, argv + optind, dashdash - optind);
	if (!file_names.pro && !file_names.sch)
		fatal("project or top sheet name required");

	if (dashdash == argc) {
		gfx_argc = 1;
		gfx_argv = alloc_type_n(char *, 2);
		gfx_argv[0] = "diff";
		gfx_argv[1] = NULL;
	} else {
		gfx_argc = argc - dashdash - 1;
		if (!gfx_argc)
			usage(*argv);
		gfx_argv = alloc_type_n(char *, gfx_argc + 1);
		memcpy(gfx_argv, argv + dashdash + 1,
		    sizeof(const char *) * (gfx_argc + 1));
	}

	if (file_names.pro) {
		if (!file_open(&pro_file, file_names.pro, NULL))
			return 1;
		fn = pro_parse_file(&pro_file, &file_names);
	}

	gfx = gfx_init(&diff_ops);
	if (!gfx_args(gfx, gfx_argc, gfx_argv, OPTIONS))
		return 1;
	if (!gfx_multi_sheet(gfx))
		one_sheet = 1;

	free(gfx_argv);

	sch_init(&sch_ctx, !one_sheet);
	if (!file_open(&sch_file, fn->sch, file_names.pro ? &pro_file : NULL))
		return 1;

	lib_init(&lib);
	for (i = 0; i != fn->n_libs; i++)
		if (!lib_parse(&lib, fn->libs[i],
		    file_names.pro ? &pro_file : &sch_file))
			return 1;

	if (file_names.pro)
		file_close(&pro_file);

	if (fn->pl) {
		struct file file;

		if (!file_open(&file, fn->pl, &sch_file))
			return 1;
		pl = pl_parse(&file);
		file_close(&file);
		if (!pl)
			return 1;
	}

	if (fn != &file_names) {
		free_file_names(fn);
		free(fn);
	}
	free_file_names(&file_names);

	if (!sch_parse(&sch_ctx, &sch_file, &lib, NULL))
		return 1;
	file_close(&sch_file);

	if (one_sheet) {
		sch_render(sch_ctx.sheets, gfx);
		if (extra)
			sch_render_extra(sch_ctx.sheets, gfx);
		if (pl)
			pl_render(pl, gfx, sch_ctx.sheets, sch_ctx.sheets);
	} else {
		const struct sheet *sheet;

		for (sheet = sch_ctx.sheets; sheet; sheet = sheet->next) {
			gfx_sheet_name(gfx, sheet->title);
			sch_render(sheet, gfx);
			if (extra)
				sch_render_extra(sheet, gfx);
			if (pl)
				pl_render(pl, gfx, sch_ctx.sheets, sheet);
			if (sheet->next)
				gfx_new_sheet(gfx);
		}
	}
	retval = gfx_end(gfx);

	sch_free(&sch_ctx);
	lib_free(&lib);
	if (pl)
		pl_free(pl);

	file_cleanup();
	cairo_debug_reset_static_data();

	return retval;
}
