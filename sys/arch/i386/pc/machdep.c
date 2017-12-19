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
 * machdep.c - machine-dependent routines
 */

#include <kernel.h>
#include <page.h>
#include <syspage.h>
#include <cpu.h>
#include <locore.h>
#include <cpufunc.h>
#include <irq.h>

#ifdef CONFIG_MMU
/*
 * Virtual and physical address mapping
 *
 *      { virtual, physical, size, type }
 */
static struct mmumap mmumap_table[] =
{
	/*
	 * RAM
	 */
	{ 0x80000000, 0x00000000, AUTOSIZE, VMT_RAM },

	{ 0,0,0,0 }
};
#endif

/*
 * Cause an i386 machine reset.
 */
void
machine_reset(void)
{
	int i;

	/*
	 * Try to do keyboard reset.
	 */
	interrupt_disable();
	outb(0xfe, 0x64);
	for (i = 0; i < 10000; i++)
		outb(0, 0x80);

	/*
	 * Do cpu reset.
	 */
	cpu_reset();
	/* NOTREACHED */
}

/*
 * Idle
 */
void
machine_idle(void)
{

	cpu_idle();
}

/*
 * Set system power
 */
void
machine_setpower(int state)
{

	irq_lock();
#ifdef DEBUG
	printf("The system is halted. You can turn off power.");
#endif
	for (;;)
		machine_idle();
}

/*
 * Machine-dependent startup code
 */
void
machine_init(void)
{

	/*
	 * Initialize CPU and basic hardware.
	 */
	cpu_init();
	cache_init();

	/*
	 * Reserve system pages.
	 */
	page_reserve(virt_to_phys(SYSPAGE_BASE), SYSPAGE_SIZE);

#ifdef CONFIG_MMU
	/*
	 * Modify page mapping
	 * We assume the first block in ram[] for i386 is main memory.
	 */
	mmumap_table[0].size = bootinfo->ram[0].size;

	/*
	 * Initialize MMU
	 */
	mmu_init(mmumap_table);
#endif
}
