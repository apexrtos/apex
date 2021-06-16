#pragma once

/*
 * arch/mmu.h - architecture specific memory management
 */

#include <address.h>
#include <conf/config.h>
#include <lib/expect.h>
#include <span>
#include <sys/types.h>

struct as;

struct mmumap {
	phys paddr;	    /* physical address */
#if defined(CONFIG_MMU)
	void *vaddr;	    /* virtual address */
#endif
	size_t size;	    /* size */
	int prot;	    /* memory protection PROT_{READ,WRITE,EXEC} */
	unsigned flags;	    /* machine specific flags */
};

class pgd {
public:
	virtual ~pgd() = 0;
};

#if defined(CONFIG_MMU)
void mmu_init(std::span<const mmumap>);
expect<std::unique_ptr<pgd>> mmu_newmap(pid_t);
expect_ok mmu_map(as &, phys, void *, size_t, int prot);
void mmu_unmap(as &, void *, size_t);
void mmu_early_map(phys, void *, size_t, unsigned flags);
void mmu_switch(const as &);
expect<phys> mmu_extract(const as &, void *, size_t, int prot);
void mmu_dump();
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
