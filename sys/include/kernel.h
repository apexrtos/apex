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

#ifndef _KERNEL_H
#define _KERNEL_H

#include <sys/cdefs.h>
#include <sys/param.h>
#include <sys/list.h>
#include <sys/errno.h>
#include <prex/bootinfo.h>
#include <machine/stdarg.h>
#include <arch.h>
#include <platform.h>
#include <debug.h>

typedef struct object	*object_t;
typedef struct task	*task_t;
typedef struct thread	*thread_t;
typedef struct device	*device_t;
typedef struct mutex	*mutex_t;
typedef struct cond	*cond_t;
typedef struct sem	*sem_t;
typedef struct vm_map	*vm_map_t;
typedef struct irq	*irq_t;

typedef void (*sysfn_t)(void);
typedef void (*dkifn_t)(void);


#include <prex/sysinfo.h>

/*
 * Magic numbers
 */
#define OBJECT_MAGIC	0x4f626a3f	/* 'Obj?' */
#define TASK_MAGIC	0x54736b3f	/* 'Tsk?' */
#define THREAD_MAGIC	0x5468723f	/* 'Thr?' */
#define DEVICE_MAGIC	0x4465763f	/* 'Dev?' */
#define MUTEX_MAGIC	0x4d75783f	/* 'Mux?' */
#define COND_MAGIC	0x436f6e3f	/* 'Con?' */
#define SEM_MAGIC	0x53656d3f	/* 'Sem?' */

/*
 * Global variables in the kernel
 */
extern struct thread	*cur_thread;	/* pointer to the current thread */
extern struct task	kern_task;	/* kernel task */
extern struct bootinfo	*bootinfo;	/* pointer to boot information */
extern volatile int	irq_level;	/* current interrupt level */
extern const dkifn_t	driver_service[];
extern const sysfn_t	syscall_table[];
extern const u_int	nr_syscalls;


__BEGIN_DECLS
size_t	 strlcpy(char *, const char *, size_t);
int	 strncmp(const char *, const char *, size_t);
size_t	 strnlen(const char *, size_t);
void	*memcpy(void *, const void *, size_t);
void	*memset(void *, int, size_t);
int	 vsprintf(char *, const char *, va_list);
__BEGIN_DECLS

#endif /* !_KERNEL_H */
