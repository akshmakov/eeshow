/*
 * gui/aoi.h - GUI: areas of interest
 *
 * Written 2016 by Werner Almesberger
 * Copyright 2016 by Werner Almesberger
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#ifndef GUI_AOI_H
#define	GUI_AOI_H

#include <stdbool.h>


struct aoi {
	int x, y, w, h;		/* activation box, eeschema coordinates */
				/* points to hovered aoi, or NULL */

	bool (*hover)(void *user, bool on);
	void (*click)(void *user);
	void (*drag)(void *user, int dx, int dy);
	void *user;

	struct aoi *next;
};


struct aoi *aoi_add(struct aoi **aois, const struct aoi *cfg);
void aoi_update(struct aoi *aoi, const struct aoi *cfg);
bool aoi_hover(const struct aoi *aois, int x, int y);

bool aoi_move(const struct aoi *aois, int x, int y);
bool aoi_down(const struct aoi *aois, int x, int y);
bool aoi_up(const struct aoi *aois, int x, int y);

void aoi_remove(struct aoi **aois, const struct aoi *aoi);
void aoi_dehover(void);

#endif /* !GUI_AOI_H */
