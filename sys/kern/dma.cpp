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

