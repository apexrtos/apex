/*
 * console.c - console driver
 */

#include "init.h"

#include <debug.h>
#include <device.h>
#include <fcntl.h>
#include <fs.h>
#include <sync.h>
#include <sys/ioctl.h>
#include <thread.h>

static int fd;
static semaphore sem;

/*
 * read
 */
static ssize_t console_read(file *file, const iovec *iov,
    size_t count, off_t offset)
{
	return kpreadv(fd, iov, count, offset);
}

/*
 * write
 */
static ssize_t console_write(file *file, const iovec *iov,
    size_t count, off_t offset)
{
	return kpwritev(fd, iov, count, offset);
}

/*
 * ioctl
 */
int console_ioctl(file *file, unsigned long cmd, void *data)
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
	char buf[2048];
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
console_start()
{
	semaphore_post(&sem);
}

/*
 * Initialise
 */
void
console_init(const char *dev, tcflag_t cflag)
{
	static devio io = {
		.read = console_read,
		.write = console_write,
		.ioctl = console_ioctl,
	};

	fd = kopen(dev, O_RDWR);
	if (fd < 0)
		panic("console_init");

	/* Configure console baud rate, etc. */
	termios tio;
	if (kioctl(fd, TCGETS, &tio))
		panic("console_init");
	tio.c_cflag = cflag;
	if (kioctl(fd, TCSETS, &tio) < 0)
		panic("console_init");

	semaphore_init(&sem);
	syslog_output(console_start);
	console_start();

	if (!kthread_create(&console_thread, nullptr, PRI_KERN_LOW, "console",
	    MA_NORMAL))
		panic("console_init");

	if (!device_create(&io, "console", DF_CHR, nullptr))
		panic("console_init");
}
