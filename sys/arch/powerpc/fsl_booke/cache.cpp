/*
 * NOTE: this file is also used by boot loader
 */
#include <sys/include/arch/cache.h>

#include <algorithm>
#include <conf/config.h>
#include <cpu.h>
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

	compiler_barrier();

	const size_t sz = std::max(CONFIG_DCACHE_LINE_SIZE, CONFIG_ICACHE_LINE_SIZE);
	const std::byte *bp = static_cast<const std::byte *>(p);
	const std::byte *start = TRUNCn(bp, sz);
	const std::byte *end = ALIGNn(bp + len, sz);

	for (const std::byte *l = start; l != end; l += sz)
		asm volatile("dcbst 0, %0" :: "r"(l));
	asm volatile("sync");
	for (const std::byte *l = start; l != end; l += sz)
		asm volatile("icbi 0, %0" :: "r"(l));
	asm volatile("sync");
	asm volatile("isync");
#endif
}

/*
 * Flush data cache to memory
 */
void
cache_flush(const void *p, size_t len)
{
#warning cache_flush not implemented
assert(0);

#if defined(CONFIG_CACHE)


#endif
}

/*
 * Mark data cache lines as invalid
 */
void
cache_invalidate(const void *p, size_t len)
{
#warning cache_invalidate not implemented
assert(0);

#if defined(CONFIG_CACHE)


#endif
}

/*
 * Flush data cache to memory and mark cache lines as invalid
 */
void
cache_flush_invalidate(const void *p, size_t len)
{
#warning cache_flush_invalidate not implemented
assert(0);

#if defined(CONFIG_CACHE)


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
