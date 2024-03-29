#include <as.h>
#include <seg.h>
#include <vm.h>

#include <access.h>
#include <algorithm>
#include <arch/cache.h>
#include <arch/mmu.h>
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

/*
 * vm_read - read data from address space
 */
expect_pos
vm_read(as *a, void *l, const void *r, size_t s)
{
	if (auto r = as_transfer_begin(a); r < 0)
		return r;
	if (!u_access_okfor(a, r, s, PROT_READ)) {
		as_transfer_end(a);
		return DERR(std::errc::bad_address);
	}
	memcpy(l, r, s);
	as_transfer_end(a);
	return s;
}

/*
 * vm_write - write data to address space
 */
expect_pos
vm_write(as *a, const void *l, void *r, size_t s)
{
	if (auto r = as_transfer_begin(a); r < 0)
		return r;
	if (!u_access_okfor(a, r, s, PROT_WRITE)) {
		as_transfer_end(a);
		return DERR(std::errc::bad_address);
	}
	memcpy(r, l, s);
	as_transfer_end(a);
	return s;
}

/*
 * vm_copy - copy data in address space
 */
expect_pos
vm_copy(as *a, void *dst, const void *src, size_t s)
{
	if (auto r = as_transfer_begin(a); r < 0)
		return r;
	if (!u_access_okfor(a, src, s, PROT_READ) ||
	    !u_access_okfor(a, dst, s, PROT_WRITE)) {
		as_transfer_end(a);
		return DERR(std::errc::bad_address);
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
expect<void *>
as_map(as *a, void *req_addr, size_t len, int prot, int flags,
    std::unique_ptr<vnode> vn, off_t off, long attr)
{
	const auto fixed = flags & MAP_FIXED;
	const auto pg_off = PAGE_OFF(req_addr);

	page_ptr pages{fixed
			? page_reserve(virt_to_phys(req_addr), len, attr, a)
			: page_alloc(pg_off + len, attr, a)};

	if (!pages)
		return std::errc::not_enough_memory;

	std::byte *addr = (std::byte*)phys_to_virt(pages);

	/* read data & zero-fill (partial pages if not anonymous) */
	ssize_t r = 0;
	memset(addr, 0, pg_off);
	if (vn)
		if (r = vn_pread(vn.get(), addr + pg_off, len, off);
		    r != (ssize_t)len)
			return to_errc(r, DERR(std::errc::no_such_device_or_address));
	r += pg_off;
	auto pg_len{PAGE_ALIGN(pg_off + len)};
	memset(addr + r, 0, pg_len - r);

	if (prot & PROT_EXEC)
		cache_coherent_exec(addr, pg_len);

	if (auto r = as_insert(a, std::move(pages), len, prot, flags,
			       std::move(vn), off, attr);
	    !r.ok())
		return r.err();

#if defined(CONFIG_MPU)
	if (a == task_cur()->as)
		mpu_map(addr, pg_len, prot);
#endif

	return addr + pg_off;
}

/*
 * as_unmap - unmap memory from address space
 *
 * nommu cannot mark pages as dirty.
 */
expect_ok
as_unmap(as *a, void *addr, size_t len, vnode *vn, off_t off)
{
#if defined(DEBUG)
	memset(addr, 0, len);
#endif

#if defined(CONFIG_MPU)
	if (a == task_cur()->as)
		mpu_unmap(addr, len);
#endif

	return page_free(virt_to_phys(addr), len, a);
}

/*
 * as_mprotect - set protection flags on memory in address space
 */
expect_ok
as_mprotect(as *a, void *addr, size_t len, int prot)
{
#if defined(CONFIG_MPU)
	if (a == task_cur()->as)
		mpu_protect(addr, len, prot);
#endif

	return {};
}

/*
 * as_madvise - act on advice about intended memory use
 */
expect_ok
as_madvise(as *a, seg *s, void *addr, size_t len, int advice)
{
	switch (advice) {
	case MADV_DONTNEED:
		if (seg_vnode(s))
			return {};
		/* anonymous private mappings must be zero-filled */
		memset(addr, 0, len);
		break;
	case MADV_FREE:
		if (seg_vnode(s))
			return DERR(std::errc::bad_address);
		/* no need to zero as free is allowed to be lazy */
		break;
	}

	return {};
}

/*
 * access.h
 */
ssize_t
u_strnlen(const char *u_str, const size_t maxlen)
{
	const seg *seg;
	if (auto r = as_find_seg(task_cur()->as, u_str); !r.ok())
		return DERR(-EFAULT);
	else seg = r.val();
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
	const seg *seg;
	if (auto r = as_find_seg(task_cur()->as, u_arr); !r.ok())
		return DERR(-EFAULT);
	else seg = r.val();
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
	const seg *seg;
	if (auto r = as_find_seg(a, u_addr); !r.ok())
		return false;
	else seg = r.val();
	if ((uintptr_t)seg_end(seg) - (uintptr_t)u_addr < len)
		return false;
	if ((access & seg_prot(seg)) != access)
		return false;
	return true;
}

bool
k_access_ok(const void *k_addr, size_t len, int access)
{
	return page_valid(virt_to_phys(k_addr), len, &kern_task);
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

int
u_access_begin_interruptible()
{
	/* recursive u_access_begin makes no sense */
	assert(!(thread_cur()->state & TH_U_ACCESS));
	if (auto r{as_transfer_begin_interruptible(task_cur()->as)}; r < 0)
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
	return as_find_seg(a, u_addr).ok();
}

bool
k_address(const void *k_addr)
{
	return page_valid(virt_to_phys(k_addr), 0, &kern_task);
}
