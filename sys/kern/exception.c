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
 * exception.c - exception handling routines
 */

/**
 * An user mode task can specify its own exception handler with
 * exception_setup() system call.
 *
 * There are two different types of exceptions in a system - H/W and
 * S/W exception. The kernel determines to which thread it delivers
 * depending on the exception type.
 *
 *  - H/W exception
 *
 *   This type of exception is caused by H/W trap & fault. The
 *   exception will be sent to the thread which caused the trap.
 *   If no handler is specified by the task, it will be terminated
 *   by the kernel immediately.
 *
 *  - S/W exception
 *
 *   The user mode task can send S/W exception to another task by
 *   exception_raise() system call.
 *   The exception  will be sent to the thread that is sleeping with
 *   exception_wait() call. If no thread is waiting for the exception,
 *   the exception is sent to the first thread in the target task.
 *
 * Kernel supports 32 types of exceptions. The following pre-defined
 * exceptions are raised by kernel itself.
 *
 *   Exception Type Reason
 *   --------- ---- -----------------------
 *   SIGILL    h/w  illegal instruction
 *   SIGTRAP   h/w  break point
 *   SIGFPE    h/w  math error
 *   SIGSEGV   h/w  invalid memory access
 *   SIGALRM   s/w  alarm event
 *
 * The POSIX emulation library will setup own exception handler to
 * convert the Prex exceptions into UNIX signals. It will maintain its
 * own signal mask, and transfer control to the POSIX signal handler.
 */

#include <kernel.h>
#include <event.h>
#include <task.h>
#include <thread.h>
#include <sched.h>
#include <task.h>
#include <irq.h>
#include <exception.h>

static struct event exception_event;

/*
 * Install an exception handler for the current task.
 *
 * NULL can be specified as handler to remove current handler.
 * If handler is removed, all pending exceptions are discarded
 * immediately. In this case, all threads blocked in
 * exception_wait() are unblocked.
 *
 * Only one exception handler can be set per task. If the
 * previous handler exists in task, exception_setup() just
 * override that handler.
 */
int
exception_setup(void (*handler)(int))
{
	task_t self = cur_task();
	list_t head, n;
	thread_t th;

	if (handler != NULL && !user_area(handler))
		return EFAULT;

	sched_lock();
	if (self->handler && handler == NULL) {
		/*
		 * Remove existing exception handler. Do clean up
		 * job for all threads in the target task.
		 */
		head = &self->threads;
		for (n = list_first(head); n != head; n = list_next(n)) {
			/*
			 * Clear pending exceptions.
			 */
			th = list_entry(n, struct thread, task_link);
			irq_lock();
			th->excbits = 0;
			irq_unlock();

			/*
			 * If the thread is waiting for an exception,
			 * cancel it.
			 */
			if (th->slpevt == &exception_event)
				sched_unsleep(th, SLP_BREAK);
		}
	}
	self->handler = handler;
	sched_unlock();
	return 0;
}

/*
 * exception_raise - system call to raise an exception.
 *
 * The exception pending flag is marked here, and it is
 * processed by exception_deliver() later. If the task
 * want to raise an exception to another task, the caller
 * task must have CAP_KILL capability. If the exception
 * is sent to the kernel task, this routine just returns
 * error.
 */
int
exception_raise(task_t task, int exc)
{
	int err;

	sched_lock();

	if (!task_valid(task)) {
		err = ESRCH;
		goto out;
	}
	if (task != cur_task() && !task_capable(CAP_KILL)) {
		err = EPERM;
		goto out;
	}
	if (task == &kern_task || task->handler == NULL ||
	    list_empty(&task->threads)) {
		err = EPERM;
		goto out;
	}
	err = exception_post(task, exc);
 out:
	sched_unlock();
	return err;
}

/*
 * exception_post-- the internal version of exception_raise().
 */
int
exception_post(task_t task, int exc)
{
	list_t head, n;
	thread_t th;

	if (exc < 0 || exc >= NEXC)
		return EINVAL;

	/*
	 * Determine which thread should we send an exception.
	 * First, search the thread that is waiting an exception
	 * by calling exception_wait(). Then, if no thread is
	 * waiting exceptions, it is sent to the master thread in
	 * task.
	 */
	head = &task->threads;
	for (n = list_first(head); n != head; n = list_next(n)) {
		th = list_entry(n, struct thread, task_link);
		if (th->slpevt == &exception_event)
			break;
	}
	if (n == head) {
		n = list_first(head);
		th = list_entry(n, struct thread, task_link);
	}
	/*
	 * Mark pending bit for this exception.
	 */
	irq_lock();
	th->excbits |= (1 << exc);
	irq_unlock();

	/*
	 * Wakeup the target thread regardless of its
	 * waiting event.
	 */
	sched_unsleep(th, SLP_INTR);

	return 0;
}

/*
 * exception_wait - block a current thread until some exceptions
 * are raised to the current thread.
 *
 * The routine returns EINTR on success.
 */
int
exception_wait(int *exc)
{
	task_t self = cur_task();
	int i, rc;

	self = cur_task();
	if (self->handler == NULL)
		return EINVAL;
	if (!user_area(exc))
		return EFAULT;

	sched_lock();

	/*
	 * Sleep until some exceptions occur.
	 */
	rc = sched_sleep(&exception_event);
	if (rc == SLP_BREAK) {
		sched_unlock();
		return EINVAL;
	}
	irq_lock();
	for (i = 0; i < NEXC; i++) {
		if (cur_thread->excbits & (1 << i))
			break;
	}
	irq_unlock();
	ASSERT(i != NEXC);
	sched_unlock();

	if (umem_copyout(&i, exc, sizeof(i)))
		return EFAULT;
	return EINTR;
}

/*
 * Mark an exception flag for the current thread.
 *
 * This is called from architecture dependent code when H/W
 * trap is occurred. If current task does not have exception
 * handler, then current task will be terminated. This routine
 * may be called at interrupt level.
 */
void
exception_mark(int exc)
{
	ASSERT(exc > 0 && exc < NEXC);

	/* Mark pending bit */
	irq_lock();
	cur_thread->excbits |= (1 << exc);
	irq_unlock();
}

/*
 * exception_deliver - deliver pending exception to the task.
 *
 * Check if pending exception exists for current task, and
 * deliver it to the exception handler if needed. All
 * exception is delivered at the time when the control goes
 * back to the user mode. This routine is called from
 * architecture dependent code. Some application may use
 * longjmp() during its signal handler. So, current context
 * must be saved to user mode stack.
 */
void
exception_deliver(void)
{
	thread_t th = cur_thread;
	task_t self = cur_task();
	void (*handler)(int);
	uint32_t bitmap;
	int exc;

	sched_lock();
	irq_lock();
	bitmap = th->excbits;
	irq_unlock();

	if (bitmap != 0) {
		/*
		 * Find a pending exception.
		 */
		for (exc = 0; exc < NEXC; exc++) {
			if (bitmap & (1 << exc))
				break;
		}
		handler = self->handler;
		if (handler == NULL) {
			DPRINTF(("Exception #%d is not handled by task.\n",
				exc));
			DPRINTF(("Terminate task:%s (id:%x)\n",
				 self->name != NULL ? self->name : "no name",
				 self));

			task_terminate(self);
			goto out;
		}
		/*
		 * Transfer control to an exception handler.
		 */
		context_save(&th->ctx);
		context_set(&th->ctx, CTX_UENTRY, (vaddr_t)handler);
		context_set(&th->ctx, CTX_UARG, (vaddr_t)exc);

		irq_lock();
		th->excbits &= ~(1 << exc);
		irq_unlock();
	}
 out:
	sched_unlock();
}

/*
 * exception_return() is called from exception handler to
 * restore the original context.
 */
int
exception_return(void)
{

	context_restore(&cur_thread->ctx);
	return 0;
}

void
exception_init(void)
{

	event_init(&exception_event, "exception");
}
