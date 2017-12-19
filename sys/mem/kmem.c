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

#include <kernel.h>
#include <page.h>
#include <sched.h>
#include <vm.h>
#include <kmem.h>

/*
 * Block header
 *
 * All free blocks that have same size are linked each other.
 * In addition, all free blocks within same page are also linked.
 */
struct block_hdr {
	u_short	magic;			/* magic number */
	u_short	size;			/* size of this block */
	struct	list link;		/* link to the free list */
	struct	block_hdr *pg_next;	/* next block in same page */
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
	u_short	magic;			/* magic number */
	u_short	nallocs;		/* number of allocated blocks */
	struct	block_hdr first_blk;	/* first block in this page */
};

#define ALIGN_SIZE	16
#define ALIGN_MASK	(ALIGN_SIZE - 1)
#define ALLOC_ALIGN(n)	(((vaddr_t)(n) + ALIGN_MASK) & (vaddr_t)~ALIGN_MASK)

#define BLOCK_MAGIC	0xdead
#define PAGE_MAGIC	0xbeef

#define BLKHDR_SIZE	(sizeof(struct block_hdr))
#define PGHDR_SIZE	(sizeof(struct page_hdr))
#define MAX_ALLOC_SIZE	(size_t)(PAGE_SIZE - PGHDR_SIZE)

#define MIN_BLOCK_SIZE	(BLKHDR_SIZE + 16)
#define MAX_BLOCK_SIZE	(u_short)(PAGE_SIZE - (PGHDR_SIZE - BLKHDR_SIZE))

/* macro to point the page header from specific address */
#define PAGE_TOP(n)	(struct page_hdr *) \
			    ((vaddr_t)(n) & (vaddr_t)~(PAGE_SIZE - 1))

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
static struct list free_blocks[NR_BLOCK_LIST];

/*
 * Find the free block for the specified size.
 * Returns pointer to free block, or NULL on failure.
 *
 * First, it searches from the list of same size. If it does not
 * exists, then it will search the list of larger one.
 * It will use the block of smallest size that satisfies the
 * specified size.
 */
static struct block_hdr *
block_find(size_t size)
{
	int i;
	list_t n;

	for (i = (int)((u_int)size >> 4); i < NR_BLOCK_LIST; i++) {
		if (!list_empty(&free_blocks[i]))
			break;
	}
	if (i >= NR_BLOCK_LIST)
		return NULL;

	n = list_first(&free_blocks[i]);
	return list_entry(n, struct block_hdr, link);
}

/*
 * Allocate memory block for kernel
 *
 * This function does not fill the allocated block by 0 for performance.
 * kmem_alloc() returns NULL on failure.
 */
void *
kmem_alloc(size_t size)
{
	struct block_hdr *blk, *newblk;
	struct page_hdr *pg;
	void *p;

	ASSERT(irq_level == 0);

	sched_lock();		/* Lock scheduler */
	/*
	 * First, the free block of enough size is searched
	 * from the page already used. If it does not exist,
	 * new page is allocated for free block.
	 */
	size = (size_t)ALLOC_ALIGN(size + BLKHDR_SIZE);

	ASSERT(size && size <= MAX_ALLOC_SIZE);

	blk = block_find(size);
	if (blk) {
		/* Block found */
		list_remove(&blk->link); /* Remove from free list */
		pg = PAGE_TOP(blk);	 /* Get the page address */
	} else {
		/* No block found. Allocate new page */
		if ((pg = page_alloc(PAGE_SIZE)) == NULL) {
			sched_unlock();
			return NULL;
		}
		pg = (struct page_hdr *)phys_to_virt(pg);
		pg->nallocs = 0;
		pg->magic = PAGE_MAGIC;

		/* Setup first block */
		blk = &(pg->first_blk);
		blk->magic = BLOCK_MAGIC;
		blk->size = MAX_BLOCK_SIZE;
		blk->pg_next = NULL;
	}
	/* Sanity check */
	if (pg->magic != PAGE_MAGIC || blk->magic != BLOCK_MAGIC)
		panic("kmem_alloc: overrun");
	/*
	 * If the found block is large enough, split it.
	 */
	if (blk->size - size >= MIN_BLOCK_SIZE) {
		/* Make new block */
		newblk = (struct block_hdr *)((char *)blk + size);
		newblk->magic = BLOCK_MAGIC;
		newblk->size = (u_short)(blk->size - size);
		list_insert(&free_blocks[BLKIDX(newblk)], &newblk->link);

		/* Update page list */
		newblk->pg_next = blk->pg_next;
		blk->pg_next = newblk;

		blk->size = (u_short)size;
	}
	/* Increment allocation count of this page */
	pg->nallocs++;
	p = (char *)blk + BLKHDR_SIZE;
	sched_unlock();
	return p;
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
	struct block_hdr *blk;
	struct page_hdr *pg;

	ASSERT(irq_level == 0);
	ASSERT(ptr);

	/* Lock scheduler */
	sched_lock();

	/* Get the block header */
	blk = (struct block_hdr *)((char *)ptr - BLKHDR_SIZE);
	if (blk->magic != BLOCK_MAGIC)
		panic("kmem_free: invalid address");

	/*
	 * Return the block to free list. Since kernel code will
	 * request fixed size of memory block, we don't merge the
	 * blocks to use it as cache.
	 */
	list_insert(&free_blocks[BLKIDX(blk)], &blk->link);

	/* Decrement allocation count of this page */
	pg = PAGE_TOP(blk);
	if (--pg->nallocs <= 0) {
		/*
		 * No allocated block in this page.
		 * Remove all blocks and deallocate this page.
		 */
		for (blk = &(pg->first_blk); blk != NULL; blk = blk->pg_next) {
			list_remove(&blk->link); /* Remove from free list */
		}
		pg->magic = 0;
		page_free(virt_to_phys(pg), PAGE_SIZE);
	}
	sched_unlock();
}

/*
 * Map specified virtual address to the kernel address
 * Returns kernel address on success, or NULL if no mapped memory.
 */
void *
kmem_map(void *addr, size_t size)
{
	void *phys;

	phys = vm_translate(addr, size);
	if (phys == NULL)
		return NULL;
	return phys_to_virt(phys);
}

void
kmem_init(void)
{
	int i;

	for (i = 0; i < NR_BLOCK_LIST; i++)
		list_init(&free_blocks[i]);
}
