/*
 * Copyright (c) 2007, Kohsuke Ohtani
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
 * tty.c - TTY signal support
 */

#include <prex/prex.h>
#include <server/proc.h>
#include <sys/list.h>

#include <unistd.h>
#include <errno.h>
#include <stdlib.h>
#include <signal.h>
#include <termios.h>

#include "proc.h"

static device_t ttydev;

/*
 * Send TTY signal.
 */
static void
tty_signal(int sig)
{
	pid_t pid;

	/*
	 * Get the process group that was active when
	 * the TTY signal was invoked.
	 */
	if (device_ioctl(ttydev, TIOCGPGRP, &pid) != 0)
		return;

	DPRINTF(("proc: tty_signal sig=%d\n", sig));
	kill_pg(pid, sig);
}

/*
 * Catch TTY related signals and forward them
 * to the appropriate processes.
 */
static void
exception_handler(int sig)
{

	/*
	 * Handle signals from tty input.
	 */
	switch (sig) {
	case SIGINT:
	case SIGQUIT:
	case SIGTSTP:
	case SIGTTIN:
	case SIGTTOU:
	case SIGINFO:
	case SIGWINCH:
	case SIGIO:
		if (ttydev != DEVICE_NULL)
			tty_signal(sig);
		break;
	}
	exception_return();
}

/*
 * Initialize TTY.
 *
 * Since we manage the process group only in the process
 * server, the TTY driver can not know anything about the
 * process group.  However, POSIX specification requires TTY
 * driver to send a signal to the specific process group.
 * So, we will catch all TTY related signals by this server
 * and forward them to the actual process or process group.
 */
void
tty_init(void)
{
	task_t self;

	/*
	 * Setup exception to receive signals from tty.
	 */
	exception_setup(exception_handler);

	if (device_open("tty", 0, &ttydev) != 0)
		ttydev = DEVICE_NULL;
	else {
		/*
		 * Notify the TTY driver to send all tty related
		 * signals in system to this task.
		 */
		self = task_self();
		device_ioctl(ttydev, TIOCSETSIGT, &self);
	}
}
