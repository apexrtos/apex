/*
 * console.c - console driver
 */

#include "console.h"

#include <debug.h>
#include <device.h>
#include <fcntl.h>
#include <fs.h>
#include <ioctl.h>
#include <sch.h>
#include <stddef.h>
#include <termios.h>

static int fd;

/*
 * read
 */
static ssize_t console_read(struct file *file, const struct iovec *iov,
    size_t count)
{
	return kpreadv(fd, iov, count, -1);
}

/*
 * write
 */
static ssize_t console_write(struct file *file, const struct iovec *iov,
    size_t count)
{
	return kpwritev(fd, iov, count, -1);
}

/*
 * DPC for syslog output
 */
static void
console_output(void *unused)
{
	int len;
	char buf[256];
	while ((len = syslog_format(buf, sizeof buf)) > 0)
		kpwrite(fd, buf, len, -1);
}

/*
 * Start syslog output on console
 *
 * Must be interrupt safe.
 */
static void
console_start(void)
{
	static struct dpc dpc;
	sch_dpc(&dpc, console_output, NULL);
}

/*
 * Initialise
 */
int
console_init(void)
{
	int err;

	static struct devio io = {
		.read = console_read,
		.write = console_write,
	};

	/* TODO: configurable console device */

	fd = kopen("/dev/ttyS0", O_RDWR);
	if (fd < 0)
		return fd;

	/* Configure console baud rate, etc. */
	struct termios tio;
	if ((err = kioctl(fd, TCGETS, &tio)) < 0)
		return err;
	tio.c_cflag = CONFIG_CONSOLE_CFLAG;
	if ((err = kioctl(fd, TCSETS, &tio)) < 0)
		return err;

	device_create(&io, "console", DF_CHR, NULL);
	syslog_output(console_start);
	console_start();

	return 0;
}
