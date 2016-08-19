/*
 * main.c - Convert Eeschema schematics to FIG
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

#include <gtk/gtk.h>

#include "misc/util.h"
#include "misc/diag.h"
#include "gfx/fig.h"
#include "gfx/cro.h"
#include "gfx/diff.h"
#include "gfx/gfx.h"
#include "file/file.h"
#include "kicad/lib.h"
#include "kicad/sch.h"
#include "gui/fmt-pango.h"
#include "file/git-hist.h"
#include "gui/gui.h"
#include "version.h"
#include "main.h"


static struct gfx_ops const *ops_list[] = {
	&fig_ops,
	&cro_png_ops,
	&cro_pdf_ops,
	&diff_ops,
};


void usage(const char *name)
{
	fprintf(stderr,
"usage: %s [gtk_flags] [-r] [-N n] [[rev:]file.lib ...] [rev:]file.sch\n"
"       %s [-r] [-v ...] [[rev:]file.lib ...] [rev:]file.sch\n"
"       %*s[-- driver_spec]\n"
"       %s [-v ...] -C [rev:]file\n"
"       %s [-v ...] -H path_into_repo\n"
"       %s -V\n"
"       %s gdb ...\n"
"\n"
"  rev   git revision\n"
"  -r    recurse into sub-sheets\n"
"  -v    increase verbosity of diagnostic output\n"
"  -C    'cat' the file to standard output\n"
"  -H    show history of repository on standard output\n"
"  -N n  limit history to n revisions (unlimited if omitted or 0)\n"
"  -V    print revision (version) number and exit\n"
"  gdb   run eeshow under gdb\n"
"\n"
"No driver spec: enter GUI\n"
"\n"
"FIG driver spec:\n"
"  fig [-t template.fig] [var=value ...]\n"
"\n"
"  var=value        substitute \"<var>\" with \"value\" in template\n"
"  -t template.fig  merge this file with generated output\n"
"\n"
"Cairo PNG driver spec:\n"
"  png [-o output.png] [-s scale]\n"
"\n"
"  -o output.png  write PNG to specified file (default; standard output)\n"
"  -s scale       scale by indicated factor (default: 1.0)\n"
"\n"
"Cairo PDF driver spec:\n"
"  pdf [-o output.pdf] [-s scale]\n"
"\n"
"  see PNG\n"
"\n"
"Diff driver spec:\n"
"  diff [-o output.pdf] [-s scale] [file.lib ...] file.sch\n"
"\n"
"  see PNG\n"
    , name, name, (int) strlen(name) + 1, "", name, name, name, name);
	exit(1);
}


int main(int argc, char **argv)
{
	struct lib lib;
	struct sch_ctx sch_ctx;
	struct file sch_file;
	bool recurse = 0;
	const char *cat = NULL;
	const char *history = NULL;
	const char *fmt = NULL;
	int limit = 0;
	char c;
	int arg, dashdash;
	bool have_dashdash = 0;
	int gfx_argc;
	char **gfx_argv;
	const struct gfx_ops **ops = ops_list;

	if (argc > 1 && !strcmp(argv[1], "gdb")) {
		char **args;

		args = alloc_type_n(char *, argc + 2);
		args[0] = "gdb";
		args[1] = "--args";
		args[2] = *argv;
		memcpy(args + 3, argv + 2, sizeof(char *) * (argc - 1));
		execvp("gdb", args);
		perror(*argv);
		return 1;
	}

	for (dashdash = 1; dashdash != argc; dashdash++)
		if (!strcmp(argv[dashdash], "--")) {
			have_dashdash = 1;
			break;
		}

	if (!have_dashdash)
		gtk_init(&argc, &argv);

	while ((c = getopt(dashdash, argv, "rvC:F:H:N:V")) != EOF)
		switch (c) {
		case 'r':
			recurse = 1;
			break;
		case 'v':
			verbose++;
			break;
		case 'C':
			cat = optarg;
			break;
		case 'F':
			fmt = optarg;
			break;
		case 'H':
			history = optarg;
			break;
		case 'N':
			limit = atoi(optarg);
			break;
		case 'V':
			fprintf(stderr, "%s\n", version);
			return 1;
		default:
			usage(*argv);
		}

	if (cat) {
		struct file file;

		if (argc != optind)
			usage(*argv);
		if (!file_open(&file, cat, NULL))
			return 1;
		if (!file_read(&file, file_cat, NULL))
			return 1;
		file_close(&file);
		return 0;
	}

	if (history) {
		struct hist *h;

		h = vcs_git_hist(history);
		dump_hist(h);
		return 0;
	}

	if (fmt) {
		char *buf;

		buf = fmt_pango(fmt, argv[optind]);
		printf("\"%s\"\n", buf);
		return 0;
	}

	if (dashdash - optind < 1)
		usage(*argv);

	if (!have_dashdash) {
		unsigned n = argc - optind;
		char **args;

		args = alloc_type_n(char *, n);
		memcpy(args, argv + optind, sizeof(const char *) * n);
	
		optind = 0; /* reset getopt */
		return gui(n, args, recurse, limit);
	}

	sch_init(&sch_ctx, recurse);
	if (!file_open(&sch_file, argv[dashdash - 1], NULL))
		return 1;

	lib_init(&lib);
	for (arg = optind; arg != dashdash - 1; arg++)
		if (!lib_parse(&lib, argv[arg], &sch_file))
			return 1;

	if (dashdash == argc) {
		gfx_argc = 1;
		gfx_argv = alloc_type_n(char *, 2);
		gfx_argv[0] = (char *) (*ops)->name;
		gfx_argv[1] = NULL;
	} else {
		gfx_argc = argc - dashdash - 1;
		if (!gfx_argc)
			usage(*argv);
		gfx_argv = alloc_type_n(char *, gfx_argc + 1);
		memcpy(gfx_argv, argv + dashdash + 1,
		    sizeof(const char *) * (gfx_argc + 1));

		for (ops = ops_list; ops != ARRAY_END(ops_list); ops++)
			if (!strcmp((*ops)->name, *gfx_argv))
				goto found;
		fatal("graphics backend \"%s\" not found\n", *gfx_argv);
found:
		;
	}

	optind = 0; /* reset getopt */

	if (!sch_parse(&sch_ctx, &sch_file, &lib, NULL))
		return 1;
	file_close(&sch_file);

	gfx_init(*ops, gfx_argc, gfx_argv);
	if (recurse) {
		const struct sheet *sheet;

		if (!gfx_multi_sheet())
			fatal("graphics backend only supports single sheet\n");
		for (sheet = sch_ctx.sheets; sheet; sheet = sheet->next) {
			sch_render(sheet);
			if (sheet->next)
				gfx_new_sheet();
		}
	} else {
		sch_render(sch_ctx.sheets);
	}
	gfx_end();

	sch_free(&sch_ctx);
	lib_free(&lib);

	return 0;
}
