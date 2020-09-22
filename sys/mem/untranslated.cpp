#include <arch.h>

#include <access.h>
#include <algorithm>
#include <cerrno>
#include <cstring>
#include <debug.h>
#include <fs.h>
#include <kernel.h>
#include <page.h>
#include <sch.h>
#include <sys/mman.h>
#include <sys/uio.h>
#include <task.h>
#include <thread.h>
#include <vm.h>

/*
 * vm_read - read data from address space
 */
int
vm_read(as *a, void *l, const void *r, size_t s)
{
	if (auto r = as_transfer_begin(a); r < 0)
		return r;
	if (!u_access_okfor(a, r, s, PROT_READ)) {
		as_transfer_end(a);
		return DERR(-EFAULT);
	}
	memcpy(l, r, s);
	as_transfer_end(a);
	return s;
}

/*
 * vm_write - write data to address space
 */
int
vm_write(as *a, const void *l, void *r, size_t s)
{
	if (auto r = as_transfer_begin(a); r < 0)
		return r;
	if (!u_access_okfor(a, r, s, PROT_WRITE)) {
		as_transfer_end(a);
		return DERR(-EFAULT);
	}
	memcpy(r, l, s);
	as_transfer_end(a);
	return s;
}

/*
 * vm_copy - copy data in address space
 */
int
vm_copy(as *a, void *dst, const void *src, size_t s)
{
	if (auto r = as_transfer_begin(a); r < 0)
		return r;
	if (!u_access_okfor(a, src, s, PROT_READ) ||
	    !u_access_okfor(a, dst, s, PROT_WRITE)) {
		as_transfer_end(a);
		return DERR(-EFAULT);
	}
	memcpy(dst, src, s);
	as_transfer_end(a);
	return s;
}

/*
 * as_switch - switch to address space
 */
void
as_switch(as *a)
{
#if defined(CONFIG_MPU)
	mpu_switch(a);
#endif
}

/*
 * as_map - map memory into address space
 */
void *
as_map(as *a, void *addr, size_t len, int prot, int flags,
    std::unique_ptr<vnode> vn, off_t off, long attr)
{
	int r = 0;
	const auto fixed = flags & MAP_FIXED;

	std::unique_ptr<phys> pages(fixed
	    ? page_reserve((phys*)addr, len, attr, a)
	    : page_alloc(len, attr, a),
	    {len, a});

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

	if ((r = as_insert(a, std::move(pages), len, prot, flags,
	    std::move(vn), off, attr)) < 0)
		return (void*)r;

#if defined(CONFIG_MPU)
	if (a == task_cur()->as)
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
as_unmap(as *a, void *addr, size_t len, vnode *vn, off_t off)
{
#if defined(DEBUG)
	memset(addr, 0, len);
#endif

#if defined(CONFIG_MPU)
	if (a == task_cur()->as)
		mpu_unmap(addr, len);
#endif

	return page_free((phys*)addr, len, a);
}

/*
 * as_mprotect - set protection flags on memory in address space
 */
int
as_mprotect(as *a, void *addr, size_t len, int prot)
{
#if defined(CONFIG_MPU)
	if (a == task_cur()->as)
		mpu_protect(addr, len, prot);
#endif

	return 0;
}

/*
 * as_madvise - act on advice about intended memory use
 */
int
as_madvise(as *a, seg *s, void *addr, size_t len, int advice)
{
	switch (advice) {
	case MADV_DONTNEED:
		if (seg_vnode(s))
			return 0;
		/* anonymous private mappings must be zero-filled */
		memset(addr, 0, len);
		break;
	case MADV_FREE:
		if (seg_vnode(s))
			return DERR(-EINVAL);
		/* no need to zero as free is allowed to be lazy */
		break;
	}

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
u_arraylen(const void *const *u_arr, const size_t maxlen)
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
u_access_okfor(as *a, const void *u_addr, size_t len, int access)
{
	/* u_access_okfor only makes sense if the address space is locked or if
	 * preemption is disabled. Otherwise the address space could be
	 * modified by some other thread. */
	assert(sch_locks() || as_locked(a));
	const auto seg = as_find_seg(a, u_addr);
	if (!seg)
		return false;
	if ((uintptr_t)seg_end(seg) - (uintptr_t)u_addr < len)
		return false;
	if ((access & seg_prot(seg)) != access)
		return false;
	return true;
}


bool
k_access_ok(const void *k_addr, size_t len, int access)
{
	return page_valid((const phys *)k_addr, len, &kern_task);
}

int
u_access_begin()
{
	/* recursive u_access_begin makes no sense */
	assert(!(thread_cur()->state & TH_U_ACCESS));
	if (auto r{as_transfer_begin(task_cur()->as)}; r < 0)
		return r;
	thread_cur()->state |= TH_U_ACCESS;
	return 0;
}

void
u_access_end()
{
	as_transfer_end(task_cur()->as);
	thread_cur()->state &= ~(TH_U_ACCESS | TH_U_ACCESS_S);
}

void
u_access_suspend()
{
	if (!(thread_cur()->state & TH_U_ACCESS))
		return;
	as_transfer_end(task_cur()->as);
	thread_cur()->state |= TH_U_ACCESS_S;
}

int
u_access_resume(const void *u_addr, size_t len, int prot)
{
	if (!(thread_cur()->state & TH_U_ACCESS))
		return 0;
	if (auto r = as_transfer_begin(task_cur()->as); r)
		return r;
	if (!u_access_ok(u_addr, len, prot))
		return DERR(-EFAULT);
	return 0;
}

bool
u_access_continue(const void *u_addr, size_t len, int prot)
{
	const auto suspended{thread_cur()->state & TH_U_ACCESS_S};
	return !suspended || u_access_ok(u_addr, len, prot);
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
u_addressfor(const as *a, const void *u_addr)
{
	return as_find_seg(a, u_addr);
}

bool
k_address(const void *k_addr)
{
	return page_valid((const phys *)k_addr, 0, &kern_task);
}
