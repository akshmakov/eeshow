/*
 * diag.h - Diagnostics
 *
 * Written 2016 by Werner Almesberger
 * Copyright 2016 by Werner Almesberger
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */


#include <stdarg.h>
#include <stdlib.h>
#include <stdio.h>


#include "diag.h"


unsigned verbose = 0;


void progress(unsigned level, const char *fmt, ...)
{
	va_list ap;

	if (level > verbose)
		return;
	va_start(ap, fmt);
	fprintf(stderr, "%*s", level * 2, "");
	vfprintf(stderr, fmt, ap);
	va_end(ap);
}
