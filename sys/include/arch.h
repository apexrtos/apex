/*
 * Copyright (c) 2005-2008, Kohsuke Ohtani
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

#ifndef _ARCH_H
#define _ARCH_H

#include <sys/cdefs.h>
#include <context.h>
#include <mmu.h>

/* Types for context_set */
#define CTX_KSTACK	0	/* set kernel mode stack address */
#define CTX_KENTRY	1	/* set kernel mode entry address */
#define CTX_KARG	2	/* set kernel mode argument */
#define CTX_USTACK	3	/* set user mode stack address */
#define CTX_UENTRY	4	/* set user mode entry addres */
#define CTX_UARG	5	/* set user mode argument */

/* Interrupt mode for interrupt_setup() */
#define IMODE_EDGE	0	/* edge trigger */
#define IMODE_LEVEL	1	/* level trigger */

/* page types for mmu_map */
#define PG_UNMAP	0	/* no page */
#define PG_READ		1	/* user - read only */
#define PG_WRITE	2	/* user - read/write */
#define PG_SYSTEM	3	/* system */
#define PG_IOMEM	4	/* system - no cache */

/*
 * Virtual/physical address mapping
 */
struct mmumap
{
	vaddr_t	virt;		/* virtual address */
	paddr_t	phys;		/* physical address */
	size_t	size;		/* size */
	int	type;		/* mapping type */
};

#define AUTOSIZE	-1

/* virtual memory mapping type */
#define VMT_RAM		PG_SYSTEM
#define VMT_ROM		PG_SYSTEM
#define VMT_DMA		PG_SYSTEM
#define VMT_IO		PG_IOMEM

#ifdef CONFIG_MMU
#define kern_area(a)	((vaddr_t)(a) >= (vaddr_t)PAGE_OFFSET)
#define user_area(a)	((vaddr_t)(a) <  (vaddr_t)UMEM_MAX)
#else
#define kern_area(a)	1
#define user_area(a)	1
#endif

/* Address translation */
#define phys_to_virt(pa)	(void *)((paddr_t)(pa) + PAGE_OFFSET)
#define virt_to_phys(va)	(void *)((vaddr_t)(va) - PAGE_OFFSET)

/* State for machine_setpower */
#define POW_SUSPEND	1
#define POW_OFF		2

__BEGIN_DECLS
void	 context_set(context_t, int, vaddr_t);
void	 context_switch(context_t, context_t);
void	 context_save(context_t);
void	 context_restore(context_t);
void	 interrupt_enable(void);
void	 interrupt_disable(void);
void	 interrupt_save(int *);
void	 interrupt_restore(int);
void	 interrupt_mask(int);
void	 interrupt_unmask(int, int);
void	 interrupt_setup(int, int);
void	 interrupt_init(void);
void	 mmu_init(struct mmumap *);
pgd_t	 mmu_newmap(void);
void	 mmu_delmap(pgd_t);
int	 mmu_map(pgd_t, void *, void *, size_t, int);
void	 mmu_premap(void *, void *);
void	 mmu_switch(pgd_t);
void	*mmu_extract(pgd_t, void *, size_t);
void	 clock_init(void);
int	 umem_copyin(const void *, void *, size_t);
int	 umem_copyout(const void *, void *, size_t);
int	 umem_strnlen(const char *, size_t, size_t *);
void	 diag_init(void);
void	 diag_print(char *);
void	 machine_init(void);
void	 machine_idle(void);
void	 machine_reset(void);
void	 machine_setpower(int);
void	 syscall_ret(void);
__END_DECLS

#endif /* !_ARCH_H */
