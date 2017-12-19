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
#include <cpufunc.h>

/* Cmos */
#define CMOS_INDEX	0x70
#define CMOS_DATA	0x71

/* CMOS address */
#define CMOS_SEC	0x00
#define CMOS_MIN	0x02
#define CMOS_HOUR	0x04
#define CMOS_DAY	0x07
#define CMOS_MON	0x08
#define CMOS_YEAR	0x09
#define CMOS_STS_A	0x0a
#define CMOS_UIP	  0x80
#define CMOS_STS_B	0x0b
#define CMOS_BCD	  0x04


#define DAYSPERYEAR	(31+28+31+30+31+30+31+31+30+31+30+31)

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

static const int daysinmonth[] =
    { 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };

static device_t rtc_dev;	/* Device object */
static u_long boot_sec;		/* Time (sec) at system boot */
static u_long boot_ticks;	/* Time (ticks) at system boot */


static u_int
cmos_read(int index)
{
	u_int value;

	irq_lock();
	outb(index, CMOS_INDEX);
	value = inb(CMOS_DATA);
	irq_unlock();
	return value;
}

/*
static void
cmos_write(int index, int value)
{
    irq_lock();
    outb(index, CMOS_INDEX);
    outb(value, CMOS_DATA);
    irq_unlock();
}
*/

static u_int
bcd2bin(u_int bcd)
{

	return (bcd & 0x0f) + ((bcd >> 4) & 0xf) * 10;
}

/*
static int bin2bcd(int bin)
{
    return ((bin / 10) << 4) + (bin % 10);
}
*/

static int
is_leap(u_int year)
{

	if ((year % 4 == 0) && (year % 100 != 0))
		return 1;
	if (year % 400 == 0)
		return 1;
	return 0;
}

/*
 * Return current seconds (seconds since Epoch 1970/1/1 0:0:0)
 */
static u_long
cmos_gettime(void)
{
	u_int sec, min, hour, day, mon, year;
	u_int i;
	u_int days;

	/* Wait until data ready */
	for (i = 0; i < 1000000; i++)
		if (!(cmos_read(CMOS_STS_A) & CMOS_UIP))
			break;

	sec = cmos_read(CMOS_SEC);
	min = cmos_read(CMOS_MIN);
	hour = cmos_read(CMOS_HOUR);
	day = cmos_read(CMOS_DAY);
	mon = cmos_read(CMOS_MON);
	year = cmos_read(CMOS_YEAR);

	if (!(cmos_read(CMOS_STS_B) & CMOS_BCD)) {
		sec = bcd2bin(sec);
		min = bcd2bin(min);
		hour = bcd2bin(hour);
		day = bcd2bin(day);
		mon = bcd2bin(mon);
		year = bcd2bin(year);
	}
	if (year < 80)
		year += 2000;
	else
		year += 1900;
#ifdef DEBUG
	printf("rtc: system time was %d/%d/%d %d:%d:%d\n",
		year, mon, day, hour, min, sec);
#endif

	days = 0;
	for (i = 1970; i < year; i++)
		days += DAYSPERYEAR + is_leap(i);
	for (i = 1; i < mon; i++)
		days += daysinmonth[i - 1];
	if ((mon > 2) && is_leap(year))
		days++;
	days += day - 1;

	sec = (((days * 24 + hour) * 60) + min) * 60 + sec;
	return sec;
}

static int
rtc_read(device_t dev, char *buf, size_t *nbyte, int blkno)
{
	u_long time;

	if (*nbyte < sizeof(u_long))
		return 0;

	time = cmos_gettime();
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
	boot_sec = cmos_gettime();
	boot_ticks = timer_count();
	return 0;
}
