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

#ifndef _I386_CPU_H
#define _I386_CPU_H

#include <sys/cdefs.h>	/* for __packed */

/*
 * GDTs
 */
#define KERNEL_CS	0x10
#define KERNEL_DS	0x18
#define USER_CS		0x20
#define USER_DS		0x28
#define KERNEL_TSS	0x38

#define NGDTS		8

/*
 * IDTs
 */
#define NIDTS		0x41
#define SYSCALL_INT	0x40
#define INVALID_INT	0xFF


#ifndef __ASSEMBLY__

/*
 * Segment Descriptor
 */
struct seg_desc {
	u_int limit_lo:16;	/* segment limit (lsb) */
	u_int base_lo:16;	/* segment base address (lsb) */
	u_int base_mid:8;	/* segment base address (middle) */
	u_int type:8;		/* type */
	u_int limit_hi:4;	/* segment limit (msb) */
	u_int size:4;		/* size */
	u_int base_hi:8;	/* segment base address (msb) */
} __packed;

/*
 * Gate Descriptor
 */
struct gate_desc {
	u_int offset_lo:16;	/* gate offset (lsb) */
	u_int selector:16;	/* gate segment selector */
	u_int nr_copy:8;	/* stack copy count */
	u_int type:8;		/* type */
	u_int offset_hi:16;	/* gate offset (msb) */
} __packed;

/*
 * Linear memory description for lgdt and lidt instructions.
 */
struct desc_p {
	uint16_t limit;
	uint32_t base;
} __packed;

/*
 * Segment size
 */
#define SIZE_32		0x4	/* 32-bit segment */
#define SIZE_16		0x0	/* 16-bit segment */
#define SIZE_4K		0x8	/* 4K limit field */

/*
 * Segment type
 */
#define ST_ACC		0x01	/* accessed */
#define ST_LDT		0x02	/* LDT */
#define ST_CALL_GATE_16	0x04	/* 16-bit call gate */
#define ST_TASK_GATE	0x05	/* task gate */
#define ST_TSS		0x09	/* task segment */
#define ST_CALL_GATE	0x0c	/* call gate */
#define ST_INTR_GATE	0x0e	/* interrupt gate */
#define ST_TRAP_GATE	0x0f	/* trap gate */

#define ST_TSS_BUSY	0x02	/* task busy */

#define ST_DATA		0x10	/* data */
#define ST_DATA_W	0x12	/* data, writable */
#define ST_DATA_E	0x14	/* data, expand-down */
#define ST_DATA_EW	0x16	/* data, expand-down, writable */

#define ST_CODE		0x18	/* code */
#define ST_CODE_R	0x1a	/* code, readable */
#define ST_CODE_C	0x1c	/* code, conforming */
#define ST_CODE_CR	0x1e	/* code, conforming, readable */

#define ST_KERN		0x00	/* kernel access only */
#define ST_USER		0x60	/* user access */

#define ST_PRESENT	0x80	/* segment present */

/*
 * Task State Segment (TSS)
 */

#define IO_BITMAP_SIZE		(65536/8 + 1)
#define INVALID_IO_BITMAP	0x8000

struct tss {
	uint32_t back_link;
	uint32_t esp0, ss0;
	uint32_t esp1, ss1;
	uint32_t esp2, ss2;
	uint32_t cr3;
	uint32_t eip;
	uint32_t eflags;
	uint32_t eax, ecx, edx, ebx;
	uint32_t esp, ebp, esi, edi;
	uint32_t es, cs, ss, ds, fs, gs;
	uint32_t ldt;
	uint16_t dbg_trace;
	uint16_t io_bitmap_offset;
#if 0
	uint32_t io_bitmap[IO_BITMAP_SIZE/4+1];
	uint32_t pad[5];
#endif
} __packed;

/*
 * i386 flags register
 */
#define EFL_CF		0x00000001	/* Carry */
#define EFL_PF		0x00000004	/* Parity */
#define EFL_AF		0x00000010	/* Carry */
#define EFL_ZF		0x00000040	/* Zero */
#define EFL_SF		0x00000080	/* Sign */
#define EFL_TF		0x00000100	/* Trap */
#define EFL_IF		0x00000200	/* Interrupt enable */
#define EFL_DF		0x00000400	/* Direction */
#define EFL_OF		0x00000800	/* Overflow */
#define EFL_IOPL	0x00003000	/* IO privilege level: */
#define EFL_IOPL_KERN	0x00000000	/* Kernel */
#define EFL_IOPL_USER	0x00003000	/* User */
#define EFL_NT		0x00004000	/* Nested task */
#define EFL_RF		0x00010000	/* Resume without tracing */
#define EFL_VM		0x00020000	/* Virtual 8086 mode */
#define EFL_AC		0x00040000	/* Alignment Check */

/*
 * CR0 register
 */
#define CR0_PG		0x80000000	/* enable paging */
#define CR0_CD		0x40000000	/* cache disable */
#define CR0_NW		0x20000000	/* no write-through */
#define CR0_AM		0x00040000	/* alignment check mask */
#define CR0_WP		0x00010000	/* write-protect kernel access */
#define CR0_NE		0x00000020	/* handle numeric exceptions */
#define CR0_ET		0x00000010	/* extension type is 80387 coprocessor */
#define CR0_TS		0x00000008	/* task switch */
#define CR0_EM		0x00000004	/* emulate coprocessor */
#define CR0_MP		0x00000002	/* monitor coprocessor */
#define CR0_PE		0x00000001	/* enable protected mode */

/*
 * Page table (PTE)
 */
typedef uint32_t *pte_t;

/*
 * Page directory entry
 */
#define PDE_PRESENT	0x00000001
#define PDE_WRITE	0x00000002
#define PDE_USER	0x00000004
#define PDE_WTHRU	0x00000008
#define PDE_NCACHE	0x00000010
#define PDE_ACCESS	0x00000020
#define PDE_SIZE	0x00000080
#define PDE_AVAIL	0x00000e00
#define PDE_ADDRESS	0xfffff000

/*
 * Page table entry
 */
#define PTE_PRESENT	0x00000001
#define PTE_WRITE	0x00000002
#define PTE_USER	0x00000004
#define PTE_WTHRU	0x00000008
#define PTE_NCACHE	0x00000010
#define PTE_ACCESS	0x00000020
#define PTE_DIRTY	0x00000040
#define PTE_AVAIL	0x00000e00
#define PTE_ADDRESS	0xfffff000

/*
 *  Virtual and physical address translation
 */
#define PAGE_DIR(virt)      (int)((((vaddr_t)(virt)) >> 22) & 0x3ff)
#define PAGE_TABLE(virt)    (int)((((vaddr_t)(virt)) >> 12) & 0x3ff)

#define pte_present(pgd, virt)  (pgd[PAGE_DIR(virt)] & PDE_PRESENT)

#define page_present(pte, virt) (pte[PAGE_TABLE(virt)] & PTE_PRESENT)

#define pgd_to_pte(pgd, virt) \
            (pte_t)phys_to_virt(((uint32_t *)pgd)[PAGE_DIR(virt)] & PDE_ADDRESS)

#define pte_to_page(pte, virt) \
            ((pte)[PAGE_TABLE(virt)] & PTE_ADDRESS)

__BEGIN_DECLS
void	 tss_set(uint32_t kstack);
uint32_t tss_get(void);
void	 trap_handler(struct cpu_regs *);
void	 cpu_init(void);
__END_DECLS

#endif /* !__ASSEMBLY__ */
#endif /* !_I386_CPU_H */
