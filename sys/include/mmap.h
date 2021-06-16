#pragma once

/*
 * mmap.h - memory mapping
 */

#include <lib/expect.h>
#include <types.h>

struct as;

expect<void *> mmapfor(as *, void *, size_t, int, int, int, off_t, long mem_attr);
expect_ok munmapfor(as *, void *, size_t);
expect_ok mprotectfor(as *, void *, size_t, int);
