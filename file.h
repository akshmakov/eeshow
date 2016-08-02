/*
 * file.h - Open and read a file
 *
 * Written 2016 by Werner Almesberger
 * Copyright 2016 by Werner Almesberger
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#ifndef FILE_H
#define	FILE_H

#include <stdbool.h>

bool file_cat(void *user, const char *line);
void file_read(const char *name, bool (*parse)(void *user, const char *line),
    void *user);

#endif /* !FILE_H */
