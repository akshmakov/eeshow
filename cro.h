/*
 * cro.h - Cairo graphics back-end
 *
 * Written 2016 by Werner Almesberger
 * Copyright 2016 by Werner Almesberger
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */


#ifndef CRO_H
#define	CRO_H

#include <stdint.h>

#include "gfx.h"


extern const struct gfx_ops cro_png_ops;
extern const struct gfx_ops cro_pdf_ops;

#define	cro_img_ops	cro_png_ops	/* just don't call cro_img_ops.end */


uint32_t *cro_img_end(void *ctx, int *w, int *h, int *stride);
void cro_img_write(void *ctx, const char *name);

#endif /* !CRO_H */
