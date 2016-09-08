/*
 * main/eeshow.c - Visualize Eeschema schematics
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
#include <locale.h>

//#include <cairo/cairo.h>
#include <gtk/gtk.h>

#include "misc/util.h"
#include "misc/diag.h"
#include "file/file.h"
#include "kicad/ext.h"
#include "kicad/pl.h"	// for suppress_page_layout
#include "gfx/cro.h"	// for disable_overline, use_pango
#include "gui/gui.h"
#include "version.h"
#include "main/common.h"
#include "main.h"


void usage(const char *name)
{
	fprintf(stderr,
"usage: %s [gtk_flags] [-1] [-N n] kicad_file ...\n"
"       %s -V\n"
"       %s gdb ...\n"
"\n"
"  kicad_file  [rev:]file.ext\n"
"    ext       .pro, .lib, .sch, or .kicad_wks\n"
"    rev       git revision\n"
"\n"
"  -1    show only one sheet - do not recurse into sub-sheets\n"
"  -v    increase verbosity of diagnostic output\n"
"  -E shell_command ...\n"
"        execute the specified shell command when the GUI is ready.\n"
"        Sets EESHOW_WINDOW_ID to the X11 window ID.\n"
"  -N n  limit history to n revisions (unlimited if omitted or 0)\n"
"  -P    use Pango to render text (experimental, slow)\n"
"  -V    print revision (version) number and exit\n"
"  gdb   run eeshow under gdb\n"
    , name, name, name);
	exit(1);
}


int main(int argc, char **argv)
{
	bool one_sheet = 0;
	const char **commands = NULL;
	unsigned n_commands = 0;
	int limit = 0;
	char c;
	struct file_names file_names;

	run_under_gdb(argc, argv);

	gtk_init(&argc, &argv);
	setlocale(LC_ALL, "C");	/* restore sanity */

	while ((c = getopt(argc, argv, "1vE:LN:OPV")) != EOF)
		switch (c) {
		case '1':
			one_sheet = 1;
			break;
		case 'v':
			verbose++;
			break;
		case 'E':
			commands = realloc_type_n(commands, const char *,
			    n_commands + 1);
			commands[n_commands++] = optarg;
			break;
		case 'L':
			suppress_page_layout = 1;
			break;
		case 'N':
			limit = atoi(optarg);
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

	if (argc - optind < 1)
		usage(*argv);

	classify_files(&file_names, argv + optind, argc - optind);
	if (!file_names.pro && !file_names.sch)
		fatal("project or top sheet name required");

	return run_gui(&file_names, !one_sheet, limit, commands, n_commands);
}
