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

#include <kernel.h>
#include <kmem.h>
#include <sched.h>
#include <thread.h>
#include <ipc.h>
#include <vm.h>
#include <page.h>
#include <task.h>

/*
 * Kernel task.
 * kern_task acts as a list head of all tasks in the system.
 */
struct task	kern_task;

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
task_create(task_t parent, int vm_option, task_t *child)
{
	struct task *task;
	vm_map_t map = NULL;
	int err = 0;

	switch (vm_option) {
	case VM_NEW:
	case VM_SHARE:
#ifdef CONFIG_MMU
	case VM_COPY:
#endif
		break;
	default:
		return EINVAL;
	}
	sched_lock();
	if (!task_valid(parent)) {
		err = ESRCH;
		goto out;
	}
	if (cur_task() != &kern_task) {
		if (!task_access(parent)) {
			err = EPERM;
			goto out;
		}
		/*
		 * Set zero as child task id before copying
		 * parent's memory space. So, the child task
		 * can identify whether it is a child.
		 */
		task = 0;
		if (umem_copyout(&task, child, sizeof(task))) {
			err = EFAULT;
			goto out;
		}
	}

	if ((task = kmem_alloc(sizeof(*task))) == NULL) {
		err = ENOMEM;
		goto out;
	}
	memset(task, 0, sizeof(*task));

	/*
	 * Setup VM mapping.
	 */
	switch (vm_option) {
	case VM_NEW:
		map = vm_create();
		break;
	case VM_SHARE:
		vm_reference(parent->map);
		map = parent->map;
		break;
	case VM_COPY:
		map = vm_fork(parent->map);
		break;
	default:
		/* DO NOTHING */
		break;
	}
	if (map == NULL) {
		kmem_free(task);
		err = ENOMEM;
		goto out;
	}

	/*
	 * Fill initial task data.
	 */
	task->map = map;
	task->handler = parent->handler;
	task->capability = parent->capability;
	task->parent = parent;
	task->magic = TASK_MAGIC;
	list_init(&task->objects);
	list_init(&task->threads);
	list_insert(&kern_task.link, &task->link);

	if (cur_task() == &kern_task)
		*child = task;
	else
		err = umem_copyout(&task, child, sizeof(task));
 out:
	sched_unlock();
	return err;
}

/*
 * Terminate the specified task.
 */
int
task_terminate(task_t task)
{
	int err = 0;
	list_t head, n;
	thread_t th;
	object_t obj;

	sched_lock();
	if (!task_valid(task)) {
		sched_unlock();
		return ESRCH;
	}
	if (!task_access(task)) {
		sched_unlock();
		return EPERM;
	}

	/* Invalidate the task. */
	task->magic = 0;

	/*
	 * Terminate all threads except a current thread.
	 */
	head = &task->threads;
	for (n = list_first(head); n != head; n = list_next(n)) {
		th = list_entry(n, struct thread, task_link);
		if (th != cur_thread)
			thread_terminate(th);
	}
	/*
	 * Delete all objects owned by the target task.
	 */
	head = &task->objects;
	for (n = list_first(head); n != head; n = list_next(n)) {
		/*
		 * A current task may not have permission to
		 * delete target objects. So, we set the owner
		 * of the object to the current task before
		 * deleting it.
		 */
		obj = list_entry(n, struct object, task_link);
		obj->owner = cur_task();
		object_destroy(obj);
	}
	/*
	 * Release all other task related resources.
	 */
	timer_stop(&task->alarm);
	vm_terminate(task->map);
	list_remove(&task->link);
	kmem_free(task);

	if (task == cur_task()) {
		cur_thread->task = NULL;
		thread_terminate(cur_thread);
	}
	sched_unlock();
	return err;
}

task_t
task_self(void)
{

	return cur_task();
}

/*
 * Suspend a task.
 */
int
task_suspend(task_t task)
{
	list_t head, n;
	thread_t th;

	sched_lock();
	if (!task_valid(task)) {
		sched_unlock();
		return ESRCH;
	}
	if (!task_access(task)) {
		sched_unlock();
		return EPERM;
	}

	if (++task->suscnt == 1) {
		/*
		 * Suspend all threads within the task.
		 */
		head = &task->threads;
		for (n = list_first(head); n != head; n = list_next(n)) {
			th = list_entry(n, struct thread, task_link);
			thread_suspend(th);
		}
	}
	sched_unlock();
	return 0;
}

/*
 * Resume a task.
 *
 * A thread in the task will begin to run only when both
 * thread suspend count and task suspend count become 0.
 */
int
task_resume(task_t task)
{
	list_t head, n;
	thread_t th;
	int err = 0;

	ASSERT(task != cur_task());

	sched_lock();
	if (!task_valid(task)) {
		err = ESRCH;
		goto out;
	}
	if (!task_access(task)) {
		err = EPERM;
		goto out;
	}
	if (task->suscnt == 0) {
		err = EINVAL;
		goto out;
	}
	if (--task->suscnt == 0) {
		/*
		 * Resume all threads in the target task.
		 */
		head = &task->threads;
		for (n = list_first(head); n != head; n = list_next(n)) {
			th = list_entry(n, struct thread, task_link);
			thread_resume(th);
		}
	}
 out:
	sched_unlock();
	return err;
}

/*
 * Set task name.
 *
 * The naming service is separated from task_create() because
 * the task name can be changed at anytime by exec().
 */
int
task_name(task_t task, const char *name)
{
	size_t len;
	int err = 0;

	sched_lock();
	if (!task_valid(task)) {
		err = ESRCH;
		goto out;
	}
	if (!task_access(task)) {
		err = EPERM;
		goto out;
	}
	if (cur_task() == &kern_task) {
		strlcpy(task->name, name, MAXTASKNAME);
	} else {
		if (umem_strnlen(name, MAXTASKNAME, &len))
			err = EFAULT;
		else if (len >= MAXTASKNAME)
			err = ENAMETOOLONG;
		else
			err = umem_copyin(name, task->name, len + 1);
	}
 out:
	sched_unlock();
	return err;
}

/*
 * Get the capability of the specified task.
 */
int
task_getcap(task_t task, cap_t *cap)
{
	cap_t curcap;

	sched_lock();
	if (!task_valid(task)) {
		sched_unlock();
		return ESRCH;
	}
	curcap = task->capability;
	sched_unlock();

	return umem_copyout(&curcap, cap, sizeof(curcap));
}

/*
 * Set the capability of the specified task.
 */
int
task_setcap(task_t task, cap_t *cap)
{
	cap_t newcap;

	if (!task_capable(CAP_SETPCAP))
		return EPERM;

	sched_lock();
	if (!task_valid(task)) {
		sched_unlock();
		return ESRCH;
	}
	if (umem_copyin(cap, &newcap, sizeof(newcap))) {
		sched_unlock();
		return EFAULT;
	}
	task->capability = newcap;
	sched_unlock();
	return 0;
}

/*
 * Check if the current task has specified capability.
 */
int
task_capable(cap_t cap)
{
	task_t task = cur_task();

	return (int)(task->capability & cap);
}

/*
 * Check if the current task can access the specified task.
 * Return true on success, or false on error.
 */
int
task_access(task_t task)
{

	if (task == &kern_task) {
		/* Do not access the kernel task. */
		return 0;
	} else {
		if (task == cur_task() ||
		    task->parent == cur_task() ||
		    task_capable(CAP_TASK))
			return 1;
	}
	return 0;
}

/*
 * Create and setup boot tasks.
 */
void
task_bootstrap(void)
{
	struct module *mod;
	task_t task;
	thread_t th;
	void *stack, *sp;
	int i;

	mod = &bootinfo->tasks[0];
	for (i = 0; i < bootinfo->nr_tasks; i++) {
		/*
		 * Create a new task.
		 */
		if (task_create(&kern_task, VM_NEW, &task))
			break;
		task_name(task, mod->name);
		if (vm_load(task->map, mod, &stack))
			break;

		/*
		 * Create and start a new thread.
		 */
		if (thread_create(task, &th))
			break;
		sp = (char *)stack + USTACK_SIZE - (sizeof(int) * 3);
		if (thread_load(th, (void (*)(void))mod->entry, sp))
			break;
		thread_resume(th);

		mod++;
	}
	if (i != bootinfo->nr_tasks)
		panic("Unable to load boot task");
}

#ifdef DEBUG
void
task_dump(void)
{
	list_t i, j;
	task_t task;
	int nthreads;

	printf("\nTask dump:\n");
	printf(" mod task      nthrds susp exc hdlr cap      name\n");
	printf(" --- --------- ------ ---- -------- -------- ------------\n");
	i = &kern_task.link;
	do {
		task = list_entry(i, struct task, link);
		nthreads = 0;
		j = list_first(&task->threads);
		do {
			nthreads++;
			j = list_next(j);
		} while (j != &task->threads);

		printf(" %s %08x%c    %3d %4d %08x %08x %s\n",
		       (task == &kern_task) ? "Knl" : "Usr", task,
		       (task == cur_task()) ? '*' : ' ', nthreads,
		       task->suscnt, task->handler, task->capability,
		       task->name != NULL ? task->name : "no name");

		i = list_next(i);
	} while (i != &kern_task.link);
}
#endif

/*
 * We assume that the VM mapping of a kernel task has
 * already been initialized in vm_init().
 */
void
task_init(void)
{
	/*
	 * Create a kernel task as the first task.
	 */
	strlcpy(kern_task.name, "kernel", MAXTASKNAME);
	list_init(&kern_task.link);
	list_init(&kern_task.objects);
	list_init(&kern_task.threads);
	kern_task.capability = 0xffffffff;
	kern_task.magic = TASK_MAGIC;
}
