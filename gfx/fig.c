/*
 * gfx/fig.c - Generate FIG output for Eeschema items
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
#include <math.h>
#include <assert.h>

#include "misc/util.h"
#include "misc/diag.h"
#include "gfx/style.h"
#include "gfx/text.h"
#include "main.h"
#include "gfx/fig.h"


/*
 * FIG works with 1/1200 in
 * KiCad works with mil
 * 1 point = 1/72 in
 */


static inline int cx(int x)
{
	return x * 1200 / 1000;
}


static inline int cy(int y)
{
	return y * 1200 / 1000;
}


static inline float pt(int x)
{
	return cx(x) * 72 * 1.5 / 1200.0;
}


/* ----- Schematics items -------------------------------------------------- */


static void fig_line(void *ctx, int sx, int sy, int ex, int ey,
    int color, unsigned layer)
{
	//	TypeStyle   FillCol AreaFil  Cap  FwdAr
	//	  SubTy  Color   Pen   StyleV  Rad  BwdAr
	//	      Thick  Depth        Join       Points
	printf("2 1 2 %d %d 7 %d -1 -1 3.0 1 1 -1 0 0 2\n",
	    WIDTH_LINE, color, layer);
	printf("\t%d %d %d %d\n", cx(sx), cy(sy), cx(ex), cy(ey));
}


/* ----- General items ----------------------------------------------------- */


static void fig_rect(void *ctx, int sx, int sy, int ex, int ey,
    int color, int fill_color, unsigned layer)
{
	//	Type  Thick    Depth    StyleV  Rad
	//	  SubTy  Color    Pen       Join   FwdAr
	//	    Style   FillCol  AreaFil  Cap    BwdAr
	printf("2 2 0 %d %d %d %d -1 %d 0.0 1 1 -1 0 0 5\n",
	    color == -1 ? 0 : WIDTH_COMP_DWG, color, fill_color, layer,
	    fill_color == -1 ? -1 : 20);
	printf("\t%d %d %d %d %d %d %d %d %d %d\n",
	    cx(sx), cy(sy), cx(ex), cy(sy), cx(ex), cy(ey), cx(sx), cy(ey),
	    cx(sx), cy(sy));
}


static void fig_poly(void *ctx,
    int points, const int x[points], const int y[points],
    int color, int fill_color, unsigned layer)
{
	int i;
	char ch = '\t';

	//	Type  Thick     Depth    StyleV  Rad
	//	  SubTy  Color     Pen       Join   FwdAr
	//	    Style   FillCol   AreaFil  Cap    BwdAr
	printf("2 1 0 %d %d %d  %d -1 %d 0.0 1 1 -1 0 0 %d\n",
	    color == -1 ? 0 : WIDTH_COMP_DWG, color, fill_color, layer,
	    fill_color == -1 ? -1 : 20, points);
	for (i = 0; i != points; i++) {
		printf("%c%d %d", ch, cx(x[i]), cy(y[i]));
		ch = ' ';
	}
	printf("\n");
}


static void fig_circ(void *ctx, int x, int y, int r,
    int color, int fill_color, unsigned layer)
{
	//	Type  Thick    Depth   StyleV    Cx    Rx    Sx    Ex
	//	  SubTy  Color    Pen       Dir      Cy    Ry    Sy    Ey
	//	    Style   FillCol  AreaFil  Angle
	printf("1 3 0 %d %d %d %d -1 %d 0.0 1 0.0 %d %d %d %d %d %d %d %d\n",
	    color == -1 ? 0 : WIDTH_COMP_DWG, color, fill_color, layer,
	    fill_color == -1 ? -1 : 20,
	    cx(x), cy(y), r, r,
	    cx(x), cy(y), cx(x) + r, cy(y));
}


static int ax(int x, int y, int r, int angle)
{
	float a = angle / 180.0 * M_PI;

	return cx(x + r * cos(a));
}


static int ay(int x, int y, int r, int angle)
{
	float a = angle / 180.0 * M_PI;

	return cy(y - r * sin(a));
}


static void fig_arc(void *ctx, int x, int y, int r, int sa, int ea,
    int color, int fill_color, unsigned layer)
{
	int ma = (sa + ea) / 2;

	//	Type  Thick    Depth   StyleV   FwdAr
	//	  SubTy  Color    Pen       Cap   BwdAr
	//	    Style   FillCol  AreaFil  Dir   points
	printf("5 1 0 %d %d %d %d -1 %d 0.0 1 1 0 0 %d %d %d %d %d %d %d %d\n",
	    color == -1 ? 0 : WIDTH_COMP_DWG, color, fill_color, layer,
	    fill_color == -1 ? -1 : 20,
	    cx(x), cy(y),
	    ax(x, y, r, sa), ay(x, y, r, sa),
	    ax(x, y, r, ma), ay(x, y, r, ma),
	    ax(x, y, r, ea), ay(x, y, r, ea));
}


static void fig_tag(void *ctx, const char *s,
    int points, const int x[points], const int y[points])
{
	printf("# href=\"%s\" alt=\"\"\n", s);
	fig_poly(ctx, points, x, y, COLOR_NONE, COLOR_NONE, 999);
}


static void fig_text(void *ctx, int x, int y, const char *s, unsigned size,
    enum text_align align, int rot, enum text_style style,
    unsigned color, unsigned layer)
{
	int font;

	switch (style) {
	case text_italic:
		font = FONT_HELVETICA_OBLIQUE;
		break;
	case text_bold:
		font = FONT_HELVETICA_BOLD;
		break;
	case text_bold_italic:
		font = FONT_HELVETICA_BOLDOB;
		break;
	default:
		font = FONT_HELVETICA;
		break;
	}

	//	Type   Depth     FontSiz Height
	//	  Just    Pen       Angle    Length
	//	    Color     Font     Flags     X  Y
	printf("4 %u %d %d -1 %d %f %f 4 0.0 0.0 %d %d %s\\001\n",
	    align, color, layer, font,
	    pt(size), rot / 180.0 * M_PI, cx(x), cy(y), s);
}


static unsigned fig_text_width(void *ctx, const char *s, unsigned size,
    enum text_style style)
{
	/*
	 * Note that we stretch the text size, so the ratio is larger than
	 * expressed here.
	 */
	return strlen(s) * size * 1.0;
}


/* ----- FIG file header --------------------------------------------------- */


static void fig_header(void)
{
	printf("#FIG 3.2\n");
	printf("Landscape\n");
	printf("Center\n");
	printf("Metric\n");
	printf("A4\n");
	printf("100.00\n");
	printf("Single\n");
	printf("-2\n");
	printf("1200 2\n");
}


static void fig_colors(void)
{
	unsigned i;

	for (i = 32; i != n_color_rgb; i++)
		printf("0 %d #%06x\n", i, color_rgb[i]);

}


static bool apply_vars(char *buf, int n_vars, const char **vars)
{
	char *p;
	const char **var, *eq;
	int var_len, value_len;

	p = strchr(buf, '<');
	if (!p)
		return 0;
	for (var = vars; var != vars + n_vars; var++) {
		eq = strchr(*var, '=');
		assert(eq);
		var_len = eq - *var;
		if (strncmp(p + 1, *var, var_len))
			continue;
		value_len = strlen(eq + 1);
		memmove(p + value_len, p + var_len + 2,
		    strlen(p + var_len + 2) + 1);
		memcpy(p, eq + 1, value_len);
		return 1;
	}
	return 0;
}



static void *fig_init(void)
{
	/* @@@ this is asking for trouble ... */
	return NULL;
}


static bool fig_args(void *ctx, int argc, char *const *argv)
{
	static char *buf = NULL;
	static size_t n = 0;
	const char *template = NULL;
	const char **vars = NULL;
	int n_vars = 0;
	char c;
	int arg;
	FILE *file;
	int lines_to_colors = 8;

	while ((c = getopt(argc, argv, "t:")) != EOF)
		switch (c) {
		case 't':
			template = optarg;
			break;
		default:
			usage(*argv);
		}

	for (arg = optind; arg != argc; arg++) {
		if (!strchr(argv[arg], '='))
		    usage(*argv);
		n_vars++;
		vars = realloc_type_n(vars, const char *, n_vars);
		vars[n_vars - 1] = argv[arg];
	}

	if (!template) {
		fig_header();
		fig_colors();
		return 1;
	}

	file = fopen(template, "r");
	if (!file)
		diag_pfatal(template);
	while (getline(&buf, &n, file) > 0) {
		while (apply_vars(buf, n_vars, vars));
		printf("%s", buf);
		if (*buf == '#')
			continue;
		if (!--lines_to_colors)
			fig_colors();
		/*
		 * @@@ known bug: if the template is empty, we won't output
		 * color 32.
		 */
	}
	fclose(file);

	return 1;
}


/* ----- Operations -------------------------------------------------------- */


const struct gfx_ops fig_ops = {
	.name		= "fig",
	.line		= fig_line,
	.rect		= fig_rect,
	.poly		= fig_poly,
	.circ		= fig_circ,
	.arc		= fig_arc,
	.text		= fig_text,
	.tag		= fig_tag,
	.text_width	= fig_text_width,
	.init		= fig_init,
	.args		= fig_args,
};
