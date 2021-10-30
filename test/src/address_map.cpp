/*
 * Configuration for this test
 */
#define KERNEL
#define CONFIG_PAGE_OFFSET 0
#define CONFIG_PAGE_SIZE 0x1000
#define CONFIG_MA_NORMAL_ATTR MA_SPEED_0
#define CONFIG_MA_FAST_ATTR MA_SPEED_1

/*
 * Test victim
 */
#define ADDRESS_MAP_DEBUG
#include <sys/lib/address_map.h>

/*
 * Test suite
 */
#include <gtest/gtest.h>

static int
rand_in_range(int min, int max)
{
	const int range = max - min + 1;
	return rand() % range + min;
}

struct alloc_malloc {
	static constexpr auto initial_size = 64;

	static void *
	calloc(size_t size, void *)
	{
		return ::calloc(size, 1);
	}

	static void
	free(void *p, size_t, void *)
	{
		::free(p);
	}
};

using file_map_test = address_map_4k_32b_32B<uint64_t, alloc_malloc>;
using address_map_test = address_map_4k_32b_32B<void *, alloc_malloc>;

TEST(address_map, simple)
{
	address_map_test am;
	address_map_test::find_result r;

	am.map((void *)0x1000, phys{0x10001000}, PAGE_SIZE, 0);
	am.map((void *)0x2000, phys{0x10002000}, PAGE_SIZE, 1);

	r = am.find((void *)0);
	EXPECT_FALSE(r);

	r = am.find((void *)0x1000);
	EXPECT_EQ(r->phys.phys(), 0x10001000);
	EXPECT_EQ(r->size, PAGE_SIZE);
	EXPECT_EQ(r->attr, 0);

	r = am.find((void *)0x2000);
	EXPECT_EQ(r->phys.phys(), 0x10002000);
	EXPECT_EQ(r->size, PAGE_SIZE);
	EXPECT_EQ(r->attr, 1);

	r = am.find((void *)0x3000);
	EXPECT_FALSE(r);

	am.unmap((void *)0x1000, PAGE_SIZE);

	r = am.find((void *)0x1000);
	EXPECT_FALSE(r);

	am.unmap((void *)0x2000, PAGE_SIZE);

	r = am.find((void *)0x2000);
	EXPECT_FALSE(r);
}

TEST(address_map, address_limits)
{
	address_map_test am;
	address_map_test::find_result r;

	/* map virt 0 -> phys 0xfffff000 */
	am.map((void *)0, phys{0xfffff000}, PAGE_SIZE, 2);

	/* map virt 0xfffff000 -> phys 0 */
	am.map((void *)0xfffff000, phys{0}, PAGE_SIZE, 3);

	r = am.find((void *)0);
	EXPECT_EQ(r->phys.phys(), 0xfffff000);
	EXPECT_EQ(r->size, PAGE_SIZE);
	EXPECT_EQ(r->attr, 2);

	r = am.find((void *)0x1000);
	EXPECT_FALSE(r);

	r = am.find((void *)0xffffe000);
	EXPECT_FALSE(r);

	r = am.find((void *)0xfffff000);
	EXPECT_EQ(r->phys.phys(), 0);
	EXPECT_EQ(r->size, PAGE_SIZE);
	EXPECT_EQ(r->attr, 3);

	/* unmap virt 0 */
	am.unmap((void *)0, PAGE_SIZE);

	/* unmap virt 0xfffff000 */
	am.unmap((void *)0xfffff000, PAGE_SIZE);

	r = am.find((void *)0);
	EXPECT_FALSE(r);

	r = am.find((void *)0xfffff000);
	EXPECT_FALSE(r);
}

TEST(address_map, assertions)
{
	address_map_test am;

	am.map((void *)0x10000000, phys{0xf0000000}, PAGE_SIZE, 0);

	/* invalid page size */
	EXPECT_DEATH(am.map((void *)0, phys{0}, 0, 0), "");
	EXPECT_DEATH(am.map((void *)0, phys{0}, 1, 0), "");

	/* virt alignment < size alignment */
	EXPECT_DEATH(am.map((void *)0x1000, phys{0}, 0x2000, 0), "");

	/* invalid page size */
	EXPECT_DEATH(am.unmap((void *)0, 0), "");
	EXPECT_DEATH(am.unmap((void *)0, 1), "");

	/* virt alignment < size alignment */
	EXPECT_DEATH(am.unmap((void *)0x1000, 0x2000), "");

	/* not mapped */
	EXPECT_DEATH(am.unmap((void *)0, PAGE_SIZE), "");

	/* wrong size */
	EXPECT_DEATH(am.unmap((void *)0x10000000, 0x2000), "");

	/* bad max_load */
	EXPECT_DEATH({address_map_test bad(0, 100);}, "");

	/* attr out of range */
	EXPECT_DEATH(am.map((void *)0, phys{0}, PAGE_SIZE, 7), "");
}

TEST(address_map, simple_multi_page)
{
	address_map_test am;
	address_map_test::find_result r;

	/* map virt 0x10000000 -> phys 0x20000000 */
	am.map((void *)0x10000000, phys{0x20000000}, PAGE_SIZE * 65536, 4);

	r = am.find((void *)0xffff000);
	EXPECT_FALSE(r);

	for (size_t i = 0; i < 65536; ++i) {
		r = am.find((void *)(0x10000000 + PAGE_SIZE * i));
		EXPECT_EQ(r->phys.phys(), 0x20000000);
		EXPECT_EQ(r->size, PAGE_SIZE * 65536);
		EXPECT_EQ(r->attr, 4);
	}

	r = am.find((void *)10040000);
	EXPECT_FALSE(r);

	/* unmap */
	am.unmap((void *)0x10000000, PAGE_SIZE * 65536);

	for (size_t i = 0; i < 64; ++i) {
		r = am.find((void *)(0x10000000 + PAGE_SIZE * i));
		EXPECT_FALSE(r);
	}
}

TEST(address_map, huge_entries)
{
	address_map_test am;
	address_map_test::find_result r;

	/* map virt 0 -> phys 0x80000000 */
	am.map((void *)0, phys{0x80000000}, 0x80000000, 5);

	/* map virt 0x80000000 -> phys 0 */
	am.map((void *)0x80000000, phys{0}, 0x80000000, 6);

	for (size_t i = 0; i < 0x80000000 / PAGE_SIZE; ++i) {
		r = am.find((void *)(i * PAGE_SIZE));
		EXPECT_EQ(r->phys.phys(), 0x80000000);
		EXPECT_EQ(r->size, 0x80000000);
		EXPECT_EQ(r->attr, 5);
	}

	for (size_t i = 0; i < 0x80000000 / PAGE_SIZE; ++i) {
		r = am.find((void *)(0x80000000 + i * PAGE_SIZE));
		EXPECT_EQ(r->phys.phys(), 0);
		EXPECT_EQ(r->size, 0x80000000);
		EXPECT_EQ(r->attr, 6);
	}

	/* unmap */
	am.unmap((void *)0, 0x80000000);
	am.unmap((void *)0x80000000, 0x80000000);

	for (size_t i = 0; i < 0xffffffff / PAGE_SIZE; ++i) {
		r = am.find((void *)(i * PAGE_SIZE));
		EXPECT_FALSE(r);
	}
}

TEST(address_map, many_entries)
{
	address_map_test am;
	address_map_test::find_result r;

	/* map entire address space in opposite order */
	for (size_t i = 0; i < 0xffffffff / PAGE_SIZE; ++i)
		am.map((void *)(i * PAGE_SIZE), phys{0xfffff000 - i * PAGE_SIZE}, PAGE_SIZE, i % 7);

	for (size_t i = 0; i < 0xffffffff / PAGE_SIZE; ++i) {
		r = am.find((void *)(i * PAGE_SIZE));
		EXPECT_EQ(r->phys.phys(), 0xfffff000 - i * PAGE_SIZE);
		EXPECT_EQ(r->size, PAGE_SIZE);
		EXPECT_EQ(r->attr, i % 7);
	}

	/* unmap */
	for (size_t i = 0; i < 0xffffffff / PAGE_SIZE; ++i)
		am.unmap((void *)(i * PAGE_SIZE), PAGE_SIZE);

	for (size_t i = 0; i < 0xffffffff / PAGE_SIZE; ++i) {
		r = am.find((void *)(i * PAGE_SIZE));
		EXPECT_FALSE(r);
	}

}

TEST(address_map, clear)
{
	address_map_test am;
	address_map_test::find_result r;

	EXPECT_EQ(am.size(), 0);
	EXPECT_TRUE(am.empty());

	am.map((void *)0x1000, phys{0x10001000}, PAGE_SIZE, 0);
	EXPECT_GE(am.size(), 1);    /* number of clusters */
	EXPECT_FALSE(am.empty());

	am.map((void *)0x2000, phys{0x10002000}, PAGE_SIZE, 1);
	EXPECT_GE(am.size(), 1);    /* number of clusters */
	EXPECT_FALSE(am.empty());

	r = am.find((void *)0);
	EXPECT_FALSE(r);

	r = am.find((void *)0x1000);
	EXPECT_EQ(r->phys.phys(), 0x10001000);
	EXPECT_EQ(r->size, PAGE_SIZE);
	EXPECT_EQ(r->attr, 0);

	r = am.find((void *)0x2000);
	EXPECT_EQ(r->phys.phys(), 0x10002000);
	EXPECT_EQ(r->size, PAGE_SIZE);
	EXPECT_EQ(r->attr, 1);

	am.clear();
	EXPECT_EQ(am.size(), 0);
	EXPECT_TRUE(am.empty());

	r = am.find((void *)0);
	EXPECT_FALSE(r);

	r = am.find((void *)0x1000);
	EXPECT_FALSE(r);

	r = am.find((void *)0x2000);
	EXPECT_FALSE(r);

	am.map((void *)0x1000, phys{0x10001000}, PAGE_SIZE, 0);
	am.map((void *)0x2000, phys{0x10002000}, PAGE_SIZE, 1);

	r = am.find((void *)0);
	EXPECT_FALSE(r);

	r = am.find((void *)0x1000);
	EXPECT_EQ(r->phys.phys(), 0x10001000);
	EXPECT_EQ(r->size, PAGE_SIZE);
	EXPECT_EQ(r->attr, 0);

	r = am.find((void *)0x2000);
	EXPECT_EQ(r->phys.phys(), 0x10002000);
	EXPECT_EQ(r->size, PAGE_SIZE);
	EXPECT_EQ(r->attr, 1);
}

TEST(address_map, fuzz)
{
	address_map_test am;
	address_map_test::find_result r;

	constexpr unsigned pages = 128;
	std::vector<std::pair<uint32_t, uint32_t>> as(pages);

	for (size_t i = 0; i < 100000; ++i) {
		/* random area in address space */
		uintptr_t vnr = rand_in_range(0, pages - 1);
		uintptr_t pnr = rand_in_range(0, pages - 1);
		uintptr_t align = std::min(std::countr_zero(vnr), std::countr_zero(pages));
		uintptr_t size = 1ul << rand_in_range(0, align);

		/* unmap any overlaps */
		for (auto i = vnr; i < vnr + size; ++i) {
			auto &a = as[i];
			if (!a.second)
				continue;
			uintptr_t begin = i & ~(a.second - 1);
			am.unmap((void *)(begin * PAGE_SIZE), a.second * PAGE_SIZE);
			for (auto j = begin, end = begin + a.second; j < end; ++j)
				as[j].second = 0;
		}

		/* map! */
		am.map((void *)(vnr * PAGE_SIZE), phys{pnr * PAGE_SIZE}, size * PAGE_SIZE, pnr % 7);
		for (auto i = vnr; i < vnr + size; ++i)
			as[i] = {pnr, size};

		/* verify that address map matches */
		for (uintptr_t i = 0; i < pages; ++i) {
			r = am.find((void *)(i * PAGE_SIZE));
			const auto &a = as[i];
			if (!a.second) {
				EXPECT_FALSE(r);
				continue;
			}
			EXPECT_EQ(r->phys.phys(), a.first * PAGE_SIZE);
			EXPECT_EQ(r->size, a.second * PAGE_SIZE);
			EXPECT_EQ(r->attr, a.first % 7);
		}
	}
}

/*
 * file_map with 4k/32b/32B clusters supports file offsets up to ~128TiB.
 */
TEST(file_map, address_limits)
{
	file_map_test fm;
	file_map_test::find_result r;

	/* map virt 0 -> phys 0xfffff000 */
	fm.map(0, phys{0xfffff000}, PAGE_SIZE, 2);

	/* map virt 0x7fffffff7000 -> phys 0 */
	fm.map(0x7fffffff7000, phys{0}, PAGE_SIZE, 3);

	r = fm.find(0);
	EXPECT_EQ(r->phys.phys(), 0xfffff000);
	EXPECT_EQ(r->size, PAGE_SIZE);
	EXPECT_EQ(r->attr, 2);

	r = fm.find(0x1000);
	EXPECT_FALSE(r);

	r = fm.find(0x7ffffffef000);
	EXPECT_FALSE(r);

	r = fm.find(0x7fffffff7000);
	EXPECT_EQ(r->phys.phys(), 0);
	EXPECT_EQ(r->size, PAGE_SIZE);
	EXPECT_EQ(r->attr, 3);

	/* unmap virt 0 */
	fm.unmap(0, PAGE_SIZE);

	/* unmap virt 0x7fffffff7000 */
	fm.unmap(0x7fffffff7000, PAGE_SIZE);

	r = fm.find(0);
	EXPECT_FALSE(r);

	r = fm.find(0x7fffffff7000);
	EXPECT_FALSE(r);

	/* address out of range */
	EXPECT_DEATH(fm.map(0x7fffffff8000, phys{0}, PAGE_SIZE, 0), "");
	EXPECT_DEATH(fm.map(0xffffffffffffffff, phys{0}, PAGE_SIZE, 0), "");
}

