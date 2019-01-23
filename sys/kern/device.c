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
 * device.c - device I/O support routine
 */
#include <device.h>

#include <access.h>
#include <assert.h>
#include <compiler.h>
#include <debug.h>
#include <errno.h>
#include <kmem.h>
#include <sch.h>
#include <string.h>
#include <sys/uio.h>

#define DEVICE_MAGIC   0x4465763f      /* 'Dev?' */

static struct list device_list;		/* list of the device objects */

/*
 * Decrement the reference count on a device. If the
 * reference count becomes zero, we can release the
 * resource for the target device. Assumes the device
 * is already validated by caller.
 */
static void
device_release(struct device *dev)
{
	if (--dev->refcnt == 0) {
		dev->magic = 0;
		list_remove(&dev->link);
		kmem_free(dev);
	}
}

/*
 * device_valid - check device validity.
 */
bool
device_valid(struct device *dev)
{
	return k_address(dev) && dev->magic == DEVICE_MAGIC;
}

/*
 * Look up a device object by device name.
 * Return device ID on success, or NULL on failure.
 */
struct device *
device_lookup(const char *name)
{
	struct list *head, *n;
	struct device *dev;

	if (name == NULL)
		return NULL;

	sch_lock();
	head = &device_list;
	for (n = list_first(head); n != head; n = list_next(n)) {
		dev = list_entry(n, struct device, link);
		if (!strcmp(dev->name, name)) {
			++dev->refcnt;
			sch_unlock();
			return dev;
		}
	}
	sch_unlock();
	return NULL;
}

/*
 * device_create - create new device object.
 *
 * A device object is created by the device driver to provide
 * I/O services to applications.
 * Returns device ID on success, or 0 on failure.
 */
struct device *
device_create(const struct devio *io, const char *name, int flags, void *info)
{
	struct device *dev;
	size_t len;

	dbg("Create /dev/%s\n", name);

	len = strlen(name);
	if (len == 0 || len >= ARRAY_SIZE(dev->name))	/* Invalid name? */
		return 0;

	sch_lock();
	if ((dev = device_lookup(name)) != NULL) {
		/*
		 * Error - the device name is already used.
		 */
		sch_unlock();
		return 0;
	}
	if ((dev = kmem_alloc(sizeof(*dev), MEM_NORMAL)) == NULL) {
		sch_unlock();
		return 0;
	}
	strlcpy(dev->name, name, len + 1);
	dev->devio = io;
	dev->info = info;
	dev->flags = flags;
	dev->refcnt = 1;
	dev->magic = DEVICE_MAGIC;
	list_insert(&device_list, &dev->link);
	sch_unlock();
	return dev;
}

/*
 * Destroy a device object. If some other threads still
 * refer the target device, the destroy operating will be
 * pending until its reference count becomes 0.
 */
int
device_destroy(struct device *dev)
{
	int err = 0;

	sch_lock();
	if (device_valid(dev))
		device_release(dev);
	else
		err = -ENODEV;
	sch_unlock();
	return err;
}

/*
 * device_broadcast - broadcast an event to all device objects.
 *
 * If "force" argument is true, a kernel will continue event
 * notification even if some driver returns error. In this case,
 * this routine returns EIO error if at least one driver returns
 * an error.
 *
 * If force argument is false, a kernel stops the event processing
 * when at least one driver returns an error. In this case,
 * device_broadcast will return the error code which is returned
 * by the driver.
 */
int
device_broadcast(int event, int force)
{
	struct device *dev;
	struct list *head, *n;
	int err, ret = 0;

	sch_lock();
	head = &device_list;
	for (n = list_first(head); n != head; n = list_next(n)) {
		dev = list_entry(n, struct device, link);
		if (dev->devio->event != NULL) {
			/*
			 * Call driver's event routine.
			 */
			err = (*dev->devio->event)(event);
			if (err) {
				if (force)
					ret = EIO;
				else {
					ret = err;
					break;
				}
			}
		}
	}
	sch_unlock();
	return ret;
}

/*
 * Return device information (for devfs).
 */
int
device_info(u_long index, int *flags, char *name)
{
	int err = -ESRCH;
	struct device *dev;

	/* REVISIT(efficiency): this interface is horribly inefficient */

	sch_lock();
	list_for_each_entry(dev, &device_list, link) {
		if (index-- > 0)
			continue;
		*flags = dev->flags;
		strcpy(name, dev->name);
		err = 0;
	}
	sch_unlock();

	return err;
}

/*
 * Initialize device driver module.
 */
void
device_init(void)
{
	list_init(&device_list);
}

