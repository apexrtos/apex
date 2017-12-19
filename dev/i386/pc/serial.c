/*-
 * Copyright (c) 2008, Kohsuke Ohtani
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
 * serial.c - Serial console driver
 */

#include <driver.h>
#include <sys/ioctl.h>
#include <sys/tty.h>
#include <cpufunc.h>

/* #define DEBUG_SERIAL 1 */

#ifdef DEBUG_SERIAL
#define DPRINTF(a) printf a
#else
#define DPRINTF(a)
#endif

#define TERM_COLS	80
#define TERM_ROWS	25

#define COM_IRQ		4
#define COM_PORT	0x3F8

/* Register offsets */
#define COM_RBR		(COM_PORT + 0x00)	/* receive buffer register */
#define COM_THR		(COM_PORT + 0x00)	/* transmit holding register */
#define COM_IER		(COM_PORT + 0x01)	/* interrupt enable register */
#define COM_FCR		(COM_PORT + 0x02)	/* FIFO control register */
#define COM_IIR		(COM_PORT + 0x02)	/* interrupt identification register */
#define COM_LCR		(COM_PORT + 0x03)	/* line control register */
#define COM_MCR		(COM_PORT + 0x04)	/* modem control register */
#define COM_LSR		(COM_PORT + 0x05)	/* line status register */
#define COM_MSR		(COM_PORT + 0x06)	/* modem status register */
#define COM_DLL		(COM_PORT + 0x00)	/* divisor latch LSB (LCR[7] = 1) */
#define COM_DLM		(COM_PORT + 0x01)	/* divisor latch MSB (LCR[7] = 1) */

/* Interrupt enable register */
#define	IER_RDA		0x01	/* enable receive data available */
#define	IER_THRE	0x02	/* enable transmitter holding register empty */
#define	IER_RLS		0x04	/* enable recieve line status */
#define	IER_RMS		0x08	/* enable receive modem status */

/* Interrupt identification register */
#define	IIR_MSR		0x00	/* modem status change */
#define	IIR_IP		0x01	/* 0 when interrupt pending */
#define	IIR_TXB		0x02	/* transmitter holding register empty */
#define	IIR_RXB		0x04	/* received data available */
#define	IIR_LSR		0x06	/* line status change */
#define	IIR_MASK	0x07	/* mask off just the meaningful bits */

/* Forward functions */
static int serial_init(void);
static int serial_read(device_t, char *, size_t *, int);
static int serial_write(device_t, char *, size_t *, int);
static int serial_ioctl(device_t, u_long, void *);

/*
 * Driver structure
 */
struct driver serial_drv = {
	/* name */	"Serial Console",
	/* order */	4,
	/* init */	serial_init,
};

/*
 * Device I/O table
 */
static struct devio serial_io = {
	/* open */	NULL,
	/* close */	NULL,
	/* read */	serial_read,
	/* write */	serial_write,
	/* ioctl */	serial_ioctl,
	/* event */	NULL,
};

static device_t serial_dev;	/* device object */
static struct tty serial_tty;	/* tty structure */
static irq_t serial_irq;	/* handle for irq */

static int
serial_read(device_t dev, char *buf, size_t *nbyte, int blkno)
{

	return tty_read(&serial_tty, buf, nbyte);
}

static int
serial_write(device_t dev, char *buf, size_t *nbyte, int blkno)
{

	return tty_write(&serial_tty, buf, nbyte);
}

static int
serial_ioctl(device_t dev, u_long cmd, void *arg)
{

	return tty_ioctl(&serial_tty, cmd, arg);
}

static void
put_char(char c)
{

	while (!(inb(COM_LSR) & 0x20))
		;
	outb(c, COM_THR);
}

/*
 * Start output operation.
 */
static void
serial_start(struct tty *tp)
{
	int c;

	sched_lock();
	while ((c = ttyq_getc(&tp->t_outq)) >= 0)
		put_char(c);
	sched_unlock();
}

/*
 * Interrupt service routine
 */
static int
serial_isr(int irq)
{

	switch (inb(COM_IIR) & IIR_MASK) {
	case IIR_MSR:		/* Modem status change */
		break;
	case IIR_LSR:		/* Line status change */
		inb(COM_LSR);
		break;
	case IIR_TXB:		/* Transmitter holding register empty */
		tty_done(&serial_tty);
		break;
	case IIR_RXB:		/* Received data available */
		inb(COM_LSR);
		tty_input(inb(COM_RBR), &serial_tty);
		break;
	}
	return 0;
}

#if defined(DEBUG) && defined(CONFIG_DIAG_SERIAL)
/*
 * Diag print handler
 */
static void
diag_print(char *str)
{
	size_t count;
	char c;

	sched_lock();
	for (count = 0; count < 128; count++) {
		c = *str;
		if (c == '\0')
			break;
		if (c == '\n')
			put_char('\r');
		put_char(c);
		str++;
	}
	sched_unlock();
}
#endif

static int
port_init(void)
{
	if (inb(COM_LSR) == 0xff)
		return -1;	/* Port is disabled */

	outb(0x00, COM_IER);	/* Disable interrupt */
	outb(0x80, COM_LCR);	/* Access baud rate */
	outb(0x01, COM_DLL);	/* 115200 baud */
	outb(0x00, COM_DLM);
	outb(0x03, COM_LCR);	/* N, 8, 1 */
	outb(0x00, COM_FCR);	/* Disable FIFO */

	/* Install interrupt handler */
	serial_irq = irq_attach(COM_IRQ, IPL_COMM, 0, serial_isr, NULL);

	outb(0x0b, COM_MCR);  /* Enable OUT2 interrupt */
	outb(IER_RDA|IER_THRE|IER_RLS, COM_IER);  /* Enable interrupt */

	inb(COM_PORT);
	inb(COM_PORT);
	return 0;
}

/*
 * Initialize
 */
static int
serial_init(void)
{

	DPRINTF(("serial_init\n"));

	/* Initialize port */
	if (port_init() == -1)
		return -1;

	/* Create device object */
	serial_dev = device_create(&serial_io, "console", DF_CHR);
	ASSERT(serial_dev);

#if defined(DEBUG) && defined(CONFIG_DIAG_SERIAL)
	debug_attach(diag_print);
#endif
	tty_attach(&serial_io, &serial_tty);

	serial_tty.t_oproc = serial_start;
	serial_tty.t_winsize.ws_row = (u_short)TERM_ROWS;
	serial_tty.t_winsize.ws_col = (u_short)TERM_COLS;
	return 0;
}
