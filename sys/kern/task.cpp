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
 * task.c - task management routines
 */

#include <task.h>

#include <access.h>
#include <address.h>
#include <as.h>
#include <cassert>
#include <cstdlib>
#include <cstring>
#include <debug.h>
#include <errno.h>
#include <fs.h>
#include <futex.h>
#include <kernel.h>
#include <proc.h>
#include <sch.h>
#include <sys/mman.h>
#include <thread.h>

#define TASK_MAGIC     0x54736b3f      /* 'Tsk?' */

/*
 * Kernel task.
 * kern_task acts as a list head of all tasks in the system.
 */
task kern_task;
task *init_task;

/*
 * task_cur - return current active task.
 */
task *
task_cur()
{
	return thread_cur()->task;
}

/*
 * task_find - convert process id to task pointer
 */
task *
task_find(pid_t pid)
{
	if (pid == 0)
		return task_cur();
	if (pid == 1)
		return init_task;
	const unsigned shift = floor_log2(alignof(task));
	task *t = (task *)phys_to_virt(phys(static_cast<uintptr_t>(pid << shift)));
	if (!k_access_ok(t, sizeof *t, PROT_WRITE))
		return 0;
	if (!task_valid(t))
		return 0;
	return t;
}

/*
 * task_pid - convert task pointer to process id
 */
pid_t
task_pid(task *t)
{
	if (t == &kern_task)
		return 0;
	if (t == init_task)
		return 1;
	const unsigned shift = floor_log2(alignof(task));
	return virt_to_phys(t).phys() >> shift;
}

/*
 * task_valid - test task validity.
 */
bool
task_valid(task *t)
{
	return k_address(t) && t->magic == TASK_MAGIC;
}

/**
 * task_create - create a new task.
 *
 * The child task will inherit some task states from its parent.
 *
 * Inherit status:
 *   Child data        Inherit from parent ?
 *   ----------------- ---------------------
 *   Task name         No
 *   Object list       No
 *   Threads           No
 *   Memory map        New/Copy/Share
 *   Suspend count     No
 *   Exception handler Yes
 *   Capability        Yes
 *
 * vm_option:
 *   VM_NEW:   The child task will have clean memory image.
 *   VM_SHARE: The child task will share whole memory image with parent.
 *   VM_COPY:  The parent's memory image is copied to the child's one.
 *             However, the text region and read-only region will be
 *             physically shared among them. VM_COPY is supported only
 *             with MMU system.
 *
 * Note: The child task initially contains no threads.
 */
int
task_create(task *parent, int vm_option, task **child)
{
	task *task;
	int err = 0;

	sch_lock();
	if (!task_valid(parent)) {
		err = DERR(-ESRCH);
		goto out;
	}
	if (task_cur() != &kern_task) {
		if (!task_access(parent)) {
			err = DERR(-EPERM);
			goto out;
		}
		/*
		 * Set zero as child task id before copying
		 * parent's memory space. So, the child task
		 * can identify whether it is a child.
		 */
		*child = 0;
	}

	/*
	 * Allocate task
	 */
	if ((task = (task *)malloc(sizeof(*task))) == NULL) {
		err = DERR(-ENOMEM);
		goto out;
	}
	memset(task, 0, sizeof(*task));
	task->magic = TASK_MAGIC;

	/*
	 * Setup VM mapping.
	 */
	switch (vm_option) {
	case VM_NEW:
		if (!(task->as = as_create(task_pid(task))))
			err = DERR(-ENOMEM);
		break;
	case VM_SHARE:
		as_reference(parent->as);
		task->as = parent->as;
		err = task_path(task, parent->path);
		break;
	case VM_COPY:
		if ((task->as = as_copy(parent->as, task_pid(task))) > (as*)-4096UL) {
			err = (int)task->as;
			task->as = 0;
			break;
		}
		err = task_path(task, parent->path);
		break;
	default:
		err = DERR(-EINVAL);
		break;
	}

	/*
	 * Fill initial task data.
	 */
	task->capability = parent->capability;
	task->parent = parent;
	list_init(&task->threads);
	futexes_init(&task->futexes);
	task->pgid = parent->pgid;
	task->sid = parent->sid;
	task->state = PS_RUN;
	rwlock_init(&task->fs_lock);
	event_init(&task->child_event, "child", event::ev_SLEEP);
	event_init(&task->thread_event, "thread", event::ev_SLEEP);
	list_insert(&kern_task.link, &task->link);

	if (err < 0) {
		task_destroy(task);
		goto out;
	}

	/*
	 * Register init task
	 */
	if (parent == &kern_task)
		init_task = task;

	*child = task;
 out:
	sch_unlock();
	return err;
}

/*
 * Destroy the specified task.
 *
 * NOTE: this function is for special cleanup use only and is not the normal
 * way for a task to quit. See proc_exit for that.
 *
 * This function only releases resources allocated by task_create.
 */
int
task_destroy(task *task)
{
	assert(task != task_cur());
	assert(list_empty(&task->threads));

	sch_lock();
	list_remove(&task->link);
	sch_unlock();

	if (task->as) {
		as_modify_begin(task->as);
		as_destroy(task->as);
	}
	task->magic = 0;
	free(task->path);
	free(task);

	return 0;
}

/*
 * Suspend a task.
 */
int
task_suspend(task *task)
{
	list *head, *n;
	thread *th;

	sch_lock();
	if (!task_valid(task)) {
		sch_unlock();
		return DERR(-ESRCH);
	}
	if (!task_access(task)) {
		sch_unlock();
		return DERR(-EPERM);
	}

	if (++task->suscnt == 1) {
		/*
		 * Suspend all threads within the task.
		 */
		head = &task->threads;
		for (n = list_first(head); n != head; n = list_next(n)) {
			th = list_entry(n, thread, task_link);
			sch_suspend(th);
		}
	}
	sch_unlock();
	return 0;
}

/*
 * Resume a task.
 *
 * A thread in the task will begin to run only when both
 * thread suspend count and task suspend count become 0.
 */
int
task_resume(task *task)
{
	list *head, *n;
	thread *th;
	int err = 0;

	assert(task != task_cur());

	sch_lock();
	if (!task_valid(task)) {
		err = DERR(-ESRCH);
		goto out;
	}
	if (!task_access(task)) {
		err = DERR(-EPERM);
		goto out;
	}
	assert(task->suscnt > 0);
	if (--task->suscnt == 0) {
		/*
		 * Resume all threads in the target task.
		 */
		head = &task->threads;
		for (n = list_first(head); n != head; n = list_next(n)) {
			th = list_entry(n, thread, task_link);
			sch_resume(th);
		}
	}
 out:
	sch_unlock();
	return err;
}

/*
 * Set task executable path.
 *
 * The naming service is separated from task_create() because
 * the task name can be changed at anytime by exec().
 */
int
task_path(task *t, const char *path)
{
	int err = 0;
	char *copy = 0;

	assert(path);

	sch_lock();
	if (!task_valid(task)) {
		err = DERR(-ESRCH);
		goto out;
	}
	if (!task_access(task)) {
		err = DERR(-EPERM);
		goto out;
	}
	if (!task->path || strcmp(path, task->path)) {
		if ((copy = strdup(path)) == NULL) {
			err = DERR(-ENOMEM);
			goto out;
		}
		free(task->path);
		task->path = copy;
	}

out:
	sch_unlock();
	return err;
}

/*
 * Check if the current task has specified capability.
 */
bool
task_capable(unsigned cap)
{
	return task_cur()->capability & cap;
}

/*
 * Check if the current task can access the specified task.
 * Return true on success, or false on error.
 */
bool
task_access(task *task)
{
	/* Do not access the kernel task. */
	if (task == &kern_task)
		return false;

	return task == task_cur() ||
	    task->parent == task_cur() ||
	    task_capable(CAP_TASK);
}

futexes *
task_futexes(task *t)
{
	return &t->futexes;
}

void
task_dump()
{
	static const char state[][6] = { "INVAL", "  RUN", " ZOMB", " STOP" };
	list *i, *j;
	task *task;
	int nthreads;

	info("task dump\n");
	info("=========\n");
	info(" task        nthrds susp cap      state parent     pid       path\n");
	info(" ----------- ------ ---- -------- ----- ---------- --------- ------------\n");
	i = &kern_task.link;
	do {
		task = list_entry(i, task, link);
		nthreads = 0;
		j = list_first(&task->threads);
		while (j != &task->threads) {
			nthreads++;
			j = list_next(j);
		}

		info(" %p%c    %3d %4d %08x %s %10p %9d %s\n",
		       task, (task == task_cur()) ? '*' : ' ', nthreads,
		       task->suscnt, task->capability,
		       state[task->state], task->parent, task_pid(task),
		       task->path ?: "kernel");

		i = list_next(i);
	} while (i != &kern_task.link);
}

/*
 * We assume that the VM mapping of a kernel task has
 * already been initialized in vm_init().
 */
void
task_init()
{
	/*
	 * Create a kernel task as the first task.
	 */
	list_init(&kern_task.link);
	list_init(&kern_task.threads);
	kern_task.capability = 0xffffffff;
	kern_task.magic = TASK_MAGIC;
	kern_task.state = PS_RUN;
	kern_task.as = as_create(0);
	rwlock_init(&kern_task.fs_lock);
	event_init(&kern_task.child_event, "child", event::ev_SLEEP);
	event_init(&kern_task.thread_event, "thread", event::ev_SLEEP);
}
