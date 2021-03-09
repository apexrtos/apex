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
#include <arch/context.h>
#include <arch/machine.h>
#include <arch/stack.h>
#include <cassert>
#include <compiler.h>
#include <cstring>
#include <debug.h>
#include <errno.h>
#include <futex.h>
#include <kernel.h>
#include <kmem.h>
#include <page.h>
#include <sch.h>
#include <sched.h>
#include <sections.h>
#include <sig.h>
#include <sync.h>
#include <sys/mman.h>
#include <task.h>

#define THREAD_MAGIC   0x5468723f      /* 'Thr?' */

#ifdef CONFIG_KSTACK_CHECK
#define KSTACK_MAGIC 0x4B53544B /* KSTK */
#define KSTACK_CHECK_INIT(th) *(uint32_t*)(th)->kstack = KSTACK_MAGIC
#define KSTACK_CHECK(th) (*(uint32_t*)(th)->kstack == KSTACK_MAGIC)
#else  /* !CONFIG_KSTACK_CHECK */
#define KSTACK_CHECK_INIT(th)
#define KSTACK_CHECK(th) 1
#endif	/* !CONFIG_KSTACK_CHECK */

__fast_bss thread idle_thread;
__fast_data static list zombie_list = LIST_INIT(zombie_list);
static spinlock zombie_lock;

/*
 * Allocate a new thread and attach a kernel stack to it.
 * Returns thread pointer on success, or NULL on failure.
 */
static thread *
thread_alloc(long mem_attr)
{
	thread *th;
	phys *stack;

	if ((th = kmem_alloc(sizeof(*th), MA_FAST)) == NULL)
		return NULL;

	if ((stack = page_alloc(CONFIG_KSTACK_SIZE, mem_attr, th)) == NULL) {
		kmem_free(th);
		return NULL;
	}
	memset(th, 0, sizeof(*th));
	th->kstack = phys_to_virt(stack);
	th->magic = THREAD_MAGIC;
#if defined(CONFIG_KSTACK_CHECK)
	memset(th->kstack, 0xaa, CONFIG_KSTACK_SIZE);
	KSTACK_CHECK_INIT(th);
#endif
	return th;
}

/*
 * Free thread memory
 */
static void
thread_free(thread *th)
{
	assert(th->magic == THREAD_MAGIC);

	th->magic = 0;
	context_free(&th->ctx);
	page_free(virt_to_phys(th->kstack), CONFIG_KSTACK_SIZE, th);
	kmem_free(th);
}

/*
 * Free zombie threads
 */
static void
thread_reap_zombies()
{
	int s = spinlock_lock_irq_disable(&zombie_lock);
	while (!list_empty(&zombie_list)) {
		thread *th = list_entry(list_first(&zombie_list),
			thread, task_link);
		list_remove(&th->task_link);
		spinlock_unlock_irq_restore(&zombie_lock, s);
		assert(th->state & TH_ZOMBIE);
		thread_free(th);
		s = spinlock_lock_irq_disable(&zombie_lock);
	}
	spinlock_unlock_irq_restore(&zombie_lock, s);
}

/*
 * Get current thread
 */
thread *
thread_cur()
{
	return sch_active();
}

/*
 * thread_valid - check thread validity.
 */
bool
thread_valid(thread *th)
{
	return k_address(th) && th->magic == THREAD_MAGIC;
}

/*
 * Create a new thread.
 *
 * The new thread is initially set to suspend state, and so, sch_resume()
 * must be called to start it.
 */
int
thread_createfor(task *task, as *as, thread **thp,
    void *sp, long mem_attr, void (*entry)(), long arg)
{
	int r;
	thread *th;

	thread_reap_zombies();

	if ((th = thread_alloc(mem_attr)) == NULL)
		return DERR(-ENOMEM);

	*thp = th;

	/*
	 * Initialize thread state.
	 */
	th->task = task;
	void *const ksp = arch_kstack_align(th->kstack + CONFIG_KSTACK_SIZE);
	if ((r = context_init_uthread(&th->ctx, as, ksp, sp, entry, arg)) < 0) {
		thread_free(th);
		return r;
	}

	sch_lock();

	/* can't add thread to zombie task */
	if (task->state == PS_ZOMB) {
		sch_unlock();
		thread_free(th);
		return DERR(-EOWNERDEAD);
	}

	/* add new threads to end of list (master thread at head) */
	list_insert(list_last(&task->threads), &th->task_link);
	sch_start(th);

	sch_unlock();

	return 0;
}

/*
 * Stop execution of the specified thread
 *
 * Can be called under interrupt.
 */
void
thread_terminate(thread *th)
{
	sch_stop(th);
	sig_thread(th, 0);		/* signal 0 is special */
	context_terminate(th);
}

/*
 * Queue zombie thread for deletion
 *
 * Can be called under interrupt.
 */
void
thread_zombie(thread *th)
{
	const int s = spinlock_lock_irq_disable(&zombie_lock);
	list_insert(&zombie_list, &th->task_link);
	spinlock_unlock_irq_restore(&zombie_lock, s);
}

/*
 * Set thread name.
 *
 * The naming service is separated from thread_create() so
 * the thread name can be changed at any time.
 */
int
thread_name(thread *th, const char *name)
{
	sch_lock();
	strlcpy(th->name, name, ARRAY_SIZE(th->name));
	sch_unlock();

	return 0;
}

/*
 * Convert thread pointer to thread id
 */
int
thread_id(thread *t)
{
	const unsigned shift = floor_log2(alignof(thread));
	return (unsigned long)virt_to_phys(t) >> shift;
}

/*
 * Convert thread id to thread pointer
 */
thread *
thread_find(int id)
{
	const unsigned shift = floor_log2(alignof(thread));
	thread *th = phys_to_virt((phys*)(id << shift));
	if (!k_access_ok(th, sizeof *th, PROT_WRITE))
		return 0;
	if (!thread_valid(th))
		return 0;
	return th;
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
[[noreturn]] void
thread_idle()
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
 * This routine assumes the scheduler is already locked.
 */
thread *
kthread_create(void (*entry)(void *), void *arg, int prio, const char *name,
    long mem_attr)
{
	thread *th;
	void *sp;

	assert(name);

	thread_reap_zombies();

	/*
	 * If there is not enough core for the new thread,
	 * just drop to panic().
	 */
	if ((th = thread_alloc(mem_attr)) == NULL)
		return NULL;

	strlcpy(th->name, name, ARRAY_SIZE(th->name));
	th->task = &kern_task;
	sp = arch_kstack_align((char *)th->kstack + CONFIG_KSTACK_SIZE);
	context_init_kthread(&th->ctx, sp, entry, arg);
	/* add new threads to end of list (idle_thread at head) */
	sch_lock();
	list_insert(list_last(&kern_task.threads), &th->task_link);
	sch_unlock();

	/*
	 * Start scheduling of this thread.
	 */
	sch_start(th);
	sch_setpolicy(th, SCHED_FIFO);
	sch_setprio(th, prio, prio);
	sch_resume(th);
	return th;
}

void
thread_check()
{
#ifdef CONFIG_THREAD_CHECK
	list *task_link;
	thread *th;
	task *task;

	if (likely(idle_thread.magic == THREAD_MAGIC)) { /* not early in boot */
		task_link = &kern_task.link;
		do {
			task = list_entry(task_link, task, link);
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
thread_dump()
{
	static const char pol[][5] =
		{ "OTHR", "FIFO", "  RR", "BTCH", "IDLE", "DDLN" };
	list *i;
	thread *th;
	task *task;

	info("thread dump\n");
	info("===========\n");
	info(" thread      name     task       stat pol  prio base time(ms) "
	     "sleep event task path\n");
	info(" ----------- -------- ---------- ---- ---- ---- ---- -------- "
	     "----------- ------------\n");

	sch_lock();
	i = &kern_task.link;
	do {
		task = list_entry(i, task, link);

		list_for_each_entry(th, &task->threads, task_link) {
			info(" %p%c %8s %p %c%c%c%c %s %4d %4d %8llu %11s %s\n",
			    th, (th == thread_cur()) ? '*' : ' ',
			    th->name, task,
			    th->state & TH_SLEEP ? 'S' : ' ',
			    th->state & TH_SUSPEND ? 'U' : ' ',
			    th->state & TH_EXIT ? 'E' : ' ',
			    th->state & TH_ZOMBIE ? 'Z' : ' ',
			    pol[th->policy], th->prio, th->baseprio,
			    th->time / 1000000,
			    th->slpevt != NULL ? th->slpevt->name : "-",
			    task->path ?: "kernel");
		}
		i = list_next(i);
	} while (i != &kern_task.link);
	sch_unlock();
}

/*
 * The first thread in system is created here by hand.
 * This thread will become an idle thread when thread_idle()
 * is called later in main().
 */
void
thread_init()
{
	extern char __stack_start[1], __stack_size[1];

	idle_thread.kstack = __stack_start;
	idle_thread.magic = THREAD_MAGIC;
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
	spinlock_init(&zombie_lock);
}
