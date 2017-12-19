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
 * syscalls.c - system call table
 */

#include <kernel.h>
#include <thread.h>
#include <timer.h>
#include <vm.h>
#include <task.h>
#include <exception.h>
#include <ipc.h>
#include <device.h>
#include <sync.h>
#include <system.h>

#define SYSENT(func)	(sysfn_t)(func)

const sysfn_t syscall_table[] = {
	/*  0 */ SYSENT(object_create),
	/*  1 */ SYSENT(object_destroy),
	/*  2 */ SYSENT(object_lookup),
	/*  3 */ SYSENT(msg_send),
	/*  4 */ SYSENT(msg_receive),
	/*  5 */ SYSENT(msg_reply),
	/*  6 */ SYSENT(vm_allocate),
	/*  7 */ SYSENT(vm_free),
	/*  8 */ SYSENT(vm_attribute),
	/*  9 */ SYSENT(vm_map),
	/* 10 */ SYSENT(task_create),
	/* 11 */ SYSENT(task_terminate),
	/* 12 */ SYSENT(task_self),
	/* 13 */ SYSENT(task_suspend),
	/* 14 */ SYSENT(task_resume),
	/* 15 */ SYSENT(task_name),
	/* 16 */ SYSENT(task_getcap),
	/* 17 */ SYSENT(task_setcap),
	/* 18 */ SYSENT(thread_create),
	/* 19 */ SYSENT(thread_terminate),
	/* 20 */ SYSENT(thread_load),
	/* 21 */ SYSENT(thread_self),
	/* 22 */ SYSENT(thread_yield),
	/* 23 */ SYSENT(thread_suspend),
	/* 24 */ SYSENT(thread_resume),
	/* 25 */ SYSENT(thread_schedparam),
	/* 26 */ SYSENT(timer_sleep),
	/* 27 */ SYSENT(timer_alarm),
	/* 28 */ SYSENT(timer_periodic),
	/* 29 */ SYSENT(timer_waitperiod),
	/* 30 */ SYSENT(exception_setup),
	/* 31 */ SYSENT(exception_return),
	/* 32 */ SYSENT(exception_raise),
	/* 33 */ SYSENT(exception_wait),
	/* 34 */ SYSENT(device_open),
	/* 35 */ SYSENT(device_close),
	/* 36 */ SYSENT(device_read),
	/* 37 */ SYSENT(device_write),
	/* 38 */ SYSENT(device_ioctl),
	/* 39 */ SYSENT(mutex_init),
	/* 40 */ SYSENT(mutex_destroy),
	/* 41 */ SYSENT(mutex_lock),
	/* 42 */ SYSENT(mutex_trylock),
	/* 43 */ SYSENT(mutex_unlock),
	/* 44 */ SYSENT(cond_init),
	/* 45 */ SYSENT(cond_destroy),
	/* 46 */ SYSENT(cond_wait),
	/* 47 */ SYSENT(cond_signal),
	/* 48 */ SYSENT(cond_broadcast),
	/* 49 */ SYSENT(sem_init),
	/* 50 */ SYSENT(sem_destroy),
	/* 51 */ SYSENT(sem_wait),
	/* 52 */ SYSENT(sem_trywait),
	/* 53 */ SYSENT(sem_post),
	/* 54 */ SYSENT(sem_getvalue),
	/* 55 */ SYSENT(sys_log),
	/* 56 */ SYSENT(sys_panic),
	/* 57 */ SYSENT(sys_info),
	/* 58 */ SYSENT(sys_time),
	/* 59 */ SYSENT(sys_debug),
};
const u_int nr_syscalls = sizeof(syscall_table) / sizeof(sysfn_t);
