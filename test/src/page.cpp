/*
 * Configuration for this test
 */
#define KERNEL
#define CONFIG_PAGE_OFFSET 0
#define CONFIG_PAGE_SIZE 0x1000
#define CONFIG_MA_NORMAL_ATTR MA_SPEED_0
#define CONFIG_MA_FAST_ATTR MA_SPEED_1
#define CONFIG_INFO
// #define CONFIG_DEBUG

/*
 * Test victim
 */
#include <sys/mem/page.cpp>

/*
 * Test suite
 */
#include <gtest/gtest.h>

#include <task.h>

struct Hdrs {
	ElfN_Ehdr ehdr;
	ElfN_Phdr phdr[8];
} hdrs = {
	.ehdr = {
		.e_ident = {ELFMAG0, ELFMAG1, ELFMAG2, ELFMAG3, ELFCLASSN},
		.e_phoff = offsetof(Hdrs, phdr),
		.e_phnum = 0,
	}
};
extern ElfN_Ehdr __elf_headers __attribute__((alias("hdrs")));

task kern_task;

/*
 * bitmap_idx_to_order - order from bitmap index
 */
static size_t
bitmap_idx_to_order(const region &r, const size_t idx)
{
	assert(idx < bitmap_size(r));
	return r.nr_orders - floor_log2(idx + 1) - 2;
}

/*
 * bitmap_idx_to_block_a - page number of first page in buddy a for index
 */
static size_t
bitmap_idx_to_block_a(const region &r, const size_t idx)
{
	assert(idx < bitmap_size(r));
	const auto o = bitmap_idx_to_order(r, idx);
	return (idx - order_to_first_bitmap_idx(r, o)) * (1 << (o + 1));
}

/*
 * bitmap_idx_to_block_b - page number of first page in buddy b for index
 */
static size_t
bitmap_idx_to_block_b(const region &r, const size_t idx)
{
	assert(idx < bitmap_size(r));
	return bitmap_idx_to_block_a(r, idx) +
		(1 << bitmap_idx_to_order(r, idx));
}

/*
 * bitmap_bit - value of bitmap bit at index
 */
static bool
bitmap_bit(const region &r, const size_t idx)
{
	assert(idx < bitmap_size(r));
	constexpr auto bits_per_word = sizeof *r.bitmap * 8;
	const auto word_idx = idx / bits_per_word;
	const auto bit_idx = idx % bits_per_word;
	return r.bitmap[word_idx] & 1ul << bit_idx;
};

/*
 * verify_allocator - verify that allocator state is sane
 */
static void
verify_allocator(const region &r)
{
	/* build free_entries from raw page state */
	std::map<size_t, std::set<size_t>> free_entries;
	std::size_t total_free = 0;
	std::size_t total_usable = 0;

	auto handle_pages = [&](size_t begin, const size_t end, const PG_STATE state) {
		const std::size_t len = (end - begin) * PAGE_SIZE;
		switch (state) {
		case PG_FREE:
			dbg("PG_FREE %zu -> %zu\n", begin, end - 1);
			while (begin != end) {
				const auto size = end - begin;
				const auto order = std::min<size_t>(
					begin ? std::countr_zero(begin) : r.nr_orders - 1,
					floor_log2(size));
				free_entries[order].insert(begin);
				begin += 1 << order;
			}
			total_free += len;
			total_usable += len;
			break;
		case PG_HOLE:
			dbg("PG_HOLE %zu -> %zu\n", begin, end - 1);
			break;
		case PG_SYSTEM:
			dbg("PG_SYSTEM %zu -> %zu\n", begin, end - 1);
			break;
		case PG_FIXED:
			dbg("PG_FIXED %zu -> %zu\n", begin, end - 1);
			total_usable += len;
			break;
		case PG_MAPPED:
			dbg("PG_MAPPED %zu -> %zu\n", begin, end - 1);
			total_usable += len;
			break;
		}
	};

	PG_STATE state = static_cast<PG_STATE>(r.pages[0].state);
	size_t p_begin = 0;
	size_t p_end = 1;
	for (; p_end < r.nr_pages; ++p_end) {
		const auto &p = r.pages[p_end];
		if (p.state == state)
			continue;
		handle_pages(p_begin, p_end, state);
		state = static_cast<PG_STATE>(p.state);
		p_begin = p_end;
	}
	handle_pages(p_begin, p_end, state);

	/* make sure all entries are in free list */
	for (const auto &os : free_entries) {
		const auto &fl = r.blocks[os.first];
		for (const auto &fe : os.second) {
			bool found = false;
			const struct page *p;
			list_for_each_entry(p, &fl, link) {
				if ((size_t)(p - r.pages) != fe)
					continue;
				found = true;
				break;
			}
			EXPECT_TRUE(found) << "Missing free list entry page "
			    << fe << " order " << os.first;
		}
	}

	/* make sure there are no spurious entries in free list */
	for (size_t i = 0; i < r.nr_orders; ++i) {
		const auto &fl = r.blocks[i];
		const auto &fe = free_entries[i];
		const struct page *p;
		list_for_each_entry(p, &fl, link) {
			EXPECT_TRUE(fe.find(p - r.pages) != fe.end()) <<
			    "Spurious free list entry page "
			    << (size_t)(p - r.pages) << " order " << i;
		}
	}

	/* make sure bitmap matches free list */
	for (size_t i = 0; i < (1u << (r.nr_orders - 1)) - 1; ++i) {
		const auto order = bitmap_idx_to_order(r, i);
		const auto page_a = bitmap_idx_to_block_a(r, i);
		const auto page_b = bitmap_idx_to_block_b(r, i);

		const auto &fe = free_entries[order];
		const auto page_a_free = fe.find(page_a) != fe.end();
		const auto page_b_free = fe.find(page_b) != fe.end();
		if (bitmap_bit(r, i)) {
			/* one of page_a or page_b must be free */
			EXPECT_TRUE(page_a_free != page_b_free) <<
			   "Bitmap bit " << i << " order " << order <<
			   " page_a " << page_a <<
			   "(" << (page_a_free ? "free" : "alloc") <<
			   ") page_b " << page_b <<
			   "(" << (page_b_free ? "free" : "alloc") <<
			   ") indicates that one page should be free!";
		} else {
			/* page_a and page_b must be both free or both allocated */
			EXPECT_TRUE(page_a_free == page_b_free) <<
			   "Bitmap bit " << i << " order " << order <<
			   " page_a " << page_a <<
			   "(" << (page_a_free ? "free" : "alloc") <<
			   ") page_b " << page_b <<
			   "(" << (page_b_free ? "free" : "alloc") <<
			   ") indicates that both pages should be free or allocated!";
		}
	}

	EXPECT_EQ(r.free, total_free);
	EXPECT_EQ(r.usable, total_usable);
}

/*
 * verify_region - verify that region r matches meminfo description m
 */
static void
verify_region(const meminfo &m, const region &r, const meminfo &a)
{
	EXPECT_EQ(r.attr, m.attr);
	EXPECT_EQ(r.begin, PAGE_ALIGN(m.base.phys()));
	EXPECT_EQ(r.end, PAGE_TRUNC(m.base.phys() + m.size));
	EXPECT_EQ(r.nr_pages, 1 << (r.nr_orders - 1));
	EXPECT_EQ(r.size, r.nr_pages * PAGE_SIZE);
	EXPECT_LE(r.base, PAGE_ALIGN(m.base.phys()));
	EXPECT_GE(r.base, PAGE_TRUNC(m.base.phys() + m.size) - r.size);
	EXPECT_EQ(r.base, TRUNCn(r.begin, 1 << ceil_log2(r.end - r.begin)));
	EXPECT_EQ(r.nr_orders, ceil_log2(r.size) - floor_log2(PAGE_SIZE) + 1);

	EXPECT_GE((uintptr_t)r.pages, a.base.phys());
	EXPECT_LT((uintptr_t)r.pages, a.base.phys() + a.size);

	EXPECT_GE((uintptr_t)r.blocks, a.base.phys());
	EXPECT_LT((uintptr_t)r.blocks, a.base.phys() + a.size);
	EXPECT_GE((uintptr_t)r.bitmap, a.base.phys());
	EXPECT_LT((uintptr_t)r.bitmap, a.base.phys() + a.size);
	verify_allocator(r);

	/* make sure usable size is sane */
	size_t usable = 0;
	for (size_t i = 0; i < r.nr_pages; ++i) {
		const auto &p = r.pages[i];
		switch (p.state) {
		case PG_HOLE:
		case PG_SYSTEM:
			break;
		case PG_FREE:
		case PG_FIXED:
		case PG_MAPPED:
			usable += PAGE_SIZE;
		}
	}
	EXPECT_EQ(r.usable, usable);
}

/*
 * verify_reserved
 */
static void
verify_reserved(const region *r, const phys addr, size_t len)
{
	const auto begin = page_num(*r, addr.phys());
	const auto end = page_num(*r, addr.phys() + len - 1) + 1;

	for (auto i = begin; i != end; ++i)
		EXPECT_EQ(r->pages[i].state, PG_SYSTEM);
}

static void
verify_reserved(const phys addr, size_t len)
{
	for (size_t i = 0; i < s.nr_regions; ++i) {
		const auto &r = s.regions[i];
		auto begin = std::max(addr.phys(), r.begin);
		auto end = std::min(addr.phys() + len, r.end);
		if (begin >= end)
			continue;
		verify_reserved(&r, phys{begin}, end - begin);
	}
}

/*
 * verify_regions - verify all memory descriptions in meminfo match regions
 */
static void
verify_regions(const struct meminfo *mi, const size_t mi_size,
	       const bootargs &ba)
{
	for (size_t i = 0, j; i < mi_size; ++i) {
		const auto &m = mi[i];
		for (j = 0; j < s.nr_regions; ++j) {
			const auto &r = s.regions[j];
			if (r.attr != m.attr)
				continue;
			verify_region(m, r, mi[0]);
			break;
		}
		EXPECT_LT(j, s.nr_regions) << "Region not found!";
	}

	/* find MA_NORMAL region */
	region *nr = nullptr;
	for (size_t i = 0; i < s.nr_regions; ++i) {
		const auto r = s.regions + i;
		if ((r->attr & MA_SPEED_MASK) != MA_NORMAL)
			continue;
		nr = r;
		break;
	}
	ASSERT_NE(nr, nullptr) << "missing MA_NORMAL region!";

	/* make sure that allocator state is reserved */
	for (size_t i = 0; i < s.nr_regions; ++i) {
		const auto r = s.regions + i;
		verify_reserved(nr, virt_to_phys(r->pages), sizeof(page) * r->nr_pages);
		verify_reserved(nr, virt_to_phys(r->blocks), sizeof(list) * r->nr_orders);
		verify_reserved(nr, virt_to_phys(r->bitmap), (bitmap_size(*r) + 7) / 8);
	}

	/* make sure that archive is reserved */
	if (ba.archive_size)
		verify_reserved(phys{ba.archive_addr}, ba.archive_size);

	/* make sure that PT_LOAD segments are reserved */
	for (size_t i = 0; i < hdrs.ehdr.e_phnum; ++i) {
		const auto ph = hdrs.phdr[i];
		if (ph.p_type != PT_LOAD)
			continue;
		verify_reserved(virt_to_phys((void *)ph.p_vaddr), ph.p_memsz);
	}
}

static int
rand_in_range(int min, int max)
{
	const int range = max - min + 1;
	return rand() % range + min;
}

/*
 * page_test - test fixture for page allocator
 */
class page_test : public ::testing::Test
{
public:
	page_test();

	void init_normal();
	void init_random();

	void verify_regions();
	void verify_fast_zero();

protected:
	meminfo mi_[8] = {};
	size_t mi_size_;
	bootargs ba_;
	phys mem_normal_;
	phys mem_fast_;

	const std::size_t normal_size_ = 2048 * 1024;
	const std::size_t fast_size_ = 1024 * 1024;
};

page_test::page_test()
{
	mem_normal_ = virt_to_phys(aligned_alloc(normal_size_, normal_size_));
	mem_fast_ = virt_to_phys(aligned_alloc(fast_size_, fast_size_));

	assert(mem_normal_.phys() && mem_fast_.phys());
	memset(phys_to_virt(mem_normal_), 0, normal_size_);
	memset(phys_to_virt(mem_fast_), 0, fast_size_);
}

void
page_test::init_normal()
{
	memset(&s, 0, sizeof(s));
	panic_fail = false;
	ba_.archive_size = 0;
	hdrs.ehdr.e_phnum = 0;
	mi_[0].base = mem_normal_;
	mi_[0].size = normal_size_;
	mi_[0].attr = MA_NORMAL;
	mi_[1].base = mem_fast_;
	mi_[1].size = fast_size_;
	mi_[1].attr = MA_FAST;
	mi_size_ = 2;
	page_init(mi_, mi_size_, &ba_);
	EXPECT_EQ(panic_fail, false);
	EXPECT_EQ(s.nr_regions, 2);
	verify_regions();
}

void
page_test::init_random()
{
	memset(&s, 0, sizeof(s));
	panic_fail = false;
	const size_t normal_off = rand_in_range(0, 64 * 1024);
	mi_[0].base = phys{mem_normal_.phys() + normal_off};
	mi_[0].size = normal_size_ + rand_in_range(-128 * 1024, -normal_off);
	mi_[0].attr = MA_NORMAL;
	const size_t fast_off = rand_in_range(0, 64 * 1024);
	mi_[1].base = phys{mem_fast_.phys() + fast_off};
	mi_[1].size = fast_size_ + rand_in_range(-128 * 1024, -fast_off);
	mi_[1].attr = MA_FAST;
	mi_size_ = 2;
	ba_.archive_addr = mi_[0].base.phys() + rand_in_range(0, 128 * 1024);
	ba_.archive_size = rand_in_range(0, 128 * 1024);
	hdrs.phdr[0].p_type = PT_LOAD;
	hdrs.phdr[0].p_vaddr = reinterpret_cast<ElfN_Addr>(phys_to_virt(phys{ba_.archive_addr + ba_.archive_size + rand_in_range(0, 128 * 1024)}));
	hdrs.phdr[0].p_memsz = rand_in_range(0, 128 * 1024);
	hdrs.phdr[1].p_type = PT_LOAD;
	hdrs.phdr[1].p_vaddr = reinterpret_cast<ElfN_Addr>(phys_to_virt(phys{hdrs.phdr[0].p_vaddr + hdrs.phdr[0].p_memsz + rand_in_range(0, 128 * 1024)}));
	hdrs.phdr[1].p_memsz = rand_in_range(0, 128 * 1024);
	hdrs.phdr[2].p_type = PT_LOAD;
	hdrs.phdr[2].p_vaddr = reinterpret_cast<ElfN_Addr>(phys_to_virt(phys{mi_[1].base.phys() + rand_in_range(0, 128 * 1024)}));
	hdrs.phdr[2].p_memsz = rand_in_range(0, 128 * 1024);
	hdrs.phdr[3].p_type = PT_LOAD;
	hdrs.phdr[3].p_vaddr = reinterpret_cast<ElfN_Addr>(phys_to_virt(phys{hdrs.phdr[2].p_vaddr + hdrs.phdr[2].p_memsz + rand_in_range(0, 128 * 1024)}));
	hdrs.phdr[3].p_memsz = rand_in_range(0, 128 * 1024);
	hdrs.ehdr.e_phnum = 4;
	page_init(mi_, mi_size_, &ba_);
	EXPECT_EQ(panic_fail, false);
	EXPECT_EQ(s.nr_regions, 2);
	verify_regions();
}

void
page_test::verify_regions()
{
	::verify_regions(mi_, mi_size_, ba_);
}

void
page_test::verify_fast_zero()
{
	const unsigned long *fast = (unsigned long *)phys_to_virt(mem_fast_);

	for (size_t i = 0; i < fast_size_ / sizeof(unsigned long); ++i)
		assert(fast[i] == 0);
}

/*
 * init_normal - Test initialisation of nicely aligned memory
 */
TEST_F(page_test, init_normal)
{
	init_normal();
}

/*
 * init_fuzz - Fuzz testing of initialisation
 */
TEST_F(page_test, init_fuzz)
{
	for (size_t i = 0; i < 10000; ++i)
		init_random();
	verify_fast_zero();
}

TEST(page, init_cornercases)
{
	auto new_page_init = [](const meminfo *mi, size_t mi_size,
				const bootargs &ba) {
		memset(&s, 0, sizeof(s));
		page_init(mi, mi_size, &ba);
	};

	std::vector<char> buf(2048*1024);
	phys *mem = (phys *)buf.data();
	meminfo mi[8] = {};
	bootargs ba;
	ba.archive_size = 0;
	hdrs.ehdr.e_phnum = 0;

	/* 1MiB aligned region */
	mi[0].base = phys{((uintptr_t)mem + 1024 * 1024) & -(1024*1024)};
	mi[0].size = 1024*1024;
	mi[0].attr = MA_NORMAL;
	new_page_init(mi, 1, ba);
	EXPECT_EQ(panic_fail, false);
	EXPECT_EQ(s.nr_regions, 1);
	verify_regions(mi, 1, ba);

	/* 1MiB byte unaligned region */
	mi[0].base = phys{(((uintptr_t)mem + 1024 * 1024) & -(1024*1024)) - 4095};
	mi[0].size = 1024*1024 + 4095 + 4095;
	mi[0].attr = MA_NORMAL;
	new_page_init(mi, 1, ba);
	EXPECT_EQ(panic_fail, false);
	EXPECT_EQ(s.nr_regions, 1);
	verify_regions(mi, 1, ba);

	/* 1MiB page unaligned region */
	mi[0].base = phys{(((uintptr_t)mem + 1024 * 1024) & -(1024*1024)) - 4096};
	mi[0].size = 1024*1024 + 4096 + 4096;
	mi[0].attr = MA_NORMAL;
	new_page_init(mi, 1, ba);
	EXPECT_EQ(panic_fail, false);
	EXPECT_EQ(s.nr_regions, 1);
	verify_regions(mi, 1, ba);

	/* Reserved at start */
	mi[0].base = phys{((uintptr_t)mem + 1024 * 1024) & -(1024*1024)};
	mi[0].size = 1024*1024;
	mi[0].attr = MA_NORMAL;
	ba.archive_addr = mi[0].base.phys();
	ba.archive_size = 16*1024;
	new_page_init(mi, 1, ba);
	EXPECT_EQ(panic_fail, false);
	EXPECT_EQ(s.nr_regions, 1);
	verify_regions(mi, 1, ba);

	/* Reserved in middle */
	mi[0].base = phys{((uintptr_t)mem + 1024 * 1024) & -(1024*1024)};
	mi[0].size = 1024*1024;
	mi[0].attr = MA_NORMAL;
	ba.archive_addr = mi[0].base.phys() + 16*1024;
	ba.archive_size = 16*1024;
	new_page_init(mi, 1, ba);
	EXPECT_EQ(panic_fail, false);
	EXPECT_EQ(s.nr_regions, 1);
	verify_regions(mi, 1, ba);

	/* Reserved at end */
	mi[0].base = phys{((uintptr_t)mem + 1024 * 1024) & -(1024*1024)};
	mi[0].size = 1024*1024;
	mi[0].attr = MA_NORMAL;
	ba.archive_addr = mi[0].base.phys() + mi[0].size - 16*1024;
	ba.archive_size = 16*1024;
	new_page_init(mi, 1, ba);
	EXPECT_EQ(panic_fail, false);
	EXPECT_EQ(s.nr_regions, 1);
	verify_regions(mi, 1, ba);
}

TEST_F(page_test, alloc_free_cornercases)
{
	init_normal();

	/* allocate/free entire region */
	EXPECT_EQ(page_alloc(fast_size_, MA_FAST, 0).release(), mem_fast_);
	verify_regions();
	EXPECT_EQ(page_free(mem_fast_, fast_size_, 0), 0);
	verify_regions();

	/* allocate/free single page */
	EXPECT_EQ(page_alloc(1, MA_FAST, 0).release(), mem_fast_);
	verify_regions();
	EXPECT_EQ(page_free(mem_fast_, 1, 0), 0);
	verify_regions();

	/* allocate/free multiple single pages */
	for (size_t i = 0; i < 32; ++i) {
		EXPECT_EQ(page_alloc(1, MA_FAST, 0).release().phys(),
		    mem_fast_.phys() + PAGE_SIZE * i);
		verify_regions();
	}
	for (size_t i = 0; i < 32; ++i) {
		EXPECT_EQ(page_free(phys{mem_fast_.phys() + PAGE_SIZE * i}, 1, 0), 0);
		verify_regions();
	}

	/* free partial range */
	EXPECT_EQ(page_alloc(16 * PAGE_SIZE, MA_FAST, 0).release(), mem_fast_);
	verify_regions();
	EXPECT_EQ(page_free(phys{mem_fast_.phys() + PAGE_SIZE * 4}, PAGE_SIZE * 8, 0), 0);
	verify_regions();
	EXPECT_EQ(page_free(phys{mem_fast_.phys() + PAGE_SIZE * 12}, PAGE_SIZE * 4, 0), 0);
	verify_regions();
	EXPECT_EQ(page_free(phys{mem_fast_.phys()}, PAGE_SIZE * 4, 0), 0);
	verify_regions();

	/* alloc too large */
	EXPECT_EQ(page_alloc(fast_size_ * 2, MA_FAST, 0).release(), phys{});
}

TEST_F(page_test, alloc_free_fuzz)
{
	for (size_t i = 0; i < 1000; ++i) {
		init_random();

		for (size_t i = 0; i < 1000; ++i) {
			page_alloc(rand_in_range(0, 128 * PAGE_SIZE), rand_in_range(MA_NORMAL, MA_FAST) | rand_in_range(0, 1) * PAF_MAPPED, 0).release();
			page_alloc(rand_in_range(0, 128 * PAGE_SIZE), rand_in_range(MA_FAST, MA_FAST) | rand_in_range(0, 1) * PAF_MAPPED, 0).release();
			page_free(phys{mem_normal_.phys() + rand_in_range(0, normal_size_)}, rand_in_range(0, 128 * PAGE_SIZE), 0);
			page_free(phys{mem_fast_.phys() + rand_in_range(0, fast_size_)}, rand_in_range(0, 128 * PAGE_SIZE), 0);
		}
		verify_regions();
	}
}

TEST_F(page_test, invalid_lengths)
{
	init_normal();
	EXPECT_EQ(page_alloc_order(-1, MA_FAST, 0).release(), phys{});
	verify_regions();
	EXPECT_EQ(page_alloc(-PAGE_SIZE, MA_FAST, 0).release(), phys{});
	verify_regions();
	EXPECT_EQ(page_reserve(mem_fast_, -PAGE_SIZE, 0, 0).release(), phys{});
	verify_regions();
	EXPECT_EQ(page_alloc(PAGE_SIZE, MA_FAST, 0).release(), mem_fast_);
	verify_regions();
	EXPECT_LT(page_free(mem_fast_, -PAGE_SIZE, 0), 0);
	verify_regions();
}
