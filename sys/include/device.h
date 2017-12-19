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

#ifndef _DEVICE_H
#define _DEVICE_H

#include <sys/cdefs.h>

/*
 * Device I/O table
 */
struct devio {
	int	(*open)	(device_t, int);
	int	(*close)(device_t);
	int	(*read)	(device_t, char *, size_t *, int);
	int	(*write)(device_t, char *, size_t *, int);
	int	(*ioctl)(device_t, u_long, void *);
	int	(*event)(int);
};

/*
 * Device structure
 */
struct device {
	int		magic;		/* magic number */
	int		refcnt;		/* reference count */
	int		flags;		/* device characteristics */
	struct list	link;		/* linkage on device list */
	struct devio	*devio;		/* device i/o table */
	char		name[MAXDEVNAME]; /* name of device */
};

#define device_valid(dev) (kern_area(dev) && ((dev)->magic == DEVICE_MAGIC))

__BEGIN_DECLS
device_t device_create(struct devio *, const char *, int);
int	 device_destroy(device_t);
int	 device_broadcast(int, int);
int	 device_open(const char *, int, device_t *);
int	 device_close(device_t);
int	 device_read(device_t, void *, size_t *, int);
int	 device_write(device_t, void *, size_t *, int);
int	 device_ioctl(device_t, u_long, void *);
int	 device_info(struct info_device *);
void	 device_init(void);
__BEGIN_DECLS

#endif /* !_DEVICE_H */
