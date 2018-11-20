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
#include <thread.h>
#include <stdatomic.h>

static int fd;
struct event evt;
atomic_int wakeups;

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
 * Console writer thread
 */
static void
console_thread(void *unused)
{
	int len;
	char buf[256];
	while (true) {
		if (!--wakeups)
			sch_sleep(&evt);
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
	++wakeups;
	sch_wakeup(&evt, 0);
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

	event_init(&evt, "console", ev_SLEEP);

	sch_lock();
	if (!kthread_create(&console_thread, NULL, PRI_BACKGROUND, "console",
	    MEM_NORMAL))
		panic("console_init");
	sch_unlock();

	device_create(&io, "console", DF_CHR, NULL);
	syslog_output(console_start);
	console_start();

	return 0;
}
