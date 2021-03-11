#include "dma.h"

#include <arch/cache.h>
#include <conf/config.h>
#include <debug.h>
#include <errno.h>
#include <kernel.h>
#include <list>
#include <page.h>
#include <sys/uio.h>

#define dmadbg(...) // dbg(__VA_ARGS__)

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
	if (len > PAGE_SIZE)
		return nullptr;

	/* find page with free space >= len */
	for (auto p : pages) {
		if (PAGE_SIZE - p.alloc < len)
			continue;
		auto r = static_cast<std::byte *>(phys_to_virt(p.page)) + p.alloc;
		p.alloc += len;
		return r;
	}

	/* allocate new page */
	auto p = page_alloc_order(0, MA_DMA | MA_CACHE_COHERENT, &dma_id);
	if (!p)
		return nullptr;
	pages.push_back({len, p});
	return phys_to_virt(p);
}

/*
 * Iterate iov calling do_transfer/do_bounce as appropriate.
 */
int
dma_iterate(const bool from_iov, const iovec *iov, size_t iov_offset,
    size_t total_len, const size_t transfer_min, const size_t transfer_max,
    const size_t transfer_modulo, size_t address_align, void *const bounce_buf,
    const size_t bounce_size,
    std::function<bool(void *, size_t, bool)> do_transfer,
    std::function<void(void *, size_t, void *)> do_bounce)
{
#if defined(CONFIG_CACHE)
	const size_t dclsz = CONFIG_DCACHE_LINE_SIZE;
#else
	const size_t dclsz = 1;
#endif
	std::byte *bounce_p = static_cast<std::byte *>(bounce_buf);
	std::byte *bounce_it = bounce_p;
	std::byte *const bounce_end = bounce_p + bounce_size;

	if (bounce_size) {
		auto attr = page_attr(virt_to_phys(bounce_p), bounce_size);

		if (attr < 0)
			return DERR(-EFAULT);

		/* bounce buffer must reside in DMA capable memory */
		if (!(attr & MA_DMA))
			return DERR(-EINVAL);

		auto bounce = reinterpret_cast<uintptr_t>(bounce_p);

		/* bounce buffer must meet address alignment requirements */
		if (bounce & (address_align - 1))
			return DERR(-EINVAL);

		/* bounce buffer must meet cache alignment requirements */
		if (!(attr & MA_CACHE_COHERENT) && (bounce & (dclsz - 1) ||
		    bounce_size & (dclsz - 1)))
			return DERR(-EINVAL);
	}

	/* sanity check */
	if (!is_pow2(address_align) || !is_pow2(transfer_modulo) ||
	    !is_pow2(transfer_min) || total_len % transfer_modulo)
		return DERR(-EINVAL);

	/* add a transfer to bounce buffer */
	auto add_bounce = [&](std::byte *p, size_t len) {
		if (bounce_it == bounce_end)
			return false;
		if (static_cast<size_t>(bounce_end - bounce_p) <
		    ALIGNn(transfer_min, transfer_modulo))
			return false;
		len = std::min<size_t>(bounce_end - bounce_it, len);
		do_bounce(p, len, bounce_it);
		bounce_it += len;
		iov_offset += len;
		total_len -= len;
		return true;
	};

	/* align bounce buffer to a valid transfer */
	auto align_bounce = [&](std::byte *p, size_t len) {
		const size_t bounce_len = bounce_it - bounce_p;
		const size_t fix = ALIGNn(std::max(bounce_len, transfer_min),
		    transfer_modulo) - bounce_len;
		if (bounce_len == 0 || fix == 0)
			return true;
		dmadbg("  bounce: align\n");
		add_bounce(p, std::min<size_t>(fix, len));
		return false;
	};

	/* flush pending bounce transfer */
	auto flush_bounce = [&] {
		const auto len = bounce_it - bounce_p;
		if (len == 0)
			return true;
		if (!do_transfer(bounce_p, len, true))
			return false;
		bounce_p = bounce_it;
		return true;
	};

	for (int attr, nb = true; total_len;)  {
		/* move to next iov if this one is complete */
		while (iov_offset >= iov->iov_len) {
			iov_offset -= iov->iov_len;
			++iov;
			nb = true;
		}

		std::byte *buf = static_cast<std::byte *>(iov->iov_base);
		std::byte *p = buf + iov_offset;
		size_t len = std::min(iov->iov_len - iov_offset, total_len);

		if (nb)
			dmadbg(" p %p len %zu\n", p, len);

		/* get attributes for next buffer */
		if (nb)
			attr = page_attr(virt_to_phys(p), len);
		if (attr < 0)
			return DERR(-EFAULT);

		/* bounce if destination cannot be written by DMA */
		if (!(attr & MA_DMA)) {
			dmadbg("  bounce: not DMA memory\n");
			if (!add_bounce(p, len))
				break;
			continue;
		}

		/* bounce if destination is misaligned */
		if (reinterpret_cast<uintptr_t>(p) & (address_align - 1)) {
			dmadbg("  bounce: destination misaligned\n");
			if (!add_bounce(p, len))
				break;
			continue;
		}

		const bool cache_ok = attr & MA_CACHE_COHERENT || from_iov;

		/* bounce short requests */
		if (len < std::max(transfer_min, cache_ok ? 0 : dclsz)) {
			dmadbg("  bounce: short\n");
			if (!add_bounce(p, len))
				break;
			continue;
		}

		/* partial cacheline at start of buffer or transfer */
		if (auto ca = ALIGNn(p, dclsz); nb && !cache_ok && p < ca) {
			dmadbg("  bounce: partial cacheline\n");
			if (!add_bounce(p, static_cast<std::byte *>(ca) - p))
				break;
			continue;
		}

		nb = false;

		/* calculate direct transfer size */
		size_t d = len;

		/* partial cacheline at end of buffer */
		if (!cache_ok) {
			auto buf_end = buf + iov->iov_len;
			d = std::min<size_t>(d, TRUNCn(buf_end, dclsz) - p);
		}

		/* limit length to maximum transfer size */
		d = std::min(d, transfer_max);

		/* obey transfer_modulo */
		d = TRUNCn(d, transfer_modulo);

		/* obey transfer_min */
		if (d < len)
			d = TRUNCn(std::min(d, len - transfer_min), transfer_modulo);

		/* bounce short requests */
		if (d < transfer_min) {
			dmadbg("  bounce: min transfer\n");
			if (!add_bounce(p, len))
				break;
			continue;
		}

		/* align & queue bounce buffer */
		if (!align_bounce(p, len))
			continue;
		if (!flush_bounce())
			break;

		/* queue direct transfer */
		if (!do_transfer(p, d, false))
			break;
		iov_offset += d;
		total_len -= d;
	}

	flush_bounce();

	return 0;
}

/*
 * Prepare iov for DMA transfer.
 *
 * A DMA transaction is made up of one or more transfers.
 *
 * This function considers the entries in iov and repeatedly calls add_transfer
 * until add_transfer returns false or all iov entries have been processed.
 *
 * This function caters for:
 * - partial cache lines (only matters when !from_iov)
 * - memory address alignment requirements
 * - data length restrictions on transfers
 *
 * Data is transferred through the bounce buffer if the conditions for direct
 * DMA transfer are not met.
 *
 * Must be followed by a call to dma_finalise with matching arguments.
 */
ssize_t
dma_prepare(
    const bool from_iov,	    /* true if transfer is host->device */
    const iovec *iov,		    /* i/o vector */
    size_t iov_offset,		    /* byte offset into i/o vector */
    size_t len,			    /* byte length of transfer */
    const size_t transfer_min,	    /* minimum transfer length */
    const size_t transfer_max,	    /* maximum transfer length */
    const size_t transfer_modulo,   /* transfer modulo */
    const size_t address_align,	    /* minimum address alignment */
    void *const bounce_buf,	    /* address of bounce buffer */
    const size_t bounce_size,	    /* size of bounce buffer */
    std::function<bool(phys *, size_t)> add_transfer)
{
	dmadbg("dma_prepare: from_iov %d iov_offset %zu transfer_min %zu\n",
	    from_iov, iov_offset, transfer_min);
	dmadbg("             transfer_max %zu transfer_modulo %zu address_align %zu\n",
	    transfer_max, transfer_modulo, address_align);
	dmadbg("             bounce_buf %p bounce_size %zu\n",
	    bounce_buf, bounce_size);

	size_t txn_len = 0;

	auto do_transfer = [&](void *p, size_t len, bool bounce) {
		dmadbg("   do_transfer %p %zu\n", p, len);
		txn_len += len;
		if (from_iov)
			cache_flush(p, len);
		else
			cache_invalidate(p, len);
		return add_transfer(virt_to_phys(p), len);
	};

	auto do_bounce = [&](void *p, size_t len, void *bounce) {
		dmadbg("   do_bounce %p %zu %p\n", p, len, bounce);
		if (from_iov)
			memcpy(bounce, p, len);
	};

	auto r = dma_iterate(from_iov, iov, iov_offset, len, transfer_min,
	    transfer_max, transfer_modulo, address_align, bounce_buf,
	    bounce_size, do_transfer, do_bounce);

	dmadbg(" r %d txn_len %zu\n", r, txn_len);

	return r < 0 ? r : txn_len;
}

/*
 * Finalise DMA transfer.
 */
void dma_finalise(bool from_iov, const iovec *iov, size_t iov_offset,
    size_t len, size_t transfer_min, size_t transfer_max,
    size_t transfer_modulo, size_t address_align, void *bounce_buf,
    size_t bounce_size, size_t transferred)
{
	if (from_iov)
		return;

	dmadbg("dma_finalise:\n");

	size_t txn_len = 0;
	bool bounce_used = false;

	auto invalidate = [&](void *p, size_t len, bool bounce) {
		len = std::min<size_t>(len, transferred - txn_len);
		dmadbg("   invalidate %p %zu\n", p, len);
		cache_invalidate(p, len);
		bounce_used = bounce_used || bounce;
		txn_len += len;
		return txn_len != transferred;
	};

	auto skip_bounce = [](void *p, size_t len, void *bounce) {};

	auto skip_invalidate = [&](void *p, size_t len, bool bounce) {
		txn_len += len;
		return txn_len != transferred;
	};

	auto do_bounce = [&](void *p, size_t len, void *bounce) {
		dmadbg("   do_bounce %p %zu %p\n", p, len, bounce);
		memcpy(p, bounce, len);
	};

	/* first, invalidate all buffers */
	dma_iterate(from_iov, iov, iov_offset, len, transfer_min,
	    transfer_max, transfer_modulo, address_align, bounce_buf,
	    bounce_size, invalidate, skip_bounce);

	/* then, if data was bounced, copy all bounces */
	if (bounce_used) {
		txn_len = 0;
		dma_iterate(from_iov, iov, iov_offset, len, transfer_min,
		    transfer_max, transfer_modulo, address_align, bounce_buf,
		    bounce_size, skip_invalidate, do_bounce);
	}
}
