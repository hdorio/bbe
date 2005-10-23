/*
 *    bbe - Binary block editor
 *
 *    Copyright (C) 2005 Timo Savinen
 *    This file is part of bbe.
 * 
 *    bbe is free software; you can redistribute it and/or modify
 *    it under the terms of the GNU General Public License as published by
 *    the Free Software Foundation; either version 2 of the License, or
 *    (at your option) any later version.
 *
 *    bbe is distributed in the hope that it will be useful,
 *    but WITHOUT ANY WARRANTY; without even the implied warranty of
 *    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *    GNU General Public License for more details.
 *
 *    You should have received a copy of the GNU General Public License
 *    along with bbe; if not, write to the Free Software
 *    Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */

/* $Id: xmalloc.c,v 1.3 2005/10/19 18:39:13 timo Exp $ */

#include "bbe.h"
#include <stdlib.h>
#include <string.h>

void *
xmalloc (size_t size)
{
    register void *value = malloc(size);
    if (value == 0) panic("Out of memory",NULL,NULL);
    return value;
}

char *
xstrdup(char *str)
{
    char *ret = strdup(str);
    if (ret == NULL) panic("Out of memory",NULL,NULL);
    return ret;
}

