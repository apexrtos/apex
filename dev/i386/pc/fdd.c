/*-
 * Copyright (c) 2005-2006, Kohsuke Ohtani
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
 * fdd.c - Floppy disk driver
 */

/*
 *  State transition table:
 *
 *    State     Interrupt Timeout   Error
 *    --------- --------- --------- ---------
 *    Off       N/A       On        N/A
 *    On        N/A       Reset     N/A
 *    Reset     Recal     Off       N/A
 *    Recal     Seek      Off       Off
 *    Seek      IO        Reset     Off
 *    IO        Ready     Reset     Off
 *    Ready     N/A       Off       N/A
 *
 */

#include <driver.h>
#include <cpufunc.h>

#include "dma.h"

/* #define DEBUG_FDD 1 */

#ifdef DEBUG_FDD
#define DPRINTF(a) printf a
#else
#define DPRINTF(a)
#endif

#define FDD_IRQ		6	/* IRQ6 */
#define FDD_DMA		2	/* DMA2 */

#define SECTOR_SIZE	512
#define TRACK_SIZE	(SECTOR_SIZE * 18)
#define INVALID_TRACK	-1

/* I/O ports */
#define FDC_DOR		0x3f2	/* Digital output register */
#define FDC_MSR		0x3f4	/* Main status register (in) */
#define FDC_DSR		0x3f4	/* Data rate select register (out) */
#define FDC_DAT		0x3f5	/* Data register */
#define FDC_DIR		0x3f7	/* Digital input register (in) */
#define FDC_CCR		0x3f7	/* Configuration control register (out) */

/* Command bytes */
#define CMD_SPECIFY	0x03	/* Specify drive timing */
#define CMD_DRVSTS	0x04
#define CMD_WRITE	0xc5	/* Sector write, multi-track */
#define CMD_READ	0xe6	/* Sector read */
#define CMD_RECAL	0x07	/* Recalibrate */
#define CMD_SENSE	0x08	/* Sense interrupt status */
#define CMD_FORMAT	0x4d	/* Format track */
#define CMD_SEEK	0x0f	/* Seek track */
#define CMD_VERSION	0x10	/* FDC version */

/* Floppy Drive Geometries */
#define FDG_HEADS	2
#define FDG_TRACKS	80
#define FDG_SECTORS	18
#define FDG_GAP3FMT	0x54
#define FDG_GAP3RW	0x1b

/* FDC state */
#define FDS_OFF		0	/* Motor off */
#define FDS_ON		1	/* Motor on */
#define FDS_RESET	2	/* Reset */
#define FDS_RECAL	3	/* Recalibrate */
#define FDS_SEEK	4	/* Seek */
#define FDS_IO		5	/* Read/write */
#define FDS_READY	6	/* Ready */

static void fdc_timeout(void *);
static void fdc_recal(void);
static void fdc_off(void);
static void fdc_io(void);

static int fdd_init(void);
static int fdd_open(device_t, int);
static int fdd_close(device_t);
static int fdd_read(device_t, char *, size_t *, int);
static int fdd_write(device_t, char *, size_t *, int);

/*
 * I/O request
 */
struct io_req {
	int	cmd;
	int	nr_retry;
	int	blkno;
	u_long	blksz;
	void	*buf;
	int	errno;
};
typedef struct io_req *ioreq_t;

/* I/O command */
#define IO_NONE		0
#define IO_READ		1
#define IO_WRITE	2
#define IO_FORMAT	3	/* not supported */
#define IO_CANCEL	4

/*
 * Driver structure
 */
struct driver fdd_drv = {
	/* name */	"Floppy Disk Controller",
	/* order */	5,
	/* init */	fdd_init,
};

/*
 * Device I/O table
 */
static struct devio fdd_io = {
	/* open */	fdd_open,
	/* close */	fdd_close,
	/* read */	fdd_read,
	/* write */	fdd_write,
	/* ioctl */	NULL,
	/* event */	NULL,
};

static device_t fdd_dev;	/* Device object */
static irq_t fdd_irq;		/* Interrupt handle */
static int fdd_dma;		/* DMA handle */
static int nr_open;		/* Open count */
static struct timer fdd_tmr;	/* Timer */

static int fdc_stat;		/* Current state */
static struct io_req ioreq;	/* I/O request */
static void *read_buf;		/* DMA buffer for read (1 track) */
static void *write_buf;		/* DMA buffer for write (1 sector) */
static u_char result[7];	/* Result from fdc */
static struct event fdd_event;
static int track_cache;		/* Current track of read buffer */

/*
 * Send data to FDC
 * Return -1 on failure
 */
static int
fdc_out(int dat)
{
	int i;

	for (i = 0; i < 100000; i++) {
		if ((inb_p(FDC_MSR) & 0xc0) == 0x80) {
			outb_p(dat, FDC_DAT);
			return 0;
		}
	}
	DPRINTF(("fdc: timeout msr=%x\n", inb(FDC_MSR)));
	return -1;
}

/* Return number of result bytes */
static int
fdc_result(void)
{
	int i, msr, index = 0;

	for (i = 0; i < 50000; i++) {	/* timeout=500msec */
		msr = inb_p(FDC_MSR);
		if ((msr & 0xd0) == 0x80) {
			return index;
		}
		if ((msr & 0xd0) == 0xd0) {
			if (index > 6) {
				DPRINTF(("fdc: overrun\n"));
				return -1;
			}
			result[index++] = inb_p(FDC_DAT);
			/*
			DPRINTF(("result[%d]=%x\n", index - 1,
				result[index - 1]));
			*/
		}
		delay_usec(10);
	}
	DPRINTF(("fdc: timeout\n"));
	return -1;
}

static void
fdc_error(int errno)
{
	DPRINTF(("fdc: errno=%d\n", errno));

	dma_stop(fdd_dma);
	ioreq.errno = errno;
	sched_wakeup(&fdd_event);
	fdc_off();
}

/*
 * Stop motor. (No interrupt)
 */
static void
fdc_off(void)
{
	DPRINTF(("fdc: motor off\n"));

	fdc_stat = FDS_OFF;
	timer_stop(&fdd_tmr);
	outb_p(0x0c, FDC_DOR);
}

/*
 * Start motor and wait 250msec. (No interrupt)
 */
static void
fdc_on(void)
{
	DPRINTF(("fdc: motor on\n"));

	fdc_stat = FDS_ON;
	outb_p(0x1c, FDC_DOR);
	timer_callout(&fdd_tmr, 250, &fdc_timeout, NULL);
}

/*
 * Reset FDC and wait an intterupt.
 * Timeout is 500msec.
 */
static void
fdc_reset(void)
{
	DPRINTF(("fdc: reset\n"));

	fdc_stat = FDS_RESET;
	timer_callout(&fdd_tmr, 500, &fdc_timeout, NULL);
	outb_p(0x18, FDC_DOR);	/* Motor0 enable, DMA enable */
	delay_usec(20);		/* Wait 20 usec while reset */
	outb_p(0x1c, FDC_DOR);	/* Clear reset */
}

/*
 * Recalibrate FDC and wait an interrupt.
 * Timeout is 5sec.
 */
static void
fdc_recal(void)
{
	DPRINTF(("fdc: recalibrate\n"));

	fdc_stat = FDS_RECAL;
	timer_callout(&fdd_tmr, 5000, &fdc_timeout, NULL);
	fdc_out(CMD_RECAL);
	fdc_out(0);		/* Drive 0 */
}

/*
 * Seek FDC and wait an interrupt.
 * Timeout is 4sec.
 */
static void
fdc_seek(void)
{
	int head, track;

	DPRINTF(("fdc: seek\n"));
	fdc_stat = FDS_SEEK;
	head = (ioreq.blkno % (FDG_SECTORS * FDG_HEADS)) / FDG_SECTORS;
	track = ioreq.blkno / (FDG_SECTORS * FDG_HEADS);

	timer_callout(&fdd_tmr, 4000, &fdc_timeout, NULL);

	fdc_out(CMD_SPECIFY);	/* specify command parameter */
	fdc_out(0xd1);		/* Step rate = 3msec, Head unload time = 16msec */
	fdc_out(0x02);		/* Head load time = 2msec, Dma on (0) */

	fdc_out(CMD_SEEK);
	fdc_out(head << 2);
	fdc_out(track);
}

/*
 * Read/write data and wait an interrupt.
 * Timeout is 2sec.
 */
static void
fdc_io(void)
{
	int head, track, sect;
	u_long io_size;
	int read;

	DPRINTF(("fdc: read/write\n"));
	fdc_stat = FDS_IO;

	head = (ioreq.blkno % (FDG_SECTORS * FDG_HEADS)) / FDG_SECTORS;
	track = ioreq.blkno / (FDG_SECTORS * FDG_HEADS);
	sect = ioreq.blkno % FDG_SECTORS + 1;
	io_size = ioreq.blksz * SECTOR_SIZE;
	read = (ioreq.cmd == IO_READ) ? 1 : 0;

	DPRINTF(("fdc: hd=%x trk=%x sec=%x size=%d read=%d\n",
		 head, track, sect, io_size, read));

	timer_callout(&fdd_tmr, 2000, &fdc_timeout, NULL);

	dma_setup(fdd_dma, ioreq.buf, io_size, read);

	/* Send command */
	fdc_out(read ? CMD_READ : CMD_WRITE);
	fdc_out(head << 2);
	fdc_out(track);
	fdc_out(head);
	fdc_out(sect);
	fdc_out(2);		/* sector size = 512 bytes */
	fdc_out(FDG_SECTORS);
	fdc_out(FDG_GAP3RW);
	fdc_out(0xff);
}

/*
 * Wake up iorequester.
 * FDC motor is set to off after 5sec.
 */
static void
fdc_ready(void)
{
	DPRINTF(("fdc: wakeup requester\n"));

	fdc_stat = FDS_READY;
	sched_wakeup(&fdd_event);
	timer_callout(&fdd_tmr, 5000, &fdc_timeout, NULL);
}

/*
 * Timeout handler
 */
static void
fdc_timeout(void *arg)
{
	DPRINTF(("fdc: fdc_stat=%d\n", fdc_stat));

	switch (fdc_stat) {
	case FDS_ON:
		fdc_reset();
		break;
	case FDS_RESET:
	case FDS_RECAL:
		DPRINTF(("fdc: reset/recal timeout\n"));
		fdc_error(EIO);
		break;
	case FDS_SEEK:
	case FDS_IO:
		DPRINTF(("fdc: seek/io timeout retry=%d\n", ioreq.nr_retry));
		if (++ioreq.nr_retry <= 3)
			fdc_reset();
		else
			fdc_error(EIO);
		break;
	case FDS_READY:
		fdc_off();
		break;
	default:
		panic("fdc: unknown timeout");
	}
}

/*
 * Interrupt service routine
 * Do not change the fdc_stat in isr.
 */
static int
fdc_isr(int irq)
{
	DPRINTF(("fdc_stat=%d\n", fdc_stat));

	timer_stop(&fdd_tmr);
	switch (fdc_stat) {
	case FDS_IO:
		dma_stop(fdd_dma);
		/* Fall through */
	case FDS_RESET:
	case FDS_RECAL:
	case FDS_SEEK:
		if (ioreq.cmd == IO_NONE) {
			DPRINTF(("fdc: invalid interrupt\n"));
			timer_stop(&fdd_tmr);
			break;
		}
		return INT_CONTINUE;
	case FDS_OFF:
		break;
	default:
		DPRINTF(("fdc: unknown interrupt\n"));
		break;
	}
	return 0;
}

/*
 * Interrupt service thread
 * This is called when command completion.
 */
static void
fdc_ist(int irq)
{
	int i;

	DPRINTF(("fdc_stat=%d\n", fdc_stat));
	if (ioreq.cmd == IO_NONE)
		return;

	switch (fdc_stat) {
	case FDS_RESET:
		/* clear output buffer */
		for (i = 0; i < 4; i++) {
			fdc_out(CMD_SENSE);
			fdc_result();
		}
		fdc_recal();
		break;
	case FDS_RECAL:
		fdc_out(CMD_SENSE);
		fdc_result();
		if ((result[0] & 0xf8) != 0x20) {
			DPRINTF(("fdc: recal error\n"));
			fdc_error(EIO);
			break;
		}
		fdc_seek();
		break;
	case FDS_SEEK:
		fdc_out(CMD_SENSE);
		fdc_result();
		if ((result[0] & 0xf8) != 0x20) {
			DPRINTF(("fdc: seek error\n"));
			if (++ioreq.nr_retry <= 3)
				fdc_reset();
			else
				fdc_error(EIO);
			break;
		}
		fdc_io();
		break;
	case FDS_IO:
		fdc_result();
		if ((result[0] & 0xd8) != 0x00) {
			DPRINTF(("fdc: i/o error st0=%x st1=%x st2=%x st3=%x "
				 "retry=%d\n",
				 result[0], result[1], result[2], result[3],
				 ioreq.nr_retry));
			if (++ioreq.nr_retry <= 3)
				fdc_reset();
			else
				fdc_error(EIO);
			break;
		}
		DPRINTF(("fdc: i/o complete\n"));
		fdc_ready();
		break;
	case FDS_OFF:
		/* Ignore */
		break;
	default:
		ASSERT(0);
	}
}

/*
 * Open
 */
static int
fdd_open(device_t dev, int mode)
{

	nr_open++;
	DPRINTF(("fdd_open: nr_open=%d\n", nr_open));
	return 0;
}

/*
 * Close
 */
static int
fdd_close(device_t dev)
{
	DPRINTF(("fdd_close: dev=%x\n", dev));

	if (nr_open < 1)
		return EINVAL;
	nr_open--;
	if (nr_open == 0) {
		ioreq.cmd = IO_NONE;
		fdc_off();
	}
	return 0;
}

/*
 * Common routine for read/write
 */
static int
fdd_rw(int cmd, char *buf, u_long blksz, int blkno)
{
	int err;

	DPRINTF(("fdd_rw: cmd=%x buf=%x blksz=%d blkno=%x\n",
		 cmd, buf, blksz, blkno));
	ioreq.cmd = cmd;
	ioreq.nr_retry = 0;
	ioreq.blkno = blkno;
	ioreq.blksz = blksz;
	ioreq.buf = buf;
	ioreq.errno = 0;

	sched_lock();
	if (fdc_stat == FDS_OFF)
		fdc_on();
	else
		fdc_seek();

	if (sched_sleep(&fdd_event) == SLP_INTR)
		err = EINTR;
	else
		err = ioreq.errno;

	sched_unlock();
	return err;
}

/*
 * Read
 *
 * Error:
 *  EINTR   ... Interrupted by signal
 *  EIO     ... Low level I/O error
 *  ENXIO   ... Write protected
 *  EFAULT  ... No physical memory is mapped to buffer
 */
static int
fdd_read(device_t dev, char *buf, size_t *nbyte, int blkno)
{
	char *kbuf;
	int track, sect, err;
	u_int i, nr_sect;

	DPRINTF(("fdd_read: buf=%x nbyte=%d blkno=%x\n", buf, *nbyte, blkno));

	/* Check overrun */
	if (blkno > FDG_HEADS * FDG_TRACKS * FDG_SECTORS)
		return EIO;

	/* Translate buffer address to kernel address */
	kbuf = kmem_map(buf, *nbyte);
	if (kbuf == NULL)
		return EFAULT;

	nr_sect = *nbyte / SECTOR_SIZE;
	err = 0;
	for (i = 0; i < nr_sect; i++) {
		/* Translate the logical sector# to logical track#/sector#. */
		track = blkno / FDG_SECTORS;
		sect = blkno % FDG_SECTORS;
		/*
		 * If target sector does not exist in buffer,
		 * read 1 track (18 sectors) at once.
		 */
		if (track != track_cache) {
			err = fdd_rw(IO_READ, read_buf, FDG_SECTORS,
				     track * FDG_SECTORS);
			if (err) {
				track_cache = INVALID_TRACK;
				break;
			}
			track_cache = track;
		}
		memcpy(kbuf, (char *)read_buf + sect * SECTOR_SIZE,
		       SECTOR_SIZE);
		blkno++;
		kbuf += SECTOR_SIZE;
	}
	*nbyte = i * SECTOR_SIZE;
	return err;
}

/*
 * Write
 *
 * Error:
 *  EINTR   ... Interrupted by signal
 *  EIO     ... Low level I/O error
 *  ENXIO   ... Write protected
 *  EFAULT  ... No physical memory is mapped to buffer
 */
static int
fdd_write(device_t dev, char *buf, size_t *nbyte, int blkno)
{
	char *kbuf, *wbuf;
	int track, sect, err;
	u_int i, nr_sect;

	DPRINTF(("fdd_write: buf=%x nbyte=%d blkno=%x\n", buf, *nbyte, blkno));

	/* Check overrun */
	if (blkno > FDG_HEADS * FDG_TRACKS * FDG_SECTORS)
		return EIO;

	/* Translate buffer address to kernel address */
	kbuf = kmem_map(buf, *nbyte);
	if (kbuf == NULL)
		return EFAULT;

	nr_sect = *nbyte / SECTOR_SIZE;
	err = 0;
	for (i = 0; i < nr_sect; i++) {
		/* Translate the logical sector# to track#/sector#. */
		track = blkno / FDG_SECTORS;
		sect = blkno % FDG_SECTORS;
		/*
		 * If target sector exists in read buffer, use it as
		 * write buffer to keep the cache cohrency.
		 */
		if (track == track_cache)
			wbuf = (char *)read_buf + sect * SECTOR_SIZE;
		else
			wbuf = write_buf;

		memcpy(wbuf, kbuf, SECTOR_SIZE);
		err = fdd_rw(IO_WRITE, wbuf, 1, blkno);
		if (err) {
			track_cache = INVALID_TRACK;
			break;
		}
		blkno++;
		kbuf += SECTOR_SIZE;
	}
	*nbyte = i * SECTOR_SIZE;

	DPRINTF(("fdd_write: err=%d\n", err));
	return err;
}

/*
 * Initialize
 */
static int
fdd_init(void)
{
	char *buf;
	int i;

	DPRINTF(("fdd_init\n"));
	if (inb(FDC_MSR) == 0xff) {
		printf("floppy drive not found!\n");
		return -1;
	}

	/* Create device object */
	fdd_dev = device_create(&fdd_io, "fd0", DF_BLK);
	ASSERT(fdd_dev);

	event_init(&fdd_event, "fdd i/o");

	/*
	 * Allocate physical pages for DMA buffer.
	 * Buffer: 1 track for read, 1 sector for write.
	 */
	buf = dma_alloc(TRACK_SIZE + SECTOR_SIZE);
	ASSERT(buf);
	read_buf = buf;
	write_buf = buf + TRACK_SIZE;

	/* Allocate DMA */
	fdd_dma = dma_attach(FDD_DMA);
	ASSERT(fdd_dma != -1);

	/* Allocate IRQ */
	fdd_irq = irq_attach(FDD_IRQ, IPL_BLOCK, 0, fdc_isr, fdc_ist);
	ASSERT(fdd_irq != IRQ_NULL);

	timer_init(&fdd_tmr);
	fdc_stat = FDS_OFF;
	ioreq.cmd = IO_NONE;
	track_cache = INVALID_TRACK;

	/* Reset FDC */
	outb_p(0x08, FDC_DOR);
	delay_usec(20);
	outb_p(0x0C, FDC_DOR);

	/* Data rate 500kbps */
	outb_p(0x00, FDC_CCR);

	/* clear output buffer */
	for (i = 0; i < 4; i++) {
		fdc_out(CMD_SENSE);
		fdc_result();
	}
	return 0;
}
