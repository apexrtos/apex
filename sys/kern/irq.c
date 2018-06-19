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
 *  ISR is started by an actual hardware interrupt. The associated
 *  interrupt is disabled in Interrupt Control Unit (ICU), and CPU
 *  interrupt is enabled while ISR runs.
 *  If ISR determines that the corresponding device generated the
 *  interrupt, ISR must program the device to stop that interrupt.
 *  Then, ISR should do minimum I/O operation and return control
 *  as quickly as possible. ISR will run within a context of the
 *  thread running when interrupt occurs. So, only few kernel
 *  services are available within ISR.
 *
 * - Interrupt Service Thread (IST)
 *
 *  IST is automatically activated if ISR returns INT_CONTINUE. It
 *  will be called when the system enters safer condition than ISR.
 *  A device driver should use IST to do heavy I/O operation as much
 *  as possible. Since ISR for the same IRQ line may be invoked
 *  during IST, the shared data, resources, and device registers
 *  must be synchronized by using irq_lock(). IST does not have to
 *  be reentrant because it is not interrupted by same IST itself.
 */

#include <irq.h>

#include <arch.h>
#include <assert.h>
#include <debug.h>
#include <kmem.h>
#include <sch.h>
#include <sections.h>
#include <stddef.h>
#include <string.h>
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

/* forward declarations */
static void
irq_thread(void *);

/* IRQ descriptor table */
static struct irq *irq_table[CONFIG_IRQS] __mem_fast_bss;

/*
 * irq_attach - attach ISR and IST to the specified interrupt.
 *
 * Returns irq handle, or NULL on failure.  The interrupt of
 * attached irq will be unmasked (enabled) in this routine.
 * TODO: Interrupt sharing is not supported, for now.
 */
struct irq *
irq_attach(int vector, int prio, int shared, int (*isr)(int, void *),
    void (*ist)(int, void *), void *data)
{
	struct irq *irq;
	int mode;

	assert(isr != NULL);
	assert(vector < CONFIG_IRQS);

	sch_lock();
	if ((irq = kmem_alloc(sizeof(*irq), MEM_FAST)) == NULL) {
		sch_unlock();
		return NULL;
	}
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
		    interrupt_to_ist_priority(prio), "ist", MEM_FAST);
		if (irq->thread == NULL) {
			kmem_free(irq);
			sch_unlock();
			return NULL;
		}
		event_init(&irq->istevt, "interrupt", ev_SLEEP);
	}
	irq_table[vector] = irq;
	const int s = irq_disable();
	mode = shared ? IMODE_LEVEL : IMODE_EDGE;
	interrupt_setup(vector, mode);
	interrupt_unmask(vector, prio);
	interrupt_restore(s);

	sch_unlock();
	dbg("IRQ%d attached priority=%d\n", vector, prio);
	return irq;
}

/*
 * Detach an interrupt handler from the interrupt chain.
 * The detached interrupt will be masked off if nobody
 * attaches to it, anymore.
 */
void
irq_detach(struct irq *irq)
{
	assert(irq);
	assert(irq->vector < CONFIG_IRQS);

	const int s = irq_disable();
	interrupt_mask(irq->vector);
	irq_restore(s);

	irq_table[irq->vector] = NULL;
	if (irq->thread != NULL)
		kthread_terminate(irq->thread);

	kmem_free(irq);
}

/*
 * Disable IRQ.
 *
 * All H/W interrupts are masked off.
 * Current interrupt state is returned and must be passed to irq_restore.
 */
int
irq_disable(void)
{
	int s;
	interrupt_save(&s);
	interrupt_disable();
	return s;
}

/*
 * Restore IRQ.
 *
 * Resture interrupt state to original state before irq_disable call.
 */
void
irq_restore(int s)
{
	interrupt_restore(s);
}

/*
 * Interrupt service thread.
 * This is a common dispatcher to all interrupt threads.
 */
static void
irq_thread(void *arg)
{
	int vec;
	void (*func)(int, void *);
	void *data;
	struct irq *irq;

	interrupt_enable();

	irq = (struct irq *)arg;
	func = irq->ist;
	vec = irq->vector;
	data = irq->data;

	for (;;) {
		interrupt_disable();
		if (irq->istreq <= 0) {
			/*
			 * Since the interrupt is disabled above,
			 * an interrupt for this vector keeps
			 * pending until this thread enters sleep
			 * state. Thus, we don't lose any IST
			 * requests even if the interrupt is fired
			 * here.
			 */
			sch_sleep(&irq->istevt);
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
irq_dump(void)
{
	info("irq count\n");
	for (size_t vector = 0; vector < ARRAY_SIZE(irq_table); vector++) {
		const struct irq *irq = irq_table[vector];
		if (!irq || irq->isrreq == 0)
			continue;
		info("%3d %d\n", vector, irq->isrreq);
	}
}

/*
 * Interrupt handler.
 *
 * This routine will call the corresponding ISR for the
 * requested interrupt vector. This routine is called from
 * the code in the architecture dependent layer. We
 * assumes the scheduler is already locked by caller.
 */
void
irq_handler(int vector)
{
	struct irq *irq;
	int rc;

	irq = irq_table[vector];
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
		sch_wakeup(&irq->istevt, SLP_SUCCESS);
		assert(irq->istreq != 0);
	}
}

void
irq_init(void)
{
	/* Start interrupt processing. */
	interrupt_init();
	interrupt_enable();
}
