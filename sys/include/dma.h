#pragma once

#include <cstddef>
#include <types.h>

struct iovec;

void *dma_alloc(size_t);

#include <functional>

ssize_t dma_prepare(bool from_iov, const iovec *, size_t iov_offset,
		    size_t len, size_t transfer_min, size_t transfer_max,
		    size_t transfer_modulo, size_t address_alignment,
		    void *bounce_buf, size_t bounce_size,
		    std::function<bool(phys *, size_t)> add_transfer);

void dma_finalise(bool from_iov, const iovec *, size_t iov_offset,
		  size_t len, size_t transfer_min, size_t transfer_max,
		  size_t transfer_modulo, size_t address_alignment,
		  void *bounce_buf, size_t bounce_size, size_t transferred);
