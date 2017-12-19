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
 * machdep.c - machine-dependent routines for ARM Integrator-CP
 */

#include <kernel.h>
#include <cpu.h>
#include <page.h>
#include <syspage.h>
#include <locore.h>
#include <cpufunc.h>

/* System control reg */
#define SC_CTRL		(*(volatile uint32_t *)(FPGA_BASE + 0x0c))

#define SCCTRL_SOFTRESET	0x08

#ifdef CONFIG_MMU
/*
 * Virtual and physical address mapping
 *
 *      { virtual, physical, size, type }
 */
struct mmumap mmumap_table[] =
{
	/*
	 * Internal SRAM (4M)
	 */
	{ 0x80000000, 0x00000000, 0x400000, VMT_RAM },

	/*
	 * FPGA core control (4K)
	 */
	{ 0xD0000000, 0x10000000, 0x1000, VMT_IO },

	/*
	 * Counter/Timers (1M)
	 */
	{ 0xD3000000, 0x13000000, 0x100000, VMT_IO },

	/*
	 * Interrupt controller (1M)
	 */
	{ 0xD4000000, 0x14000000, 0x100000, VMT_IO },

	/*
	 * Real-time clock (1M)
	 */
	{ 0xD5000000, 0x15000000, 0x100000, VMT_IO },

	/*
	 * UART 0 (1M)
	 */
	{ 0xD6000000, 0x16000000, 0x100000, VMT_IO },

	{ 0,0,0,0 }
};
#endif

/*
 * Reset system.
 */
void
machine_reset(void)
{

	SC_CTRL = SCCTRL_SOFTRESET;

	for (;;) ;
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

	/*
	 * Setup vector page.
	 */
	vector_copy((vaddr_t)phys_to_virt(ARM_VECTORS_LOW));

#ifdef CONFIG_MMU
	/*
	 * Initialize MMU
	 */
	mmu_init(mmumap_table);
#endif
}
