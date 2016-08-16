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
#include <math.h>
#include <assert.h>

#include "util.h"
#include "gui-aoi.h"


#define	DRAG_RADIUS	5


static const struct aoi *hovering = NULL;
static const struct aoi *clicked = NULL;
static const struct aoi *dragging = NULL;
static int clicked_x, clicked_y;


struct aoi *aoi_add(struct aoi **aois, const struct aoi *cfg)
{
	struct aoi *new;

	new = alloc_type(struct aoi);
	*new = *cfg;
	new->next = *aois;
	*aois = new;

	return new;
}


void aoi_update(struct aoi *aoi, const struct aoi *cfg)
{
	struct aoi *next = aoi->next;

	*aoi = *cfg;
	aoi->next = next;
}


static const struct aoi *find_aoi(const struct aoi *aois, int x, int y)
{
	const struct aoi *aoi;

	for (aoi = aois; aoi; aoi = aoi->next)
		if (x >= aoi->x && x < aoi->x + aoi->w &&
		    y >= aoi->y && y < aoi->y + aoi->h)
			break;
	return aoi;
}


static bool aoi_on_list(const struct aoi *aois, const struct aoi *ref)
{
	const struct aoi *aoi;
	
	for (aoi = aois; aoi; aoi = aoi->next)
		if (aoi == ref)
			return 1;
	return 0;
}


bool aoi_hover(const struct aoi *aois, int x, int y)
{
	const struct aoi *aoi;

	if (dragging)
		return 0;
	if (hovering) {
		if (x >= hovering->x && x < hovering->x + hovering->w &&
		    y >= hovering->y && y < hovering->y + hovering->h)
			return 1;
		hovering->hover(hovering->user, 0);
		hovering = NULL;
	}

	aoi = find_aoi(aois, x, y);
	if (aoi && aoi->hover && aoi->hover(aoi->user, 1)) {
		hovering = aoi;
		return 1;
	}
	return 0;
}


bool aoi_move(const struct aoi *aois, int x, int y)
{
	if (dragging) {
		if (aoi_on_list(aois, dragging)) {
			dragging->drag(dragging->user,
			    x - clicked_x, y - clicked_y);
			clicked_x = x;
			clicked_y = y;
		}
		return 1;
	}
	if (!clicked)
		return 0;

	/*
	 * Ensure we're on the right list and are using the same coordinate
	 * system.
	 */
	if (!aoi_on_list(aois, clicked))
		return 0;

	if (hypot(x - clicked_x, y - clicked_y) > DRAG_RADIUS) {
		if (clicked && clicked->drag) {
			dragging = clicked;
			dragging->drag(dragging->user,
			    x - clicked_x, y - clicked_y);
			clicked_x = x;
			clicked_y = y;
		}
		clicked = NULL;
	}
	return 1;
}


bool aoi_down(const struct aoi *aois, int x, int y)
{
	assert(!clicked);

	aoi_dehover();

	clicked = find_aoi(aois, x, y);
	if (!clicked)
		return 0;
	if (!clicked->click) {
		clicked = NULL;
		return 0;
	}

	clicked_x = x;
	clicked_y = y;

	return 1;
}


bool aoi_up(const struct aoi *aois, int x, int y)
{
	const struct aoi *aoi;

	if (!aoi_move(aois, x, y))
		return 0;

	/*
	 * Ensure we're on the right list and are using the same coordinate
	 * system.
	 */
	for (aoi = aois; aoi; aoi = aoi->next)
		if (aoi == clicked || aoi == dragging)
			break;
	if (!aoi)
		return 0;
	if (dragging) {
		dragging = NULL;
		return 1;
	}

	clicked->click(clicked->user);
	clicked = NULL;
	return 1;
}


void aoi_remove(struct aoi **aois, const struct aoi *aoi)
{
	if (hovering == aoi) {
		aoi->hover(aoi->user, 0);
		hovering = NULL;
	}
	while (*aois != aoi)
		aois = &(*aois)->next;
	*aois = aoi->next;
	free((void *) aoi);
}


void aoi_dehover(void)
{
	if (hovering)
		hovering->hover(hovering->user, 0);
	hovering = NULL;
}

