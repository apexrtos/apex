/*
 * page.cpp - phyiscal page allocator
 *
 * The physical page allocator is responsible for allocating physical memory
 * and keeping track of free memory.
 *
 * The caller needs to remember which pages have been allocated and free them
 * when they are no longer required.
 *
 * This allocator supports partially freeing allocations.
 *
 * The allocator is implemented using a buddy allocation scheme with a bitmap
 * optimising the lookup of buddy states. There is one bit in the bitmap for
 * each pair of buddies. If the bitmap contains a '1' it indicates that only
 * one of the two buddies is free.
 *
 * Example bitmap - bit_index (buddy_a, buddy_b)
 * [		     0 (0,4)		      ] order 2 (4 pages)
 * [	  1 (0,2)	][	2 (4,6)       ] order 1 (2 pages)
 * [ 3 (0,1) ][ 4 (2,3) ][ 5 (4,5) ][ 6 (6,7) ] order 0 (1 page)
 *
 * TODO: optimise: don't allocate page structs for holes at beginning & end
 */
#include <page.h>

#include <algorithm>
#include <bootargs.h>
#include <cassert>
#include <cinttypes>
#include <cstdio>
#include <cstring>
#include <debug.h>
#include <elf_native.h>
#include <errno.h>
#include <kernel.h>
#include <lib/expect.h>
#include <list.h>
#include <sync.h>

enum PG_STATE {
	PG_FREE,		/* Free page */
	PG_HOLE,		/* No backing physical memory, cannot free */
	PG_SYSTEM,		/* Kernel, page info, etc, cannot free */
	PG_FIXED,		/* Page must remain fixed in memory */
	PG_MAPPED,		/* Page is part of a vm mapping, can move */
};

using paddr_t = phys::value_type;

#if defined(CONFIG_PAE)
#define PRIpa PRIx64
#else
#define PRIpa PRIxPTR
#endif

struct page {
	PG_STATE state;
	void *owner;
	list link;
};

struct region {
	unsigned long attr;	/* Region attributes, bitfield of MA_* */
	a::spinlock lock;	/* For pages, blocks & bitmap */
	paddr_t begin;		/* First physical address in region */
	paddr_t end;		/* Last physical address in region + 1 */
	paddr_t base;		/* power-of-2 aligned base address of region */
	size_t usable;		/* Total usable bytes in region */
	size_t free;		/* Total free bytes in region */
	size_t size;		/* Total size of region */
	size_t nr_orders;	/* size = PAGE_SIZE * 2^nr_orders */
	size_t nr_pages;	/* Number of pages in pages array */
	unsigned priority;	/* Region priority */
	page *pages;		/* Page descriptors */
	list *blocks;		/* Linked list of free blocks for each order */
	unsigned long *bitmap;	/* Bitmap of allocated ranges */
};

static struct {
	region *regions;
	region **regions_by_priority;
	size_t nr_regions;
} s;

/*
 * Page ownership identifier for page allocator
 */
static char page_id;

/*
 * to_string - convert enums to strings for display
 */
constexpr const char *to_string(PG_STATE v)
{
	switch (v) {
	case PG_FREE: return "FREE";
	case PG_HOLE: return "HOLE";
	case PG_SYSTEM: return "SYSTEM";
	case PG_FIXED: return "FIXED";
	case PG_MAPPED: return "MAPPED";
	}
	return nullptr;
}

/*
 * bitmap_size - number of bitmap bits required for region
 */
static size_t
bitmap_size(const region &r)
{
	return (1 << (r.nr_orders - 1)) - 1;
}

/*
 * order_to_first_bitmap_idx - first bitmap index of order
 */
static size_t
order_to_first_bitmap_idx(const region &r, const size_t o)
{
	assert(o <= r.nr_orders - 2);
	return (1 << (r.nr_orders - o - 2)) - 1;
}

/*
 * page_to_bitmap_idx - bitmap index for page in block of order
 */
static size_t
page_to_bitmap_idx(const region &r, const size_t page, const size_t o)
{
	return order_to_first_bitmap_idx(r, o) + page / (1 << (o + 1));
}

/*
 * bitmap_toggle - toggle bitmap bit at index
 */
static bool
bitmap_toggle(const region &r, const size_t idx)
{
	assert(idx < bitmap_size(r));
	constexpr auto bits_per_word = sizeof *r.bitmap * 8;
	const auto word_idx = idx / bits_per_word;
	const auto bit_idx = idx % bits_per_word;
	return (r.bitmap[word_idx] ^= 1ul << bit_idx) & 1ul << bit_idx;
}

/*
 * bitmap_toggle - toggle bitmap bit for page in block of order
 */
static bool
bitmap_toggle(const region &r, const size_t page, const size_t o)
{
	return bitmap_toggle(r, page_to_bitmap_idx(r, page, o));
}

/*
 * first_page_in_block - first page in block of order containing page
 */
static size_t
first_page_in_block(const size_t page, const size_t o)
{
	return (page >> o) << o;
}

/*
 * page_to_max_order - maximum allocation order for page
 */
static size_t
page_to_max_order(const region &r, const size_t page)
{
	if (!page)
		return r.nr_orders - 1;
	return std::countr_zero(page);
}

/*
 * find_region - find region containing pages
 */
static region *
find_region(const paddr_t begin, size_t len)
{
	const paddr_t end = begin + len;
	if (end < begin)
		return nullptr;
	for (size_t i = 0; i < s.nr_regions; ++i) {
		auto &r = s.regions[i];
		if (begin >= r.begin && end <= r.end)
			return &r;
	}
	return nullptr;
}

/*
 * page_num - get page number for address in region
 */
static size_t
page_num(const region &r, const paddr_t addr)
{
	assert(addr >= r.base && addr - r.base < r.size);
	return (addr - r.base) / PAGE_SIZE;
}

/*
 * page_addr - get page address for page in region
 */
static phys
page_addr(const region &r, const size_t page)
{
	assert(page < r.nr_pages);
	return phys{r.base + page * PAGE_SIZE};
}

/*
 * block_alloc - allocate block in region 'r' starting at page 'page' of size
 *		 1 << 'o' pages
 */
static void
block_alloc(region &r, const size_t page, const size_t o)
{
	assert(page_to_max_order(r, page) >= o);

	/* split blocks if necessary */
	size_t i = o;
	for (; i < r.nr_orders - 1; ++i) {
		/* if bit is now clear then only one block was free */
		if (!bitmap_toggle(r, page, i))
			break;
	}
	for (; i > o; --i) {
		/* split blocks */
		const auto pa = first_page_in_block(page, i);
		const auto pb = pa + (1 << (i - 1));
		list_remove(&r.pages[pa].link);
		list_insert(&r.blocks[i - 1], &r.pages[pa].link);
		list_insert(&r.blocks[i - 1], &r.pages[pb].link);
	}

	/* remove allocated block from free list */
	list_remove(&r.pages[page].link);
}

/*
 * block_free
 */
static void
block_free(region &r, const size_t page, size_t o)
{
	assert(page_to_max_order(r, page) >= o);

	/* insert free block into free list */
	list_insert(&r.blocks[o], &r.pages[page].link);

	/* join blocks if necessary */
	for (; o != r.nr_orders - 1; ++o) {
		/* if bit is now set then only one block is free */
		if (bitmap_toggle(r, page, o))
			return;

		/* join blocks */
		const auto pa = first_page_in_block(page, o + 1);
		const auto pb = pa + (1 << o);
		list_remove(&r.pages[pa].link);
		list_remove(&r.pages[pb].link);
		list_insert(&r.blocks[o + 1], &r.pages[pa].link);
	}
}

/*
 * do_alloc
 */
static page_ptr
do_alloc(region &r, const size_t page, const size_t o, const PG_STATE st,
    void *owner)
{
	assert(st != PG_FREE);
	assert(o < r.nr_orders);

	const size_t len = PAGE_SIZE << o;

	/* holes & system memory can never be released */
	if (st == PG_HOLE || st == PG_SYSTEM)
		r.usable -= len;
	r.free -= len;

	/* set page states */
	for (auto i = page; i != page + (1 << o); ++i) {
		auto &p = r.pages[i];
		assert(p.state == PG_FREE);
		p.state = st;
		p.owner = owner;
	}

	/* update buddy allocator */
	block_alloc(r, page, o);

	return {page_addr(r, page), len, owner};
}

/*
 * page_alloc_order - allocate physical memory of size 1 << 'o' pages with
 *		      attributes 'attr'
 *
 * tries to allocate using requested attributes but falls back if memory is low.
 */
page_ptr
page_alloc_order(const size_t o, unsigned long attr, void *owner)
{
	/* extract page allocation flags */
	const auto st = attr & PAF_MAPPED ? PG_MAPPED : PG_FIXED;
	const auto exact_speed = attr & PAF_EXACT_SPEED;
	attr &= ~PAF_MASK;

	/* find_block returns first page of free block with order >= o */
	auto find_block = [](const region &r, const size_t o) -> ptrdiff_t {
		if (o >= r.nr_orders)
			return -1;
		for (auto fl = r.blocks + o; fl != r.blocks + r.nr_orders; ++fl) {
			if (list_empty(fl))
				continue;
			return list_entry(list_first(fl), page, link) - r.pages;
		}
		return -1;
	};

	while (true) {
		/* find first region with compatible attributes */
		for (size_t i = 0; i < s.nr_regions; ++i) {
			auto &r = *s.regions_by_priority[i];
			if ((r.attr & attr) != attr)
				continue;
			if (exact_speed &&
			    (r.attr & MA_SPEED_MASK) != (attr & MA_SPEED_MASK))
				continue;
			std::lock_guard l(r.lock);
			const auto p = find_block(r, o);
			if (p != -1)
				return do_alloc(r, p, o, st, owner);
		}

		/* try again allowing slower regions */
		if (!exact_speed && attr & MA_SPEED_MASK) {
			attr &= ~MA_SPEED_MASK;
			continue;
		}

		/* can't find suitable pages */
		return page_ptr{};
	}

	return page_ptr{};
}

/*
 * page_alloc - allocate physical pages of size 'len' in region of type 'mt'
 *              for use in allocation of type 'at'
 *
 * tries to allocate using requested type but falls back if memory is low.
 * 'len' is rounded up to next page sized boundary.
 */
page_ptr
page_alloc(size_t len, unsigned long attr, void *owner)
{
	if (len == 0)
		return page_ptr{};
	len = PAGE_ALIGN(len);
	const auto order = ceil_log2(len) - floor_log2(PAGE_SIZE);
	auto pages = page_alloc_order(order, attr, owner);
	if (!pages)
		return pages;
	const auto excess = (PAGE_SIZE << order) - len;
	page_free(pages.get(), excess, owner);
	return {phys{pages.release().phys() + excess}, len, owner};
}

/*
 * page_reserve - reserve pages in region 'r' starting at physical address
 *		  'addr' of size 'len' with state 'st'
 *
 * 'addr' and 'len' are rounded to the nearest page boundaries.
 * extending an existing allocation is supported.
 * returns invalid address on failure, physical address otherwise.
 */
static expect<phys>
page_reserve(region &r, const paddr_t addr, const size_t len,
    const PG_STATE st, unsigned long attr, void *owner)
{
	assert(st == PG_HOLE || st == PG_SYSTEM || st == PG_FIXED);
	if (len == 0)
		return phys{addr};

	/* find page range */
	const auto begin = page_num(r, addr);
	const auto end = page_num(r, addr + len - 1) + 1;

	/* check that range is free or overlaps existing allocation */
	for (auto i = begin; i != end; ++i) {
		if (r.pages[i].state == PG_FREE)
			continue;
		if (r.pages[i].state == st && r.pages[i].owner == owner &&
		    attr & PAF_REALLOC)
			continue;
		return std::errc::address_in_use;
	}

	/* reserve pages */
	for (auto i = begin; i != end; ++i) {
		if (r.pages[i].state != PG_FREE)
			continue;
		do_alloc(r, i, 0, st, owner).release();
	}

	return page_addr(r, begin);
}

/*
 * page_reserve - reserve pages starting at physical address 'addr' of size
 *		  'len' with state 'st'
 *
 * 'addr' and 'len' are rounded to the nearest page boundaries.
 * returns invalid address on failure, physical address otherwise.
 */
static expect<phys>
page_reserve(paddr_t addr, size_t len, const PG_STATE st, unsigned long attr,
	     void *owner)
{
	auto *r = find_region(addr, len);
	if (!r)
		return std::errc::invalid_argument;
	return page_reserve(*r, addr, len, st, attr, owner);
}

/*
 * page_reserve - reserve pages starting at physical address 'addr' of size
 *                'len'
 *
 * 'addr' and 'len' are rounded to the nearest page boundaries.
 */
page_ptr
page_reserve(phys addr, size_t len, unsigned long attr, void *owner)
{
	const auto st = attr & PAF_MAPPED ? PG_MAPPED : PG_FIXED;

	auto *r = find_region(addr.phys(), len);
	if (!r)
		return page_ptr{};
	std::lock_guard l(r->lock);
	auto p = page_reserve(*r, addr.phys(), len, st, attr, owner);
	if (!p.ok())
		return page_ptr{};
	return {p.val(), len, owner};
}

/*
 * page_free - free pages in region 'r' starting at 'page' of size
 *		     1 << 'o' pages
 *
 * Assumes that the caller knows that the range to be freed is allocated.
 */
static void
page_free(region &r, const size_t page, const size_t o)
{
	/* set page states */
	for (auto i = page; i != page + (1 << o); ++i) {
		auto &p = r.pages[i];
		assert(p.state == PG_FIXED || p.state == PG_MAPPED);
		p.state = PG_FREE;
		p.owner = nullptr;
	}

	r.free += PAGE_SIZE << o;

	/* update buddy allocator */
	block_free(r, page, o);
}

/*
 * page_free - free reserved or allocated pages
 *
 * returns 0 on success, -ve error code on failure
 */
expect_ok
page_free(phys addr, size_t len, void *owner)
{
	if (len == 0)
		return {};

	auto *r = find_region(addr.phys(), len);
	if (!r)
		return DERR(std::errc::bad_address);

	std::lock_guard l(r->lock);

	/* find page range */
	const auto begin = page_num(*r, addr.phys());
	const auto end = page_num(*r, addr.phys() + len - 1) + 1;

	/* verify that range is allocated and correctly owned */
	for (auto i = begin; i != end; ++i) {
		if (r->pages[i].owner != owner)
			return DERR(std::errc::invalid_argument);
		switch (r->pages[i].state) {
		case PG_FREE:
		case PG_HOLE:
		case PG_SYSTEM:
			return DERR(std::errc::bad_address);
		case PG_FIXED:
		case PG_MAPPED:
			continue;
		}
	}

	/* free pages */
	for (auto i = begin; i != end;) {
		const auto size = end - i;
		const auto o = std::min<size_t>(
		    page_to_max_order(*r, i), floor_log2(size));
		page_free(*r, i, o);
		i += 1 << o;
	}

	return {};
}

/*
 * page_valid - check if address range refers to valid, writable pages
 */
bool
page_valid(const phys addr, size_t len, void *owner)
{
	/* no need to lock - we aren't modifying anything, and after page_init
	   the page layout is immutable */

	/* find region */
	auto *r = find_region(addr.phys(), len);
	if (!r)
		return false;

	/* if length is 0, address is ok, in valid region */
	if (len == 0)
		return true;

	/* find page range */
	const auto begin = page_num(*r, addr.phys());
	const auto end = page_num(*r, addr.phys() + len - 1) + 1;

	/* verify that range is allocated and correctly owned */
	for (auto i = begin; i != end; ++i) {
		if (r->pages[i].owner != owner)
			return false;
		switch (r->pages[i].state) {
		case PG_HOLE:
		case PG_SYSTEM:
			return false;
		case PG_FREE:
		case PG_FIXED:
		case PG_MAPPED:
			continue;
		}
	}

	return true;
}

/*
 * page_attr - retrieve page attributes
 */
expect_pos
page_attr(const phys addr, size_t len)
{
	/* no need to lock - we aren't modifying anything, and after page_init
	   the page layout is immutable */

	/* find region */
	auto *r = find_region(addr.phys(), len);
	if (!r)
		return DERR(std::errc::bad_address);

	return r->attr;
}

/*
 * page_init - initialise page allocator
 *
 * REVISIT: We currently allocate state starting at a page boundary. This is
 * wasteful as there could be a partially empty page available for use.
 */
void
page_init(const meminfo *mi, const size_t mi_size, const bootargs *args)
{
	/* make sure kernel ELF headers are sane */
	extern ElfN_Ehdr __elf_headers;
	const ElfN_Ehdr *eh = &__elf_headers;
	if (eh->e_ident[EI_MAG0] != ELFMAG0 ||
	    eh->e_ident[EI_MAG1] != ELFMAG1 ||
	    eh->e_ident[EI_MAG2] != ELFMAG2 ||
	    eh->e_ident[EI_MAG3] != ELFMAG3 ||
	    eh->e_ident[EI_CLASS] != ELFCLASSN)
		panic("bad ELF header");

	/* analyse meminfo to count regions & find the first piece of
	 * contiguous normal memory in which to allocate state */
	paddr_t m_alloc = 0;
	paddr_t m_end = 0;
	for (size_t i = 0; i < mi_size; ++i) {
		const auto &m = mi[i];
		++s.nr_regions;
		if (m_end)
			continue;
		if ((m.attr & MA_SPEED_MASK) != MA_NORMAL)
			continue;
		m_alloc = PAGE_ALIGN(m.base.phys());
		m_end = PAGE_TRUNC(m.base.phys() + m.size);
	}

	if (!m_end)
		panic("no memory");

	/* helper to iterate reserved memory regions in bootargs & kernel */
	auto for_each_reserved_range = [&](auto fn) {
		if (args->archive_size)
			fn(args->archive_addr, args->archive_size);
		const ElfN_Phdr *ph = (ElfN_Phdr *)((uintptr_t)eh + eh->e_phoff);
		for (size_t i = 0; i < eh->e_phnum; ++i, ++ph) {
			if (ph->p_type != PT_LOAD)
				continue;
			fn(virt_to_phys((void *)ph->p_vaddr).phys(), ph->p_memsz);
		}
	};

	/* adjust m_alloc and m_end for reserved areas */
	for_each_reserved_range([&] (paddr_t p, size_t len) {
		const paddr_t r_begin = PAGE_TRUNC(p);
		const paddr_t r_end = PAGE_ALIGN(p + len);

		/* no overlap */
		if (r_end < m_alloc || r_begin >= m_end)
			return;
		if (r_begin <= m_alloc) {
			/* reserved start */
			m_alloc = r_end;
		} else if (r_end >= m_end) {
			/* reserved end */
			m_end = r_begin;
		} else if (r_begin - m_alloc > m_end - r_end) {
			/* reserved hole -- lower region larger */
			m_end = r_begin;
		} else {
			/* reserved hole -- higher region larger */
			m_alloc = r_end;
		}
		if (m_alloc >= m_end)
			panic("no memory");
	});

	dbg("page_init: allocate state at %" PRIpa "(%" PRIpa " bytes "
	    "usable), %zu regions\n", m_alloc, m_end - m_alloc, s.nr_regions);

	const paddr_t m_begin = m_alloc;
	auto alloc = [&](size_t len) {
		len = ALIGN(len);
		if (len > (size_t)(m_end - m_alloc))
			panic("OOM");
		void *tmp = phys_to_virt(phys{m_alloc});
		m_alloc += len;
		memset(tmp, 0, len);
		return tmp;
	};

	/* allocate regions */
	s.regions = (region*)alloc(sizeof *s.regions * s.nr_regions);
	s.regions_by_priority = (region**)alloc(sizeof(s.regions) * s.nr_regions);

	/* initialise regions & pages in ascending address order */
	paddr_t init_addr = 0;
	for (size_t i = 0; i != s.nr_regions; ++i) {
		const meminfo *m = nullptr;
		for (size_t j = 0; j < mi_size; ++j) {
			const auto &t = mi[j];
			if (t.base.phys() < init_addr)
				continue;
			if (!m || (t.base.phys() >= init_addr &&
				   t.base.phys() < m->base.phys()))
				m = &t;
		}
		assert(m);

		region &r = *new(s.regions + i) region;
		r.attr = m->attr;
		r.begin = PAGE_ALIGN(m->base.phys());
		r.end = PAGE_TRUNC(m->base.phys() + m->size);
		const size_t size_order = ceil_log2(r.end - r.begin);
		r.base = TRUNCn(r.begin, 1 << size_order);
		r.size = ALIGNn(r.end, 1 << size_order) - r.base;
		const size_t max_order = ceil_log2(r.size);
		r.nr_orders = max_order - floor_log2(PAGE_SIZE) + 1;
		r.nr_pages = 1 << (r.nr_orders - 1);
		r.priority = m->priority;
		r.size = r.nr_pages * PAGE_SIZE;
		r.usable = r.size;
		r.free = r.size;

		/* allocate pages */
		r.pages = (page*)alloc(sizeof *r.pages * r.nr_pages);

		/* allocate memory for buddy allocator */
		r.blocks = (list*)alloc(r.nr_orders * sizeof *r.blocks);
		r.bitmap = (unsigned long*)alloc((bitmap_size(r) + 7) / 8);
		for (size_t k = 0; k < r.nr_orders; ++k)
			list_init(r.blocks + k);
		list_insert(&r.blocks[r.nr_orders - 1], &r.pages[0].link);

		/* reserve pages without physical backing */
		if (!page_reserve(r, r.base, r.begin - r.base, PG_HOLE, 0, nullptr).ok())
			panic("bad meminfo");
		if (!page_reserve(r, r.end, r.base + r.size - r.end, PG_HOLE, 0, nullptr).ok())
			panic("bad meminfo");

		dbg("page_init: region %zu: %" PRIpa " -> %" PRIpa" covering "
		    "%" PRIpa " -> %" PRIpa "\n",
		    i, r.base, r.base + r.size, r.begin, r.end);

		init_addr = r.end;
	}

	/* make sure that regions don't overlap */
	for (size_t i = 1; i < s.nr_regions; ++i) {
		if (s.regions[i - 1].end > s.regions[i].begin)
			panic("overlapping regions");
	}

	/* reserve unusable memory */
	for_each_reserved_range([&](paddr_t p, size_t len) {
		/* find regions overlapping range */
		for (size_t i = 0; i < s.nr_regions; ++i) {
			auto &r = s.regions[i];
			auto begin = std::max(p, r.begin);
			auto end = std::min(p + len, r.end);
			if (begin >= end)
				continue;
			dbg("page_init: reserve %" PRIpa " -> %" PRIpa "\n",
			    begin, end);
			if (!page_reserve(r, begin, end - begin,
					  PG_SYSTEM, PAF_REALLOC, &kern_task).ok())
				panic("bug");
		}
	});

	/* reserve memory allocated for region & page structures */
	if (!page_reserve(m_begin, m_alloc - m_begin, PG_SYSTEM, 0, &page_id).ok())
		panic("bug");

	/* initialise regions_by_priority */
	for (size_t priority = 0, pri_idx = 0; pri_idx < s.nr_regions;) {
		for (size_t i = 0; i < s.nr_regions; ++i) {
			auto &r = s.regions[i];
			if (r.priority == priority)
				s.regions_by_priority[pri_idx++] = &r;
		}
		++priority;
	}

#if defined(CONFIG_DEBUG)
	page_dump();
#endif
}

/*
 * page_dump - dump page allocator state
 */
void page_dump()
{
	info("page dump\n");
	info("=========\n");
	for (size_t i = 0; i < s.nr_regions; ++i) {
#if defined(CONFIG_INFO)
		const region &r = s.regions[i];
#endif
		info(" %" PRIpa " -> %" PRIpa "\n", r.begin, r.end);
		info("  attr      speed %ld%s%s%s%s\n",
		    r.attr & MA_SPEED_MASK,
		    r.attr & MA_DMA ? ", dma" : "",
		    r.attr & MA_CACHE_COHERENT ? ", coherent" : "",
		    r.attr & MA_PERSISTENT ? ", persistent" : "",
		    r.attr & MA_SECURE ? ", secure" : "");
		info("  base      %" PRIpa "\n", r.base);
		info("  size      %zu\n", r.size);
		info("  usable    %zu\n", r.usable);
		info("  free      %zu\n", r.free);
		info("  nr_orders %zu\n", r.nr_orders);
		info("  nr_pages  %zu\n", r.nr_pages);
		info("  priority  %u\n", r.priority);

		constexpr auto bufsz = 128;
		char buf[bufsz], *s = buf;
		for (int j = r.nr_orders - 1; j >= 0; --j) {
			int n = 0;
			page *p;
			list_for_each_entry(p, r.blocks + j, link)
				++n;
			s += snprintf(s, buf + bufsz - s, "%d ", n);
		}
		s[-1] = 0;
		info("  available %s\n", buf);

		info("  allocated\n");
		for (size_t j = 0; j < r.nr_pages; ++j) {
			const page *begin = r.pages + j;

			if (begin->state == PG_FREE)
				continue;

			const page *end = begin;

			while (end->state == begin->state &&
			       end->owner == begin->owner)
				++end;

			info("    %zu[%" PRIdPTR "]: %s %p\n", j, end - begin,
			     to_string(begin->state), begin->owner);

			j += end - begin - 1;
		}
	}
}

/*
 * page_ptr
 */
page_ptr::page_ptr()
: size_{0}
{}

page_ptr::page_ptr(phys p, size_t size, void *owner)
: phys_{p}
, size_{size}
, owner_{owner}
{}

page_ptr::page_ptr(page_ptr &&o)
: phys_{o.phys_}
, size_{o.size_}
, owner_{o.owner_}
{
	o.size_ = 0;
}

page_ptr::~page_ptr()
{
	if (!size_)
		return;
	page_free(phys_, size_, owner_);
}

page_ptr &
page_ptr::operator=(page_ptr &&o)
{
	phys_ = o.phys_;
	size_ = o.size_;
	owner_ = o.owner_;
	o.size_ = 0;
	return *this;
}

phys
page_ptr::release()
{
	assert(size_);
	size_ = 0;
	return phys_;
}

void
page_ptr::reset()
{
	if (!size_)
		return;
	page_free(phys_, size_, owner_);
	size_ = 0;
}

phys
page_ptr::get()
{
	assert(size_);
	return phys_;
}

size_t
page_ptr::size() const
{
	return size_;
}

page_ptr::operator bool() const
{
	return size_;
}
