/*-
 * Copyright (c) 2005, Kohsuke Ohtani
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the author nor the names of any co-contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/*
 * dma.c - DMA management routines for intel 8237 controller
 */

/**
 *  Memo:
 *
 *  [Mode Register]
 *
 *      Bits     Function
 *      -------- Mode Selection
 *      00          Demand Mode
 *      01          Single Mode
 *      10          Block Mode
 *      11          Cascade Mode
 *      -------- Address Increment/Decrement
 *        1         Address Decrement
 *        0         Address Increment
 *      -------- Auto-Initialization Enable
 *         1        Auto-Initialization DMA
 *         0        Single-Cycle DMA
 *      -------- Transfer Type
 *          00      Verify
 *          01      Write
 *          10      Read
 *          11      Illegal
 *      -------- Channel Selection
 *            00    Channel 0 (4)
 *            01    Channel 1 (5)
 *            10    Channel 2 (6)
 *            11    Channel 3 (7)
 *
 *  [Single Mask Register]
 *
 *      Bits     Function
 *      --------
 *      00000    Unused, Set to 0
 *      -------- Set/Clear Mask
 *           1      Set (Disable Channel)
 *           0      Clear (Enable Channel)
 *      -------- Channel Selection
 *            00    Channel 0 (4)
 *            01    Channel 1 (5)
 *            10    Channel 2 (6)
 *            11    Channel 3 (7)
 *
 *
 */

#include <sys/cdefs.h>
#include <driver.h>
#include <cpufunc.h>
#include "dma.h"

#ifdef DEBUG
#define DPRINTF(a) printf a
#else
#define DPRINTF(a)
#endif

#define NR_DMAS		8

#define DMA_MAX		(1024 * 64)
#define DMA_MASK	(DMA_MAX-1)
#define DMA_ALIGN(n)	((((paddr_t)(n)) + DMA_MASK) & ~DMA_MASK)

void dma_stop(int handle);

/*
 * DMA descriptor
 */
struct dma {
	int	chan;		/* dma channel */
	int	in_use;		/* true if used */
};

/*
 * DMA i/o port
 */
struct dma_port {
	int mask;
	int mode;
	int clear;
	int addr;
	int count;
	int page;
};

static const struct dma_port dma_regs[] = {
/*	mask,  mode,  clear, addr,  count, page */
	{0x0a, 0x0b, 0x0c, 0x00, 0x01, 0x87},	/* Channel 0 */
	{0x0a, 0x0b, 0x0c, 0x02, 0x03, 0x83},	/* Channel 1 */
	{0x0a, 0x0b, 0x0c, 0x04, 0x05, 0x81},	/* Channel 2 */
	{0x0a, 0x0b, 0x0c, 0x06, 0x07, 0x82},	/* Channel 3 */
	{0xd4, 0xd6, 0xd8, 0xc0, 0xc2, 0x8f},	/* Channel 4 (n/a) */
	{0xd4, 0xd6, 0xd8, 0xc4, 0xc6, 0x8b},	/* Channel 5 */
	{0xd4, 0xd6, 0xd8, 0xc8, 0xca, 0x89},	/* Channel 6 */
	{0xd4, 0xd6, 0xd8, 0xcc, 0xce, 0x8a},	/* Channel 7 */
};

static struct dma dma_table[NR_DMAS];

/*
 * Attach dma.
 * Return dma handle on success, or -1 on failure.
 * DMA4 can not be used with pc.
 */
int
dma_attach(int chan)
{
	struct dma *dma;

	ASSERT(chan >= 0 && chan < NR_DMAS);
	ASSERT(chan != 4);
	DPRINTF(("DMA%d attached\n", chan));

	irq_lock();
	dma = &dma_table[chan];
	if (dma->in_use) {
		irq_unlock();
		return -1;
	} else {
		dma->chan = chan;
		dma->in_use = 1;
	}
	dma_stop((int)dma);
	irq_unlock();
	return (int)dma;
}

/*
 * Detach dma.
 */
void
dma_detach(int handle)
{
	struct dma *dma = (struct dma *)handle;

	ASSERT(dma->in_use);
	DPRINTF(("DMA%d detached\n", dma->chan));

	irq_lock();
	dma->in_use = 0;
	irq_unlock();
}

void
dma_setup(int handle, void *addr, u_long count, int read)
{
	struct dma *dma = (struct dma *)handle;
	const struct dma_port *regs;
	u_int chan, bits, mode;
	paddr_t paddr;

	ASSERT(handle);
	paddr = (paddr_t)virt_to_phys(addr);

	/* dma address must be under 16M. */
	ASSERT(paddr < 0xffffff);

	irq_lock();

	chan = (u_int)dma->chan;
	regs = &dma_regs[chan];
	bits = (chan < 4) ? chan : chan >> 2;
	mode = read ? 0x44 : 0x48;
	count--;

	outb_p(bits | 0x04, regs->mask);	/* Disable channel */
	outb_p(0x00, regs->clear);		/* Clear byte pointer flip-flop */
	outb_p(bits | mode, regs->mode);	/* Set mode */
	outb_p(paddr >> 0, regs->addr);		/* Address low */
	outb_p(paddr >> 8, regs->addr);		/* Address high */
	outb_p(paddr >> 16, regs->page);	/* Page address */
	outb_p(0x00, regs->clear);		/* Clear byte pointer flip-flop */
	outb_p(count >> 0, regs->count);	/* Count low */
	outb_p(count >> 8, regs->count);	/* Count high */
	outb_p(bits, regs->mask);		/* Enable channel */

	irq_unlock();
}

void
dma_stop(int handle)
{
	struct dma *dma = (struct dma *)handle;
	u_int chan;
	u_int bits;

	ASSERT(handle);
	irq_lock();
	chan = (u_int)dma->chan;

	bits = (chan < 4) ? chan : chan >> 2;
	outb_p(bits | 0x04, dma_regs[chan].mask);	/* Disable channel */
	irq_unlock();
}

/*
 * Allocate DMA buffer
 *
 * Return page address in 64K byte boundary.
 * The caller must deallocate the pages by using page_free().
 */
void *
dma_alloc(size_t size)
{
	void *tmp, *base;

	if (size > DMA_MAX)
		return NULL;

	/* Disable interrupts */
	irq_lock();

	/*
	 * Try to allocate temporary buffer for enough size (64K + size).
	 */
	size = (size_t)PAGE_ALIGN(size);
	tmp = page_alloc((size_t)(DMA_MAX + size));
	if (!tmp) {
		irq_unlock();
		return NULL;
	}
	page_free(tmp, (size_t)(DMA_MAX + size));

	/*
	 * Now, we know the free address with 64k boundary.
	 */
	base = (void *)DMA_ALIGN((paddr_t)tmp);
	page_reserve(base, size);

	/* Restore interrupts */
	irq_unlock();
	return phys_to_virt(base);
}
