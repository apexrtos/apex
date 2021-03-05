/*
 * proc.c - process management routines.
 *
 * An Apex task maps loosely to a process. All processes are
 * tasks, but a task is not necessarily a process.
 *
 * Tasks which have no process mapping have pid == -1.
 *
 * Apex supports a single session only.
 */

#include <proc.h>

#include <access.h>
#include <arch.h>
#include <assert.h>
#include <debug.h>
#include <errno.h>
#include <fs.h>
#include <futex.h>
#include <kernel.h>
#include <sch.h>
#include <sig.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <sys/wait.h>
#include <task.h>
#include <thread.h>
#include <unistd.h>
#include <vm.h>

/*
 * proc_exit - exit a process
 *
 * Can be called under interrupt.
 */
void
proc_exit(struct task *task, int status, int signal)
{
	struct list *head, *n;
	struct thread *th;

	if (task->state == PS_ZOMB)
		return;

	sch_lock();

	/*
	 * init is not allowed to die
	 */
	if (task == task_find(1))
		panic("init died");

	/*
	 * Set the parent pid of all child processes to init.
	 * Clear all child process vfork thread references.
	 */
	struct task *child;
	list_for_each_entry(child, &kern_task.link, link) {
		if (child->parent == task) {
			child->parent = task_find(1);
			child->vfork = 0;
		}
	}

	/*
	 * Stop task events
	 */
	timer_stop(&task->itimer_real);

	/*
	 * Set task as a zombie
	 * FIXME: only if parent has not set the disposition of SIGCHLD
	 * to SIG_IGN or the SA_NOCLDWAIT flag is set
	 */
	task->state = PS_ZOMB;
	task->exitcode = (status & 0xff) << 8 | (signal & 0x7f);

	/*
	 * Resume vfork thread if this process was vforked and didn't exec or
	 * child process failed to run
	 */
	if (task->vfork)
		sch_resume(task->vfork);

	/*
	 * Signal parent process
	 */
	sch_wakeone(&task->parent->child_event);
	if (task->termsig)
		sig_task(task->parent, task->termsig);

	/*
	 * Terminate all threads in task
	 */
	head = &task->threads;
	for (n = list_first(head); n != head; n = list_next(n)) {
		th = list_entry(n, struct thread, task_link);
		thread_terminate(th);
	}

	sch_unlock();

	/*
	 * Notify filesystem of exit
	 */
	fs_exit(task);
}

/*
 * Find the zombie process in the child processes. It just
 * returns the pid and exit code if it find at least one zombie
 * process.
 *
 * The behavior is different for the pid value.
 *
 *  if (pid > 0)
 *    Wait for a specific process.
 *
 *  if (pid == 0)
 *    Wait for a process in same process group.
 *
 *  if (pid == -1)
 *    Wait for any child process.
 *
 *  if (pid < -1)
 *    Wait for a child process whose process group id is equal to -pid.
 */
pid_t
sc_wait4(pid_t pid, int *ustatus, int options, struct rusage *rusage)
{
	int err, status;
	struct task *task, *cur = task_cur();
	pid_t cpid = 0;
	int have_children;

	if (rusage)
		dbg("WARNING: wait4 rusage not yet implemented\n");

again:
	sch_lock();
	have_children = 0;
	list_for_each_entry(task, &kern_task.link, link) {
		if (task->parent != cur)
			continue;

		have_children = 1;

		int match = 0;
		if (pid > 0) {
			/*
			 * Wait for a specific child process.
			 *
			 * REVISIT(optimisation): we could optimise this case
			 *                        by hoisting it from the search
			 */
			if (task_pid(task) == pid)
				match = 1;
		} else if (pid == 0) {
			/*
			 * Wait for a process who has same pgid.
			 */
			if (task->pgid == cur->pgid)
				match = 1;
		} else if (pid != -1) {
			/*
			 * Wait for a specific pgid.
			 */
			if (task->pgid == -pid)
				match = 1;
		} else {
			/*
			 * pid = -1
			 * Wait for any child process.
			 */
			match = 1;
		}
		if (match) {
			/*
			 * Get the exit code.
			 */
			if (task->state == PS_STOP) {
				cpid = task_pid(task);
				status = task->exitcode;
				break;
			} else if (task->state == PS_ZOMB) {
				cpid = task_pid(task);
				status = task->exitcode;

				/*
				 * Wait for child threads to finish
				 */
				const k_sigset_t sig_mask = sig_block_all();
				while (!list_empty(&task->threads)) {
					sch_prepare_sleep(&task->thread_event, 0);
					sch_unlock();
					sch_continue_sleep();
					sch_lock();
				}
				sig_restore(&sig_mask);

				/*
				 * Free child resources
				 */
				list_remove(&task->link);
				sch_unlock();
				fs_exit(task);
				futexes_destroy(task_futexes(task));
				as_modify_begin(task->as);
				as_destroy(task->as);
				task->magic = 0;
				free(task->path);
				free(task);
				sch_lock();
				break;
			}
		}

	}

	/*
	 * No children to wait for
	 */
	if (!have_children)
		err = -ECHILD;
	else if (cpid) {
		err = cpid;
		if (ustatus) {
			sch_unlock();
			int r = vm_write(task_cur()->as, &status, ustatus,
					 sizeof *ustatus);
			if (r < 0)
				err = r;
			sch_lock();
		}
	} else if (options & WNOHANG) {
		/*
		 * No child exited, but caller has asked us not to block
		 */
		err = 0;
	} else {
		/*
		 * Wait for a signal or child exit
		 */
		if (!(err = sch_prepare_sleep(&task_cur()->child_event, 0))) {
			sch_unlock();
			if (!(err = sch_continue_sleep()))
				goto again;
			sch_lock();
		}
	}

	sch_unlock();
	return err;
}

/*
 * sc_tkill - send a signal to a thread
 */
int
sc_tkill(int tid, int sig)
{
	struct thread *th;
	if (!(th = thread_find(tid)))
		return DERR(-ESRCH);
	if (sig <= 0 || sig > NSIG)
		return DERR(-EINVAL);
	sig_thread(th, sig);
	return 0;
}

/*
 * sc_tgkill - send a signal to a thread in a specific process
 */
int
sc_tgkill(pid_t pid, int tid, int sig)
{
	struct thread *th;
	if (!(th = thread_find(tid)))
		return DERR(-ESRCH);
	if (task_pid(th->task) != pid)
		return DERR(-ESRCH);
	if (sig <= 0 || sig > NSIG)
		return DERR(-EINVAL);
	sig_thread(th, sig);
	return 0;
}

/*
 * setpgid - set process group ID for job control.
 *
 * If the specified pid is equal to 0, the process ID of
 * the calling process is used. Also, if pgid is 0, the process
 * ID of the indicated process is used.
 */
int
setpgid(pid_t pid, pid_t pgid)
{
	struct task *task;

	if ((pid < 0) || (pgid < 0))
		return DERR(-EINVAL);

	sch_lock();
	if (!(task = task_find(pid))) {
		sch_unlock();
		return DERR(-ESRCH);
	}

	pid = task_pid(task);

	if (pgid == 0)
		pgid = pid;
	else {
		if (!task_find(pgid)) {
			sch_unlock();
			return DERR(-ESRCH);
		}
	}

	task->pgid = pgid;

	sch_unlock();
	return 0;
}

/*
 * getpgid - get the process group ID for a process.
 *
 * If the specified pid is equal to 0, it returns the process
 * group ID of the calling process.
 */
pid_t
getpgid(pid_t pid)
{
	struct task *task;
	pid_t pgid;

	if (pid < 0)
		return DERR(-EINVAL);

	sch_lock();
	if (!(task = task_find(pid))) {
		sch_unlock();
		return DERR(-ESRCH);
	}

	pgid = task->pgid;
	sch_unlock();

	return pgid;
}

/*
 * getpid - get the process ID of the current process
 */
pid_t
getpid()
{
	return task_pid(task_cur());
}

/*
 * getppid - get the process ID of the parent of the current process
 */
pid_t
getppid()
{
	return task_pid(task_cur()->parent);
}

/*
 * getuid - get the real user ID of the current process
 */
uid_t
getuid()
{
	/* TODO: users */
	return 0;
}

/*
 * geteuid - get the effective user ID of the current process
 */
uid_t
geteuid()
{
	/* TODO: users */
	return 0;
}

/*
 * setsid - create session and set process group ID.
 */
pid_t setsid()
{
	struct task *cur = task_cur();
	pid_t pid = task_pid(cur);

	/*
	 * setsid fails if pid is already a process group leader
	 */
	if (cur->pgid == pid)
		return DERR(-EPERM);

	cur->pgid = pid;
	cur->sid = pid;
	return pid;
}

/*
 * getsid - get the session ID of a process.
 */
pid_t
getsid(pid_t pid)
{
	struct task *task;
	pid_t sid;

	if (pid < 0)
		return DERR(-EINVAL);

	sch_lock();
	if (!(task = task_find(pid))) {
		sch_unlock();
		return DERR(-ESRCH);
	}

	sid = task->sid;
	sch_unlock();

	return sid;
}

/*
 * Send a signal.
 *
 * The behavior is different for the pid value.
 *
 *  if (pid > 0)
 *    Send a signal to specific process.
 *
 *  if (pid == 0)
 *    Send a signal to all processes in same process group.
 *
 *  if (pid == -1)
 *    Send a signal to all processes except init.
 *
 *  if (pid < -1)
 *     Send a signal to the process group whose id is -pid.
 *
 *  if (sig == 0)
 *     No signal is sent, but error checking is still performed.
 *
 * Note: Need CAP_KILL capability to send a signal to the different
 * process/group.
 */
int
kill(pid_t pid, int sig)
{
	switch (sig) {
	case SIGFPE:
	case SIGILL:
	case SIGSEGV:
		return DERR(-EINVAL);
	}

	int err = 0;
	struct task *task, *cur = task_cur();

	sch_lock();
	if (pid > 0) {
		if ((pid != task_pid(cur)) && !task_capable(CAP_KILL))
			err = DERR(-EPERM);
		else {
			if (!(task = task_find(pid)))
				err = DERR(-ESRCH);
			else
				err = sig_task(task, sig);
		}
	} else if (pid == -1) {
		if (!task_capable(CAP_KILL))
			err = DERR(-EPERM);
		else {
			list_for_each_entry(task, &kern_task.link, link) {
				if ((task_pid(task) > 1) &&
				    ((err = sig_task(task, sig)) != 0))
					break;
			}
		}
	} else if (pid == 0) {
		list_for_each_entry(task, &kern_task.link, link) {
			if ((task->pgid == cur->pgid) &&
			    ((err = sig_task(task, sig)) != 0))
				break;
		}
	} else {
		if ((cur->pgid != -pid) && !task_capable(CAP_KILL))
			err = DERR(-EPERM);
		else {
			list_for_each_entry(task, &kern_task.link, link) {
				if ((task->pgid == -pid) &&
				    ((err = sig_task(task, sig)) != 0))
					break;
			}
		}
	}
	sch_unlock();
	return err;
}
