/*-
 * Copyright (c) 2008, Kohsuke Ohtani
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
 * mmu.c - memory management unit support routines
 */

/*
 * This module provides virtual/physical address translation for
 * ARM MMU. This kernel will do only page level translation
 * and protection and it does not use ARM protection domain.
 */

#include <kernel.h>
#include <page.h>
#include <syspage.h>
#include <cpu.h>
#include <cpufunc.h>

#define L1TBL_MASK	(L1TBL_SIZE - 1)
#define PGD_ALIGN(n)	((((paddr_t)(n)) + L1TBL_MASK) & ~L1TBL_MASK)

/*
 * Boot page directory.
 * This works as a template for all page directory in the system.
 */
static pgd_t boot_pgd = (pgd_t)BOOT_PGD;

/*
 * Allocate pgd
 *
 * The page directory for ARM must be aligned in 16K bytes
 * boundary. So, we allocates 32K bytes first, and use
 * 16K-aligned area in it.
 */
pgd_t
alloc_pgd(void)
{
	void *pg, *pgd;
	size_t gap;

	/* Allocate 32K first. */
	if ((pg = page_alloc(L1TBL_SIZE * 2)) == NULL)
		return NULL;

	/* Find 16K aligned pointer */
	pgd = (void *)PGD_ALIGN(pg);

	/* Release un-needed area */
	gap = (size_t)((paddr_t)pgd - (paddr_t)pg);
	if (gap != 0)
		page_free(pg, gap);
	page_free((void *)((paddr_t)pgd + L1TBL_SIZE), L1TBL_SIZE - gap);

	return pgd;
}

/*
 * Map physical memory range into virtual address
 *
 * Returns 0 on success, or -1 on failure.
 *
 * Map type can be one of the following type.
 *   PG_UNMAP  - Remove mapping
 *   PG_READ   - Read only mapping
 *   PG_WRITE  - Read/write allowed
 *   PG_SYSTEM - Kernel page
 *   PG_IO     - I/O memory
 *
 * Setup the appropriate page tables for mapping. If there is no
 * page table for the specified address, new page table is
 * allocated.
 *
 * This routine does not return any error even if the specified
 * address has been already mapped to other physical address.
 * In this case, it will just override the existing mapping.
 *
 * In order to unmap the page, pg_type is specified as 0.  But,
 * the page tables are not released even if there is no valid
 * page entry in it. All page tables are released when mmu_delmap()
 * is called when task is terminated.
 */
int
mmu_map(pgd_t pgd, void *phys, void *virt, size_t size, int type)
{
	uint32_t pte_flag = 0;
	pte_t pte;
	void *pg;	/* page */
	vaddr_t va;
	paddr_t pa;

	pa = (paddr_t)PAGE_ALIGN(phys);
	va = (vaddr_t)PAGE_ALIGN(virt);
	size = PAGE_TRUNC(size);

	/*
	 * Set page flag
	 */
	switch (type) {
	case PG_UNMAP:
		pte_flag = 0;
		break;
	case PG_READ:
		pte_flag = (uint32_t)(PTE_PRESENT | PTE_WBUF | PTE_CACHE |
				      PTE_USER_RO);
		break;
	case PG_WRITE:
		pte_flag = (uint32_t)(PTE_PRESENT | PTE_WBUF | PTE_CACHE |
				      PTE_USER_RW);
		break;
	case PG_SYSTEM:
		pte_flag = (uint32_t)(PTE_PRESENT | PTE_WBUF | PTE_CACHE |
				      PTE_SYSTEM);
		break;
	case PG_IOMEM:
		pte_flag = (uint32_t)(PTE_PRESENT | PTE_SYSTEM);
		break;
	default:
		panic("mmu_map");
	}
	/*
	 * Map all pages
	 */
	flush_tlb();

	while (size > 0) {
		if (pte_present(pgd, va)) {
			/* Page table already exists for the address */
			pte = pgd_to_pte(pgd, va);
		} else {
			ASSERT(pte_flag != 0);
			if ((pg = page_alloc(L2TBL_SIZE)) == NULL) {
				DPRINTF(("Error: MMU mapping failed\n"));
				return -1;
			}
			pgd[PAGE_DIR(va)] = (uint32_t)pg | PDE_PRESENT;
			pte = phys_to_virt(pg);
			memset(pte, 0, L2TBL_SIZE);
		}
		/* Set new entry into page table */
		pte[PAGE_TABLE(va)] = (uint32_t)pa | pte_flag;

		/* Process next page */
		pa += PAGE_SIZE;
		va += PAGE_SIZE;
		size -= PAGE_SIZE;
	}
	flush_tlb();
	return 0;
}

/*
 * Create new page map.
 *
 * Returns a page directory on success, or NULL on failure.  This
 * routine is called when new task is created. All page map must
 * have the same kernel page table in it. So, the kernel page
 * tables are copied to newly created map.
 */
pgd_t
mmu_newmap(void)
{
	pgd_t pgd;
	int i;

	pgd = alloc_pgd();
	pgd = phys_to_virt(pgd);
	memset(pgd, 0, L1TBL_SIZE);

	/* Copy kernel page tables */
	i = PAGE_DIR(PAGE_OFFSET);
	memcpy(&pgd[i], &boot_pgd[i], (size_t)(L1TBL_SIZE - i * 4));

	/* Map vector page (address 0) */
	mmu_map(pgd, 0, 0, PAGE_SIZE, PG_SYSTEM);
	return pgd;
}

/*
 * Delete all page map.
 */
void
mmu_delmap(pgd_t pgd)
{
	int i;
	pte_t pte;

	flush_tlb();

	/* Release all user page table */
	for (i = 0; i < PAGE_DIR(PAGE_OFFSET); i++) {
		pte = (pte_t)pgd[i];
		if (pte != 0)
			page_free((void *)((paddr_t)pte & PTE_ADDRESS),
				  L2TBL_SIZE);
	}
	/* Release page directory */
	page_free(virt_to_phys(pgd), L1TBL_SIZE);
}

/*
 * Switch to new page directory
 *
 * This is called when context is switched.
 * Whole TLB/cache must be flushed after loading
 * TLTB register
 */
void
mmu_switch(pgd_t pgd)
{
	uint32_t phys = (uint32_t)virt_to_phys(pgd);

	if (phys != get_ttb())
		switch_ttb(phys);
}

/*
 * Returns the physical address for the specified virtual address.
 * This routine checks if the virtual area actually exist.
 * It returns NULL if at least one page is not mapped.
 */
void *
mmu_extract(pgd_t pgd, void *virt, size_t size)
{
	pte_t pte;
	vaddr_t start, end, pg;

	start = PAGE_TRUNC(virt);
	end = PAGE_TRUNC((vaddr_t)virt + size - 1);

	/* Check all pages exist */
	for (pg = start; pg <= end; pg += PAGE_SIZE) {
		if (!pte_present(pgd, pg))
			return NULL;
		pte = pgd_to_pte(pgd, pg);
		if (!page_present(pte, pg))
			return NULL;
	}

	/* Get physical address */
	pte = pgd_to_pte(pgd, start);
	pg = pte_to_page(pte, start);
	return (void *)(pg + ((vaddr_t)virt - start));
}

/*
 * Map I/O memory for diagnostic device at very early stage.
 */
void
mmu_premap(void *phys, void *virt)
{
	pte_t pte = (pte_t)BOOT_PTE1;

	memset(pte, 0, L2TBL_SIZE);
	boot_pgd[PAGE_DIR(virt)] = (uint32_t)virt_to_phys(pte) | PDE_PRESENT;
	pte[PAGE_TABLE(virt)] = (uint32_t)phys | PTE_PRESENT | PTE_SYSTEM;
	flush_tlb();
}

/*
 * Initialize mmu
 *
 * Paging is already enabled in locore.S. And, physical address
 * 0-4M has been already mapped into kernel space in locore.S.
 * Now, all physical memory is mapped into kernel virtual address
 * as straight 1:1 mapping. User mode access is not allowed for
 * these kernel pages.
 * page_init() must be called before calling this routine.
 */
void
mmu_init(struct mmumap *mmumap_table)
{
	struct mmumap *map;

	for (map = mmumap_table; map->type != 0; map++) {
		if (mmu_map(boot_pgd, (void *)map->phys, (void *)map->virt,
			    map->size, map->type))
			panic("mmu_init");
	}
	/*
	 * Map vector page.
	 */
	if (mmu_map(boot_pgd, 0, ARM_VECTORS, PAGE_SIZE, PG_SYSTEM))
		panic("mmu_init");
}
