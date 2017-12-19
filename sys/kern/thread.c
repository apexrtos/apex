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
 * thread.c - thread management routines.
 */

#include <kernel.h>
#include <kmem.h>
#include <task.h>
#include <thread.h>
#include <ipc.h>
#include <sched.h>
#include <sync.h>
#include <system.h>

/* forward declarations */
static void do_terminate(thread_t);

static struct thread	idle_thread;
static thread_t		zombie;

/* global variable */
thread_t cur_thread = &idle_thread;

/*
 * Allocate a new thread and attach a kernel stack to it.
 * Returns thread pointer on success, or NULL on failure.
 */
static thread_t
thread_alloc(void)
{
	struct thread *th;
	void *stack;

	if ((th = kmem_alloc(sizeof(*th))) == NULL)
		return NULL;

	if ((stack = kmem_alloc(KSTACK_SIZE)) == NULL) {
		kmem_free(th);
		return NULL;
	}
	memset(th, 0, sizeof(*th));
	th->kstack = stack;
	th->magic = THREAD_MAGIC;
	list_init(&th->mutexes);
	return th;
}

static void
thread_free(thread_t th)
{

	kmem_free(th->kstack);
	kmem_free(th);
}

/*
 * Create a new thread.
 *
 * The context of a current thread will be copied to the
 * new thread. The new thread will start from the return
 * address of thread_create() call in user mode code.
 * Since a new thread will share the user mode stack with
 * a current thread, user mode applications are
 * responsible to allocate stack for it. The new thread is
 * initially set to suspend state, and so, thread_resume()
 * must be called to start it.
 */
int
thread_create(task_t task, thread_t *thp)
{
	thread_t th;
	int err = 0;
	void *sp;

	sched_lock();
	if (!task_valid(task)) {
		err = ESRCH;
		goto out;
	}
	if (!task_access(task)) {
		err = EPERM;
		goto out;
	}
	if ((th = thread_alloc()) == NULL) {
		err = ENOMEM;
		goto out;
	}
	/*
	 * First, we copy a new thread id as return value.
	 * This is done here to simplify all error recoveries
	 * of the subsequent code.
	 */
	if (cur_task() == &kern_task)
		*thp = th;
	else {
		if (umem_copyout(&th, thp, sizeof(th))) {
			thread_free(th);
			err = EFAULT;
			goto out;
		}
	}
	/*
	 * Initialize thread state.
	 */
	th->task = task;
	th->suscnt = task->suscnt + 1;
	memcpy(th->kstack, cur_thread->kstack, KSTACK_SIZE);
	sp = (char *)th->kstack + KSTACK_SIZE;
	context_set(&th->ctx, CTX_KSTACK, (vaddr_t)sp);
	context_set(&th->ctx, CTX_KENTRY, (vaddr_t)&syscall_ret);
	list_insert(&task->threads, &th->task_link);
	sched_start(th);
 out:
	sched_unlock();
	return err;
}

/*
 * Permanently stop execution of the specified thread.
 * If given thread is a current thread, this routine
 * never returns.
 */
int
thread_terminate(thread_t th)
{

	sched_lock();
	if (!thread_valid(th)) {
		sched_unlock();
		return ESRCH;
	}
	if (!task_access(th->task)) {
		sched_unlock();
		return EPERM;
	}
	do_terminate(th);
	sched_unlock();
	return 0;
}

/*
 * Terminate thread-- the internal version of thread_terminate.
 */
static void
do_terminate(thread_t th)
{
	/*
	 * Clean up thread state.
	 */
	msg_cleanup(th);
	timer_cleanup(th);
	mutex_cleanup(th);
	list_remove(&th->task_link);
	sched_stop(th);
	th->excbits = 0;
	th->magic = 0;

	/*
	 * We can not release the context of the "current"
	 * thread because our thread switching always
	 * requires the current context. So, the resource
	 * deallocation is deferred until another thread
	 * calls thread_terminate().
	 */
	if (zombie != NULL) {
		/*
		 * Deallocate a zombie thread which was killed
		 * in previous request.
		 */
		ASSERT(zombie != cur_thread);
		thread_free(zombie);
		zombie = NULL;
	}
	if (th == cur_thread) {
		/*
		 * If the current thread is being terminated,
		 * enter zombie state and wait for somebody
		 * to be killed us.
		 */
		zombie = th;
	} else {
		thread_free(th);
	}
}

/*
 * Load entry/stack address of the user mode context.
 *
 * The entry and stack address can be set to NULL.
 * If it is NULL, old state is just kept.
 */
int
thread_load(thread_t th, void (*entry)(void), void *stack)
{

	if (entry != NULL && !user_area(entry))
		return EINVAL;
	if (stack != NULL && !user_area(stack))
		return EINVAL;

	sched_lock();
	if (!thread_valid(th)) {
		sched_unlock();
		return ESRCH;
	}
	if (!task_access(th->task)) {
		sched_unlock();
		return EPERM;
	}
	if (entry != NULL)
		context_set(&th->ctx, CTX_UENTRY, (vaddr_t)entry);
	if (stack != NULL)
		context_set(&th->ctx, CTX_USTACK, (vaddr_t)stack);

	sched_unlock();
	return 0;
}

thread_t
thread_self(void)
{

	return cur_thread;
}

/*
 * Release current thread for other thread.
 */
void
thread_yield(void)
{

	sched_yield();
}

/*
 * Suspend thread.
 *
 * A thread can be suspended any number of times.
 * And, it does not start to run again unless the
 * thread is resumed by the same count of suspend
 * request.
 */
int
thread_suspend(thread_t th)
{

	sched_lock();
	if (!thread_valid(th)) {
		sched_unlock();
		return ESRCH;
	}
	if (!task_access(th->task)) {
		sched_unlock();
		return EPERM;
	}
	if (++th->suscnt == 1)
		sched_suspend(th);

	sched_unlock();
	return 0;
}

/*
 * Resume thread.
 *
 * A thread does not begin to run, unless both thread
 * suspend count and task suspend count are set to 0.
 */
int
thread_resume(thread_t th)
{
	int err = 0;

	ASSERT(th != cur_thread);

	sched_lock();
	if (!thread_valid(th)) {
		err = ESRCH;
		goto out;
	}
	if (!task_access(th->task)) {
		err = EPERM;
		goto out;
	}
	if (th->suscnt == 0) {
		err = EINVAL;
		goto out;
	}

	th->suscnt--;
	if (th->suscnt == 0 && th->task->suscnt == 0)
		sched_resume(th);
out:
	sched_unlock();
	return err;
}

/*
 * thread_schedparam - get/set scheduling parameter.
 *
 * If the caller has CAP_NICE capability, all operations are
 * allowed.  Otherwise, the caller can change the parameter
 * for the threads in the same task, and it can not set the
 * priority to higher value.
 */
int
thread_schedparam(thread_t th, int op, int *param)
{
	int prio, policy, err = 0;

	sched_lock();
	if (!thread_valid(th)) {
		err = ESRCH;
		goto out;
	}
	if (th->task == &kern_task) {
		err = EPERM;
		goto out;
	}
	if (th->task != cur_task() && !task_capable(CAP_NICE)) {
		err = EPERM;
		goto out;
	}

	switch (op) {
	case OP_GETPRIO:
		prio = sched_getprio(th);
		err = umem_copyout(&prio, param, sizeof(prio));
		break;

	case OP_SETPRIO:
		if ((err = umem_copyin(param, &prio, sizeof(prio))))
			break;
		if (prio < 0) {
			prio = 0;
		} else if (prio >= PRIO_IDLE) {
			prio = PRIO_IDLE - 1;
		} else {
			/* DO NOTHING */
		}

		if (prio < th->prio && !task_capable(CAP_NICE)) {
			err = EPERM;
			break;
		}
		/*
		 * If a current priority is inherited for mutex,
		 * we can not change the priority to lower value.
		 * In this case, only the base priority is changed,
		 * and a current priority will be adjusted to
		 * correct value, later.
		 */
		if (th->prio != th->baseprio && prio > th->prio)
			prio = th->prio;

		mutex_setprio(th, prio);
		sched_setprio(th, prio, prio);
		break;

	case OP_GETPOLICY:
		policy = sched_getpolicy(th);
		err = umem_copyout(&policy, param, sizeof(policy));
		break;

	case OP_SETPOLICY:
		if ((err = umem_copyin(param, &policy, sizeof(policy))))
			break;
		if (sched_setpolicy(th, policy))
			err = EINVAL;
		break;

	default:
		err = EINVAL;
		break;
	}
 out:
	sched_unlock();
	return err;
}

/*
 * Idle thread.
 *
 * This routine is called only once after kernel
 * initialization is completed. An idle thread has the
 * role of cutting down the power consumption of a
 * system. An idle thread has FIFO scheduling policy
 * because it does not have time quantum.
 */
void
thread_idle(void)
{

	for (;;) {
		machine_idle();
		sched_yield();
	}
	/* NOTREACHED */
}

/*
 * Create a thread running in the kernel address space.
 *
 * A kernel thread does not have user mode context, and its
 * scheduling policy is set to SCHED_FIFO. kthread_create()
 * returns thread ID on success, or NULL on failure.
 *
 * Important: Since sched_switch() will disable interrupts in
 * CPU, the interrupt is always disabled at the entry point of
 * the kernel thread. So, the kernel thread must enable the
 * interrupt first when it gets control.
 *
 * This routine assumes the scheduler is already locked.
 */
thread_t
kthread_create(void (*entry)(void *), void *arg, int prio)
{
	thread_t th;
	void *sp;

	ASSERT(cur_thread->locks > 0);

	/*
	 * If there is not enough core for the new thread,
	 * just drop to panic().
	 */
	if ((th = thread_alloc()) == NULL)
		return NULL;

	th->task = &kern_task;
	memset(th->kstack, 0, KSTACK_SIZE);
	sp = (char *)th->kstack + KSTACK_SIZE;
	context_set(&th->ctx, CTX_KSTACK, (vaddr_t)sp);
	context_set(&th->ctx, CTX_KENTRY, (vaddr_t)entry);
	context_set(&th->ctx, CTX_KARG, (vaddr_t)arg);
	list_insert(&kern_task.threads, &th->task_link);

	/*
	 * Start scheduling of this thread.
	 */
	sched_start(th);
	sched_setpolicy(th, SCHED_FIFO);
	sched_setprio(th, prio, prio);
	sched_resume(th);
	return th;
}

/*
 * Terminate kernel thread.
 */
void
kthread_terminate(thread_t th)
{

	ASSERT(th);
	ASSERT(th->task == &kern_task);

	sched_lock();
	do_terminate(th);
	sched_unlock();
}

/*
 * Return thread information for ps command.
 */
int
thread_info(struct info_thread *info)
{
	u_long index, target = info->cookie;
	list_t i, j;
	thread_t th;
	task_t task;
	int err = 0, found = 0;

	sched_lock();

	/*
	 * Search a target thread from the given index.
	 */
	index = 0;
	i = &kern_task.link;
	do {
		task = list_entry(i, struct task, link);
		j = list_first(&task->threads);
		do {
			th = list_entry(j, struct thread, task_link);
			if (index++ == target) {
				found = 1;
				goto done;
			}
			j = list_next(j);
		} while (j != &task->threads);
		i = list_next(i);
	} while (i != &kern_task.link);
 done:
	if (found) {
		info->policy = th->policy;
		info->prio = th->prio;
		info->time = th->time;
		info->task = th->task;
		strlcpy(info->taskname, task->name, MAXTASKNAME);
		strlcpy(info->slpevt,
			th->slpevt ? th->slpevt->name : "-", MAXEVTNAME);
	} else {
		err = ESRCH;
	}
	sched_unlock();
	return err;
}

#ifdef DEBUG
void
thread_dump(void)
{
	static const char state[][4] = \
		{ "RUN", "SLP", "SUS", "S&S", "EXT" };
	static const char pol[][5] = { "FIFO", "RR  " };
	list_t i, j;
	thread_t th;
	task_t task;

	printf("\nThread dump:\n");
	printf(" mod thread   task     stat pol  prio base time     "
	       "susp sleep event\n");
	printf(" --- -------- -------- ---- ---- ---- ---- -------- "
	       "---- ------------\n");

	i = &kern_task.link;
	do {
		task = list_entry(i, struct task, link);
		j = list_first(&task->threads);
		do {
			th = list_entry(j, struct thread, task_link);

			printf(" %s %08x %8s %s%c %s  %3d  %3d %8d %4d %s\n",
			       (task == &kern_task) ? "Knl" : "Usr", th,
			       task->name, state[th->state],
			       (th == cur_thread) ? '*' : ' ',
			       pol[th->policy], th->prio, th->baseprio,
			       th->time, th->suscnt,
			       th->slpevt != NULL ? th->slpevt->name : "-");

			j = list_next(j);
		} while (j != &task->threads);
		i = list_next(i);
	} while (i != &kern_task.link);
}
#endif

/*
 * The first thread in system is created here by hand.
 * This thread will become an idle thread when thread_idle()
 * is called later in main().
 */
void
thread_init(void)
{
	void *stack, *sp;

	if ((stack = kmem_alloc(KSTACK_SIZE)) == NULL)
		panic("thread_init: out of memory");

	memset(stack, 0, KSTACK_SIZE);
	idle_thread.kstack = stack;
	idle_thread.magic = THREAD_MAGIC;
	idle_thread.task = &kern_task;
	idle_thread.state = TH_RUN;
	idle_thread.policy = SCHED_FIFO;
	idle_thread.prio = PRIO_IDLE;
	idle_thread.baseprio = PRIO_IDLE;
	idle_thread.locks = 1;

	sp = (char *)stack + KSTACK_SIZE;
	context_set(&idle_thread.ctx, CTX_KSTACK, (vaddr_t)sp);
	list_insert(&kern_task.threads, &idle_thread.task_link);
}
