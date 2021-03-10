#include "init.h"

#include <address.h>
#include <bootargs.h>
#include <cassert>
#include <cstring>
#include <debug.h>
#include <device.h>
#include <errno.h>
#include <fs.h>
#include <fs/file.h>
#include <fs/util.h>
#include <kmem.h>

#define rdbg(...)

static const char *archive_addr;
static size_t archive_size;

static ssize_t
bootdisk_read(void *buf, size_t len, off_t offset)
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
bootdisk_read_iov(file *f, const iovec *iov, size_t count, off_t offset)
{
	return for_each_iov(iov, count, offset,
	    [](std::span<std::byte> buf, off_t offset) {
		return bootdisk_read(data(buf), size(buf), offset);
	});
}

/*
 * Device I/O table
 */
static devio io = {
	.read = bootdisk_read_iov,
};

/*
 * Initialize
 */
void
bootdisk_init(bootargs *args)
{
	if (!args->archive_size)
		return;

	archive_addr = (char *)phys_to_virt(phys{args->archive_addr});
	archive_size = args->archive_size;

	dbg("Bootdisk at %p (%uK bytes)\n", archive_addr, archive_size / 1024);

	device *d = device_create(&io, "bootdisk0", DF_BLK, nullptr);
	assert(d);
}
