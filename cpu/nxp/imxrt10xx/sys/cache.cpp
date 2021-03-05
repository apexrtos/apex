/*
 * NOTE: this file is also used by boot loader
 */
#include <sys/include/arch/cache.h>

#include <conf/config.h>
#include <cstdint>

/*
 * cache_coherent_range - test if address range is in a cache coherent region
 */
bool
cache_coherent_range(const void *pv, size_t len)
{
	uintptr_t p = (uintptr_t)pv;

	return p >= CONFIG_DMA_BASE_PHYS &&
	    p + len <= CONFIG_DMA_BASE_PHYS + CONFIG_DMA_SIZE;
}

