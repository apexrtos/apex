#include "mmap.h"
#include "vm.h"

#include <algorithm>
#include <arch.h>
#include <cerrno>
#include <debug.h>
#include <fcntl.h>
#include <fs.h>
#include <kernel.h>
#include <kmem.h>
#include <list.h>
#include <sections.h>
#include <sys/mman.h>
#include <sys/uio.h>
#include <task.h>
#include <types.h>

/*
 * TODO:
 *  - shared mappings
 *  - mprotect can leave address space inconsistent on OOM. This can be
 *    fixed by splitting and inserting segments before making changes.
 *  - mmap can leave address space inconsistent on OOM. This is because
 *    the munmap in as_insert can succeed, leaving existing pages allocated,
 *    but the subsequent seg_insert may fail.
 */

struct seg {
	list link;	/* entry in segment list */
	int prot;	/* segment protection PROT_* */
	void *base;	/* virtual base address of this segment */
	size_t len;	/* size of segment */
	long attr;	/* preferred memory attributes for pages */
	off_t off;	/* (optional) offset into vnode */
	vnode *vn;	/* (optional) vnode backing region */
};

struct as {
	list segs;	/* list of segments */
	void *base;	/* base address of address space */
	size_t len;	/* size of address space */
	void *brk;	/* current program break */
	unsigned ref;	/* reference count */
	a::rwlock lock;	/* address space lock */
#if defined(CONFIG_MMU)
	struct pgd *pgd;/* page directory */
#endif
};

/*
 * do_vm_io - walk local and remote iovs calling f for each overlapping area
 */
template<typename fn>
static ssize_t
do_vm_io(as *a, const iovec *liov, size_t lc, const iovec *riov, size_t rc, fn f)
{
	if (!lc || !rc)
		return 0;

	ssize_t ret = 0;
	iovec r = *riov;
	for (; lc; --lc, ++liov) {
		iovec l = *liov;
		while (l.iov_len) {
			const auto s = std::min(l.iov_len, r.iov_len);
			const auto err = f(a, l.iov_base, r.iov_base, s);
			if (err < 0)
				return err;
			l.iov_len -= s;
			l.iov_base = (char*)l.iov_base + s;
			ret += s;

			if (r.iov_len == s) {
				if (!--rc)
					return ret;
				r = *++riov;
			} else {
				r.iov_len -= s;
				r.iov_base = (char*)r.iov_base + s;
			}
		}
	}

	return ret;
}

/*
 * oflags - calculate required oflags from map flags & protection
 */
static int
oflags(int prot, int flags)
{
	constexpr auto rdwr = PROT_READ | PROT_WRITE;

	/* private mappings don't require write access to underlying file */
	if (flags & MAP_PRIVATE)
		prot &= ~PROT_WRITE;
	if ((prot & rdwr) == rdwr)
		return O_RDWR;
	if (prot & PROT_READ)
		return O_RDONLY;
	if (prot & PROT_WRITE)
		return O_WRONLY;
	return DERR(-EINVAL);
}

/*
 * seg_insert - insert new segment
 */
static int
seg_insert(seg *prev, std::unique_ptr<phys> pages, size_t len, int prot,
    std::unique_ptr<vnode> vn, off_t off, long attr)
{
	seg *ns;
	if (!(ns = (seg*)kmem_alloc(sizeof(seg), MA_FAST)))
		return DERR(-ENOMEM);
	ns->prot = prot;
	ns->base = pages.release();
	ns->len = len;
	ns->attr = attr;
	ns->off = off;
	ns->vn = vn.release();
	list_insert(&prev->link, &ns->link);
	return 0;
}

/*
 * seg_combine - combine contiguous segments
 */
static void
seg_combine(as *a)
{
	assert(!list_empty(&a->segs));

	seg *p = list_entry(list_first(&a->segs), seg, link), *s, *tmp;
	list_for_each_entry_safe(s, tmp, list_next(&a->segs), link) {
		if (p->prot != s->prot || seg_end(p) != s->base ||
		    p->attr != s->attr || p->vn != s->vn ||
		    (p->vn && p->off + p->len != s->off)) {
			p = s;
			continue;
		}
		/* segments are contiguous, combine */
		p->len += s->len;
		if (s->vn)
			vn_close(s->vn);
		list_remove(&s->link);
		kmem_free(s);
	}
}

/*
 * do_munmapfor - unmap memory from locked address space
 *
 * Must be called with address space write lock held.
 */
static int
do_munmapfor(as *a, void *const vaddr, const size_t ulen, bool remap)
{
	int err = 0;

	if ((uintptr_t)vaddr & PAGE_MASK || ulen & PAGE_MASK)
		return DERR(-EINVAL);
	if (!ulen)
		return 0;

	const auto uaddr = (char*)vaddr;
	const auto uend = uaddr + ulen;

	seg *s, *tmp;
	list_for_each_entry_safe(s, tmp, &a->segs, link) {
		const auto send = (char*)s->base + s->len;
		if (send <= uaddr)
			continue;
		if (s->base >= uend)
			break;
		if (s->base >= uaddr && send <= uend) {
			/* entire segment */
			if (!remap)
				err = as_unmap(a, s->base, s->len, s->vn,
				    s->off);
			list_remove(&s->link);
			if (s->vn)
				vn_close(s->vn);
			kmem_free(s);
		} else if (s->base < uaddr && send > uend) {
			/* hole in segment */
			seg *ns;
			if (!(ns = (seg*)kmem_alloc(sizeof(seg), MA_FAST)))
				return DERR(-ENOMEM);
			s->len = uaddr - (char*)s->base;
			if (!remap)
				err = as_unmap(a, uaddr, ulen, s->vn,
				    s->off + s->len);
			*ns = *s;
			ns->base = uend;
			ns->len = send - uend;
			if (ns->vn) {
				vn_reference(ns->vn);
				ns->off += uend - (char*)s->base;
			}
			list_insert(&s->link, &ns->link);

			break;
		} else if (s->base < uaddr) {
			/* end of segment */
			const auto l = uaddr - (char*)s->base;
			if (!remap)
				err = as_unmap(a, uaddr, s->len - l, s->vn,
				    s->off + l);
			s->len = l;
		} else if (s->base < uend) {
			/* start of segment */
			const auto l = uend - (char*)s->base;
			if (!remap)
				err = as_unmap(a, s->base, l, s->vn, s->off);
			if (s->vn)
				s->off += l;
			s->base = (char*)s->base + l;
			s->len -= l;
		} else
			panic("BUG");
		if (err < 0)
			break;
	}

	return err;
}

/*
 * do_mmapfor - map memory into locked address space
 *
 * Must be called with address space write lock held.
 */
static void *
do_mmapfor(as *a, void *addr, size_t len, int prot, int flags, int fd,
    off_t off, long attr)
{
	std::unique_ptr<vnode> vn;
	const bool anon = flags & MAP_ANONYMOUS;
	const bool priv = flags & MAP_PRIVATE;
	const bool shared = flags & MAP_SHARED;

	if ((uintptr_t)addr & PAGE_MASK || len & PAGE_MASK || off & PAGE_MASK ||
	    priv == shared)
		return (void*)DERR(-EINVAL);
	if (!anon) {
		int oflg;
		if ((oflg = oflags(prot, flags)) < 0)
			return (void*)oflg;
		/* REVISIT: do we need to check if file is executable? */
		vn.reset(vn_open(fd, oflg));
		if (!vn.get())
			return (void*)DERR(-EBADF);
	}

	return as_map(a, addr, len, prot, flags, std::move(vn), off, attr);
}


/*
 * mmapfor - map memory into task address space
 */
void *
mmapfor(as *a, void *addr, size_t len, int prot, int flags, int fd, off_t off,
    long attr)
{
	interruptible_lock l(a->lock.write());
	if (int err = l.lock(); err < 0)
		return (void*)err;

	/* mmap replaces existing mappings */
	attr |= PAF_REALLOC;

	return do_mmapfor(a, addr, len, prot, flags, fd, off, attr);
}

/*
 * munmapfor - unmap memory in address space
 */
int
munmapfor(as *a, void *const vaddr, const size_t ulen)
{
	interruptible_lock l(a->lock.write());
	if (int err = l.lock(); err < 0)
		return err;

	return do_munmapfor(a, vaddr, ulen, false);
}

/*
 * mprotectfor
 */
int
mprotectfor(as *a, void *const vaddr, const size_t ulen, const int prot)
{
	int err = 0;

	if ((uintptr_t)vaddr & PAGE_MASK || ulen & PAGE_MASK)
		return DERR(-EINVAL);
	if ((prot & (PROT_READ | PROT_WRITE | PROT_EXEC)) != prot)
		return DERR(-EINVAL);
	if (!ulen)
		return 0;

	interruptible_lock l(a->lock.write());
	if (err = l.lock(); err < 0)
		return err;

	const auto uaddr = (char*)vaddr;
	const auto uend = uaddr + ulen;

	seg *s, *tmp;
	list_for_each_entry_safe(s, tmp, &a->segs, link) {
		const auto send = (char*)s->base + s->len;
		if (send <= uaddr)
			continue;
		if (s->base >= uend)
			break;
		if (s->prot == prot)
			continue;
		if (s->base >= uaddr && send <= uend) {
			/* entire segment */
			err = as_mprotect(a, s->base, s->len, prot);
			s->prot = prot;
		} else if (s->base < uaddr && send > uend) {
			/* hole in segment */
			seg *ns1, *ns2;
			if (!(ns1 = (seg*)kmem_alloc(sizeof(seg), MA_FAST)))
				return DERR(-ENOMEM);
			if (!(ns2 = (seg*)kmem_alloc(sizeof(seg), MA_FAST))) {
				kmem_free(ns1);
				return DERR(-ENOMEM);
			}

			err = as_mprotect(a, uaddr, ulen, prot);

			s->len = uaddr - (char*)s->base;

			*ns1 = *s;
			ns1->prot = prot;
			ns1->base = uaddr;
			ns1->len = ulen;
			if (ns1->vn) {
				vn_reference(ns1->vn);
				ns1->off += s->len;
			}
			list_insert(&s->link, &ns1->link);

			*ns2 = *s;
			ns2->base = uend;
			ns2->len = send - uend;
			if (ns2->vn) {
				vn_reference(ns2->vn);
				ns2->off += s->len + ns1->len;
			}
			list_insert(&ns1->link, &ns2->link);

			break;
		} else if (s->base < uaddr) {
			/* end of segment */
			seg *ns;
			if (!(ns = (seg*)kmem_alloc(sizeof(seg), MA_FAST)))
				return DERR(-ENOMEM);

			const auto l = uaddr - (char*)s->base;
			err = as_mprotect(a, uaddr, s->len - l, prot);

			*ns = *s;
			ns->prot = prot;
			ns->base = uaddr;
			ns->len = s->len - l;
			if (ns->vn) {
				vn_reference(ns->vn);
				ns->off += s->len;
			}
			list_insert(&s->link, &ns->link);

			s->len = l;
		} else if (s->base < uend) {
			/* start of segment */
			seg *ns;
			if (!(ns = (seg*)kmem_alloc(sizeof(seg), MA_FAST)))
				return DERR(-ENOMEM);
			const auto l = uend - (char*)s->base;
			err = as_mprotect(a, s->base, l, prot);

			*ns = *s;
			ns->prot = prot;
			ns->len = l;
			if (ns->vn)
				vn_reference(ns->vn);
			list_insert(list_prev(&s->link), &ns->link);

			if (s->vn)
				s->off += l;
			s->base = (char*)s->base + l;
			s->len -= l;
		} else
			panic("BUG");
		if (err < 0)
			break;
	}

	seg_combine(a);
	return err;
}

/*
 * vm_set_brk - initialise program break
 */
void
vm_init_brk(as *a, void *brk)
{
	assert(!a->brk);
	assert(!((uintptr_t)brk & PAGE_MASK));
	a->brk = brk;
}

/*
 * vm_readv
 */
ssize_t
vm_readv(as *a, const iovec *liov, size_t lc, const iovec *riov, size_t rc)
{
	return do_vm_io(a, liov, lc, riov, rc, vm_read);
}

/*
 * vm_writev
 */
ssize_t
vm_writev(as *a, const iovec *liov, size_t lc, const iovec *riov, size_t rc)
{
	return do_vm_io(a, liov, lc, riov, rc, vm_write);
}

/*
 * sc_mmap2
 */
void*
sc_mmap2(void *addr, size_t len, int prot, int flags, int fd, int pgoff)
{
	/* mmap maps whole pages, Apex requires that addr is page aligned */
	return mmapfor(task_cur()->as, addr, PAGE_ALIGN(len), prot, flags, fd,
	    (off_t)pgoff * PAGE_SIZE, MA_NORMAL);
}

/*
 * sc_munmap
 */
int
sc_munmap(void *addr, size_t len)
{
	/* munmap unmaps any whole page in the range [addr, addr + len) */
	return munmapfor(task_cur()->as, PAGE_TRUNC(addr), PAGE_ALIGN(len));
}

/*
 * sc_mprotect
 */
int
sc_mprotect(void *addr, size_t len, int prot)
{
	return mprotectfor(task_cur()->as, addr, len, prot);
}

/*
 * sc_madvise
 */
int
sc_madvise(void *vaddr, size_t ulen, int advice)
{
	if ((uintptr_t)vaddr & PAGE_MASK || ulen & PAGE_MASK)
		return DERR(-EINVAL);
	if (!ulen)
		return 0;

	switch (advice) {
	case MADV_NORMAL:
	case MADV_RANDOM:
	case MADV_SEQUENTIAL:
	case MADV_WILLNEED:
		return 0;
	case MADV_DONTNEED:
	case MADV_FREE:
		break;
	case MADV_REMOVE:
	case MADV_DONTFORK:
	case MADV_DOFORK:
	case MADV_MERGEABLE:
	case MADV_UNMERGEABLE:
	case MADV_HUGEPAGE:
	case MADV_NOHUGEPAGE:
	case MADV_DONTDUMP:
	case MADV_DODUMP:
	case MADV_WIPEONFORK:
	case MADV_KEEPONFORK:
	case MADV_COLD:
	case MADV_PAGEOUT:
	case MADV_HWPOISON:
	case MADV_SOFT_OFFLINE:
		return DERR(-ENOTSUP);
	default:
		return DERR(-EINVAL);
	}

	int err;
	as *a = task_cur()->as;

	interruptible_lock l(a->lock.write());
	if (err = l.lock(); err < 0)
		return err;

	const auto uaddr = (char*)vaddr;
	const auto uend = uaddr + ulen;

	seg *s;
	list_for_each_entry(s, &a->segs, link) {
		const auto send = (char*)s->base + s->len;
		if (send <= uaddr)
			continue;
		if (s->base >= uend)
			break;
		if (s->base >= uaddr && send <= uend) {
			/* entire segment */
			err = as_madvise(a, s, s->base, s->len, advice);
		} else if (s->base < uaddr && send > uend) {
			/* part of segment */
			err = as_madvise(a, s, uaddr, ulen, advice);
			break;
		} else if (s->base < uaddr) {
			/* end of segment */
			const auto l = uaddr - (char*)s->base;
			err = as_madvise(a, s, uaddr, s->len - l, advice);
		} else if (s->base < uend) {
			/* start of segment */
			const auto l = uend - (char*)s->base;
			err = as_madvise(a, s, s->base, l, advice);
		} else
			panic("BUG");
		if (err < 0)
			break;
	}

	return err;
}

/*
 * sc_brk
 */
void *
sc_brk(void *addr)
{
	auto a = task_cur()->as;

	if (!addr)
		return a->brk;

	interruptible_lock l(a->lock.write());
	if (auto r = l.lock(); r < 0)
		return (void*)r;

	/* shrink */
	if (addr < a->brk)
		if (auto r = do_munmapfor(a, addr,
		    (uintptr_t)a->brk - (uintptr_t)addr, false); r < 0)
			return (void*)r;

	/* grow */
	if (addr > a->brk)
		if (auto r = do_mmapfor(a, a->brk,
		    (uintptr_t)addr - (uintptr_t)a->brk,
		    PROT_READ | PROT_WRITE,
		    MAP_FIXED | MAP_ANONYMOUS | MAP_PRIVATE,
		    0, 0, MA_NORMAL); r > (void*)-4096UL)
			return r;

	return a->brk = addr;
}

/*
 * vm_init
 */
void
vm_init()
{

}

/*
 * vm_dump
 */
void
vm_dump()
{
	info("vm dump\n");
	info("=======\n");
	info(" Address space for kernel\n");
	as_dump(kern_task.as);

	struct task *task;
	list_for_each_entry(task, &kern_task.link, link) {
		dbg(" Address space for %s\n", task->path);
		as_dump(task->as);
	}
}

/*
 * as_init
 */
as *
as_create(pid_t pid)
{
	auto a = std::make_unique<as>();

	list_init(&a->segs);

#if defined(CONFIG_MMU)
	a->base = PAGE_SIZE;
	a->len = CONFIG_USER_LIMIT - PAGE_SIZE;
	if (auto r = mmu_newmap(&a->pgd, pid); r < 0)
		return r;
#else
	a->base = 0;
	a->len = SIZE_MAX;
#endif
	a->brk = 0;
	a->ref = 1;

	return a.release();
}

/*
 * as_copy
 */
as *
as_copy(as *a, pid_t pid)
{
	/* REVISIT: implement */
	return (as*)DERR(-ENOSYS);
}

/*
 * as_destroy
 */
void
as_destroy(as *a)
{
	if (--a->ref > 0) {
		a->lock.write().unlock();
		return;
	}

	seg *s, *tmp;
	list_for_each_entry_safe(s, tmp, &a->segs, link) {
		as_unmap(a, s->base, s->len, s->vn, s->off);
		if (s->vn)
			vn_close(s->vn);
		kmem_free(s);
	}
	a->lock.write().unlock();
	kmem_free(a);
}

/*
 * as_reference
 */
void
as_reference(as *a)
{
	++a->ref;
}

/*
 * as_transfer_begin - start transfer during which address space must not change
 */
int
as_transfer_begin(as *a)
{
	return a->lock.read().lock();
}

int
as_transfer_begin_interruptible(as *a)
{
	return a->lock.read().interruptible_lock();
}

/*
 * as_transfer_end - finish transfer to address space memory
 */
void
as_transfer_end(as *a)
{
	a->lock.read().unlock();
}

/*
 * as_locked - query state of address space lock
 */
bool
as_locked(as *a)
{
	return a->lock.locked();
}

/*
 * as_modify_begin - start transaction which will modify address space
 */
int
as_modify_begin(as *a)
{
	return a->lock.write().lock();
}

int
as_modify_begin_interruptible(as *a)
{
	return a->lock.write().interruptible_lock();
}

/*
 * as_modify_end - finish transaction which modified address space
 */
void
as_modify_end(as *a)
{
	a->lock.write().unlock();
}

/*
 * as_dump
 */
void
as_dump(const as *a)
{
	seg *s;
	list_for_each_entry(s, &a->segs, link) {
		info("  %p-%p %c%c%c%c %8lld %s\n",
		    s->base, (char*)s->base + s->len,
		    s->prot & PROT_READ ? 'r' : '-',
		    s->prot & PROT_WRITE ? 'w' : '-',
		    s->prot & PROT_EXEC ? 'x' : '-',
		    'p',	/* REVISIT: shared regions */
		    s->off,
		    s->vn ? vn_name(s->vn) : "");
	}
}

/*
 * as_find_seg - find segment containing address
 *
 * REVISIT: This really needs to be as fast as possible. We should use some
 * kind of tree rather than a linear search in the future.
 */
__fast_text const seg *
as_find_seg(const as *a, const void *uaddr)
{
	seg *s;
	list_for_each_entry(s, &a->segs, link) {
		if (seg_begin(s) <= uaddr && seg_end(s) > uaddr)
			return s;
	}
	return nullptr;
}

/*
 * seg_begin - get start address of segment
 */
void *
seg_begin(const seg *s)
{
	return s->base;
}

/*
 * seg_end - get end address of segment
 */
void *
seg_end(const seg *s)
{
	return (char *)s->base + s->len;
}

/*
 * seg_size - get size of segment
 */
size_t
seg_size(const seg *s)
{
	return s->len;
}

/*
 * seg_prot - get protection flags for segment
 */
int
seg_prot(const seg *s)
{
	return s->prot;
}

/*
 * seg_vnode - get vnode backing segment
 */
vnode *
seg_vnode(seg *s)
{
	return s->vn;
}

/*
 * as_insert - insert memory into address space (nommu)
 */
int
as_insert(as *a, std::unique_ptr<phys> pages, size_t len, int prot,
    int flags, std::unique_ptr<vnode> vn, off_t off, long attr)
{
	int err;
	const bool fixed = flags & MAP_FIXED;

	/* remove any existing mappings */
	if (fixed && (err = do_munmapfor(a, pages.get(), len, true)) < 0)
		return err;

	/* find insertion point */
	seg *s;
	list_for_each_entry(s, &a->segs, link) {
		if (s->base > pages.get())
			break;
	}
	s = list_entry(list_prev(&s->link), seg, link);

	/* insert new segment */
	if ((err = seg_insert(s, std::move(pages), len, prot, std::move(vn),
	    off, attr)) < 0)
		return err;

	seg_combine(a);
	return 0;
}
