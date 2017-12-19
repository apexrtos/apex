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
 * serial.c - Serial console driver for Integrator-CP (PL011 UART)
 */

#include <driver.h>
#include <sys/ioctl.h>
#include <sys/tty.h>

/* #define DEBUG_SERIAL 1 */

#ifdef DEBUG_SERIAL
#define DPRINTF(a) printf a
#else
#define DPRINTF(a)
#endif

#define TERM_COLS	80
#define TERM_ROWS	25

#ifdef CONFIG_MMU
#define UART_BASE	(0xc0000000 + 0x16000000)
#else
#define UART_BASE	(0x16000000)
#endif

#define UART_IRQ	1
#define UART_CLK	14745600
#define BAUD_RATE	115200

/* UART Registers */
#define UART_DR		(*(volatile uint32_t *)(UART_BASE + 0x00))
#define UART_RSR	(*(volatile uint32_t *)(UART_BASE + 0x04))
#define UART_ECR	(*(volatile uint32_t *)(UART_BASE + 0x04))
#define UART_FR		(*(volatile uint32_t *)(UART_BASE + 0x18))
#define UART_IBRD	(*(volatile uint32_t *)(UART_BASE + 0x24))
#define UART_FBRD	(*(volatile uint32_t *)(UART_BASE + 0x28))
#define UART_LCRH	(*(volatile uint32_t *)(UART_BASE + 0x2c))
#define UART_CR		(*(volatile uint32_t *)(UART_BASE + 0x30))
#define UART_IMSC	(*(volatile uint32_t *)(UART_BASE + 0x38))
#define UART_MIS	(*(volatile uint32_t *)(UART_BASE + 0x40))
#define UART_ICR	(*(volatile uint32_t *)(UART_BASE + 0x44))

/* Flag register */
#define FR_RXFE		0x10	/* Receive FIFO empty */
#define FR_TXFF		0x20	/* Transmit FIFO full */

/* Masked interrupt status register */
#define MIS_RX		0x10	/* Receive interrupt */
#define MIS_TX		0x20	/* Transmit interrupt */

/* Interrupt clear register */
#define ICR_RX		0x10	/* Clear receive interrupt */
#define ICR_TX		0x20	/* Clear transmit interrupt */

/* Line control register (High) */
#define LCRH_WLEN8	0x60	/* 8 bits */
#define LCRH_FEN	0x10	/* Enable FIFO */

/* Control register */
#define CR_UARTEN	0x0001	/* UART enable */
#define CR_TXE		0x0100	/* Transmit enable */
#define CR_RXE		0x0200	/* Receive enable */

/* Interrupt mask set/clear register */
#define IMSC_RX		0x10	/* Receive interrupt mask */
#define IMSC_TX		0x20	/* Transmit interrupt mask */


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
serial_putc(char c)
{

	while (UART_FR & FR_TXFF)
		;
	UART_DR = (uint32_t)c;
}

/*
 * Start output operation.
 */
static void
serial_start(struct tty *tp)
{
	int c;

	sched_lock();
	while ((c = ttyq_getc(&tp->t_outq)) >= 0) {
		if (c == '\n')
			serial_putc('\r');
		serial_putc(c);
	}
	sched_unlock();
}

/*
 * Interrupt service routine
 */
static int
serial_isr(int irq)
{
	int c;

	if (UART_MIS & MIS_RX) {	/* Receive interrupt */
		while (UART_FR & FR_RXFE)
			;
		do {
			c = UART_DR;
			tty_input(c, &serial_tty);
		} while ((UART_FR & FR_RXFE) == 0);
		UART_ICR = ICR_RX;
	}
	if (UART_MIS & MIS_TX) {	/* Transmit interrupt */
		tty_done(&serial_tty);
		UART_ICR = ICR_TX;
	}
	return 0;
}

#if defined(DEBUG) && defined(CONFIG_DIAG_SERIAL)
/*
 * Diag print handler
 */
static void
serial_puts(char *str)
{
	size_t count;
	char c;

	irq_lock();

	/* Disable interrupts */
	UART_IMSC = 0;

	for (count = 0; count < 128; count++) {
		c = *str;
		if (c == '\0')
			break;
		if (c == '\n')
			serial_putc('\r');
		serial_putc(c);
		str++;
	}

	/* Enable interrupts */
	UART_IMSC = (IMSC_RX | IMSC_TX);
	irq_unlock();
}
#endif

static int
init_port(void)
{
	unsigned int divider;
	unsigned int remainder;
	unsigned int fraction;

	UART_CR = 0x0;		/* Disable everything */
	UART_ICR = 0x07ff;	/* Clear all interrupt status */

	/*
	 * Set baud rate:
	 * IBRD = UART_CLK / (16 * BAUD_RATE)
	 * FBRD = ROUND((64 * MOD(UART_CLK,(16 * BAUD_RATE))) / (16 * BAUD_RATE))
	 */
	divider = UART_CLK / (16 * BAUD_RATE);
	remainder = UART_CLK % (16 * BAUD_RATE);
	fraction = (8 * remainder / BAUD_RATE) >> 1;
	fraction += (8 * remainder / BAUD_RATE) & 1;
	UART_IBRD = divider;
	UART_FBRD = fraction;

	UART_LCRH = (LCRH_WLEN8 | LCRH_FEN);	/* N, 8, 1, FIFO enable */
	UART_CR = (CR_RXE | CR_TXE | CR_UARTEN);	/* Enable UART */

	/* Install interrupt handler */
	serial_irq = irq_attach(UART_IRQ, IPL_COMM, 0, serial_isr, NULL);

	/* Enable TX/RX interrupt */
	UART_IMSC = (IMSC_RX | IMSC_TX);
	return 0;
}

/*
 * Initialize
 */
static int
serial_init(void)
{

	/* Initialize port */
	if (init_port() == -1)
		return -1;

#if defined(DEBUG) && defined(CONFIG_DIAG_SERIAL)
	debug_attach(serial_puts);
#endif

	/* Create device object */
	serial_dev = device_create(&serial_io, "console", DF_CHR);
	ASSERT(serial_dev);

	tty_attach(&serial_io, &serial_tty);

	serial_tty.t_oproc = serial_start;
	serial_tty.t_winsize.ws_row = (u_short)TERM_ROWS;
	serial_tty.t_winsize.ws_col = (u_short)TERM_COLS;
	return 0;
}
