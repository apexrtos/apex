/*-
 * Copyright (c) 2005-2007, Kohsuke Ohtani
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
 * cpu.c - cpu dependent routines for Intel x86
 */

#include <kernel.h>
#include <page.h>
#include <syspage.h>
#include <cpu.h>
#include <locore.h>
#include <cpufunc.h>

typedef void (*trapfn_t)(void);

/*
 * Descriptors
 */
static struct seg_desc gdt[NGDTS];
static struct gate_desc idt[NIDTS];
static struct tss tss;

/*
 * Interrupt table
 */
static const trapfn_t intr_table[] = {
	intr_0, intr_1, intr_2, intr_3,	intr_4, intr_5, intr_6,
	intr_7, intr_8, intr_9, intr_10, intr_11, intr_12, intr_13,
	intr_14, intr_15
};

/*
 * Trap table
 */
static const trapfn_t trap_table[] = {
	trap_0, trap_1, trap_2, trap_3,	trap_4, trap_5, trap_6,
	trap_7,	trap_8, trap_9, trap_10, trap_11, trap_12, trap_13,
	trap_14, trap_15, trap_16, trap_17, trap_18
};
#define NTRAPS	(int)(sizeof(trap_table) / sizeof(void *))

/*
 * Set kernel stack pointer in TSS (task state segment).
 * An actual value of the register is automatically set when
 * CPU enters kernel mode next time.
 */
void
tss_set(uint32_t kstack)
{

	tss.esp0 = kstack;
}

/*
 * tss_get() returns current esp0 value for trap handler.
 */
uint32_t
tss_get(void)
{

	return tss.esp0;
}

/*
 * Set GDT (global descriptor table) members into specified vector
 */
static void
gdt_set(int vec, void *base, size_t limit, int type, u_int size)
{
	struct seg_desc *seg = &gdt[vec];

	if (limit > 0xfffff) {
		limit >>= 12;
		size |= SIZE_4K;
	}
	seg->limit_lo = limit;
	seg->base_lo = (u_int)base & 0xffff;
	seg->base_mid = ((u_int)base >> 16) & 0xff;
	seg->limit_hi = limit >> 16;
	seg->base_hi = (u_int)base >> 24;
	seg->type = (u_int)type | ST_PRESENT;
	seg->size = size;
}

/*
 * Set IDT (interrupt descriptor table) members into specified vector
 */
static void
idt_set(int vec, trapfn_t off, u_int sel, int type)
{
	struct gate_desc *gate = &idt[vec];

	gate->offset_lo = (u_int)off & 0xffff;
	gate->selector = sel;
	gate->nr_copy = 0;
	gate->type = (u_int)type | ST_PRESENT;
	gate->offset_hi = (u_int)off >> 16;
}

/*
 * Setup the GDT and load it.
 */
static void
gdt_init(void)
{
	struct desc_p gdt_p;

	/* Set system vectors */
	gdt_set(KERNEL_CS / 8, 0, 0xffffffff, ST_KERN | ST_CODE_R, SIZE_32);
	gdt_set(KERNEL_DS / 8, 0, 0xffffffff, ST_KERN | ST_DATA_W, SIZE_32);
	gdt_set(USER_CS / 8, 0, 0xffffffff, ST_USER | ST_CODE_R, SIZE_32);
	gdt_set(USER_DS / 8, 0, 0xffffffff, ST_USER | ST_DATA_W, SIZE_32);

	/* Clear TSS Busy */
	gdt[KERNEL_TSS / 8].type &= ~ST_TSS_BUSY;

	/* Load GDT */
	gdt_p.limit = (uint16_t)(sizeof(gdt) - 1);
	gdt_p.base = (uint32_t)&gdt;
	lgdt(&gdt_p);
}

/*
 * Setup the interrupt descriptor table and load it.
 *
 * IDT layout:
 *  0x00 - 0x12 ... S/W trap
 *  0x13 - 0x1f ... Intel reserved
 *  0x20 - 0x3f ... H/W interrupt
 *  0x40        ... System call trap
 */
static void
idt_init(void)
{
	struct desc_p idt_p;
	int i;

	/* Fill all vectors with default handler */
	for (i = 0; i < NIDTS; i++)
		idt_set(i, trap_default, KERNEL_CS, ST_KERN | ST_TRAP_GATE);

	/* Setup trap handlers */
	for (i = 0; i < NTRAPS; i++)
		idt_set(i, trap_table[i], KERNEL_CS, ST_KERN | ST_TRAP_GATE);

	/* Setup interrupt handlers */
	for (i = 0; i < 16; i++)
		idt_set(0x20 + i, intr_table[i], KERNEL_CS,
			ST_KERN | ST_INTR_GATE);

	/* Setup debug trap */
	idt_set(3, trap_3, KERNEL_CS, ST_USER | ST_TRAP_GATE);

	/* Setup system call handler */
	idt_set(SYSCALL_INT, syscall_entry, KERNEL_CS,
		ST_USER | ST_TRAP_GATE);

	/* Load IDT */
	idt_p.limit = (uint16_t)(sizeof(idt) - 1);
	idt_p.base = (uint32_t)&idt;
	lidt(&idt_p);
}

/*
 * Initialize the task state segment.
 * Only one static TSS is used for all contexts.
 */
static void
tss_init(void)
{

	gdt_set(KERNEL_TSS / 8, &tss, sizeof(struct tss) - 1,
		ST_KERN | ST_TSS, 0);
	/* Setup TSS */
	memset(&tss, 0, sizeof(struct tss));
	tss.ss0 = KERNEL_DS;
	tss.esp0 = (uint32_t)BOOTSTACK_TOP;
	tss.cs = (uint32_t)USER_CS | 3;
	tss.ds = tss.es = tss.ss = tss.fs = tss.gs = (uint32_t)USER_CS | 3;
	tss.io_bitmap_offset = INVALID_IO_BITMAP;
	ltr(KERNEL_TSS);
}

/*
 * Initialize CPU state.
 * Setup segment and interrupt descriptor.
 */
void
cpu_init(void)
{

	/* Enable write protection from kernel code */
	set_cr0(get_cr0() | CR0_WP);

	/*
	 * Setup flag register.
	 * Interrupt disable, clear direction, clear nested
	 * task, i/o privilege 0
	 */
	set_eflags(get_eflags() & ~(EFL_IF | EFL_DF | EFL_NT | EFL_IOPL));

	/*
	 * Initialize descriptors.
	 * Setup segment and interrupt descriptor.
	 */
	gdt_init();
	idt_init();
	tss_init();
}
