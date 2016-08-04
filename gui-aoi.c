/*
 * gui-aoi.c - GUI: areas of interest
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

#include <stddef.h>

#include "util.h"
#include "gui-aoi.h"


static const struct aoi *hovering = NULL;


void aoi_add(struct aoi **aois, const struct aoi *aoi)
{
	struct aoi *new;

	new = alloc_type(struct aoi);
	*new = *aoi;
	new->next = *aois;
	*aois = new;
}


bool aoi_hover(const struct aoi *aois, int x, int y)
{
	const struct aoi *aoi;

	if (hovering) {
		if (x >= hovering->x && x < hovering->x + hovering->w &&
		    y >= hovering->y && y < hovering->y + hovering->h)
			return 1;
		hovering->hover(hovering->user, 0);
		hovering = NULL;
	}

	for (aoi = aois; aoi; aoi = aoi->next)
		if (x >= aoi->x && x < aoi->x + aoi->w &&
		    y >= aoi->y && y < aoi->y + aoi->h)
			break;
	if (aoi && aoi->hover && aoi->hover(aoi->user, 1)) {
		hovering = aoi;
		return 1;
	}
	return 0;
}


bool aoi_click(const struct aoi *aois, int x, int y)
{
	const struct aoi *aoi;

	if (hovering) {
		hovering->hover(hovering->user, 0);
		hovering = NULL;
	}
	for (aoi = aois; aoi; aoi = aoi->next)
		if (x >= aoi->x && x < aoi->x + aoi->w &&
		    y >= aoi->y && y < aoi->y + aoi->h)
			break;
	if (aoi && aoi->click) {
		aoi->click(aoi->user);
		return 1;
	}
	return 0;
}
