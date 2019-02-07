#ifndef dma_h
#define dma_h

#include <cstddef>

#if defined(__cplusplus)
extern "C" {
#endif

void *dma_alloc(size_t);
void dma_cache_flush(const void *, size_t);
void dma_cache_invalidate(const void *, size_t);

#if defined(__cplusplus)
}
#endif

#endif
