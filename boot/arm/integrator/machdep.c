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

#include <boot.h>
#include <platform.h>

#define UART_BASE	0x16000000
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


/*
 * Setup boot information.
 */
static void
bootinfo_setup(void)
{

	bootinfo->video.text_x = 80;
	bootinfo->video.text_y = 25;

	/*
	 * On-board SSRAM - 4M
	 */
	bootinfo->ram[0].base = 0;
	bootinfo->ram[0].size = 0x400000;
	bootinfo->ram[0].type = MT_USABLE;

	bootinfo->nr_rams = 1;
}

#ifdef DEBUG
#ifdef CONFIG_DIAG_SERIAL
/*
 * Put chracter to serial port
 */
static void
serial_putc(int c)
{

	while (UART_FR & FR_TXFF)
		;
	UART_DR = c;
}

/*
 * Setup serial port
 */
static void
serial_setup(void)
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
}
#endif /* !CONFIG_DIAG_SERIAL */

/*
 * Print one chracter
 */
void
machine_putc(int c)
{

#ifdef CONFIG_DIAG_SERIAL
	if (c == '\n')
		serial_putc('\r');
	serial_putc(c);
#endif /* !CONFIG_DIAG_SERIAL */
}
#endif /* !DEBUG */

/*
 * Panic handler
 */
void
machine_panic(void)
{

	for (;;) ;
}

/*
 * Setup machine state
 */
void
machine_setup(void)
{

#if defined(DEBUG) && defined(CONFIG_DIAG_SERIAL)
	serial_setup();
#endif
	bootinfo_setup();
}
