/*
 * NOTE: this file is also used by boot loader
 */
#include <sys/include/arch.h>

#include <conf/config.h>
#include <cpu.h>
#include <stddef.h>
#include <sys/include/kernel.h>
#include <sys/param.h>

/*
 * Make sure that instruction & data caches are coherent
 *
 * Cortex-M7 has a Harvard architecture cache. We need to flush the data
 * cache & invalidate the instruction cache.
 */
void
cache_coherent_exec(const void *p, size_t len)
{
#if defined(CONFIG_CACHE)
	if (cache_coherent_range(p, len))
		return;
	/* ensure all previous memory accesses complete before we start cache
	 * maintenance operations */
	asm volatile("dmb" ::: "memory");
	const size_t sz = MAX(CONFIG_DCACHE_LINE_SIZE, CONFIG_ICACHE_LINE_SIZE);
	const void *start = TRUNCn(p, sz);
	const void *end = ALIGNn(p + len, sz);
	for (const void *l = start; l != end; l += CONFIG_DCACHE_LINE_SIZE)
		write32(&CBP->DCCMVAU, (uintptr_t)l);
	for (const void *l = start; l != end; l += CONFIG_ICACHE_LINE_SIZE)
		write32(&CBP->ICIMVAU, (uintptr_t)l);
	/* wait for cache maintenance operations to complete */
	asm volatile("dsb");
	/* flush instruction pipeline */
	asm volatile("isb");
#endif
}

/*
 * Flush data cache to memory
 */
void
cache_flush(const void *p, size_t len)
{
#if defined(CONFIG_CACHE)
	/* ensure all previous memory accesses complete before we start cache
	 * maintenance operations */
	asm volatile("dmb" ::: "memory");
	if (cache_coherent_range(p, len))
		return;
	const void *start = TRUNCn(p, CONFIG_DCACHE_LINE_SIZE);
	const void *end = ALIGNn(p + len, CONFIG_DCACHE_LINE_SIZE);
	for (const void *l = start; l != end; l += CONFIG_DCACHE_LINE_SIZE)
		write32(&CBP->DCCMVAC, (uintptr_t)l);
	/* wait for cache maintenance operations to complete */
	asm volatile("dsb");
#endif
}

/*
 * Mark data cache lines as invalid
 */
void
cache_invalidate(const void *p, size_t len)
{
#if defined(CONFIG_CACHE)
	if (cache_coherent_range(p, len))
		return;
	const void *start = TRUNCn(p, CONFIG_DCACHE_LINE_SIZE);
	const void *end = ALIGNn(p + len, CONFIG_DCACHE_LINE_SIZE);
	for (const void *l = start; l != end; l += CONFIG_DCACHE_LINE_SIZE)
		write32(&CBP->DCIMVAC, (uintptr_t)l);
	/* wait for cache maintenance operations to complete */
	asm volatile("dsb");
#endif
}

/*
 * Flush data cache to memory and mark cache lines as invalid
 */
void
cache_flush_invalidate(const void *p, size_t len)
{
#if defined(CONFIG_CACHE)
	/* ensure all previous memory accesses complete before we start cache
	 * maintenance operations */
	asm volatile("dmb" ::: "memory");
	if (cache_coherent_range(p, len))
		return;
	const void *start = TRUNCn(p, CONFIG_DCACHE_LINE_SIZE);
	const void *end = ALIGNn(p + len, CONFIG_DCACHE_LINE_SIZE);
	for (const void *l = start; l != end; l += CONFIG_DCACHE_LINE_SIZE)
		write32(&CBP->DCCIMVAC, (uintptr_t)l);
	/* wait for cache maintenance operations to complete */
	asm volatile("dsb");
#endif
}
