#pragma once

/*
 * arch/mmu.h - architecture specific memory management
 */

#include <address.h>
#include <conf/config.h>
#include <cstddef>

struct as;
struct pgd;

struct mmumap {
	phys paddr;	    /* physical address */
#if defined(CONFIG_MMU)
	void *vaddr;	    /* virtual address */
#endif
	size_t size;	    /* size */
	unsigned flags;	    /* machine specific flags */
};

#if defined(CONFIG_MMU)
void mmu_init(mmumap *, size_t);
pgd *mmu_newmap(pid_t);
void mmu_delmap(pgd *);
int mmu_map(pgd *, phys, void *, size_t, int);
void mmu_premap(void *, void *);
void mmu_switch(pgd *);
void mmu_preload(void *, void *, void *);
phys mmu_extract(pgd *, void *, size_t, int);
#endif

#if defined(CONFIG_MPU)
void mpu_init(const mmumap*, size_t, int);
void mpu_switch(const as *);
void mpu_unmap(const void *, size_t);
void mpu_map(const void *, size_t, int);
void mpu_protect(const void *, size_t, int);
void mpu_fault(const void *, size_t);
void mpu_dump();
#endif
