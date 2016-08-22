/*
 * misc/diag.h - Diagnostics
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
#include <string.h>
#include <errno.h>


#include "misc/diag.h"


unsigned verbose = 0;


/* ----- Specialized diagnostic functions ---------------------------------- */


void diag_pfatal(const char *s)
{
	fatal("%s: %s\n", s, strerror(errno));
}


void diag_perror(const char *s)
{
	error("%s: %s\n", s, strerror(errno));
}


/* ----- General diagnostic functions -------------------------------------- */

void fatal(const char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	vfprintf(stderr, fmt, ap);
	va_end(ap);
	exit(1); /* @@@ for now ... */
}


void error(const char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	vfprintf(stderr, fmt, ap);
	va_end(ap);
	fprintf(stderr, "\n");
}


void warning(const char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	fprintf(stderr, "warning: ");
	vfprintf(stderr, fmt, ap);
	va_end(ap);
	fprintf(stderr, "\n");
}


void progress(unsigned level, const char *fmt, ...)
{
	va_list ap;

	if (level > verbose)
		return;
	va_start(ap, fmt);
	fprintf(stderr, "%*s", level * 2, "");
	vfprintf(stderr, fmt, ap);
	va_end(ap);
	fprintf(stderr, "\n");
}
