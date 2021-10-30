#include <access.h>
#include <vm.h>
#include <as.h>

#include "fs.h"
#include <arch/mmu.h>
#include <cassert>
#include <debug.h>
#include <sys/mman.h>

/*
 * vm_read - read data from address space
 */
expect_pos
vm_read(as *a, void *l, const void *r, size_t s)
{
	#warning vm_read not implemented
	assert(0);
}

/*
 * vm_write - write data to address space
 */
expect_pos
vm_write(as *a, const void *l, void *r, size_t s)
{
	#warning vm_write not implemented
	assert(0);
}

/*
 * vm_copy - copy data in address space
 */
expect_pos
vm_copy(as *a, void *dst, const void *src, size_t s)
{
	#warning vm_copy not implemented
	assert(0);
}

/*
 * as_switch - switch to address space
 */
void
as_switch(as *a)
{
	mmu_switch(*a);
}

/*
 * as_map - map memory into address space
 */
expect<void *>
as_map(as *a, void *req_addr, size_t len, int prot, int flags,
    std::unique_ptr<vnode> vn, off_t off, long attr)
{
	/* fixed replaces any existing mapping */
	const bool fixed = flags & MAP_FIXED;
	const bool shared = flags & MAP_SHARED;

	/* find free area in address space */
	char *virt;
	if (auto r = as_find_free(a, req_addr, len, flags); !r.ok())
		return r.err();
	else virt = (char *)r.val();

	if (vn) {
		virt += PAGE_OFF(off);

		/* establish mapping from file to memory */
		file_map *fmap;
		if (auto r = vn_map(vn.get(), off, len, flags, attr); !r.ok())
			return r.err();
		else fmap = r.val();

		/* establish mapping from file to virtual address space */
		char *v = virt;
		while (len) {
#warning _WHOA_ this is totally wrong
#warning find will find the mapping containg off, which could be a large mapping with
#warning off somewhere in the middle!
			auto p = fmap->find(off)->phys + PAGE_OFF(v);
			auto l = std::min(p->size, len) - PAGE_OFF(v);
			if (auto r = mmu_map(*a, p, v, l, prot); !r.ok())
				return r.err();
			off += l;
			len -= l;
			v += l;
		}
	} else {
		/* establish anonymous mapping */
		if (auto r = mmu_map(*a, virt, len, prot, attr); !r.ok())
			return r.err();
	}

	return (void *)virt;
}

/*
 * as_unmap - unmap memory from address space
 */
expect_ok
as_unmap(as *a, void *addr, size_t len, vnode *vn, off_t off)
{
	mmu_unmap(*a, addr, len);

	#warning as_unmap not implemented
	assert(0);
}

/*
 * as_mprotect - set protection flags on memory in address space
 */
expect_ok
as_mprotect(as *a, void *addr, size_t len, int prot)
{
	#warning as_mprotect not implemented
	assert(0);
}

/*
 * as_madvise - act on advice about intended memory use
 */
expect_ok
as_madvise(as *a, seg *s, void *addr, size_t len, int advice)
{
	#warning as_madvise not implemented
	assert(0);
}

/*
 * access.h
 */
ssize_t
u_strnlen(const char *u_str, const size_t maxlen)
{
#warning u_strnlen not implemented
assert(0);
}

ssize_t
u_arraylen(const void *const *u_arr, const size_t maxlen)
{
#warning u_arraylen not implemented
assert(0);
}

bool
u_access_okfor(as *a, const void *u_addr, size_t len, int access)
{
#warning u_access_okfor not implemented
assert(0);
}

int
u_access_begin()
{
	/* nothing to do, MMU handles access to userspace */
	return 0;
}

int
u_access_begin_interruptible()
{
	/* nothing to do, MMU handles access to userspace */
	return 0;
}

void
u_access_end()
{
	/* nothing to do, MMU handles access to userspace */
}

void
u_access_suspend()
{
	/* nothing to do, MMU handles access to userspace */
}

int
u_access_resume(const void *u_addr, size_t len, int prot)
{
	/* nothing to do, MMU handles access to userspace */
	return 0;
}

bool
u_access_continue(const void *u_addr, size_t len, int prot)
{
	/* nothing to do, MMU handles access to userspace */
	return true;
}
