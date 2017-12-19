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
 * system.c - system services
 */

#include <kernel.h>
#include <thread.h>
#include <sched.h>
#include <task.h>
#include <irq.h>
#include <page.h>
#include <device.h>
#include <system.h>
#include <version.h>

/*
 * kernel information.
 */
static const struct info_kernel infokern = {
	"Prex",	VERSION, __DATE__, MACHINE, "preky"
};

/*
 * Logging system call.
 *
 * Write a message to the logging device.  The log function is
 * available only when kernel is built with debug option.
 */
int
sys_log(const char *str)
{
#ifdef DEBUG
	char buf[DBGMSG_SIZE];
	size_t len;

	if (umem_strnlen(str, DBGMSG_SIZE, &len))
		return EFAULT;
	if (len >= DBGMSG_SIZE)
		return EINVAL;
	if (umem_copyin(str, buf, len + 1))
		return EFAULT;
	printf(buf);
	return 0;
#else
	return ENOSYS;
#endif
}

/*
 * Panic system call.
 *
 * If kernel is built with debug option, sys_panic() displays
 * a panic message and stops the enture system. Otherwise, it
 * terminates the task which called sys_panic().
 */
int
sys_panic(const char *str)
{
#ifdef DEBUG
	task_t self = cur_task();

	irq_lock();
	printf("\nUser mode panic: task:%s thread:%x\n",
	       self->name != NULL ? self->name : "no name", cur_thread);

	sys_log(str);
	printf("\n");

	sched_lock();
	irq_unlock();

	for (;;);
#else
	task_terminate(cur_task());
#endif
	/* NOTREACHED */
	return 0;
}

/*
 * Get system information
 */
int
sys_info(int type, void *buf)
{
	struct info_memory infomem;
	struct info_timer infotmr;
	struct info_thread infothr;
	struct info_device infodev;
	int err = 0;

	if (buf == NULL || !user_area(buf))
		return EFAULT;

	sched_lock();

	switch (type) {
	case INFO_KERNEL:
		err = umem_copyout(&infokern, buf, sizeof(infokern));
		break;

	case INFO_MEMORY:
		page_info(&infomem);
		err = umem_copyout(&infomem, buf, sizeof(infomem));
		break;

	case INFO_THREAD:
		if (umem_copyin(buf, &infothr, sizeof(infothr))) {
			err = EFAULT;
			break;
		}
		if ((err = thread_info(&infothr)))
			break;
		infothr.cookie++;
		err = umem_copyout(&infothr, buf, sizeof(infothr));
		break;

	case INFO_DEVICE:
		if (umem_copyin(buf, &infodev, sizeof(infodev))) {
			err = EFAULT;
			break;
		}
		if ((err = device_info(&infodev)))
			break;
		infodev.cookie++;
		err = umem_copyout(&infodev, buf, sizeof(infodev));
		break;

	case INFO_TIMER:
		timer_info(&infotmr);
		err = umem_copyout(&infotmr, buf, sizeof(infotmr));
		break;

	default:
		err = EINVAL;
		break;
	}
	sched_unlock();
	return err;
}

/*
 * Get system time - return ticks since OS boot.
 */
int
sys_time(u_long *ticks)
{
	u_long t;

	t = timer_count();
	return umem_copyout(&t, ticks, sizeof(t));
}

/*
 * Kernel debug service.
 */
int
sys_debug(int cmd, void *data)
{
#ifdef DEBUG
	int err = EINVAL;
	size_t size;
	int item;

	switch (cmd) {
	case DCMD_DUMP:
		if (umem_copyin(data, &item, sizeof(item)))
			err = EFAULT;
		else
			err = debug_dump(item);
		break;
	case DCMD_LOGSIZE:
		size = LOGBUF_SIZE;
		err = umem_copyout(&size, data, sizeof(size));
		break;
	case DCMD_GETLOG:
		err = debug_getlog(data);
		break;
	default:
		/* DO NOTHING */
		break;
	}
	return err;
#else
	return ENOSYS;
#endif
}
