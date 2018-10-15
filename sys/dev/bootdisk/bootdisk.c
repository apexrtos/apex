#include "init.h"

#include <assert.h>
#include <bootinfo.h>
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

struct bootdisk_info {
	char *p;
	size_t len;
};

static int
bootdisk_open(struct file *f)
{
	const struct bootdisk_info *i = f->f_data;

	if (!i || !i->p || !i->len)
		return DERR(-EIO);

	return 0;
}

static ssize_t
bootdisk_read(struct file *f, void *buf, size_t len)
{
	const struct bootdisk_info *i = f->f_data;
	const size_t off = f->f_offset;

	rdbg("bootdisk_read: buf=%p len=%zu off=%jx\n", buf, len, off);

	/* Check overrun */
	if ((size_t)off > i->len)
		return DERR(-EIO);
	if ((size_t)off + len > i->len)
		len = i->len - off;

	/* Copy data */
	memcpy(buf, i->p + off, len);
	f->f_offset += len;
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
	.open = bootdisk_open,
	.read = bootdisk_read_iov,
};

/*
 * Initialize
 */
void
bootdisk_init(void)
{
	int n = 0;
	char name[] = "bootdiskX";

	for (size_t i = 0; i < bootinfo.nr_rams; ++i) {
		const struct boot_mem *ram = bootinfo.ram + i;
		if (ram->type != MT_BOOTDISK)
			continue;
		struct bootdisk_info *i = kmem_alloc(sizeof(*i), MEM_NORMAL);
		assert(i);
		*i = (struct bootdisk_info) {
			.p = phys_to_virt(ram->base),
			.len = ram->size,
		};

		dbg("Bootdisk %d at %p (%uK bytes)\n", n, i->p, i->len / 1024);

		name[8] = '0' + n;
		struct device *d = device_create(&io, name, DF_BLK, i);
		assert(d);

		++n;
	}
}
