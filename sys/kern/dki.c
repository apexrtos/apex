/*-
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

/*
 * dki.c - Prex Driver Kernel Interface functions
 */

#include <kernel.h>
#include <irq.h>
#include <page.h>
#include <kmem.h>
#include <task.h>
#include <timer.h>
#include <sched.h>
#include <exception.h>
#include <vm.h>
#include <device.h>
#include <system.h>

/* forward declarations */
#ifndef DEBUG
static void	 nosys(void);
#endif
static void	 machine_bootinfo(struct bootinfo **);
static void	*_phys_to_virt(void *);
static void	*_virt_to_phys(void *);

#define DKIENT(func)	(dkifn_t)(func)

/*
 * Driver-Kernel Interface (DKI)
 */
const dkifn_t driver_service[] = {
	/*  0 */ DKIENT(device_create),
	/*  1 */ DKIENT(device_destroy),
	/*  2 */ DKIENT(device_broadcast),
	/*  3 */ DKIENT(umem_copyin),
	/*  4 */ DKIENT(umem_copyout),
	/*  5 */ DKIENT(umem_strnlen),
	/*  6 */ DKIENT(kmem_alloc),
	/*  7 */ DKIENT(kmem_free),
	/*  8 */ DKIENT(kmem_map),
	/*  9 */ DKIENT(page_alloc),
	/* 10 */ DKIENT(page_free),
	/* 11 */ DKIENT(page_reserve),
	/* 12 */ DKIENT(irq_attach),
	/* 13 */ DKIENT(irq_detach),
	/* 14 */ DKIENT(irq_lock),
	/* 15 */ DKIENT(irq_unlock),
	/* 16 */ DKIENT(timer_callout),
	/* 17 */ DKIENT(timer_stop),
	/* 18 */ DKIENT(timer_delay),
	/* 19 */ DKIENT(timer_count),
	/* 20 */ DKIENT(timer_hook),
	/* 21 */ DKIENT(sched_lock),
	/* 22 */ DKIENT(sched_unlock),
	/* 23 */ DKIENT(sched_tsleep),
	/* 24 */ DKIENT(sched_wakeup),
	/* 25 */ DKIENT(sched_dpc),
	/* 26 */ DKIENT(task_capable),
	/* 27 */ DKIENT(exception_post),
	/* 28 */ DKIENT(machine_bootinfo),
	/* 29 */ DKIENT(machine_reset),
	/* 30 */ DKIENT(machine_idle),
	/* 31 */ DKIENT(machine_setpower),
	/* 32 */ DKIENT(_phys_to_virt),
	/* 33 */ DKIENT(_virt_to_phys),
#ifdef DEBUG
	/* 34 */ DKIENT(debug_attach),
	/* 35 */ DKIENT(debug_dump),
	/* 36 */ DKIENT(printf),
	/* 37 */ DKIENT(panic),
#else
	/* 34 */ DKIENT(nosys),
	/* 35 */ DKIENT(nosys),
	/* 36 */ DKIENT(nosys),
	/* 37 */ DKIENT(machine_reset),
#endif
};

#ifndef DEBUG
/*
 * nonexistent driver service.
 */
static void
nosys(void)
{
}
#endif

/*
 * Return boot information
 */
static void
machine_bootinfo(struct bootinfo **info)
{
	ASSERT(info != NULL);

	*info = bootinfo;
}

/*
 *  Address translation (physical -> virtual)
 */
static void *
_phys_to_virt(void *phys)
{

	return phys_to_virt(phys);
}

/*
 *  Address translation (virtual -> physical)
 */
static void *
_virt_to_phys(void *virt)
{

	return virt_to_phys(virt);
}

