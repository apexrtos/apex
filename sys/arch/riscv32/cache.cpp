/*
 * NOTE: this file is also used by boot loader
 */
#include <sys/include/arch/cache.h>

#include <conf/config.h>
#include <sys/include/kernel.h>

/*
 * Make sure that instruction & data caches are coherent
 */
void
cache_coherent_exec(const void *p, size_t len)
{
#if defined(CONFIG_CACHE) && !defined(CONFIG_COHERENT_CACHE)
	if (cache_coherent_range(p, len))
		return;

	#warning cache_coherent_exec not implemented
	assert(0);
#endif
}

/*
 * Flush data cache to memory
 */
void
cache_flush(const void *p, size_t len)
{
#if defined(CONFIG_CACHE)
	#warning cache_flush not implemented
	assert(0);
#endif
}

/*
 * Mark data cache lines as invalid
 */
void
cache_invalidate(const void *p, size_t len)
{
#if defined(CONFIG_CACHE)
	#warning cache_invalidate not implemented
	assert(0);
#endif
}

/*
 * Flush data cache to memory and mark cache lines as invalid
 */
void
cache_flush_invalidate(const void *p, size_t len)
{
#if defined(CONFIG_CACHE)
	#warning cache_flush_invalidate not implemented
	assert(0);
#endif
}

/*
 * Test if address range covers whole data cache lines
 */
bool
cache_aligned(const void *p, size_t len)
{
#if defined(CONFIG_CACHE)
	const int mask = CONFIG_DCACHE_LINE_SIZE - 1;
	return !((uintptr_t)p & mask || len & mask);
#else
	return true;
#endif
}

