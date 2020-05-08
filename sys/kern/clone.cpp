#include <clone.h>

#include <access.h>
#include <arch.h>
#include <debug.h>
#include <errno.h>
#include <fs.h>
#include <kernel.h>
#include <sch.h>
#include <sched.h>
#include <task.h>
#include <thread.h>

static int
clone_thread(unsigned long flags, void *sp, int *ptid, void *tls,
    int *ctid)
{
	/* These flags must be set for thread creation, Apex has no way to
	   create a thread without these behaviours */
	constexpr auto mandatory_flags =
		CLONE_FILES |		/* share file descriptor table */
		CLONE_FS |		/* share cwd, umask, etc */
		CLONE_SIGHAND |		/* share signal handlers */
		CLONE_SYSVSEM |		/* share semaphore adjustment values */
		CLONE_VM;		/* share memory space */

	if ((flags & mandatory_flags) != mandatory_flags)
		return DERR(-EINVAL);

	/* Check for sane pointers */
	if ((flags & CLONE_CHILD_CLEARTID) && !u_address(ctid))
		return DERR(-EFAULT);
	if ((flags & CLONE_PARENT_SETTID) && !u_address(ptid))
		return DERR(-EFAULT);
	if ((flags & CLONE_SETTLS) && !u_address(tls))
		return DERR(-EFAULT);

	struct thread *th;
	if (auto r = thread_createfor(task_cur(), task_cur()->as, &th, sp,
	    MA_NORMAL, 0, 0); r < 0)
		return r;

	const int tid = thread_id(th);

	if (flags & CLONE_CHILD_CLEARTID)
		th->clear_child_tid = ctid;
	if (flags & CLONE_PARENT_SETTID)
		*ptid = tid;
	if (flags & CLONE_SETTLS)
		context_set_tls(&th->ctx, tls);

	sch_resume(th);

	return tid;
}

static int
clone_process(unsigned long flags, void *sp)
{
	/* only support CLONE_VM and CLONE_VFORK for process creation */
	if ((flags & (CLONE_VM | CLONE_VFORK | CSIGNAL)) != flags)
		return DERR(-EINVAL);

	struct task *child;
	if (auto r = task_create(task_cur(),
	    flags & CLONE_VM ? VM_SHARE : VM_COPY, &child); r < 0)
		return r;

	struct thread *th;
	if (auto r = thread_createfor(child, child->as, &th, sp, MA_NORMAL, 0,
	    0); r < 0) {
		task_destroy(child);
		return r;
	}

	child->termsig = flags & CSIGNAL;
	fs_fork(child);

	const auto ret = task_pid(child);

	assert(!child->vfork);
	if (flags & CLONE_VFORK)
		child->vfork = thread_cur();
	sch_suspend_resume(child->vfork, th);

	context_restore_vfork(&thread_cur()->ctx, thread_cur()->task->as);

	return ret;
}

/*
 * sc_clone - minimal clone implementation
 */
int
sc_clone(unsigned long flags, void *sp, void *ptid, unsigned long tls,
    void *ctid)
{
	sp = arch_ustack_align(sp);
	if (flags & CLONE_THREAD)
		return clone_thread(flags, sp, (int*)ptid, (void *)tls, (int*)ctid);
	else
		return clone_process(flags, sp);
}

/*
 * sc_fork - fork a new process
 */
int
sc_fork(void)
{
	return sc_clone(SIGCHLD, 0, 0, 0, 0);
}

/*
 * sc_vfork - fork a new process and suspend caller
 */
int
sc_vfork(void)
{
	return sc_clone(CLONE_VM | CLONE_VFORK | SIGCHLD, 0, 0, 0, 0);
}
