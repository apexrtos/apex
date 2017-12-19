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
#include <prex/signal.h>

#include <unistd.h>
#include <signal.h>
#include <stdlib.h>

void __exception_init(void);
void __exception_exit(void);

struct sigaction __sig_act[NSIG];
sigset_t __sig_mask;
sigset_t __sig_pending;

#ifdef _REENTRANT
volatile mutex_t __sig_lock;
#endif

/*
 * Process all pending and unmasked signal
 *
 * return 0 if at least one pending signal was processed.
 * return -1 if no signal was processed.
 */
int
__sig_flush(void)
{
	int sig;
	sigset_t active, org_mask;
	struct sigaction action;
	struct siginfo si;
	int rc = -1;

	sig = 1;
	for (;;) {
		SIGNAL_LOCK();
		active = __sig_pending & ~__sig_mask;
		action = __sig_act[sig];
		SIGNAL_UNLOCK();

		if (active == 0)
			break;
		if (active & sigmask(sig)) {

			SIGNAL_LOCK();
			org_mask = __sig_mask;
			__sig_mask |= action.sa_mask;
			SIGNAL_UNLOCK();

			if (action.sa_handler == SIG_DFL) {
				/* Default */
				switch (sig) {
				case SIGCHLD:
					/* XXX: */
					break;
				default:
					exit(0);
				}
			} else if (action.sa_handler != SIG_IGN) {
				/* User defined */
				if (action.sa_flags & SA_SIGINFO) {
					si.si_signo = sig;
					si.si_code = 0;
					si.si_value.sival_int = 0;
					action.sa_sigaction(sig, &si, NULL);
				} else {
					action.sa_handler(sig);
				}
			}
			SIGNAL_LOCK();
			__sig_pending &= ~sigmask(sig);
			__sig_mask = org_mask;
			SIGNAL_UNLOCK();

			switch (sig) {
			case SIGILL:
			case SIGTRAP:
			case SIGFPE:
			case SIGSEGV:
				for (;;);	/* exception from kernel */
				break;
			}
			if (action.sa_handler != SIG_IGN)
				rc = 0;	/* Signal is processed */
		}

		if (++sig >= NSIG)
			sig = 1;
	}
	return rc;
}

/*
 * Exception handler for signal emulation
 */
static void
__exception_handler(int excpt)
{

	if (excpt > 0 && excpt <= NSIG) {

		SIGNAL_LOCK();

		if (__sig_act[excpt].sa_handler != SIG_IGN)
			__sig_pending |= sigmask(excpt);

		SIGNAL_UNLOCK();
	}
	__sig_flush();
	exception_return();
}

/*
 * Initialize exception
 */
void
__exception_init(void)
{
	int i;

#ifdef _REENTRANT
	mutex_init(&__sig_lock);
#endif
	exception_setup(NULL);

	__sig_mask = 0;
	__sig_pending = 0;

	for (i = 0; i < NSIG; i++) {
		__sig_act[i].sa_flags = 0;
		__sig_act[i].sa_handler = SIG_DFL;
	}

	/* Install exception handler */
	exception_setup(__exception_handler);
}

/*
 * Clean up
 */
void
__exception_exit(void)
{

#ifdef _REENTRANT
	mutex_destroy(&__sig_lock);
#endif
	exception_setup(NULL);
}
