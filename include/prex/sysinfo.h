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

#ifndef _PREX_SYSINFO_H
#define _PREX_SYSINFO_H

/*
 * Data type for sys_info()
 */
#define INFO_KERNEL	1
#define INFO_MEMORY	2
#define INFO_THREAD	3
#define INFO_DEVICE	4
#define INFO_TIMER	5

#define _KSTRLN		16

struct info_kernel {
	char	sysname[_KSTRLN];	/* kernel name */
	char	version[_KSTRLN];	/* version level */
	char	blddate[_KSTRLN];	/* build date */
	char	machine[_KSTRLN];	/* architecture/platform */
	char	hostname[_KSTRLN];	/* host name */
};

/*
 * Memory information
 */
struct info_memory {
	size_t	total;		/* total memory size in bytes */
	size_t	free;		/* current free memory size in bytes */
	size_t	bootdisk;	/* total size of boot disk */
};

/*
 * Thread information
 */
struct info_thread {
	u_long	cookie;		/* index cookie - 0 for first thread */
	int	policy;		/* scheduling policy */
	int	prio;		/* current priority */
	u_int	time;		/* total running time */
	task_t	task;		/* task id */
	char	taskname[MAXTASKNAME];	/* task name */
	char	slpevt[MAXEVTNAME];	/* sleep event */
};

/*
 * Device information
 */
struct info_device {
	u_long	 cookie;	/* index cookie - 0 for first thread */
	device_t id;		/* device id */
	int	 flags;		/* device characteristics flags */
	char	 name[MAXDEVNAME]; /* device name */
};

/*
 * Device flags
 */
#define DF_CHR		0x00000001	/* character device */
#define DF_BLK		0x00000002	/* block device */
#define DF_RDONLY	0x00000004	/* read only device */
#define DF_REM		0x00000008	/* removable device */

/*
 * Timer informations
 */
struct info_timer {
	int	hz;		/* clock frequency */
};

#endif /* !_PREX_SYSINFO_H */
