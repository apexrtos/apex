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

#ifndef _IRQ_H
#define _IRQ_H

#include <sys/cdefs.h>
#include <event.h>

struct irq {
	int		vector;		/* vector number */
	int		(*isr)(int);	/* pointer to isr */
	void		(*ist)(int);	/* pointer to ist */
	int		istreq;		/* number of ist request */
	thread_t	thread;		/* thread id of ist */
	struct event	istevt;		/* event for ist */
};

/*
 * Return values from ISR.
 */
#define INT_DONE	0	/* success */
#define INT_ERROR	1	/* interrupt not handled */
#define INT_CONTINUE	2	/* continue processing (Request IST) */

/*
 * Interrupt priority levels
 */
#define IPL_NONE	0	/* nothing */
#define IPL_COMM	1	/* serial, parallel */
#define IPL_BLOCK	2	/* FDD, IDE */
#define IPL_NET		3	/* network */
#define IPL_DISPLAY	4	/* screen */
#define IPL_INPUT	5	/* keyboard, mouse */
#define IPL_AUDIO	6	/* audio */
#define IPL_BUS		7	/* USB, PCCARD */
#define IPL_RTC		8	/* RTC alarm */
#define IPL_PROFILE	9	/* profiling timer */
#define IPL_CLOCK	10	/* system clock timer */
#define IPL_HIGH	11	/* everything */

#define NIPLS		12	/* number of IPLs */

/*
 * Macro to map an interrupt priority level to IST priority.
 */
#define ISTPRIO(prio)	(PRIO_IST + (IPL_HIGH - prio))

__BEGIN_DECLS
irq_t	 irq_attach(int, int, int, int (*)(int), void (*)(int));
void	 irq_detach(irq_t);
void	 irq_lock(void);
void	 irq_unlock(void);
void	 irq_handler(int);
void	 irq_init(void);
__BEGIN_DECLS

#endif /* !_IRQ_H */
