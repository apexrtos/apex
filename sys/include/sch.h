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

#ifndef sch_h
#define sch_h

#include <queue.h>
#include <stdbool.h>
#include <stdint.h>

struct event;
struct thread;

/*
 * DPC (Deferred Procedure Call) object
 */
struct dpc {
	struct queue	link;		/* Linkage on DPC queue */
	int		state;		/* DPC_* */
	void	      (*func)(void *);	/* Callback routine */
	void	       *arg;		/* Argument to pass */
};

#if defined(__cplusplus)
extern "C" {
#endif

/*
 * Scheduler interface
 */
void		sch_switch(void);
struct thread  *sch_active(void);
int		sch_sleep(struct event *);
int		sch_nanosleep(struct event *, uint64_t);
unsigned	sch_wakeup(struct event *, int);
struct thread  *sch_wakeone(struct event *);
struct thread  *sch_requeue(struct event *, struct event *);
int		sch_prepare_sleep(struct event *, uint64_t);
int		sch_continue_sleep();
void		sch_cancel_sleep(void);
void	        sch_unsleep(struct thread *, int);
void	        sch_interrupt(struct thread *);
void	        sch_yield(void);
void	        sch_suspend(struct thread *);
void	        sch_resume(struct thread *);
void	        sch_elapse(uint32_t);
void	        sch_start(struct thread *);
void	        sch_stop(struct thread *);
void		sch_testexit(void);
void	        sch_lock(void);
void	        sch_unlock(void);
int		sch_locks(void);
int		sch_getprio(struct thread *);
void		sch_setprio(struct thread *, int, int);
int		sch_getpolicy(struct thread *);
int		sch_setpolicy(struct thread *, int);
void	        sch_dpc(struct dpc *, void (*)(void *), void *);
void	        sch_dump(void);
void	        sch_init(void);

#if defined(__cplusplus)
} /* extern "C" */

namespace a {

class sch_lock {
public:
	void lock() const { ::sch_lock(); }
	void unlock() const { ::sch_unlock(); }
};

} /* namespace a */

inline constexpr a::sch_lock global_sch_lock{};

#endif /* __cplusplus */

#endif /* !sch_h */
