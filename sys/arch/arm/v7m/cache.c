#include <sys/include/arch.h>

#include <conf/config.h>
#include <cpu.h>
#include <stddef.h>
#include <sys/include/kernel.h>

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
	const void *start = TRUNCn(p, CONFIG_DCACHE_LINE_SIZE);
	const void *end = ALIGNn(p + len, CONFIG_DCACHE_LINE_SIZE);
	asm volatile("dsb ishst");
	for (const void *l = start; l != end; l += CONFIG_DCACHE_LINE_SIZE)
		CBP->DCCMVAU = l;
	asm volatile("dsb ishst");
	for (const void *l = start; l != end; l += CONFIG_ICACHE_LINE_SIZE)
		CBP->ICIMVAU = l;
	asm volatile("dsb ishst");
	asm volatile("isb");
#endif
}
