#include <arch.h>

#include <access.h>
#include <cstring>
#include <debug.h>
#include <fs.h>
#include <page.h>
#include <sys/mman.h>
#include <sys/uio.h>
#include <task.h>
#include <vm.h>

/*
 * as_read - read data from address space
 */
int
as_read(as *as, void *l, const void *r, size_t s)
{
	memcpy(l, r, s);
	return 0;
}

/*
 * as_writev - write data to address space
 */
int
as_write(as *as, const void *l, void *r, size_t s)
{
	memcpy(r, l, s);
	return 0;
}

/*
 * as_switch - switch to address space
 */
void
as_switch(as *as)
{

}

/*
 * as_map - map memory into address space
 */
void *
as_map(struct as *as, void *addr, size_t len, int prot, int flags,
    std::unique_ptr<vnode> vn, off_t off, MEM_TYPE type)
{
	int r = 0;
	const auto fixed = flags & MAP_FIXED;

	std::unique_ptr<phys> pages(fixed ? page_reserve((phys*)addr, len) :
	    page_alloc(len, type, PAGE_ALLOC_FIXED), len);

	if (!pages.get())
		return (void*)-ENOMEM;	/* expected */

	addr = pages.get();

	/* read data */
	if (vn.get() && (r = vn_pread(vn.get(), addr, len, off)) < 0)
		return (void*)r;
	if ((size_t)r < len)
		memset((char*)addr + r, 0, len - r);
	if (prot & PROT_EXEC)
		cache_coherent_exec(addr, len);

	if ((r = as_insert(as, std::move(pages), len, prot, flags,
	    std::move(vn), off, type)) < 0)
		return (void*)r;

	return addr;
}

/*
 * as_unmap - unmap memory from address space
 *
 * nommu cannot mark pages as dirty.
 */
int
as_unmap(struct as *as, void *addr, size_t len, vnode *vn, off_t off)
{
#if defined(DEBUG)
	memset(addr, 0, len);
#endif
	return page_free((phys*)addr, len);
}

/*
 * as_mprotect - set protection flags on memory in address space
 */
int
as_mprotect(struct as *as, void *addr, size_t len, int prot)
{
	return 0;
}

/*
 * access.h
 */
bool
u_access_ok(const void *u_addr, size_t len, int access)
{
	return true;
}

bool
k_access_ok(const void *k_addr, size_t len, int access)
{
	return true;
}

int
u_access_begin()
{
	return as_transfer_begin(task_cur()->as);
}

int
u_access_end()
{
	return as_transfer_end(task_cur()->as);
}

bool
u_fault()
{
	return false;
}

void
u_fault_clear()
{

}

bool
u_address(const void *p)
{
	return true;
}

bool
k_address(const void *p)
{
	return true;
}
