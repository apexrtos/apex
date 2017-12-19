/*
 * Copyright (c) 2005, Kohsuke Ohtani
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
#include <server/proc.h>
#include <server/stdmsg.h>
#include <sys/wait.h>

#include <stddef.h>
#include <errno.h>

/*
 * If the target child task calls exit() just after we
 * send PS_WAITPID message, a process server will send an
 * exception to us. But, we can not catch the exception
 * before calling exception_wait().
 */
pid_t
waitpid(pid_t pid, int *status, int options)
{
	struct msg m;
	pid_t child;
	int sig, err;
	thread_t self;
	int prio;

	/* Boost current priority */
	self = thread_self();
	thread_getprio(self, &prio);
	thread_setprio(self, prio - 1);

	for (;;) {
		m.hdr.code = PS_WAITPID;
		m.data[0] = pid;
		m.data[1] = options;
		err = msg_send(__proc_obj, &m, sizeof(m));
		if (err == EINTR)
			continue;

		if (m.hdr.status) {
			errno = m.hdr.status;
			return -1;
		}
		child = m.data[0];
		if (child != 0 || options & WNOHANG)
			break;

		err = exception_wait(&sig);
		if (err == EINTR) {
			errno = EINTR;
			break;
		}
	}
	/* Restore priority */
	thread_setprio(self, prio);

	if (status != NULL)
		*status = m.data[1];
	return child;
}
