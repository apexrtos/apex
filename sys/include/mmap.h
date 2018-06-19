#ifndef mmap_h
#define mmap_h

#include <types.h>

struct as;

#if defined(__cplusplus)
extern "C" {
#endif

/*
 * Kernel interface
 */
void   *mmapfor(struct as *, void *, size_t, int, int, int, off_t,
		enum MEM_TYPE);
int	munmapfor(struct as *, void *, size_t);
int     mprotectfor(struct as *, void *, size_t, int);
void	vm_init_brk(struct as *, void *);
ssize_t vm_readv(struct as *, const struct iovec *, size_t,
		 const struct iovec *, size_t);
ssize_t vm_writev(struct as *, const struct iovec *, size_t,
		  const struct iovec *, size_t);
ssize_t vm_read(struct as *, void *, const void *, size_t);
ssize_t vm_write(struct as *, const void *, void *, size_t);

/*
 * Syscalls
 */
void   *sc_mmap2(void *, size_t, int, int, int, int);
int	sc_munmap(void *, size_t);
int     sc_mprotect(void *, size_t, int);
void   *sc_brk(void *addr);

#if defined(__cplusplus)
} /* extern "C" */
#endif

#endif /* mmap_h */
