#pragma once

#include <types.h>

struct as;

/*
 * Kernel interface
 */
void   *mmapfor(struct as *, void *, size_t, int, int, int, off_t, long mem_attr);
int	munmapfor(struct as *, void *, size_t);
int     mprotectfor(struct as *, void *, size_t, int);

/*
 * Syscalls
 */
void   *sc_mmap2(void *, size_t, int, int, int, int);
int	sc_munmap(void *, size_t);
int     sc_mprotect(void *, size_t, int);
int	sc_madvise(void *, size_t, int);
void   *sc_brk(void *addr);
