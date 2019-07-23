#include "init.h"

#include <assert.h>
#include <bootargs.h>
#include <debug.h>
#include <device.h>
#include <errno.h>
#include <fs.h>
#include <fs/file.h>
#include <fs/util.h>
#include <kernel.h>
#include <kmem.h>
#include <string.h>

#define rdbg(...)

static const char *archive_addr;
static size_t archive_size;

static ssize_t
bootdisk_read(struct file *f, void *buf, size_t len, off_t offset)
{
	rdbg("bootdisk_read: buf=%p len=%zu off=%jx\n", buf, len, offset);

	/* Check overrun */
	if (offset > archive_size)
		return DERR(-EIO);
	if (archive_size - offset < len)
		len = archive_size - offset;

	/* Copy data */
	memcpy(buf, archive_addr + offset, len);

	return len;
}

static ssize_t
bootdisk_read_iov(struct file *f, const struct iovec *iov, size_t count,
    off_t offset)
{
	return for_each_iov(f, iov, count, offset, bootdisk_read);
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
