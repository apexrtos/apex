#include <arch.h>

#include <access.h>
#include <algorithm>
#include <cerrno>
#include <cstring>
#include <debug.h>
#include <fs.h>
#include <kernel.h>
#include <page.h>
#include <sys/mman.h>
#include <sys/uio.h>
#include <task.h>
#include <thread.h>
#include <vm.h>

/*
 * vm_read - read data from address space
 */
int
vm_read(as *as, void *l, const void *r, size_t s)
{
	if (auto r = as_transfer_begin(as); r < 0)
		return r;
	if (!u_access_okfor(as, r, s, PROT_READ)) {
		as_transfer_end(as);
		return DERR(-EFAULT);
	}
	memcpy(l, r, s);
	as_transfer_end(as);
	return s;
}

/*
 * vm_write - write data to address space
 */
int
vm_write(as *as, const void *l, void *r, size_t s)
{
	if (auto r = as_transfer_begin(as); r < 0)
		return r;
	if (!u_access_okfor(as, r, s, PROT_WRITE)) {
		as_transfer_end(as);
		return DERR(-EFAULT);
	}
	memcpy(r, l, s);
	as_transfer_end(as);
	return s;
}

/*
 * vm_copy - copy data in address space
 */
int
vm_copy(as *as, void *dst, const void *src, size_t s)
{
	if (auto r = as_transfer_begin(as); r < 0)
		return r;
	if (!u_access_okfor(as, src, s, PROT_READ) ||
	    !u_access_okfor(as, dst, s, PROT_WRITE)) {
		as_transfer_end(as);
		return DERR(-EFAULT);
	}
	memcpy(dst, src, s);
	as_transfer_end(as);
	return s;
}

/*
 * as_switch - switch to address space
 */
void
as_switch(as *as)
{
#if defined(CONFIG_MPU)
	mpu_switch(as);
#endif
}

/*
 * as_map - map memory into address space
 */
void *
as_map(struct as *as, void *addr, size_t len, int prot, int flags,
    std::unique_ptr<vnode> vn, off_t off, long attr)
{
	int r = 0;
	const auto fixed = flags & MAP_FIXED;

	std::unique_ptr<phys> pages(fixed
	    ? page_reserve((phys*)addr, len, attr, as)
	    : page_alloc(len, attr, as),
	    {len, as});

	if (!pages.get())
		return (void *)-ENOMEM;

	addr = pages.get();

	/* read data */
	if (vn.get() && (r = vn_pread(vn.get(), addr, len, off)) < 0)
		return (void*)r;
	if ((size_t)r < len)
		memset((char*)addr + r, 0, len - r);
	if (prot & PROT_EXEC)
		cache_coherent_exec(addr, len);

	if ((r = as_insert(as, std::move(pages), len, prot, flags,
	    std::move(vn), off, attr)) < 0)
		return (void*)r;

#if defined(CONFIG_MPU)
	if (as == task_cur()->as)
		mpu_map(addr, len, prot);
#endif

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

#if defined(CONFIG_MPU)
	if (as == task_cur()->as)
		mpu_unmap(addr, len);
#endif

	return page_free((phys*)addr, len, as);
}

/*
 * as_mprotect - set protection flags on memory in address space
 */
int
as_mprotect(struct as *as, void *addr, size_t len, int prot)
{
#if defined(CONFIG_MPU)
	if (as == task_cur()->as)
		mpu_protect(addr, len, prot);
#endif

	return 0;
}

/*
 * access.h
 */
ssize_t
u_strnlen(const char *u_str, const size_t maxlen)
{
	const auto seg = as_find_seg(task_cur()->as, u_str);
	if (!seg)
		return DERR(-EFAULT);
	const auto lim = std::min<size_t>(maxlen, (char *)seg_end(seg) - u_str);
	const auto r = strnlen(u_str, lim);
	if (r == maxlen)
		return maxlen;
	if (r == lim)
		return DERR(-EFAULT);
	return r;
}

ssize_t
u_arraylen(const void **u_arr, const size_t maxlen)
{
	const auto seg = as_find_seg(task_cur()->as, u_arr);
	if (!seg)
		return DERR(-EFAULT);
	const auto lim = std::min<size_t>(maxlen, (const void **)seg_end(seg) - u_arr);
	size_t i;
	for (i = 0; i < lim && u_arr[i]; ++i);
	if (i == lim)
		return DERR(-EFAULT);
	return i;
}

bool
u_access_ok(const void *u_addr, size_t len, int access)
{
	return u_access_okfor(task_cur()->as, u_addr, len, access);
}

bool
u_access_okfor(const struct as *a, const void *u_addr, size_t len, int access)
{
	const auto seg = as_find_seg(a, u_addr);
	if (!seg)
		return false;
	if ((char *)u_addr + len > seg_end(seg))
		return false;
	if ((access & seg_prot(seg)) != access)
		return false;
	return true;
}


bool
k_access_ok(const void *k_addr, size_t len, int access)
{
	return page_valid((phys *)k_addr, len, &kern_task);
}

int
u_access_begin()
{
	return as_transfer_begin(task_cur()->as);
}

void
u_access_end()
{
	as_transfer_end(task_cur()->as);
}

bool
u_access_suspend()
{
	if (!as_transfer_running(task_cur()->as))
		return false;
	as_transfer_end(task_cur()->as);
	return true;
}

int
u_access_resume(bool suspended, const void *u_addr, size_t len, int prot)
{
	if (!suspended)
		return 0;
	if (auto r = as_transfer_begin(task_cur()->as); r)
		return r;
	if (!u_access_ok(u_addr, len, prot))
		return DERR(-EFAULT);
	return 0;
}

bool
u_fault()
{
	return false;
}

void
u_fault_clear()
{
	/* nothing to do */
}

bool
u_address(const void *u_addr)
{
	return u_addressfor(task_cur()->as, u_addr);
}

bool
u_addressfor(const struct as *a, const void *u_addr)
{
	return as_find_seg(a, u_addr);
}

bool
k_address(const void *k_addr)
{
	return page_valid((phys *)k_addr, 0, &kern_task);
}
