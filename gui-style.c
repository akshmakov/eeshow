/*
 * gui-style.c - GUI: overlay styles
 *
 * Written 2016 by Werner Almesberger
 * Copyright 2016 by Werner Almesberger
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include "gui-style.h"


#define	OVER_BORDER	8
#define	OVER_RADIUS	6
#define	OVER_SEP	8


#define	NORMAL_FONT	"Helvetica 10"
#define	BOLD_FONT	"Helvetica Bold 10"

#define	NORMAL_PAD	8
#define	NORMAL_RADIUS	6
#define	NORMAL_SKIP	8
#define	NORMAL_WIDTH	2

#define	DENSE_PAD	4
#define	DENSE_RADIUS	3
#define	DENSE_SKIP	5
#define	DENSE_WIDTH	1

#define	BG_STANDARD		{ 0.8, 0.9, 1.0, 0.8 }
#define	FG_STANDARD		{ 0.0, 0.0, 0.0, 1.0 }
#define	FRAME_STANDARD		{ 0.5, 0.5, 1.0, 0.7 }

#define	BG_SELECTED		BG_STANDARD
#define	FG_SELECTED		FG_STANDARD
#define	FRAME_SELECTED		{ 0.0, 0.0, 1.0, 0.8 }

#define	BG_DIFF_NEW		BG_STANDARD
#define	FG_DIFF_NEW		{ 0.0, 0.6, 0.0, 1.0 }
#define	FRAME_DIFF_NEW		FRAME_STANDARD

#define	BG_DIFF_NEW_SELECTED	BG_DIFF_NEW
#define	FG_DIFF_NEW_SELECTED	FG_DIFF_NEW
#define	FRAME_DIFF_NEW_SELECTED	FRAME_SELECTED

#define	BG_DIFF_OLD		BG_STANDARD
#define	FG_DIFF_OLD		{ 0.8, 0.0, 0.0, 1.0 }
#define	FRAME_DIFF_OLD		FRAME_STANDARD

#define	BG_DIFF_OLD_SELECTED	BG_DIFF_OLD
#define	FG_DIFF_OLD_SELECTED	FG_DIFF_OLD
#define	FRAME_DIFF_OLD_SELECTED	FRAME_SELECTED


#define	BOX_ATTRS(style)		\
	.pad	= style##_PAD,		\
	.radius	= style##_RADIUS,	\
	.skip	= style##_SKIP,		\
	.width	= style##_WIDTH

#define	NORMAL	BOX_ATTRS(NORMAL)
#define	DENSE	BOX_ATTRS(DENSE)

#define	COLOR_ATTRS(style)		\
	.bg	= BG_##style,		\
	.fg	= FG_##style,		\
	.frame	= FRAME_##style

#define	STANDARD		COLOR_ATTRS(STANDARD)
#define	SELECTED		COLOR_ATTRS(SELECTED)
#define	DIFF_NEW		COLOR_ATTRS(DIFF_NEW)
#define	DIFF_NEW_SELECTED	COLOR_ATTRS(DIFF_NEW_SELECTED)
#define	DIFF_OLD		COLOR_ATTRS(DIFF_OLD)
#define	DIFF_OLD_SELECTED	COLOR_ATTRS(DIFF_OLD_SELECTED)


struct overlay_style overlay_style_default = {
	.font	= NORMAL_FONT,
	NORMAL,
	STANDARD,
}, overlay_style_dense = {
	.font	= NORMAL_FONT,
	DENSE,
	STANDARD,
}, overlay_style_dense_selected = {
	.font	= BOLD_FONT,
	DENSE,
	SELECTED,
}, overlay_style_diff_new = {
	.font	= NORMAL_FONT,
	NORMAL,
	DIFF_NEW,
}, overlay_style_diff_old = {
	.font	= NORMAL_FONT,
	NORMAL,
	DIFF_OLD,
}, overlay_style_dense_diff_new = {
	.font	= BOLD_FONT,
	DENSE,
	DIFF_NEW_SELECTED,
}, overlay_style_dense_diff_old = {
	.font	= BOLD_FONT,
	DENSE,
	DIFF_OLD_SELECTED,
};
