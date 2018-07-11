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

#include <thread.h>

#include <access.h>
#include <arch.h>
#include <assert.h>
#include <debug.h>
#include <elf_load.h>
#include <errno.h>
#include <futex.h>
#include <kernel.h>
#include <kmem.h>
#include <prio.h>
#include <sch.h>
#include <sched.h>
#include <sections.h>
#include <sig.h>
#include <stdalign.h>
#include <stddef.h>
#include <stdnoreturn.h>
#include <string.h>
#include <sys/mman.h>
#include <task.h>
#include <vm.h>

#define THREAD_MAGIC   0x5468723f      /* 'Thr?' */

#ifdef CONFIG_KSTACK_CHECK
#define KSTACK_MAGIC 0x4B53544B /* KSTK */
#define KSTACK_CHECK_INIT(th) *(uint32_t*)(th)->kstack = KSTACK_MAGIC
#define KSTACK_CHECK(th) (*(uint32_t*)(th)->kstack == KSTACK_MAGIC)
#else  /* !CONFIG_KSTACK_CHECK */
#define KSTACK_CHECK_INIT(th)
#define KSTACK_CHECK(th) 1
#endif	/* !CONFIG_KSTACK_CHECK */

/* forward declarations */
static void do_terminate(struct thread *);

__fast_bss struct thread idle_thread;

/*
 * Allocate a new thread and attach a kernel stack to it.
 * Returns thread pointer on success, or NULL on failure.
 */
static struct thread *
thread_alloc(unsigned type)
{
	struct thread *th;
	void *stack;

	if ((th = kmem_alloc(sizeof(*th), MEM_FAST)) == NULL)
		return NULL;

	if ((stack = kmem_alloc(CONFIG_KSTACK_SIZE, type)) == NULL) {
		kmem_free(th);
		return NULL;
	}
	memset(th, 0, sizeof(*th));
	th->kstack = stack;
	th->magic = THREAD_MAGIC;
	list_init(&th->mutexes);
	list_init(&th->futexes);
#if defined(CONFIG_KSTACK_CHECK)
	memset(th->kstack, 0xaa, CONFIG_KSTACK_SIZE);
	KSTACK_CHECK_INIT(th);
#endif
	return th;
}

/*
 * Get current thread
 */
struct thread *
thread_cur(void)
{
	return sch_active();
}

/*
 * thread_valid - check thread validity.
 */
bool
thread_valid(struct thread *th)
{
	return k_address(th) && th->magic == THREAD_MAGIC;
}

/*
 * Create a new thread.
 *
 * The new thread is initially set to suspend state, and so, thread_resume()
 * must be called to start it.
 */
int
thread_createfor(struct task *task, struct thread **thp, void *sp,
    unsigned type, void (*entry)(void), long retval, const char *const prgv[],
    const char *const argv[], const char *const envp[], const unsigned auxv[])
{
	struct thread *th;
	int err = 0;

	sch_lock();
	if (task == NULL)
		task = task_cur();
	else if (!task_valid(task)) {
		err = DERR(-ESRCH);
		goto out;
	} else if (!task_access(task)) {
		err = DERR(-EPERM);
		goto out;
	}
	if ((sp = build_args(task->as, sp, prgv, argv, envp, auxv)) > (void*)-4096UL) {
		err = (int)sp;
		goto out;
	}
	if ((th = thread_alloc(type)) == NULL) {
		err = DERR(-ENOMEM);
		goto out;
	}

	*thp = th;

	/*
	 * Initialize thread state.
	 */
	th->task = task;
	th->suscnt = task->suscnt + 1;
	void *const ksp = arch_stack_align(th->kstack + CONFIG_KSTACK_SIZE);
	context_init_uthread(&th->ctx, ksp, sp, entry, retval);
	/* add new threads to end of list (master thread at head) */
	list_insert(list_last(&task->threads), &th->task_link);
	sch_start(th);
out:
	sch_unlock();
	return err;
}

/*
 * Permanently stop execution of the specified thread.
 * If given thread is a current thread, this routine
 * never returns.
 */
int
thread_terminate(struct thread *th)
{
	sch_lock();
	if (!thread_valid(th)) {
		sch_unlock();
		return DERR(-ESRCH);
	}
	if (!task_access(th->task)) {
		sch_unlock();
		return DERR(-EPERM);
	}
	do_terminate(th);
	sch_unlock();
	return 0;
}

/*
 * Free thread memory
 */
void
thread_free(struct thread *th)
{
	assert(th->state & TH_ZOMBIE);

	th->magic = 0;
	context_cleanup(&th->ctx);
	kmem_free(th->kstack);
	kmem_free(th);
}

/*
 * Terminate thread-- the internal version of thread_terminate.
 */
static void
do_terminate(struct thread *th)
{
	/*
	 * Clean up thread state.
	 */
	sch_stop(th);
	sig_thread(th, 0);		/* signal 0 is special */
	list_remove(&th->task_link);

	if (th->clear_child_tid) {
		const int zero = 0;
		as_write(th->task->as, &zero, th->clear_child_tid, sizeof zero);
		futex(th->task, th->clear_child_tid, FUTEX_PRIVATE | FUTEX_WAKE,
		    1, 0, 0);
	}
}

/*
 * Set thread name.
 *
 * The naming service is separated from thread_create() so
 * the thread name can be changed at any time.
 */
int
thread_name(struct thread *th, const char *name)
{
	int err = 0;

	sch_lock();
	if (!thread_valid(th))
		err = DERR(-ESRCH);
	else if (task_cur() != &kern_task && !task_access(th->task))
		err = DERR(-EPERM);
	else
		strlcpy(th->name, name, ARRAY_SIZE(th->name));
	sch_unlock();
	return err;
}

/*
 * Convert thread pointer to thread id
 */
int
thread_id(struct thread *t)
{
	const unsigned shift = floor_log2(alignof(struct thread));
	return (unsigned long)virt_to_phys(t) >> shift;
}

/*
 * Convert thread id to thread pointer
 */
struct thread *
thread_find(int id)
{
	const unsigned shift = floor_log2(alignof(struct thread));
	struct thread *th = phys_to_virt((phys*)(id << shift));
	if (!k_access_ok(th, sizeof *th, PROT_WRITE))
		return 0;
	if (!thread_valid(th))
		return 0;
	return th;
}

/*
 * Release current thread for other thread.
 */
void
thread_yield(void)
{
	sch_yield();
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
thread_suspend(struct thread *th)
{
	sch_lock();
	if (!thread_valid(th)) {
		sch_unlock();
		return DERR(-ESRCH);
	}
	if (!task_access(th->task)) {
		sch_unlock();
		return DERR(-EPERM);
	}
	if (++th->suscnt == 1)
		sch_suspend(th);

	sch_unlock();
	return 0;
}

/*
 * Resume thread.
 *
 * A thread does not begin to run, unless both thread
 * suspend count and task suspend count are set to 0.
 */
int
thread_resume(struct thread *th)
{
	int err = 0;

	assert(th != thread_cur());

	sch_lock();
	if (!thread_valid(th)) {
		err = DERR(-ESRCH);
		goto out;
	}
	if (!task_access(th->task)) {
		err = DERR(-EPERM);
		goto out;
	}
	if (th->suscnt == 0) {
		err = DERR(-EINVAL);
		goto out;
	}

	th->suscnt--;
	if (th->suscnt == 0 && th->task->suscnt == 0)
		sch_resume(th);
out:
	sch_unlock();
	return err;
}

/*
 * thread_interrupt
 *
 * Interrupt a thread. This will cause it to wake up from any events it may
 * be sleeping on.
 */
int
thread_interrupt(struct thread *th)
{
	int err = 0;

	sch_lock();
	if (!thread_valid(th)) {
		err = DERR(-ESRCH);
		goto out;
	}
	if (th->task != task_cur()) {
		err = DERR(-EPERM);
		goto out;
	}

	sch_interrupt(th);

out:
	sch_unlock();
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
noreturn void
thread_idle(void)
{
	for (;;) {
		machine_idle();
		sch_yield();
	}
}

/*
 * Create a thread running in the kernel address space.
 *
 * A kernel thread does not have user mode context, and its
 * scheduling policy is set to SCHED_FIFO. kthread_create()
 * returns thread ID on success, or NULL on failure.
 *
 * Important: Since sch_switch() will disable interrupts in
 * CPU, the interrupt is always disabled at the entry point of
 * the kernel thread. So, the kernel thread must enable the
 * interrupt first when it gets control.
 *
 * This routine assumes the scheduler is already locked.
 */
struct thread *
kthread_create(void (*entry)(void *), void *arg, int prio, const char *name,
    unsigned type)
{
	struct thread *th;
	void *sp;

	assert(sch_locked());
	assert(name);

	/*
	 * If there is not enough core for the new thread,
	 * just drop to panic().
	 */
	if ((th = thread_alloc(type)) == NULL)
		return NULL;

	strlcpy(th->name, name, ARRAY_SIZE(th->name));
	th->task = &kern_task;
	sp = arch_stack_align((char *)th->kstack + CONFIG_KSTACK_SIZE);
	context_init_kthread(&th->ctx, sp, entry, arg);
	/* add new threads to end of list (idle_thread at head) */
	list_insert(list_last(&kern_task.threads), &th->task_link);

	/*
	 * Start scheduling of this thread.
	 */
	sch_start(th);
	sch_setpolicy(th, SCHED_FIFO);
	sch_setprio(th, prio, prio);
	sch_resume(th);
	return th;
}

/*
 * Terminate kernel thread.
 */
void
kthread_terminate(struct thread *th)
{

	assert(th);
	assert(th->task == &kern_task);

	sch_lock();
	do_terminate(th);
	sch_unlock();
}

void
thread_check(void)
{
#ifdef CONFIG_THREAD_CHECK
	struct list *task_link;
	struct thread *th;
	struct task *task;

	if (likely(idle_thread.magic == THREAD_MAGIC)) { /* not early in boot */
		task_link = &kern_task.link;
		do {
			task = list_entry(task_link, struct task, link);
			assert(task_valid(task));
			list_for_each_entry(th, &task->threads, task_link) {
				assert(th->magic == THREAD_MAGIC);
				assert(KSTACK_CHECK(th));
			}
			task_link = list_next(task_link);
		} while (task_link != &kern_task.link);
	}
#endif	/* CONFIG_THREAD_CHECK */
}

void
thread_dump(void)
{
	static const char state[][4] = \
		{ "RUN", "SLP", "SUS", "S&S", "EXT" };
	static const char pol[][5] = { "FIFO", "RR  " };
	struct list *i, *j;
	struct thread *th;
	struct task *task;

	info("\nThread dump:\n");
	info(" mod thread   name     task     stat pol  prio base time     "
	     "susp sleep event\n");
	info(" --- -------- -------- -------- ---- ---- ---- ---- -------- "
	     "---- -----------\n");

	i = &kern_task.link;
	do {
		task = list_entry(i, struct task, link);
		j = list_first(&task->threads);
		do {
			th = list_entry(j, struct thread, task_link);

			info(" %s %p %8s %8s %s%c %s  %3d  %3d %8llu %4d %s\n",
			       (task == &kern_task) ? "Knl" : "Usr", th,
			       th->name, task->name, state[th->state],
			       (th == thread_cur()) ? '*' : ' ',
			       pol[th->policy], th->prio, th->baseprio,
			       th->time, th->suscnt,
			       th->slpevt != NULL ? th->slpevt->name : "-");

			j = list_next(j);
		} while (j != &task->threads);
		i = list_next(i);
	} while (i != &kern_task.link);
}

/*
 * The first thread in system is created here by hand.
 * This thread will become an idle thread when thread_idle()
 * is called later in main().
 */
void
thread_init(void)
{
	extern char __stack_start[1], __stack_size[1];

	idle_thread.kstack = __stack_start;
	idle_thread.magic = THREAD_MAGIC;
	list_init(&idle_thread.mutexes);
	list_init(&idle_thread.futexes);
	idle_thread.task = &kern_task;
	idle_thread.policy = SCHED_FIFO;
	idle_thread.prio = PRI_IDLE;
	idle_thread.baseprio = PRI_IDLE;
	strcpy(idle_thread.name, "idle");
	context_init_idle(&idle_thread.ctx, __stack_start + (int)__stack_size);
	list_insert(&kern_task.threads, &idle_thread.task_link);
#if defined(CONFIG_KSTACK_CHECK)
	size_t free = ((void *)__builtin_frame_address(0) -
		       (void *)idle_thread.kstack);
	/* do not use memset here as it uses stack... */
	char *sp = (char*)idle_thread.kstack;
	while (free--)
		*sp++ = 0xaa;
	KSTACK_CHECK_INIT(&idle_thread);
#endif
	thread_check();
}
