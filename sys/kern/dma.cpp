#include "dma.h"

#include <arch.h>
#include <conf/config.h>
#include <kernel.h>
#include <list>
#include <page.h>

namespace {

struct dma_page {
	size_t alloc;
	phys *page;
};

std::list<dma_page> pages;

char dma_id;

}

/*
 * Allocate a buffer suitable for use with a DMA controller.
 *
 * For now, all DMA allocations are cache coherent.
 *
 * REVISIT: replace with slab allocator?
 */
void *
dma_alloc(size_t len)
{
#if defined(CONFIG_CACHE) && !defined(CONFIG_CACHE_COHERENT_DMA)
	len = ALIGNn(len, CONFIG_DCACHE_LINE_SIZE);
#endif
	if (len > CONFIG_PAGE_SIZE)
		return nullptr;

	/* find page with free space >= len */
	for (auto p : pages) {
		if (CONFIG_PAGE_SIZE - p.alloc < len)
			continue;
		auto r = phys_to_virt(p.page + p.alloc);
		p.alloc += len;
		return r;
	}

	/* allocate new page */
	auto p = page_alloc_order(0, MA_FAST | MA_DMA | MA_CACHE_COHERENT, &dma_id);
	if (!p)
		return nullptr;
	pages.push_back({len, p});
	return phys_to_virt(p);
}

/*
 * Flush cache if necessary for DMA transfer.
 */
void
dma_cache_flush(const void *p, size_t len)
{
#if !defined(CONFIG_CACHE_COHERENT_DMA)
	cache_flush(p, len);
#endif
}

/*
 * Invalidate cache if necessary after DMA transfer.
 */
void
dma_cache_invalidate(const void *p, size_t len)
{
#if !defined(CONFIG_CACHE_COHERENT_DMA)
	cache_invalidate(p, len);
#endif
}
