#pragma once

#include <cassert>
#include <cinttypes>
#include <compare>
#include <conf/config.h>

/*
 * Physical (Real) Addresses
 *
 * Note: physical address 0 is valid on many platforms.
 */
template<typename T>
class physT {
public:
	using value_type = T;
	static constexpr T invalid = -1;

	constexpr physT()
	: phys_{invalid}
	{ }

	explicit constexpr physT(unsigned long long phys)
	: phys_(phys)
	{
		assert(phys_ != invalid);
		assert(phys_ == phys);
	}

	/* get physical address pointer */
	void *phys_ptr() const
	{
		assert(phys_ != invalid);
		assert(phys_ <= UINTPTR_MAX);
		return reinterpret_cast<void *>(phys_);
	}

	/* get physical address */
	T phys() const
	{
		assert(phys_ != invalid);
		return phys_;
	}

	explicit operator bool() const
	{
		return phys_ != invalid;
	}

	auto operator<=>(const physT &) const = default;

private:
	T phys_;
};

#if defined(CONFIG_PAE)
using phys = physT<uint64_t>;
#else
using phys = physT<uintptr_t>;
#endif

constexpr phys operator""_phys(unsigned long long addr)
{
	return phys{addr};
}

/*
 * Address translation
 */
static inline void *
phys_to_virt(const phys pa)
{
	assert(pa.phys() < (UINTPTR_MAX - CONFIG_PAGE_OFFSET));
	return (void *)(pa.phys() + CONFIG_PAGE_OFFSET);
}

static inline phys
virt_to_phys(const void *va)
{
	assert(reinterpret_cast<uintptr_t>(va) >= CONFIG_PAGE_OFFSET);
	return phys{reinterpret_cast<uintptr_t>(va) - CONFIG_PAGE_OFFSET};
}

