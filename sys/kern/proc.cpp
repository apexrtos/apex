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
#include <as.h>
#include <cassert>
#include <cstdlib>
#include <debug.h>
#include <errno.h>
#include <fs.h>
#include <futex.h>
#include <kernel.h>
#include <sch.h>
#include <sig.h>
#include <sys/mman.h>
#include <sys/wait.h>
#include <syscalls.h>
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
proc_exit(task *t, int status, int signal)
{
	list *head, *n;
	thread *th;

	if (t->state == PS_ZOMB)
		return;

	sch_lock();

	/*
	 * init is not allowed to die
	 */
	if (t == task_find(1))
		panic("init died");

	/*
	 * Set the parent pid of all child processes to init.
	 * Clear all child process vfork thread references.
	 */
	task *child;
	list_for_each_entry(child, &kern_task.link, link) {
		if (child->parent == t) {
			child->parent = task_find(1);
			child->vfork = 0;
		}
	}

	/*
	 * Stop task events
	 */
	timer_stop(&t->itimer_real);

	/*
	 * Set task as a zombie
	 * FIXME: only if parent has not set the disposition of SIGCHLD
	 * to SIG_IGN or the SA_NOCLDWAIT flag is set
	 */
	t->state = PS_ZOMB;
	t->exitcode = (status & 0xff) << 8 | (signal & 0x7f);

	/*
	 * Resume vfork thread if this process was vforked and didn't exec or
	 * child process failed to run
	 */
	if (t->vfork)
		sch_resume(t->vfork);

	/*
	 * Signal parent process
	 */
	sch_wakeone(&t->parent->child_event);
	if (t->termsig)
		sig_task(t->parent, t->termsig);

	/*
	 * Terminate all threads in task
	 */
	head = &t->threads;
	for (n = list_first(head); n != head; n = list_next(n)) {
		th = list_entry(n, thread, task_link);
		thread_terminate(th);
	}

	sch_unlock();

	/*
	 * Notify filesystem of exit
	 */
	fs_exit(t);
}

/*
 * proc_reap_zombie - reap a zombie process
 *
 * Must be called with scheduler locked.
 */
static
void
proc_reap_zombie(task *t)
{
	assert(sch_locks() == 1);
	assert(t->state == PS_ZOMB);

	/*
	 * Wait for threads to finish
	 */
	const k_sigset_t sig_mask = sig_block_all();
	while (!list_empty(&t->threads)) {
		sch_prepare_sleep(&t->thread_event, 0);
		sch_unlock();
		sch_continue_sleep();
		sch_lock();
	}
	sig_restore(&sig_mask);

	/*
	 * Free resources
	 */
	list_remove(&t->link);
	sch_unlock();
	fs_exit(t);
	futexes_destroy(task_futexes(t));
	as_modify_begin(t->as);
	as_destroy(t->as);
	t->magic = 0;
	free(t->path);
	free(t);
	sch_lock();
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
sc_wait4(pid_t pid, int *ustatus, int options, rusage *rusage)
{
	int err, status;
	task *t, *cur = task_cur();
	pid_t cpid = 0;
	int have_children;

	if (rusage)
		DERR(-ENOTSUP);

again:
	sch_lock();
	have_children = 0;
	list_for_each_entry(t, &kern_task.link, link) {
		if (t->parent != cur)
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
			if (task_pid(t) == pid)
				match = 1;
		} else if (pid == 0) {
			/*
			 * Wait for a process who has same pgid.
			 */
			if (t->pgid == cur->pgid)
				match = 1;
		} else if (pid != -1) {
			/*
			 * Wait for a specific pgid.
			 */
			if (t->pgid == -pid)
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
			if (t->state == PS_STOP) {
				cpid = task_pid(t);
				status = t->exitcode;
				break;
			} else if (t->state == PS_ZOMB) {
				cpid = task_pid(t);
				status = t->exitcode;
				proc_reap_zombie(t);
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
			if (auto r = vm_write(task_cur()->as, &status, ustatus,
					      sizeof *ustatus);
			    !r.ok())
				err = r.sc_rval();
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
 * sc_waitid
 */
int
sc_waitid(idtype_t type, id_t id, siginfo_t *uinfop, int options, rusage *ru)
{
	int err, status, code;
	task *t, *cur = task_cur();
	pid_t cpid = 0;
	int have_children;

	if (!(options & (WSTOPPED | WEXITED | WCONTINUED)))
		return DERR(-EINVAL);

	/* Apex doesn't support all options for waitid yet */
	if (ru || options & WCONTINUED)
		return DERR(-ENOTSUP);

again:
	sch_lock();
	have_children = 0;
	list_for_each_entry(t, &kern_task.link, link) {
		if (t->parent != cur)
			continue;

		have_children = 1;

		int match = 0;
		if (type == P_ALL) {
			/* wait for any child process */
			match = 1;
		} else if (type == P_PID) {
			/* wait for a specific child process */
			if (task_pid(t) == static_cast<pid_t>(id))
				match = 1;
		} else if (type == P_PGID) {
			/* wait for a process with matching pgid */
			if (t->pgid == cur->pgid)
				match = 1;
		} else {
			err = DERR(-EINVAL);
			break;
		}
		if (!match)
			continue;
		if (options & WSTOPPED && t->state == PS_STOP) {
			cpid = task_pid(t);
			status = t->exitcode;
			code = CLD_STOPPED;
			break;
		}
		if (options & WEXITED && t->state == PS_ZOMB) {
			cpid = task_pid(t);
			status = t->exitcode;
			code = CLD_EXITED;
			if (!(options & WNOWAIT))
				proc_reap_zombie(t);
			break;
		}
	}

	siginfo_t info = {};
	if (!have_children)
		err = -ECHILD;
	else if (cpid) {
		err = 0;
		info.si_pid = cpid;
		info.si_code = code;
		info.si_status = status;
		info.si_signo = SIGCHLD;
	} else if (options & WNOHANG) {
		/* No child exited, but caller has asked us not to block */
		err = 0;
		/* si_signo and si_pid are already set to 0 */
	} else {
		/* Wait for a signal or child exit */
		if (!(err = sch_prepare_sleep(&task_cur()->child_event, 0))) {
			sch_unlock();
			if (!(err = sch_continue_sleep()))
				goto again;
			sch_lock();
		}
	}

	sch_unlock();
	if (err == 0) {
		if (auto r = vm_write(task_cur()->as, &info, uinfop,
				      sizeof *uinfop);
		    !r.ok())
			err = r.sc_rval();
	}

	return err;
}

/*
 * sc_tkill - send a signal to a thread
 */
int
sc_tkill(int tid, int sig)
{
	thread *th;
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
	thread *th;
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
	task *t;

	if ((pid < 0) || (pgid < 0))
		return DERR(-EINVAL);

	sch_lock();
	if (!(t = task_find(pid))) {
		sch_unlock();
		return DERR(-ESRCH);
	}

	pid = task_pid(t);

	if (pgid == 0)
		pgid = pid;
	else {
		if (!task_find(pgid)) {
			sch_unlock();
			return DERR(-ESRCH);
		}
	}

	t->pgid = pgid;

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
	task *t;
	pid_t pgid;

	if (pid < 0)
		return DERR(-EINVAL);

	sch_lock();
	if (!(t = task_find(pid))) {
		sch_unlock();
		return DERR(-ESRCH);
	}

	pgid = t->pgid;
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
	task *t = task_cur();
	pid_t pid = task_pid(t);

	/*
	 * setsid fails if pid is already a process group leader
	 */
	if (t->pgid == pid)
		return DERR(-EPERM);

	t->pgid = pid;
	t->sid = pid;
	return pid;
}

/*
 * getsid - get the session ID of a process.
 */
pid_t
getsid(pid_t pid)
{
	task *t;
	pid_t sid;

	if (pid < 0)
		return DERR(-EINVAL);

	sch_lock();
	if (!(t = task_find(pid))) {
		sch_unlock();
		return DERR(-ESRCH);
	}

	sid = t->sid;
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
	task *t, *cur = task_cur();

	sch_lock();
	if (pid > 0) {
		if ((pid != task_pid(cur)) && !task_capable(CAP_KILL))
			err = DERR(-EPERM);
		else {
			if (!(t = task_find(pid)))
				err = DERR(-ESRCH);
			else
				err = sig_task(t, sig);
		}
	} else if (pid == -1) {
		if (!task_capable(CAP_KILL))
			err = DERR(-EPERM);
		else {
			list_for_each_entry(t, &kern_task.link, link) {
				if ((task_pid(t) > 1) &&
				    ((err = sig_task(t, sig)) != 0))
					break;
			}
		}
	} else if (pid == 0) {
		list_for_each_entry(t, &kern_task.link, link) {
			if ((t->pgid == cur->pgid) &&
			    ((err = sig_task(t, sig)) != 0))
				break;
		}
	} else {
		if ((cur->pgid != -pid) && !task_capable(CAP_KILL))
			err = DERR(-EPERM);
		else {
			list_for_each_entry(t, &kern_task.link, link) {
				if ((t->pgid == -pid) &&
				    ((err = sig_task(t, sig)) != 0))
					break;
			}
		}
	}
	sch_unlock();
	return err;
}
