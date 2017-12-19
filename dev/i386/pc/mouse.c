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
 * mouse.c - ps2 mouse support
 */

/*
 * PS/2 mouse packet
 *
 *         Bit7   Bit6   Bit5   Bit4   Bit3  Bit2   Bit1   Bit0
 *  ------ ------ ------ ------ ------ ----- ------ ------ ------
 *  Byte 1 Yovf   Xovf   Ysign  Xsign    1   MidBtn RgtBtn LftBtn
 *  Byte 2 X movement
 *  Byte 3 Y movement
 */

#include <driver.h>
#include <cpufunc.h>

#include "kmc.h"

/* #define DEBUG_MOUSE 1 */

#ifdef DEBUG_MOUSE
#define DPRINTF(a) printf a
#else
#define DPRINTF(a)
#endif

#define MOUSE_IRQ	12

static int mouse_init(void);
static int mouse_open(device_t, int);
static int mouse_close(device_t);
static int mouse_read(device_t, char *, size_t *, int);

/*
 * Driver structure
 */
struct driver mouse_drv = {
	/* name */	"PS/2 Mouse",
	/* order */	6,
	/* init */	mouse_init,
};

/*
 * Device I/O table
 */
static struct devio mouse_io = {
	/* open */	mouse_open,
	/* close */	mouse_close,
	/* read */	mouse_read,
	/* write */	NULL,
	/* ioctl */	NULL,
	/* event */	NULL,
};

static device_t mouse_dev;	/* Mouse object */
static irq_t mouse_irq;		/* Handle for mouse irq */
static u_char packet[3];	/* Mouse packet */
static int index = 0;

/*
 * Write aux device command
 */
static void
aux_command(u_char val)
{

	DPRINTF(("aux_command: %x\n", val));
	wait_ibe();
	outb(0x60, KMC_CMD);
	wait_ibe();
	outb(val, KMC_DATA);
}

/*
 * Returns 0 on success, -1 on failure.
 */
static int
aux_write(u_char val)
{
	int rc = -1;

	DPRINTF(("aux_write: val=%x\n", val));
	irq_lock();

	/* Write the value to the device */
	wait_ibe();
	outb(0xd4, KMC_CMD);
	wait_ibe();
	outb(val, KMC_DATA);

	/* Get the ack */
	wait_obf();
	if ((inb(KMC_STS) & 0x20) == 0x20) {
		if (inb(KMC_DATA) == 0xfa)
			rc = 0;
	}
	irq_unlock();
#ifdef DEBUG_MOUSE
	if (rc)
		printf("aux_write: error val=%x\n", val);
#endif
	return rc;
}

/*
 * Interrupt handler
 */
static int
mouse_isr(int irq)
{
	u_char dat, id;

	if ((inb(KMC_STS) & 0x21) != 0x21)
		return 0;

	dat = inb(KMC_DATA);
	if (dat == 0xaa) {	/* BAT comp (reconnect) ? */
		DPRINTF(("BAT comp"));
		index = 0;
		wait_obf();
		if ((inb(KMC_STS) & 0x20) == 0x20) {
			id = inb(KMC_DATA);
			DPRINTF(("Mouse ID=%x\n", id));
		}
		aux_write(0xf4);	/* Enable aux device */
		return 0;
	}

	packet[index++] = dat;
	if (index < 3)
		return 0;
	index = 0;
	DPRINTF(("mouse packet %x:%d:%d\n", packet[0], packet[1], packet[2]));
	return 0;
}

/*
 * Open
 */
static int
mouse_open(device_t dev, int mode)
{
	DPRINTF(("mouse_open: dev=%x\n", dev));
	return 0;
}

/*
 * Close
 */
static int
mouse_close(device_t dev)
{
	DPRINTF(("mouse_close: dev=%x\n", dev));
	return 0;
}

/*
 * Read
 */
static int
mouse_read(device_t dev, char *buf, size_t *nbyte, int blkno)
{
	return 0;
}

/*
 * Initialize
 */
static int
mouse_init(void)
{

#ifdef DEBUG
	printk("Mouse sampling rate=100 samples/sec\n");
#endif
	/* Create device object */
	mouse_dev = device_create(&mouse_io, "mouse", DF_CHR);
	ASSERT(mouse_dev);

	/* Allocate IRQ */
	mouse_irq = irq_attach(MOUSE_IRQ, IPL_INPUT, 0, mouse_isr, NULL);
	ASSERT(mouse_irq != IRQ_NULL);

	wait_ibe();
	outb(0xa8, KMC_CMD);	/* Enable aux */

	aux_write(0xf3);	/* Set sample rate */
	aux_write(100);		/* 100 samples/sec */

	aux_write(0xe8);	/* Set resolution */
	aux_write(3);		/* 8 counts per mm */
	aux_write(0xe7);	/* 2:1 scaling */

	aux_write(0xf4);	/* Enable aux device */
	aux_command(0x47);	/* Enable controller ints */
	return 0;
}
