/*-
 * Copyright (c) 2005-2006, Kohsuke Ohtani
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
 * kmem.c - kernel memory allocator
 */

/*
 * This is a memory allocator optimized for the low foot print
 * kernel. It works on top of the underlying page allocator, and
 * manages more smaller memory than page size. It will divide one
 * page into two or more blocks, and each page is linked as a
 * kernel page.
 *
 * There are following 3 linked lists to manage used/free blocks.
 *  1) All pages allocated for the kernel memory are linked.
 *  2) All blocks divided in the same page are linked.
 *  3) All free blocks of the same size are linked.
 *
 * Currently, it can not handle the memory size exceeding one page.
 * Instead, a driver can use page_alloc() to allocate larger memory.
 *
 * The kmem functions are used by not only the kernel core but
 * also by the buggy drivers. If such kernel code illegally
 * writes data in exceeding the allocated area, the system will
 * crash easily. In order to detect the memory over run, each
 * free block has a magic ID.
 */

#include <kmem.h>

#include <access.h>
#include <cassert>
#include <conf/config.h>
#include <cstdlib>
#include <cstring>
#include <debug.h>
#include <kernel.h>
#include <page.h>
#include <sync.h>
#include <task.h>

#define MIN(a, b) ((a) < (b) ? (a) : (b))

/*
 * Number of allocatable memory types
 */
#define MEM_ALLOC (1 + (MA_NORMAL != MA_FAST))

/*
 * Block header
 *
 * All free blocks that have same size are linked each other.
 * In addition, all free blocks within same page are also linked.
 */
struct block_hdr {
	uint16_t	    magic;	/* magic number */
	uint16_t	    size;	/* size of this block */
	list		    link;	/* link to the free list */
	block_hdr	   *pg_next;	/* next block in same page */
};

/*
 * Page header
 *
 * The page header is placed at the top of each page. This
 * header is used in order to free the page when there are no
 * used block left in the page. If nr_alloc value becomes zero,
 * that page can be removed from kernel page.
 */
struct page_hdr {
	uint32_t	    magic;	/* magic number */
	uint32_t	    nallocs;	/* number of allocated blocks */
	list		    link;	/* link to page list */
	block_hdr	    first_blk;	/* first block in this page */
};

#define ALIGN_SIZE	16
#define ALIGN_MASK	(ALIGN_SIZE - 1)
#define ALLOC_ALIGN(n)	(((uintptr_t)(n) + ALIGN_MASK) & (uintptr_t)~ALIGN_MASK)

#define ALLOC_MAGIC	0xcafe		/* alloc magic offset by memory type */
#define FREE_MAGIC	0xdead
#define PAGE_MAGIC	0xabcdbeef	/* page magic offset by memory type */

#define ALLOC_MAGIC_OK(x) ((x)->magic >= ALLOC_MAGIC && (x)->magic < ALLOC_MAGIC + MEM_ALLOC)
#define FREE_MAGIC_OK(x) ((x)->magic == FREE_MAGIC)
#define PAGE_MAGIC_OK(x) ((x)->magic >= PAGE_MAGIC && (x)->magic < PAGE_MAGIC + MEM_ALLOC)

#define BLKHDR_SIZE	(sizeof(block_hdr))
#define PGHDR_SIZE	(sizeof(page_hdr))
#define MAX_ALLOC_SIZE	(size_t)(PAGE_SIZE - PGHDR_SIZE)

#define MIN_BLOCK_SIZE	(BLKHDR_SIZE + 16)
#define MAX_BLOCK_SIZE	(uint16_t)(PAGE_SIZE - (PGHDR_SIZE - BLKHDR_SIZE))

/* macro to point the page header from specific address */
#define PAGE_TOP(n)	(page_hdr *) \
			    ((uintptr_t)(n) & (uintptr_t)~(PAGE_SIZE - 1))

/* index of free block list */
#define BLKIDX(b)	((u_int)((b)->size) >> 4)

/* number of free block list */
#define NR_BLOCK_LIST	(PAGE_SIZE / ALIGN_SIZE)

/**
 * Array of the head block of free block list.
 *
 * The index of array is decided by the size of each block.
 * All block has the size of the multiple of 16.
 *
 * ie. free_blocks[0] = list for 16 byte block
 *     free_blocks[1] = list for 32 byte block
 *     free_blocks[2] = list for 48 byte block
 *         .
 *         .
 *     free_blocks[255] = list for 4096 byte block
 *
 * Generally, only one list is used to search the free block with
 * a first fit algorithm. Basically, this allocator also uses a
 * first fit method. However it uses multiple lists corresponding
 * to each block size. A search is started from the list of the
 * requested size. So, it is not necessary to search smaller
 * block's list wastefully.
 *
 * Most of kernel memory allocator is using 2^n as block size.
 * But, these implementation will throw away much memory that
 * the block size is not fit. This is not suitable for the
 * embedded system with low foot print.
 */
static list free_blocks[MEM_ALLOC][NR_BLOCK_LIST];
static list kmem_pages[MEM_ALLOC];
static spinlock kmem_lock;

/*
 * Map memory type to associated memory attributes
 */
long
type_to_attr(unsigned type)
{
	switch (type) {
	case 0:
		return MA_NORMAL;
	case 1:
		return MA_FAST;
	default:
		assert(0);
	}
}

/*
 * Find the free block for the specified size.
 * Returns pointer to free block, or NULL on failure.
 *
 * First, it searches from the list of same size. If it does not
 * exists, then it will search the list of larger one.
 * It will use the block of smallest size that satisfies the
 * specified size.
 */
static block_hdr *
block_find(size_t size, unsigned type)
{
	int i;
	list *n;

	assert(type < MEM_ALLOC);

	for (i = (int)((u_int)size >> 4); i < NR_BLOCK_LIST; i++) {
		if (!list_empty(&free_blocks[type][i]))
			break;
	}
	if (i >= NR_BLOCK_LIST)
		return NULL;

	n = list_first(&free_blocks[type][i]);
	return list_entry(n, block_hdr, link);
}

/*
 * Allocate memory block for kernel
 *
 * This function does not fill the allocated block by 0 for performance.
 * kmem_alloc() returns NULL on failure.
 */
static void *
kmem_alloc_internal(size_t size, unsigned type)
{
	block_hdr *blk, *newblk;
	page_hdr *pg;
	void *p = 0;

	assert(type < MEM_ALLOC);

	spinlock_lock(&kmem_lock);
	kmem_check();
	/*
	 * First, the free block of enough size is searched
	 * from the page already used. If it does not exist,
	 * new page is allocated for free block.
	 */
	size = (size_t)ALLOC_ALIGN(size + BLKHDR_SIZE);

	assert(size && size <= MAX_ALLOC_SIZE);

	blk = block_find(size, type);
	if (blk) {
		/* Block found */
		list_remove(&blk->link); /* Remove from free list */
		pg = PAGE_TOP(blk);	 /* Get the page address */
	} else {
		/* No block found. Allocate new page */
		phys *const pp = page_alloc_order(0,
		    type_to_attr(type) | PAF_EXACT_SPEED, &kern_task);
		if (!pp)
			goto out;
		pg = (page_hdr *)phys_to_virt(pp);
		pg->nallocs = 0;
		pg->magic = PAGE_MAGIC + type;
		list_insert(&kmem_pages[type], &pg->link);

		/* Setup first block */
		blk = &(pg->first_blk);
		blk->magic = FREE_MAGIC;
		blk->size = MAX_BLOCK_SIZE;
		blk->pg_next = NULL;
	}
	/* Sanity check */
	if (!PAGE_MAGIC_OK(pg) || !FREE_MAGIC_OK(blk))
		panic("kmem_alloc: overrun");
	/*
	 * If the found block is large enough, split it.
	 */
	if (blk->size - size >= MIN_BLOCK_SIZE) {
		/* Make new block */
		newblk = (block_hdr *)((char *)blk + size);
		newblk->magic = FREE_MAGIC;
		newblk->size = (uint16_t)(blk->size - size);
		list_insert(&free_blocks[type][BLKIDX(newblk)], &newblk->link);

		/* Update page list */
		newblk->pg_next = blk->pg_next;
		blk->pg_next = newblk;

		blk->size = (uint16_t)size;
	}
	blk->magic = ALLOC_MAGIC + type;
	/* Increment allocation count of this page */
	pg->nallocs++;
	p = (char *)blk + BLKHDR_SIZE;
out:
	spinlock_unlock(&kmem_lock);
	return p;
}

void *
kmem_alloc(size_t size, long mem_attr)
{
	void *p;
	unsigned type, t;

	if (mem_attr == MA_NORMAL)
		type = 0;
	else if (mem_attr == MA_FAST)
		type = 1;
	else
		panic("bad attr");

	t = type;
	do {
		if ((p = kmem_alloc_internal(size, t)))
			return p;
		if (++t == MEM_ALLOC)
			t = 0;
	} while (t != type);

	dbg("kmem_alloc: out of memory allocating %d\n", size);

	return NULL;
}

/*
 * malloc - allocate memory block
 */
void *
malloc(size_t size)
{
	return kmem_alloc(size, MA_NORMAL);
}

/*
 * calloc - allocate zeroed memory block
 */
void *
calloc(size_t m, size_t n)
{
	if (n && m > SIZE_MAX/n)
		return NULL;
	n *= m;
	void *p = malloc(n);
	if (p)
		memset(p, 0, n);
	return p;
}

/*
 * realloc - change size of memory block
 */
void *
realloc(void *p, size_t size)
{
	if (!size) {
		free(p);
		return NULL;
	}

	if (!p)
		return malloc(size);

	block_hdr *blk = (block_hdr *)((char *)p - BLKHDR_SIZE);
	if (!ALLOC_MAGIC_OK(blk))
		panic("realloc: invalid address");

	if (blk->size >= size)
		return p;

	void *np = malloc(size);
	if (!np)
		return NULL;

	memcpy(np, p, MIN(size, blk->size));
	free(p);

	return np;
}

/*
 * Free allocated memory block.
 *
 * Some kernel does not release the free page for the kernel memory
 * because it is needed to allocate immediately later. For example,
 * it is efficient here if the free page is just linked to the list
 * of the biggest size. However, consider the case where a driver
 * requires many small memories temporarily. After these pages are
 * freed, they can not be reused for an application.
 */
void
kmem_free(void *ptr)
{
	block_hdr *blk;
	page_hdr *pg;

	if (!ptr)
		return;

	spinlock_lock(&kmem_lock);
	kmem_check();

	/* Get the block header */
	blk = (block_hdr *)((char *)ptr - BLKHDR_SIZE);
	if (!ALLOC_MAGIC_OK(blk))
		panic("kmem_free: invalid address");

	unsigned type = blk->magic - ALLOC_MAGIC;

	/*
	 * Return the block to free list. Since kernel code will
	 * request fixed size of memory block, we don't merge the
	 * blocks to use it as cache.
	 */
	blk->magic = FREE_MAGIC;
	list_insert(&free_blocks[type][BLKIDX(blk)], &blk->link);

	/* Decrement allocation count of this page */
	pg = PAGE_TOP(blk);
	if (--pg->nallocs <= 0) {
		/*
		 * No allocated block in this page.
		 * Remove all blocks and deallocate this page.
		 */
		for (blk = &(pg->first_blk); blk != NULL; blk = blk->pg_next) {
			list_remove(&blk->link); /* Remove from free list */
			blk->magic = 0;
		}
		list_remove(&pg->link);
		pg->magic = 0;
		page_free(virt_to_phys(pg), PAGE_SIZE, &kern_task);
	}
	spinlock_unlock(&kmem_lock);
}

/*
 * free - free allocated memory block
 */
void
free(void *p)
{
	kmem_free(p);
}

/*
 * Validate kmem
 */
void
kmem_check()
{
#ifdef CONFIG_KMEM_CHECK
	list *head, *n;
	page_hdr *pg;
	block_hdr *blk;

	spinlock_assert_locked(&kmem_lock);

	for (unsigned type = 0; type < MEM_ALLOC; ++type) {
		head = &kmem_pages[type];
		for (n = list_first(head); n != head; n = list_next(n)) {
			pg = list_entry(n, page_hdr, link);
			assert(k_address(pg));
			assert(PAGE_MAGIC_OK(pg));

			for (blk = &(pg->first_blk); blk != NULL; blk = blk->pg_next) {
				assert((void *)blk > (void *)pg);
				assert((void *)blk < ((void *)pg + MAX_BLOCK_SIZE));
				assert(ALLOC_MAGIC_OK(blk) || FREE_MAGIC_OK(blk));
			}
		}
	}
#endif
}

void
kmem_dump()
{
	spinlock_lock(&kmem_lock);
	for (unsigned type = 0; type < MEM_ALLOC; ++type) {
		list *head, *n;
		int i, cnt;

		info("kmem dump (%d)\n", type);
		info("==============\n");
		info(" free size  count\n");
		info(" ---------- --------\n");

		for (i = 0; i < NR_BLOCK_LIST; i++) {
			cnt = 0;
			head = &free_blocks[type][i];
			for (n = list_first(head); n != head; n = list_next(n))
				cnt++;
			if (cnt > 0)
				info("       %4d %8d\n", i << 4, cnt);
		}
#if 0
		info(" all blocks\n");
		info(" ----------\n");

		head = &kmem_pages[type];
		for (n = list_first(head); n != head; n = list_next(n)) {
			page_hdr *pg = list_entry(n, page_hdr, link);

			info(" page %p allocs %d\n", pg, pg->nallocs);
			if (!page_valid(virt_to_phys(pg), 0, &kern_task)) {
				info(" *** page not in kern_area\n");
				continue;
			}
			if (!PAGE_MAGIC_OK(pg)) {
				info(" *** bad page magic %x\n", pg->magic);
				continue;
			}

			info(" blocks:\n");

			for (block_hdr *blk = &(pg->first_blk); blk != NULL;
			    blk = blk->pg_next) {
				if ((void *)blk <= (void *)pg) {
					info(" *** block starts before page %p\n", blk);
					continue;
				}
				if (((void *)blk >= ((void *)pg + MAX_BLOCK_SIZE))) {
					info(" *** block starts after page %p %p\n", blk, pg + MAX_BLOCK_SIZE);
					continue;
				}
				if (!ALLOC_MAGIC_OK(blk) && !FREE_MAGIC_OK(blk)) {
					info(" *** bad magic %p %x\n", blk, blk->magic);
					continue;
				}
				char type;
				if (ALLOC_MAGIC_OK(blk))
					type = 'a';
				else
					type = 'f';
				info("  %c %p %d\n", type, blk, blk->size);
			}
		}
#endif
	}
	spinlock_unlock(&kmem_lock);
}

void
kmem_init()
{
	for (unsigned type = 0; type < MEM_ALLOC; ++type) {
		list_init(&kmem_pages[type]);
		for (int i = 0; i < NR_BLOCK_LIST; i++)
			list_init(&free_blocks[type][i]);
	}
	spinlock_init(&kmem_lock);
}
