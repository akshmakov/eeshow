/*
 * util.h - Common utility functions
 *
 * Written 2016 by Werner Almesberger
 * Copyright 2016 Werner Almesberger
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */


#ifndef UTIL_H
#define UTIL_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>


#define alloc_size(s)					\
    ({	void *alloc_size_tmp = malloc(s);		\
	if (!alloc_size_tmp) {				\
		perror("malloc");			\
		exit(1);				\
	}						\
	alloc_size_tmp; })

#define alloc_type(t) ((t *) alloc_size(sizeof(t)))
#define alloc_type_n(t, n) ((t *) alloc_size(sizeof(t) * (n)))

#define stralloc(s)					\
    ({	char *stralloc_tmp = strdup(s);			\
	if (!stralloc_tmp) {				\
		perror("strdup");			\
		exit(1);				\
	}						\
	stralloc_tmp; })


#define	ARRAY_ELEMENTS(a)	(sizeof(a) / sizeof(a[0]))
#define	ARRAY_END(a)		((a) + ARRAY_ELEMENTS(a))


#define	swap(a, b) \
    ({ typeof(a) _tmp = (a); a = (b); b = _tmp; })


#define	unsupported(s) \
	fprintf(stderr, __FILE__ ":%d: unsupported: " s "\n", __LINE__)

#endif /* !UTIL_H */
