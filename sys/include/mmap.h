#pragma once

#include <types.h>

struct as;

/*
 * Kernel interface
 */
void   *mmapfor(struct as *, void *, size_t, int, int, int, off_t, long mem_attr);
int	munmapfor(struct as *, void *, size_t);
int     mprotectfor(struct as *, void *, size_t, int);
