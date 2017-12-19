/*-
 * Copyright (c) 2005-2008, Kohsuke Ohtani
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
 * debug.c - kernel debug services
 */

#include <kernel.h>
#include <task.h>
#include <thread.h>
#include <vm.h>
#include <irq.h>

#ifdef DEBUG

/*
 * diag_print() is provided by architecture dependent layer.
 */
typedef void (*prtfn_t)(char *);
static prtfn_t	print_func = &diag_print;

static char	dbg_msg[DBGMSG_SIZE];

/*
 * dmesg support
 */
static char	log_buf[LOGBUF_SIZE];
static u_long	log_head;
static u_long	log_tail;
static u_long	log_len;

#define LOGINDEX(x)	((x) & (LOGBUF_SIZE - 1))

/*
 * Print out the specified string.
 *
 * An actual output is displayed via the platform
 * specific device by diag_print() routine in kernel.
 * As an alternate option, the device driver can
 * replace the print routine by using debug_attach().
 */
void
printf(const char *fmt, ...)
{
	va_list args;
	int i;
	char c;

	irq_lock();
	va_start(args, fmt);

	vsprintf(dbg_msg, fmt, args);

	/* Print out */
	(*print_func)(dbg_msg);

	/*
	 * Record to log buffer
	 */
	for (i = 0; i < DBGMSG_SIZE; i++) {
		c = dbg_msg[i];
		if (c == '\0')
			break;
		log_buf[LOGINDEX(log_tail)] = c;
		log_tail++;
		if (log_len < LOGBUF_SIZE)
			log_len++;
		else
			log_head = log_tail - LOGBUF_SIZE;
	}
	va_end(args);
	irq_unlock();
}

/*
 * Kernel assertion.
 *
 * assert() is called only when the expression is false in
 * ASSERT() macro. ASSERT() macro is compiled only when
 * the debug option is enabled.
 */
void
assert(const char *file, int line, const char *exp)
{

	irq_lock();
	printf("\nAssertion failed: %s line:%d '%s'\n",
	       file, line, exp);
	for (;;)
		machine_idle();
	/* NOTREACHED */
}

/*
 * Kernel panic.
 *
 * panic() is called for a fatal kernel error. It shows
 * specified message, and stops CPU.
 */
void
panic(const char *msg)
{

	irq_lock();
	printf("\nKernel panic: %s\n", msg);
	irq_unlock();
	for (;;)
		machine_idle();
	/* NOTREACHED */
}

/*
 * Copy log to user buffer.
 */
int
debug_getlog(char *buf)
{
	u_long i, len, index;
	int err = 0;
	char c;

	irq_lock();
	index = log_head;
	len = log_len;
	if (len >= LOGBUF_SIZE) {
		/*
		 * Overrun found. Discard broken message.
		 */
		while (len > 0 && log_buf[LOGINDEX(index)] != '\n') {
			index++;
			len--;
		}
	}
	for (i = 0; i < LOGBUF_SIZE; i++) {
		if (i < len)
			c = log_buf[LOGINDEX(index)];
		else
			c = '\0';
		if (umem_copyout(&c, buf, 1)) {
			err = EFAULT;
			break;
		}
		index++;
		buf++;
	}
	irq_unlock();
	return err;
}

/*
 * Dump system information.
 *
 * A keyboard driver may call this routine if a user
 * presses a predefined "dump" key.  Since interrupt is
 * locked in this routine, there is no need to lock the
 * interrupt/scheduler in each dump function.
 */
int
debug_dump(int item)
{
	int err = 0;

	irq_lock();
	switch (item) {
	case DUMP_THREAD:
		thread_dump();
		break;
	case DUMP_TASK:
		task_dump();
		break;
	case DUMP_VM:
		vm_dump();
		break;
	default:
		err = ENOSYS;
		break;
	}
	irq_unlock();
	return err;
}

/*
 * Attach to a print handler.
 * A device driver can hook the function to display message.
 */
void
debug_attach(void (*fn)(char *))
{
	ASSERT(fn);

	print_func = fn;
}
#endif /* !DEBUG */
