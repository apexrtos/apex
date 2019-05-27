#include "init.h"

#include <assert.h>
#include <bootargs.h>
#include <debug.h>
#include <device.h>
#include <errno.h>
#include <fs.h>
#include <fs/file.h>
#include <fs/util.h>
#include <fs/vnode.h>
#include <kernel.h>
#include <kmem.h>
#include <string.h>

#define rdbg(...)

static const char *archive_addr;
static size_t archive_size;

static ssize_t
bootdisk_read(struct file *f, void *buf, size_t len)
{
	const size_t off = f->f_offset;

	rdbg("bootdisk_read: buf=%p len=%zu off=%jx\n", buf, len, off);

	/* Check overrun */
	if ((size_t)off > archive_size)
		return DERR(-EIO);
	if ((size_t)off + len > archive_size)
		len = archive_size - off;

	/* Copy data */
	memcpy(buf, archive_addr + off, len);

	vn_lock(f->f_vnode);
	f->f_offset += len;
	vn_unlock(f->f_vnode);

	return len;
}

static ssize_t
bootdisk_read_iov(struct file *f, const struct iovec *iov, size_t count)
{
	return for_each_iov(f, iov, count, bootdisk_read);
}

/*
 * Device I/O table
 */
static struct devio io = {
	.read = bootdisk_read_iov,
};

/*
 * Initialize
 */
void
bootdisk_init(struct bootargs *args)
{
	if (!args->archive_size)
		return;

	archive_addr = phys_to_virt(args->archive_addr);
	archive_size = args->archive_size;

	dbg("Bootdisk at %p (%uK bytes)\n", archive_addr, archive_size / 1024);

	struct device *d = device_create(&io, "bootdisk0", DF_BLK, NULL);
	assert(d);
}
