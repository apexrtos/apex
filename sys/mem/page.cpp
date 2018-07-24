/*
 * page.cpp - phyiscal page allocator
 *
 * The physical page allocator is responsible for allocating physical memory
 * and keeping track of free memory.
 *
 * Memory is split into regions based on boot information memory type. Only
 * one region per type is supported at this time.
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
#include <bootinfo.h>
#include <cassert>
#include <cstdlib>
#include <cstring>
#include <debug.h>
#include <errno.h>
#include <inttypes.h>
#include <kernel.h>
#include <list.h>
#include <mutex>
#include <sync.h>

enum PG_STATE {
	PG_FREE,		/* Free page */
	PG_HOLE,		/* No backing physical memory, cannot free */
	PG_SYSTEM,		/* Kernel, page info, etc, cannot free */
	PG_FIXED,		/* Page must remain fixed in memory */
	PG_MAPPED,		/* Page is part of a vm mapping, can move */
};

struct page {
	PG_STATE state;
	void *owner;
	struct list link;
};

struct region {
	MEM_TYPE type;		/* Type of region, MEM_* */
	a::mutex mutex;		/* For pages, blocks & bitmap */
	phys *begin;		/* First physical address in region */
	phys *end;		/* Last physical address in region + 1 */
	phys *base;		/* power-of-2 aligned base address of region */
	size_t usable;		/* Total usable bytes in region */
	size_t free;		/* Total free bytes in region */
	size_t size;		/* Total size of region */
	size_t nr_orders;	/* size = CONFIG_PAGE_SIZE * 2^nr_orders */
	size_t nr_pages;	/* Number of pages in pages array */
	page *pages;		/* Page descriptors */
	list *blocks;		/* Linked list of free blocks for each order */
	unsigned long *bitmap;	/* Bitmap of allocated ranges */
};

static struct {
	region *regions;
	size_t nr_regions;
	size_t bootdisk_size;
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

constexpr const char *to_string(MEM_TYPE v)
{
	switch (v) {
	case MEM_NORMAL: return "NORMAL";
	case MEM_FAST: return "FAST";
	case MEM_ROM: return "ROM";
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
	return __builtin_ctz(page);
}

/*
 * find_region - find region containing pages
 */
static region *
find_region(phys *begin, size_t len)
{
	const phys *end = begin + len;
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
page_num(const region &r, const phys *addr)
{
	assert(addr >= r.base && addr < (r.base + r.size));
	return (addr - r.base) / CONFIG_PAGE_SIZE;
}

/*
 * page_addr - get page address for page in region
 */
static phys *
page_addr(const region &r, const size_t page)
{
	assert(page < r.nr_pages);
	return r.base + page * CONFIG_PAGE_SIZE;
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
static phys *
do_alloc(region &r, const size_t page, const size_t o, const PG_STATE st,
    void *owner)
{
	assert(st != PG_FREE);
	assert(o < r.nr_orders);

	const size_t len = CONFIG_PAGE_SIZE << o;

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

	return page_addr(r, page);
}

/*
 * page_alloc_order - allocate physical memory of size 1 << 'o' pages in region
 *		      of type 'mt' with state 'st'
 *
 * tries to allocate using requested type but falls back if memory is low.
 * returns 0 on failure, physical address otherwise.
 */
phys *
page_alloc_order(const size_t o, const MEM_TYPE mt, const PAGE_ALLOC_TYPE at,
    void *owner)
{
	const auto st = at == PAGE_ALLOC_FIXED ? PG_FIXED : PG_MAPPED;

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

	/* try to allocate in requested type first */
	for (size_t i = 0; i < s.nr_regions; ++i) {
		auto &r = s.regions[i];
		if (r.type != mt)
			continue;
		std::lock_guard l(r.mutex);
		const auto p = find_block(r, o);
		if (p != -1)
			return do_alloc(r, p, o, st, owner);
	}

	/* if that failed try to allocate anywhere */
	for (size_t i = 0; i < s.nr_regions; ++i) {
		auto &r = s.regions[i];
		if (r.type == mt)
			continue;
		std::lock_guard l(r.mutex);
		const auto p = find_block(r, o);
		if (p != -1)
			return do_alloc(r, p, o, st, owner);
	}

	return 0;
}

/*
 * page_alloc - allocate physical pages of size 'len' in region of type 'mt'
 *              for use in allocation of type 'at'
 *
 * tries to allocate using requested type but falls back if memory is low.
 * 'len' is rounded up to next page sized boundary.
 * returns 0 on failure, physical address otherwise.
 */
phys *
page_alloc(size_t len, MEM_TYPE mt, const PAGE_ALLOC_TYPE at, void *owner)
{
	if (len == 0)
		return 0;
	len = PAGE_ALIGN(len);
	const auto order = ceil_log2(len) - floor_log2(CONFIG_PAGE_SIZE);
	const auto addr = page_alloc_order(order, mt, at, owner);
	if (addr == 0)
		return 0;
	page_free(addr + len, (CONFIG_PAGE_SIZE << order) - len, owner);
	return addr;
}

/*
 * page_reserve - reserve pages in region 'r' starting at physical address
 *		  'addr' of size 'len' with state 'st'
 *
 * 'addr' and 'len' are rounded to the nearest page boundaries.
 * returns 0 on failure, physical address otherwise.
 */
static phys *
page_reserve(region &r, phys *const addr, const size_t len,
    const PG_STATE st, void *owner)
{
	assert(st == PG_HOLE || st == PG_SYSTEM || st == PG_FIXED);
	if (len == 0)
		return addr;

	/* find page range */
	const auto begin = page_num(r, addr);
	const auto end = page_num(r, addr + len - 1) + 1;

	/* verify that range is free */
	for (auto i = begin; i != end; ++i)
		if (r.pages[i].state != PG_FREE)
			return 0;

	/* reserve pages */
	for (auto i = begin; i != end;) {
		const auto size = end - i;
		const auto o = std::min<size_t>(
		    page_to_max_order(r, i), floor_log2(size));
		do_alloc(r, i, o, st, owner);
		i += 1 << o;
	}

	return page_addr(r, begin);
}

/*
 * page_reserve - reserve pages starting at physical address 'addr' of size
 *		  'len' with state 'st'
 *
 * 'addr' and 'len' are rounded to the nearest page boundaries.
 * returns 0 on failure, physical address otherwise.
 */
static phys *
page_reserve(phys *addr, size_t len, const PG_STATE st, void *owner)
{
	auto *r = find_region(addr, len);
	if (!r)
		return 0;
	return page_reserve(*r, addr, len, st, owner);
}

/*
 * page_reserve - reserve pages starting at physical address 'addr' of size
 *                'len'
 *
 * 'addr' and 'len' are rounded to the nearest page boundaries.
 * returns 0 on failure, physical address otherwise.
 */
phys *
page_reserve(phys *addr, size_t len, void *owner)
{
	auto *r = find_region(addr, len);
	if (!r)
		return 0;
	std::lock_guard l(r->mutex);
	return page_reserve(*r, addr, len, PG_FIXED, owner);
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

	r.free += CONFIG_PAGE_SIZE << o;

	/* update buddy allocator */
	block_free(r, page, o);
}

/*
 * page_free - free reserved or allocated pages
 *
 * returns 0 on success, -ve error code on failure
 */
int
page_free(phys *addr, size_t len, void *owner)
{
	if (len == 0)
		return 0;

	auto *r = find_region(addr, len);
	if (!r)
		return DERR(-EFAULT);

	std::lock_guard l(r->mutex);

	/* find page range */
	const auto begin = page_num(*r, addr);
	const auto end = page_num(*r, addr + len - 1) + 1;

	/* verify that range is allocated and correctly owned */
	for (auto i = begin; i != end; ++i) {
		if (r->pages[i].owner != owner)
			return DERR(-EINVAL);
		switch (r->pages[i].state) {
		case PG_FREE:
		case PG_HOLE:
		case PG_SYSTEM:
			return DERR(-EFAULT);
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

	return 0;
}

/*
 * page_valid - check if address range refers to valid, writable pages
 */
bool
page_valid(phys *addr, size_t len, void *owner)
{
	/* no need to lock - we aren't modifying anything, and after page_init
	   the page layout is immutable */

	/* find region */
	auto *r = find_region(addr, len);
	if (!r)
		return false;

	/* if length is 0, address is ok, in valid region */
	if (len == 0)
		return true;

	/* find page range */
	const auto begin = page_num(*r, addr);
	const auto end = page_num(*r, addr + len - 1) + 1;

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
 * page_init - initialise page allocator
 */
void
page_init(const struct bootinfo *bi, void *owner)
{
	/* analyse bootinfo to count regions & find the first piece of
	 * contiguous normal memory in which to allocate state */
	phys *m_alloc = 0;
	phys *m_end = 0;
	for (size_t i = 0; i < bi->nr_rams; ++i) {
		const auto &m = bi->ram[i];
		switch (m.type) {
		case MT_NORMAL:
			if (!m_end) {
				m_alloc = (phys*)m.base;
				m_end = (phys*)m.base + m.size;
			}
		case MT_FAST:
			++s.nr_regions;
		}
	}

	if (!m_end)
		panic("no memory");

	/* adjust m_alloc and m_end for reserved areas */
	for (size_t i = 0; i < bi->nr_rams; ++i) {
		const auto &m = bi->ram[i];
		phys *const r_begin = (phys*)PAGE_TRUNC(m.base);
		phys *const r_end = (phys*)PAGE_ALIGN(m.base + m.size);
		switch (m.type) {
		case MT_MEMHOLE:
		case MT_KERNEL:
		case MT_BOOTDISK:
		case MT_RESERVED:
			/* no overlap */
			if (r_end < m_alloc || r_begin >= m_end)
				continue;
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
		}
	}

	dbg("page_init: allocate state at %p (%" PRIuPTR " bytes "
	    "usable), %zu regions\n", m_alloc, m_end - m_alloc, s.nr_regions);

	phys *const m_begin = m_alloc;
	auto alloc = [&](size_t len) {
		len = ALIGN(len);
		if (len > (size_t)(m_end - m_alloc))
			panic("OOM");
		void *tmp = phys_to_virt(m_alloc);
		m_alloc += len;
		memset(tmp, 0, len);
		return tmp;
	};

	/* allocate regions */
	s.regions = (region*)alloc(sizeof *s.regions * s.nr_regions);

	/* initialise regions & pages */
	for (size_t i = 0, j = 0; i < bi->nr_rams; ++i) {
		const auto &m = bi->ram[i];
		MEM_TYPE type;
		switch (m.type) {
		case MT_NORMAL:
			type = MEM_NORMAL;
			break;
		case MT_FAST:
			type = MEM_FAST;
			break;
		default:
			continue;
		}

		region &r = *new(s.regions + j) region;
		r.type = type;
		r.begin = (phys*)PAGE_ALIGN(m.base);
		r.end = (phys*)PAGE_TRUNC(m.base + m.size);
		const size_t size_order = ceil_log2(r.end - r.begin);
		r.base = TRUNCn(r.begin, 1 << size_order);
		r.size = ALIGNn(r.end, 1 << size_order) - r.base;
		const size_t max_order = ceil_log2(r.size);
		r.nr_orders = max_order - floor_log2(CONFIG_PAGE_SIZE) + 1;
		r.nr_pages = 1 << (r.nr_orders - 1);
		r.size = r.nr_pages * CONFIG_PAGE_SIZE;
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
		if (!page_reserve(r, r.base, r.begin - r.base, PG_HOLE, nullptr))
			panic("bad bootinfo");
		if (!page_reserve(r, r.end, r.base + r.size - r.end, PG_HOLE, nullptr))
			panic("bad bootinfo");

		dbg("page_init: region %zu (%s): %p -> %p covering %p -> %p\n",
		    j, to_string(r.type), r.base, r.base + r.size,
		    r.begin, r.end);

		++j;
	}

	/* sort regions by address */
	auto comp = [](const void *lv, const void *rv) {
		const region *l = (region*)lv;
		const region *r = (region*)rv;
		if (l->begin > r->begin) return 1;
		if (l->begin < r->begin) return -1;
		return 0;
	};
	qsort(s.regions, s.nr_regions, sizeof *s.regions, comp);

	/* make sure that regions don't overlap */
	for (size_t i = 1; i < s.nr_regions; ++i) {
		if (s.regions[i - 1].end > s.regions[i].begin)
			panic("bad bootinfo");
	}

	/* find allocation region */
	const region *r_alloc = nullptr;
	for (size_t i = 0; i < s.nr_regions; ++i) {
		const auto &r = s.regions[i];
		if (r.type != MEM_NORMAL)
			continue;
		r_alloc = &r;
		break;
	}
	assert(r_alloc);

	/* reserve unusable memory */
	for (size_t i = 0; i < bi->nr_rams; ++i) {
		const boot_mem *m = bi->ram + i;
		switch (m->type) {
		case MT_NORMAL:
		case MT_FAST:
			continue;
		}

		switch (m->type) {
		case MT_MEMHOLE:
			if (!page_reserve((phys*)m->base, m->size, PG_HOLE, nullptr))
				panic("bad bootinfo");
			break;
		case MT_KERNEL:
			if (!page_reserve((phys*)m->base, m->size, PG_SYSTEM, owner))
				dbg("page_init: failed to reserve %p -> %p\n",
				    m->base, m->base + m->size);
			break;
		case MT_BOOTDISK:
			s.bootdisk_size = m->size;
			/* bootdisk can be in ROM */
			page_reserve((phys*)m->base, m->size, PG_FIXED, nullptr);
			break;
		case MT_RESERVED:
			if (!page_reserve((phys*)m->base, m->size, PG_FIXED, owner))
				panic("bad bootinfo");
			break;
		}
	}

	/* reserve memory allocated for region & page structures */
	if (m_alloc > r_alloc->begin) {
		const auto begin = std::max(m_begin, r_alloc->begin);
		if (!page_reserve(begin, m_alloc - begin, PG_SYSTEM, &page_id))
			panic("bug");
	}

#if defined(CONFIG_DEBUG)
	page_dump();
#endif
}

/*
 * page_dump - dump page allocator state
 */
void page_dump(void)
{
	info("page dump\n");
	info("=========\n");
	for (size_t i = 0; i < s.nr_regions; ++i) {
#if defined(CONFIG_INFO)
		const region &r = s.regions[i];
#endif
		info(" %p -> %p\n", r.begin, r.end);
		info("  type      %s\n", to_string(r.type));
		info("  base      %p\n", r.base);
		info("  size      %zu\n", r.size);
		info("  usable    %zu\n", r.usable);
		info("  free      %zu\n", r.free);
		info("  nr_orders %zu\n", r.nr_orders);
		info("  nr_pages  %zu\n", r.nr_pages);
#if 0
		info("  free blocks\n");
		for (int j = r.nr_orders - 2; j >= 0; --j) {
			info("    order %d: ", j);
			page *p;
			list_for_each_entry(p, r.blocks + j, link)
				info("%zu ", p - r.pages);
			info("\n");
		}
#endif
	}
}

