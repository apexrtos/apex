#include <access.h>
#include <vm.h>
#include <as.h>

#include "fs.h"
#include <arch/mmu.h>
#include <cassert>
#include <debug.h>

/*
 * vm_read - read data from address space
 */
int
vm_read(as *a, void *l, const void *r, size_t s)
{
	#warning vm_read not implemented
	assert(0);
}

/*
 * vm_write - write data to address space
 */
int
vm_write(as *a, const void *l, void *r, size_t s)
{
	#warning vm_write not implemented
	assert(0);
}

/*
 * vm_copy - copy data in address space
 */
int
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
ptr_err<void>
as_map(as *a, void *req_addr, size_t len, int prot, int flags,
    std::unique_ptr<vnode> vn, off_t off, long attr)
{
	/* fixed replaces any existing mapping */
	const auto fixed = flags & MAP_FIXED;

	as_find_free(a, req_addr, len, flags);


	/* find free area in address space */


	if (vn) {
		/* establish memory mapping for vnode */
		vn_map(vn.get());
	}

	/* map file */
	/* find region in address space */
	/* pass region & file to mmu to establish mmu mapping */
	/* - RO? RW? COW? */



	/* should a vnode (file) know how to map itself? readonly for now? */
	/* vnode needs to have the same virt->phys mapping as addr space */
	/* allows for a nice XIP implementation? */
	/* all files read/write like that? */
	/* cached pages? - can be reused freely */
	/* reference page from underlying storage? e.g. file on mmc should not
	 * doubly 'map' if it is aligned nicely */

	/* what's a 'quick' way to do this? */


#warning as_map not implemented
assert(0);
}

/*
 * as_unmap - unmap memory from address space
 */
int
as_unmap(as *a, void *addr, size_t len, vnode *vn, off_t off)
{
	mmu_unmap(a, addr, len);

	if (vn)
		vn_unmap(vn);

	#warning as_unmap not implemented
	assert(0);
}

/*
 * as_mprotect - set protection flags on memory in address space
 */
int
as_mprotect(as *a, void *addr, size_t len, int prot)
{
	#warning as_mprotect not implemented
	assert(0);
}

/*
 * as_madvise - act on advice about intended memory use
 */
int
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
