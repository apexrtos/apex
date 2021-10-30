#pragma once

#include <address.h>
#include <compiler.h>
#include <conf/config.h>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <debug.h>
#include <kernel.h>
#include <optional>
#include <page.h>

/*
 * address_map - map virtual to physical addresses
 *
 * Implemented as a closed hash table where each table entry is a cacheline
 * sized cluster of virtual->physical translations.
 *
 * Supports any power-of-2 sized mapping from PAGE_SIZE up. Multi-page mappings
 * use multiple translation entries. Even entries store the address, odd store
 * the size.
 *
 * Each mapping has associated attributes which can be used to store the
 * access permissions for the mapping.
 *
 * Load factor is controllable. The table increases in size and rehashes if the
 * load factor is exceeded but will not reduce in size.
 *
 * The table entry and hash function are customisable to allow for different
 * address space sizes and cache line sizes.
 *
 * The table allocator is also customisable, useful for unit testing.
 *
 * Many of the ideas for this implementation originate from
 * https://www.cs.technion.ac.il/~dan/papers/hvsr-sigmetrics-2016.pdf
 */

namespace address_map_dtl {

/*
 * hash_32_6shift - hash a 32-bit integer using 6 shifts
 *
 * The author of this function is Thomas Wang. Unfortunately the original
 * reference is no longer available.
 *
 * See https://burtleburtle.net/bob/hash/integer.html
 */
struct hash_32_6shift {
	uint32_t
	operator()(uint32_t v)
	{
		v += ~(v << 15);
		v ^=  (v >> 10);
		v +=  (v << 3);
		v ^=  (v >> 6);
		v += ~(v << 11);
		v ^=  (v >> 16);
		return v;
	}
};

/*
 * address_map_dtl::entry_32b
 *
 * Cluster entry suitable for 4k pages and 32 bit addressing.
 */
struct entry_32b {
public:
	entry_32b(bool multi_page, unsigned attr, uint32_t value);

	bool valid() const;
	bool multi_page() const;
	unsigned attr() const;
	uint32_t value() const;

private:
	uint32_t multi_page_ : 1;	    /* set if multi page entry */
	uint32_t attr_ : 7;		    /* attributes (0 == invalid) */
	uint32_t value_ : 20;		    /* physical page number or size */
};

inline
entry_32b::entry_32b(bool multi_page, unsigned attr, uint32_t value)
: multi_page_{multi_page}
, attr_{attr}
, value_{value}
{ }

inline
bool
entry_32b::valid() const
{
	return attr_;
}

inline
bool
entry_32b::multi_page() const
{
	assert(valid());
	return multi_page_;
}

inline
unsigned
entry_32b::attr() const
{
	assert(valid());
	return attr_ - 1;
}

inline
uint32_t
entry_32b::value() const
{
	assert(valid());
	return value_;
}

/*
 * address_map_dtl::cluster_4k_32b_32B
 *
 * Cluster suitable for 4k pages, 32 bit addressing and 32 byte cachelines.
 *
 * NOTE: impl initialises clusters to zero.
 */
class cluster_4k_32b_32B {
public:
	using entry = entry_32b;	    /* type of entry in cluster */
	static constexpr auto capacity = 8; /* number of entries in cluster */

	void initialise(uint32_t);	    /* assign cluster number */
	void invalidate();		    /* set cluster as invalid */

	bool valid() const;		    /* test if cluster is valid */
	uint32_t cnr() const;		    /* get cluster number */
	bool empty() const;		    /* test if cluster is empty */

	/* entry manipulation */
	entry entry_get(size_t) const;
	void entry_set(size_t, bool multi_page, unsigned attr, uint32_t value);
	void entry_invalidate(size_t);

private:
	uint32_t cnr_;			    /* cluster number (0 == invalid) */
	struct {
		uint32_t multi_page_0 : 1;  /* set if multi page entry */
		uint32_t attr_0 : 7;	    /* attributes (0 == invalid) */
		uint32_t value_0 : 20;	    /* physical page number or size */
		uint32_t multi_page_1 : 1;  /* set if multi page entry */
		uint32_t attr_1 : 7;	    /* attributes (0 == invalid) */
		uint32_t value_1 : 20;	    /* physical page number or size */
	} __attribute__((packed)) entries_[capacity / 2];
};
static_assert(sizeof(cluster_4k_32b_32B) == 32);

inline
void
cluster_4k_32b_32B::initialise(uint32_t cnr)
{
	cnr_ = cnr + 1;

	/* check for integer overflow */
	assert(cnr_);
}

inline
void
cluster_4k_32b_32B::invalidate()
{
	cnr_ = 0;
	for (auto &e : entries_)
		e.attr_0 = e.attr_1 = 0;
}

inline
bool
cluster_4k_32b_32B::valid() const
{
	return cnr_;
}

inline
uint32_t
cluster_4k_32b_32B::cnr() const
{
	assert(valid());
	return cnr_ - 1;
}

inline
bool
cluster_4k_32b_32B::empty() const
{
	assert(valid());
	for (const auto &e : entries_)
		if (e.attr_0 || e.attr_1)
			return false;
	return true;
}

inline
cluster_4k_32b_32B::entry
cluster_4k_32b_32B::entry_get(size_t idx) const
{
	assert(valid());
	assert(idx < capacity);
	auto &e = entries_[idx / 2];
	if (idx & 1)
		return {!!e.multi_page_1, e.attr_1, e.value_1};
	else
		return {!!e.multi_page_0, e.attr_0, e.value_0};
}

inline
void
cluster_4k_32b_32B::entry_set(size_t idx, bool multi_page, unsigned attr,
			      uint32_t value)
{
	assert(valid());
	assert(idx < capacity);
	auto &e = entries_[idx / 2];
	if (idx & 1) {
		e.multi_page_1 = multi_page;
		e.attr_1 = attr + 1;
		e.value_1 = value;

		/* check for overflows */
		assert(e.attr_1);
		assert(e.attr_1 == attr + 1);
		assert(e.value_1 == value);
	} else {
		e.multi_page_0 = multi_page;
		e.attr_0 = attr + 1;
		e.value_0 = value;

		/* check for overflows */
		assert(e.attr_0);
		assert(e.attr_0 == attr + 1);
		assert(e.value_0 == value);
	}
}

inline
void
cluster_4k_32b_32B::entry_invalidate(size_t idx)
{
	assert(valid());
	assert(idx < capacity);
	auto &e = entries_[idx / 2];
	if (idx & 1)
		e.attr_1 = 0;
	else
		e.attr_0 = 0;
}

/*
 * address_map_dtl::alloc_page
 *
 * Allocate hash table using page allocator.
 */
struct alloc_page {
	static constexpr auto initial_size = PAGE_SIZE;

	static void *
	calloc(size_t size, void *owner)
	{
		auto p = page_alloc(size, MA_FAST, owner);
		if (!p)
			return nullptr;
		memset(phys_to_virt(p), 0, size);
		return phys_to_virt(p.release());
	}

	static void
	free(void *p, size_t size, void *owner)
	{
		if (!p)
			return;
		page_free(virt_to_phys(p), size, owner);
	}
};

/*
 * address_map_dtl::impl - address map implementation
 */
template<class Addr, class Cluster, class Hash, class Alloc>
class impl {
public:
	static constexpr auto cluster_capacity = Cluster::capacity;
	using hash = Hash;

	struct entry {
		Addr virt;
		::phys phys;
		size_t size;
		unsigned attr;
	};

	using find_result = std::optional<entry>;

	impl(size_t initial_entries = 0, unsigned max_load = 50);
	~impl();

	void map(Addr virt, phys phys, size_t size, unsigned attr);
	void unmap(Addr virt, size_t size);
	void clear();
	template<class Fn> void for_each(Fn);
	find_result find(Addr virt) const;
	size_t capacity() const;
	size_t size() const;
	bool empty() const;

private:
	void rehash();

	const unsigned max_load_;
	size_t capacity_;
	size_t size_;
	Cluster *t_;
};

template<class Addr, class Cluster, class Hash, class Alloc>
impl<Addr, Cluster, Hash, Alloc>::impl(size_t initial_entries, unsigned max_load)
: max_load_{max_load}
, capacity_{(initial_entries + cluster_capacity - 1) / cluster_capacity}
, size_{0}
, t_{(Cluster *)Alloc::calloc(capacity_ * sizeof(Cluster), this)}
{
	assert(max_load < 100);

	if (capacity_ && !t_)
		panic("OOM!");
}

template<class Addr, class Cluster, class Hash, class Alloc>
impl<Addr, Cluster, Hash, Alloc>::~impl()
{
	Alloc::free(t_, capacity_ * sizeof(Cluster), this);
}

template<class Addr, class Cluster, class Hash, class Alloc>
void
impl<Addr, Cluster, Hash, Alloc>::map(Addr virt, phys phys, size_t size, unsigned attr)
{
	assert(size);
	assert(std::countr_zero(size) >= std::countr_zero(PAGE_SIZE));
	assert(std::countr_zero((uintptr_t)virt) >= std::countr_zero(size));

	size /= PAGE_SIZE;

	auto pnr = phys.phys() / PAGE_SIZE;	/* physical page number */
	auto vnr = (uintptr_t)virt / PAGE_SIZE;	/* virtual page number */
	size_t pages = size;

	while (pages) {
		/* rehash if necessary */
		if (size_ >= capacity_ * max_load_ / 100)
			rehash();
		/* find table entry */
		const auto cnr = vnr / cluster_capacity; /* cluster number */
		auto ti = hash()(cnr) % capacity_;
		while (t_[ti].valid() && t_[ti].cnr() != cnr)
			ti = (ti + 1) % capacity_;
		/* either the table entry matches or does not exist */
		if (!t_[ti].valid()) {
			t_[ti].initialise(cnr);
			++size_;
		}
		/* set cluster entries */
		auto ci = vnr % cluster_capacity;
		while (pages && ci < cluster_capacity) {
			assert(!t_[ti].entry_get(ci).valid());
			if (size > 1)
				t_[ti].entry_set(ci, true, attr, ci % 2 ? size : pnr);
			else
				t_[ti].entry_set(ci, false, attr, pnr);
			--pages;
			++vnr;
			++ci;
		}
	}
}

template<class Addr, class Cluster, class Hash, class Alloc>
void
impl<Addr, Cluster, Hash, Alloc>::unmap(Addr virt, size_t size)
{
	assert(size);
	assert(std::countr_zero(size) >= std::countr_zero(PAGE_SIZE));
	assert(std::countr_zero((uintptr_t)virt) >= std::countr_zero(size));
	assert(find(virt));
	assert(find(virt)->size == size);

	size /= PAGE_SIZE;
	auto vnr = (uintptr_t)virt / PAGE_SIZE;
	size_t pages = size;

	while (pages) {
		/* find table entry */
		auto cnr = vnr / cluster_capacity;
		auto ti = hash()(cnr) % capacity_;
		while (t_[ti].cnr() != cnr)
			ti = (ti + 1) % capacity_;
		/* invalidate cluster entries */
		auto ci = vnr % cluster_capacity;
		while (pages && ci < cluster_capacity) {
			assert(t_[ti].entry_get(ci).valid());
			t_[ti].entry_invalidate(ci);
			--pages;
			++vnr;
			++ci;
		}
		/* cleanup if necessary */
		if (!t_[ti].empty())
			continue;
		t_[ti].invalidate();
		--size_;
		for (ti = (ti + 1) % capacity_; t_[ti].valid();
		     ti = (ti + 1) % capacity_) {
			for (auto tn = hash()(t_[ti].cnr()) % capacity_;
			     tn != ti;
			     tn = (tn + 1) % capacity_) {
				if (t_[tn].valid())
					continue;
				t_[tn] = t_[ti];
				t_[ti].invalidate();
			}
		}
	}
}

template<class Addr, class Cluster, class Hash, class Alloc>
void
impl<Addr, Cluster, Hash, Alloc>::clear()
{
	Alloc::free(t_, capacity_ * sizeof(Cluster), this);
	capacity_ = 0;
	size_ = 0;
	t_ = nullptr;
}

template<class Addr, class Cluster, class Hash, class Alloc>
template<class Fn>
void
impl<Addr, Cluster, Hash, Alloc>::for_each(Fn f)
{
#warning IMPLEMENT
assert(0);
}

template<class Addr, class Cluster, class Hash, class Alloc>
typename impl<Addr, Cluster, Hash, Alloc>::find_result
impl<Addr, Cluster, Hash, Alloc>::find(Addr virt) const
{
	const auto vnr = (uintptr_t)virt / PAGE_SIZE;
	const auto cnr = vnr / cluster_capacity;
	if (!t_)
		return std::nullopt;
	auto ti = hash()(cnr) % capacity_;
	while (t_[ti].valid() && t_[ti].cnr() != cnr)
		ti = (ti + 1) % capacity_;
	const auto ci = vnr % cluster_capacity;
	if (!t_[ti].valid() || !t_[ti].entry_get(ci).valid())
		return std::nullopt;
	assert(0);
#warning FIX FIX FIX!!
	if (t_[ti].entry_get(ci).multi_page())
		return entry{
#warning FIX FIX FIX!!
			0,
			phys{t_[ti].entry_get(ci & ~1ul).value() * PAGE_SIZE},
			     t_[ti].entry_get(ci | 1ul).value() * PAGE_SIZE,
			     t_[ti].entry_get(ci).attr()};
	return entry{
#warning FIX FIX FIX!!
		0,
		phys{t_[ti].entry_get(ci).value() * PAGE_SIZE},
		PAGE_SIZE,
		t_[ti].entry_get(ci).attr()};
}

template<class Addr, class Cluster, class Hash, class Alloc>
size_t
impl<Addr, Cluster, Hash, Alloc>::capacity() const
{
	return capacity_;
}

template<class Addr, class Cluster, class Hash, class Alloc>
size_t
impl<Addr, Cluster, Hash, Alloc>::size() const
{
	return size_;
}

template<class Addr, class Cluster, class Hash, class Alloc>
bool
impl<Addr, Cluster, Hash, Alloc>::empty() const
{
	return !size_;
}

template<class Addr, class Cluster, class Hash, class Alloc>
void
impl<Addr, Cluster, Hash, Alloc>::rehash()
{
	static_assert(Alloc::initial_size / sizeof(Cluster) > 0);

	auto cap = capacity_ * 2 ?: Alloc::initial_size / sizeof(Cluster);
	Cluster *t = (Cluster *)Alloc::calloc(cap * sizeof(Cluster), this);
	if (!t)
		panic("OOM!");

	/* copy entries from old table to new table */
	for (size_t to = 0; to < capacity_; ++to) {
		if (!t_[to].valid())
			continue;
		/* find new table entry */
		auto tn = hash()(t_[to].cnr()) % cap;
		while (t[tn].valid())
			tn = (tn + 1) % cap;
		t[tn] = t_[to];
	}

	Alloc::free(t_, capacity_ * sizeof(Cluster), this);
	capacity_ = cap;
	t_ = t;
}

}

/*
 * address_map_4k_32b_32B
 *
 * Address map suitable for 4k pages, 32 bit addressing and 32 byte cachelines.
 */
template<class Addr, class Alloc = address_map_dtl::alloc_page>
using address_map_4k_32b_32B = address_map_dtl::impl<
					Addr,
					address_map_dtl::cluster_4k_32b_32B,
					address_map_dtl::hash_32_6shift,
					Alloc>;

/*
 * Define a sensible default address_map for the current platform.
 */
#if UINTPTR_MAX == 0xffffffff
	#ifdef CONFIG_CACHE
		#if CONFIG_DCACHE_LINE_SIZE == 32
			using address_map = address_map_4k_32b_32B<void *>;
			using file_map = address_map_4k_32b_32B<uint64_t>;
		#else
			#error No default address_map defined for this platform.
		#endif
	#else
		/* Use 32-byte clusters when there's no cache */
		using address_map = address_map_4k_32b_32B<void *>;
		using file_map = address_map_4k_32b_32B<uint64_t>;
	#endif
#elif UINTPTR_MAX == 0xffffffffffffffff
	#warning TODO: 64-bit address_map.
#else
	#error No default address_map defined for this platform.
#endif
