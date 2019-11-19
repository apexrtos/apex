/*
 * console.c - console driver
 */

#include "console.h"

#include <arch.h>
#include <debug.h>
#include <device.h>
#include <fcntl.h>
#include <fs.h>
#include <ioctl.h>
#include <sch.h>
#include <stddef.h>
#include <sync.h>
#include <termios.h>
#include <thread.h>

static int fd;
static struct semaphore sem;

/*
 * read
 */
static ssize_t console_read(struct file *file, const struct iovec *iov,
    size_t count, off_t offset)
{
	return kpreadv(fd, iov, count, offset);
}

/*
 * write
 */
static ssize_t console_write(struct file *file, const struct iovec *iov,
    size_t count, off_t offset)
{
	return kpwritev(fd, iov, count, offset);
}

/*
 * ioctl
 */
int console_ioctl(struct file *file, unsigned long cmd, void *data)
{
	return kioctl(fd, cmd, data);
}

/*
 * Console writer thread
 */
static void
console_thread(void *unused)
{
	int len;
	char buf[256];
	while (true) {
		semaphore_wait_interruptible(&sem);
		while ((len = syslog_format(buf, sizeof buf)) > 0)
			kpwrite(fd, buf, len, -1);
	}
}

/*
 * Start syslog output on console
 *
 * Must be interrupt safe.
 */
static void
console_start(void)
{
	semaphore_post(&sem);
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
		.ioctl = console_ioctl,
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

	semaphore_init(&sem);
	syslog_output(console_start);
	console_start();

	if (!kthread_create(&console_thread, NULL, PRI_SIGNAL, "console",
	    MA_NORMAL))
		panic("console_init");

	device_create(&io, "console", DF_CHR, NULL);

	return 0;
}
