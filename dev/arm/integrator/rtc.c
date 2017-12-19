/*-
 * Copyright (c) 2005, Kohsuke Ohtani
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
 * rtc.c - Real time clock driver
 */

#include <sys/time.h>
#include <sys/ioctl.h>

#include <driver.h>

#ifdef CONFIG_MMU
#define RTC_BASE	(0xc0000000 + 0x15000000)
#else
#define RTC_BASE	(0x15000000)
#endif

#define RTC_DR		(*(volatile uint32_t *)(RTC_BASE + 0x00))
#define RTC_MR		(*(volatile uint32_t *)(RTC_BASE + 0x04))
#define RTC_STAT	(*(volatile uint32_t *)(RTC_BASE + 0x08))
#define RTC_EOI		(*(volatile uint32_t *)(RTC_BASE + 0x08))
#define RTC_LR		(*(volatile uint32_t *)(RTC_BASE + 0x0c))
#define RTC_CR		(*(volatile uint32_t *)(RTC_BASE + 0x10))

static int rtc_read(device_t dev, char *buf, size_t *nbyte, int blkno);
static int rtc_ioctl(device_t dev, u_long cmd, void *arg);
static int rtc_init(void);

/*
 * Driver structure
 */
struct driver rtc_drv = {
	/* name */	"Realtime Clock",
	/* order */	4,
	/* init */	rtc_init,
};

/*
 * Device I/O table
 */
static struct devio rtc_io = {
	/* open */	NULL,
	/* close */	NULL,
	/* read */	rtc_read,
	/* write */	NULL,
	/* ioctl */	rtc_ioctl,
	/* event */	NULL,
};

static device_t rtc_dev;	/* Device object */
static u_long boot_sec;		/* Time (sec) at system boot */
static u_long boot_ticks;	/* Time (ticks) at system boot */

static int
rtc_read(device_t dev, char *buf, size_t *nbyte, int blkno)
{
	u_long time;

	if (*nbyte < sizeof(u_long))
		return 0;

	time = RTC_DR;
	if (umem_copyout(&time, buf, sizeof(u_long)))
		return EFAULT;
	*nbyte = sizeof(u_long);
	return 0;
}

static int
rtc_ioctl(device_t dev, u_long cmd, void *arg)
{
	struct timeval tv;
	int err = 0;
	u_long msec;

	switch (cmd) {
	case RTCIOC_GET_TIME:
		/*
		 * Calculate current time (sec/usec) from
		 * boot time and current tick count.
		 */
		msec = tick_to_msec(timer_count() - boot_ticks);
		tv.tv_sec = (long)(boot_sec + (msec / 1000));
		tv.tv_usec = (long)((msec * 1000) % 1000000);
		if (umem_copyout(&tv, arg, sizeof(tv)))
			return EFAULT;
		break;
	case RTCIOC_SET_TIME:
		err = EINVAL;
		break;
	default:
		return EINVAL;
	}
	return err;
}

/*
 * Initialize
 */
static int
rtc_init(void)
{

	/* Create device object */
	rtc_dev = device_create(&rtc_io, "rtc", DF_CHR);
	ASSERT(rtc_dev);
	boot_sec = RTC_DR;
	boot_ticks = timer_count();
	return 0;
}
