#pragma once

#include <types.h>

struct as;

/*
 * Kernel interface
 */
void *mmapfor(as *, void *, size_t, int, int, int, off_t, long mem_attr);
int munmapfor(as *, void *, size_t);
int mprotectfor(as *, void *, size_t, int);
