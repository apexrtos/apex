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
 * irq.c - interrupt request management routines.
 */

/**
 * We define the following two different types of interrupt
 * services in order to improve real-time performance.
 *
 * - Interrupt Service Routine (ISR)
 *
 *  ISR is started by a hardware interrupt. If ISR determines that the
 *  corresponding device generated the interrupt, ISR must program the device
 *  to stop that interrupt.  Then, ISR should do minimum I/O operation and
 *  return control as quickly as possible. Only limited kernel services are
 *  available within ISR.
 *
 * - Interrupt Service Thread (IST)
 *
 *  IST is automatically activated if ISR returns INT_CONTINUE. It will be
 *  called when the system enters safer condition than ISR. A device driver
 *  should use IST or DPC to do heavy I/O operation as much as possible. IST
 *  does not have to be reentrant because it is not interrupted by same IST
 *  itself. Note that DPC must not sleep as all other DPC will be blocked.
 *
 * TODO: interrupt sharing
 *	 use memory barrier to avoid spinlock in irq_handler?
 */

#include <irq.h>

#include <arch/interrupt.h>
#include <cassert>
#include <compiler.h>
#include <cstring>
#include <debug.h>
#include <event.h>
#include <kmem.h>
#include <sch.h>
#include <sections.h>
#include <sync.h>
#include <thread.h>

struct irq {
	int		vector;		    /* vector number */
	int	      (*isr)(int, void *);  /* pointer to isr */
	void	      (*ist)(int, void *);  /* pointer to ist */
	int		isrreq;		    /* number of isr request */
	int		istreq;		    /* number of ist request */
	void	       *data;		    /* handler data */
	struct thread  *thread;		    /* thread id of ist */
	struct event	istevt;		    /* event for ist */
};

static void irq_thread(void *);

/* IRQ descriptor table */
static struct irq *irq_table[CONFIG_IRQS] __fast_bss;
static struct spinlock lock __fast_bss;

/*
 * irq_attach - attach ISR and IST to the specified interrupt.
 *
 * Returns irq handle, or NULL on failure.  The interrupt of
 * attached irq will be unmasked (enabled) in this routine.
 */
struct irq *
irq_attach(int vector, int prio, int mode, int (*isr)(int, void *),
    void (*ist)(int, void *), void *data)
{
	struct irq *irq;

	assert(isr != NULL);
	assert(vector < CONFIG_IRQS);
	assert(!interrupt_running());

	if ((irq = kmem_alloc(sizeof(*irq), MA_FAST)) == NULL)
		return NULL;

	memset(irq, 0, sizeof(*irq));
	irq->vector = vector;
	irq->isr = isr;
	irq->ist = ist;
	irq->data = data;

	if (ist != NULL) {
		/*
		 * Create a new thread for IST.
		 */
		irq->thread = kthread_create(&irq_thread, irq,
		    interrupt_to_ist_priority(prio), "ist", MA_FAST);
		if (irq->thread == NULL) {
			kmem_free(irq);
			return NULL;
		}
		event_init(&irq->istevt, "interrupt", ev_SLEEP);
	}

	const int s = spinlock_lock_irq_disable(&lock);
	if (irq_table[vector] != NULL) {
		spinlock_unlock_irq_restore(&lock, s);
		thread_terminate(irq->thread);
		kmem_free(irq);
		dbg("IRQ%d BUSY\n", vector);
		return NULL;
	}
	irq_table[vector] = irq;
	interrupt_setup(vector, mode);
	interrupt_unmask(vector, prio);
	spinlock_unlock_irq_restore(&lock, s);

	dbg("IRQ%d attached priority=%d\n", vector, prio);
	return irq;
}

/*
 * irq_detach - detach ISR and IST from interrupt.
 */
void
irq_detach(struct irq *irq)
{
	assert(irq);
	assert(irq->vector < CONFIG_IRQS);
	assert(!interrupt_running());

	const int s = spinlock_lock_irq_disable(&lock);
	interrupt_mask(irq->vector);
	irq_table[irq->vector] = NULL;
	spinlock_unlock_irq_restore(&lock, s);

	if (irq->thread != NULL)
		thread_terminate(irq->thread);
	kmem_free(irq);
}

/*
 * irq_disable - disable all hardware interrupts.
 *
 * All H/W interrupts are masked off.
 * Current interrupt state is returned and must be passed to irq_restore.
 */
int
irq_disable()
{
	int s;
	interrupt_save(&s);
	interrupt_disable();
	return s;
}

/*
 * irq_restore - restore hardware interrupt state.
 *
 * Resture interrupt state to original state before irq_disable call.
 */
void
irq_restore(int s)
{
	interrupt_restore(s);
}

/*
 * irq_thread - interrupt service thread.
 */
static void
irq_thread(void *arg)
{
	int vec;
	void (*func)(int, void *);
	void *data;
	struct irq *irq;

	irq = (struct irq *)arg;
	func = irq->ist;
	vec = irq->vector;
	data = irq->data;

	while (true) {
		sch_testexit();

		interrupt_disable();
		if (irq->istreq <= 0) {
			sch_prepare_sleep(&irq->istevt, 0);
			interrupt_enable();
			sch_continue_sleep();
			sch_testexit();
			interrupt_disable();
		}
		irq->istreq--;
		assert(irq->istreq >= 0);
		interrupt_enable();

		/*
		 * Call IST
		 */
		(*func)(vec, data);
	}
	/* NOTREACHED */
}

void
irq_dump()
{
	info("irq dump\n");
	info("========\n");
	info(" irq count\n");
	info(" --- ----------\n");
	const int s = spinlock_lock_irq_disable(&lock);
	for (size_t vector = 0; vector < ARRAY_SIZE(irq_table); vector++) {
		const struct irq *irq = irq_table[vector];
		if (!irq || irq->isrreq == 0)
			continue;
		info(" %3d %10d\n", vector, irq->isrreq);
	}
	spinlock_unlock_irq_restore(&lock, s);
}

/*
 * Interrupt handler.
 *
 * This routine will call the corresponding ISR for the
 * requested interrupt vector. This routine is called from
 * the code in the architecture dependent layer. We
 * assumes the scheduler is already locked by caller.
 */
__fast_text void
irq_handler(int vector)
{
	struct irq *irq;
	int rc;

	const int s = spinlock_lock_irq_disable(&lock);
	irq = irq_table[vector];
	spinlock_unlock_irq_restore(&lock, s);

	if (irq == NULL)
		return;		/* Ignore stray interrupt */
	assert(irq->isr);

	/*
	 * Call ISR
	 */
	irq->isrreq++;
	rc = (*irq->isr)(vector, irq->data);

	if (rc == INT_CONTINUE) {
		/*
		 * Kick IST
		 */
		assert(irq->ist);
		irq->istreq++;
		sch_wakeup(&irq->istevt, 0);
		assert(irq->istreq != 0);
	}
}

/*
 * irq_init - initialise interrupt processing
 */
void
irq_init()
{
	spinlock_init(&lock);
	interrupt_init();
	interrupt_enable();
}
