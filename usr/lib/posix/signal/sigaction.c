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

#include <prex/signal.h>

#include <stddef.h>
#include <signal.h>
#include <errno.h>

/*
 * If act is not NULL, set the action for signum.
 * If oact is not NULL. store the old action to oact.
 */
int
sigaction(int signum, const struct sigaction *act, struct sigaction *oact)
{
	struct sigaction *sa;

	if (signum <= 0 || signum >= NSIG ||
	    signum == SIGSTOP || signum == SIGKILL) {
		errno = EINVAL;
		return -1;
	}

	SIGNAL_LOCK();

	sa = &__sig_act[signum];

	if (oact != NULL)
		*oact = *sa;

	if (act != NULL)
		*sa = *act;

	/* Discard pending signal in some cases */
	if (sa->sa_handler == SIG_IGN ||
	    (sa->sa_handler == SIG_DFL && signum == SIGCHLD))
		__sig_pending &= ~sigmask(signum);

	SIGNAL_UNLOCK();

	/* Process pending signal */
	__sig_flush();

	return 0;
}
