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

#ifndef device_h
#define device_h

#include <list.h>
#include <stdbool.h>
#include <sys/types.h>

struct file;
struct iovec;


/*
 * Driver structure
 */
struct driver {
	const char     *name;		/* Name of device driver */
	int	      (*init)(void);	/* Initialisation routine */
};

/*
 * Device flags
 */
#define DF_CHR		0x00000001      /* character device */
#define DF_BLK		0x00000002      /* block device */
#define DF_RDONLY	0x00000004      /* read only device */
#define DF_REM		0x00000008      /* removable device */

/*
 * Device I/O table
 */
struct devio {
	int	(*open)(struct file *);
	int	(*close)(struct file *);
	ssize_t	(*read)(struct file *, const struct iovec *, size_t);
	ssize_t	(*write)(struct file *, const struct iovec *, size_t);
	int	(*ioctl)(struct file *, u_long, void *);
	int	(*event)(int);
};

/*
 * Device structure
 */
struct device {
	int		    magic;	/* magic number */
	int		    refcnt;	/* reference count */
	int		    flags;	/* device characteristics */
	struct list	    link;	/* linkage on device list */
	const struct devio *devio;	/* device i/o table */
	void		   *info;	/* device specific info */
	char		    name[12];	/* name of device */
};

#if defined(__cplusplus)
extern "C" {
#endif

bool		device_valid(struct device *);
struct device  *device_lookup(const char *name);
void		device_release(struct device *);
struct device  *device_create(const struct devio *, const char *, int, void *);
int		device_destroy(struct device *);
int		device_broadcast(int, int);
int		device_info(u_long, int *, char *);
void		device_init(void);

#if defined(__cplusplus)
}
#endif

#endif /* !device_h */
