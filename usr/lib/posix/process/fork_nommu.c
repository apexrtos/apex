/*
 * Copyright (c) 2006, Kohsuke Ohtani
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

#include <prex/prex.h>
#include <prex/posix.h>
#include <prex/signal.h>
#include <server/proc.h>
#include <server/fs.h>
#include <server/stdmsg.h>

#include <stddef.h>
#include <setjmp.h>
#include <errno.h>

static void __parent_entry(void);
static void __child_entry(void);

static jmp_buf __fork_env;
static pid_t __child_pid;
static thread_t __parent_th;

/*
 * vfork() - vfork for No-MMU system.
 *
 * RETURN VALUE:
 *
 *  vfork() returns 0 to the child process and return the process
 *  ID of the child process to the parent process. Or, -1 will be
 *  returned to the parent process if error.
 *
 * ERRORS:
 *
 *  EAGAIN
 *  ENOMEM
 *
 * NOTE:
 *
 *  Since no thread is created by task_create(), thread_create()
 *  must be called follwing task_crate(). But, when new thread is
 *  created by thread_create(), the stack pointer of new thread is
 *  used at thread_create() although the stack image is copied at
 *  task_create(). So, the stack pointer must be reset to same
 *  address of thread_create() before calling task_create();
 *  This is a little tricky...
 *
 * The new process is an exact copy of the calling process
 * except as detailed below:
 * - Process IDs are different.
 * - tms_* is set to 0.
 * - Alarm clock is set to 0.
 * - Opend semaphore is inherited.
 * - Pending signals are cleared.
 *
 * - File lock is not inherited.
 * - File descriptor is same
 * - Directory stream is same
 */
pid_t
vfork(void)
{
	struct msg m;
	task_t tsk;
	thread_t th;
	int err, sts, prio;

	/* Save current stack pointer */
	sts = setjmp(__fork_env);
	if (sts == 0) {
		/*
		 * Create new task
		 */
		if ((err = task_create(task_self(), VM_SHARE, &tsk)) != 0) {
			errno = err;
			return -1;
		}
		if ((err = thread_create(tsk, &th)) != 0) {
			task_terminate(tsk);
			errno = err;
			return -1;
		}
		/*
		 * Notify to file system server
		 */
		m.hdr.code = FS_FORK;
		m.data[0] = tsk;		/* child task */
		if (__posix_call(__fs_obj, &m, sizeof(m), 1) != 0)
			return -1;

		/*
		 * Notify to process server
		 */
		m.hdr.code = PS_FORK;
		m.data[0] = tsk;		/* child task */
		m.data[1] = 1;			/* fork type */
		if (__posix_call(__proc_obj, &m, sizeof(m), 1) != 0)
			return -1;
		__child_pid = m.data[0];

		/*
		 * Start child task.
		 *
		 * TODO:
		 * In order to synchronize the execution, we change
		 * the child's priority to lower value. This ugly hack
		 * will be replaced by some other methods...
		 */
		thread_load(th, __child_entry, NULL);
		thread_getprio(th, &prio);
		thread_setprio(th, prio + 1);
		thread_resume(th);

		/*
		 * Suspend until child process calls exec() or exit()
		 */
		__parent_th = thread_self();
		task_suspend(task_self());

	} else if (sts == 1) {
		/*
		 * Child task
		 */
		thread_load(__parent_th, __parent_entry, NULL);

		th = thread_self();
		thread_getprio(th, &prio);
		thread_setprio(th, prio - 1);

#ifdef _REENTRANT
		err = mutex_init(&__sig_lock);
#endif
		__sig_pending = 0;
		return 0;
	}
	return __child_pid;
}

static void
__parent_entry(void)
{

	longjmp(__fork_env, 2);
	/* NOTREACHED */
}

static void
__child_entry(void)
{

	longjmp(__fork_env, 1);
	/* NOTREACHED */
}

/*
 * fork() is not supported on NOMMU system.
 */
pid_t
fork(void)
{

	return ENOSYS;
}
