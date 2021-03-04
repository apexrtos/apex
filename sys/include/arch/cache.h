#pragma once

/*
 * arch/cache.h - architecture specific cache management
 */

#include <cstddef>

void cache_coherent_exec(const void *, size_t);
void cache_flush(const void *, size_t);
void cache_invalidate(const void *, size_t);
void cache_flush_invalidate(const void *, size_t);
bool cache_coherent_range(const void *, size_t);
bool cache_aligned(const void *, size_t);
