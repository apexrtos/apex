/*
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

#ifndef _PREX_H
#define _PREX_H
#ifndef KERNEL

#include <conf/config.h>

#include <sys/cdefs.h>
#include <sys/types.h>
#include <sys/param.h>
#include <prex/sysinfo.h>
#include <prex/capability.h>

/*
 * vm_option for task_crate()
 */
#define VM_NEW		0
#define VM_SHARE	1
#define VM_COPY		2

/*
 * attr flags for vm_attribute()
 */
#define VMA_READ	0x01
#define VMA_WRITE	0x02
#define VMA_EXEC	0x04

/*
 * Device open mode for device_open()
 */
#define DO_RDONLY	0x0
#define DO_WRONLY	0x1
#define DO_RDWR		0x2
#define DO_RWMASK	0x3

/*
 * Scheduling policy
 */
#define SCHED_FIFO	0	/* First In First Out */
#define SCHED_RR	1	/* Round Robin */
#define SCHED_OTHER	2	/* Other */

/*
 * Synch initializer
 */
#define MUTEX_INITIALIZER	(mutex_t)0x4d496e69
#define COND_INITIALIZER	(cond_t)0x43496e69

/*
 * System debug service
 */
#define DCMD_DUMP	0	/* kernel dump */
#define DCMD_LOGSIZE	1	/* return log size */
#define DCMD_GETLOG	2	/* return message log */

/*
 * Parameter for DCMD_DUMP
 */
#define DUMP_THREAD	1
#define DUMP_TASK	2
#define DUMP_VM		3

__BEGIN_DECLS
int	object_create(const char *name, object_t *obj);
int	object_destroy(object_t obj);
int	object_lookup(const char *name, object_t *obj);

int	msg_send(object_t obj, void *msg, size_t size);
int	msg_receive(object_t obj, void *msg, size_t size);
int	msg_reply(object_t obj, void *msg, size_t size);

int	vm_allocate(task_t task, void **addr, size_t size, int anywhere);
int	vm_free(task_t task, void *addr);
int	vm_attribute(task_t task, void *addr, int attr);
int	vm_map(task_t target, void  *addr, size_t size, void **alloc);

int	task_create(task_t parent, int vm_option, task_t *child);
int	task_terminate(task_t task);
task_t	task_self(void);
int	task_suspend(task_t task);
int	task_resume(task_t task);
int	task_name(task_t task, const char *name);
int	task_getcap(task_t task, cap_t *cap);
int	task_setcap(task_t task, cap_t *cap);

int	thread_create(task_t task, thread_t *th);
int	thread_terminate(thread_t th);
int	thread_load(thread_t th, void (*entry)(void), void *stack);
thread_t thread_self(void);
void	thread_yield(void);
int	thread_suspend(thread_t th);
int	thread_resume(thread_t th);
int	thread_getprio(thread_t th, int *prio);
int	thread_setprio(thread_t th, int	prio);
int	thread_getpolicy(thread_t th, int *policy);
int	thread_setpolicy(thread_t th, int policy);

int	timer_sleep(u_long msec, u_long *remain);
int	timer_alarm(u_long msec, u_long *remain);
int	timer_periodic(thread_t th, u_long start, u_long period);
int	timer_waitperiod(void);

int	exception_setup(void (*handler)(int));
int	exception_return(void);
int	exception_raise(task_t task, int excpt);
int	exception_wait(int *excpt);

int	device_open(const char *name, int mode, device_t *dev);
int	device_close(device_t dev);
int	device_read(device_t dev, void *buf, size_t *nbyte, int blkno);
int	device_write(device_t dev, void *buf, size_t *nbyte, int blkno);
int	device_ioctl(device_t dev, u_long cmd, void *arg);

int	mutex_init(mutex_t *mu);
int	mutex_destroy(mutex_t *mu);
int	mutex_trylock(mutex_t *mu);
int	mutex_lock(mutex_t *mu);
int	mutex_unlock(mutex_t *mu);

int	cond_init(cond_t *cond);
int	cond_destroy(cond_t *cond);
int	cond_wait(cond_t *cond, mutex_t *mu);
int	cond_signal(cond_t *cond);
int	cond_broadcast(cond_t *cond);

int	sem_init(sem_t *sem, u_int value);
int	sem_destroy(sem_t *sem);
int	sem_wait(sem_t *sem, u_long timeout);
int	sem_trywait(sem_t *sem);
int	sem_post(sem_t *sem);
int	sem_getvalue(sem_t *sem, u_int *value);

int	sys_info(int type, void *buf);
int	sys_log(const char *msg);
void	sys_panic(const char *msg);
int	sys_time(u_long *ticks);
int	sys_debug(int cmd, void *data);
void	sys_panic(const char *msg);

void	panic(const char *fmt, ...);
void	dprintf(const char *fmt, ...);
__END_DECLS

#endif /* KERNEL */
#endif /* !_PREX_H */
