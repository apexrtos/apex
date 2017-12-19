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

#include <boot.h>

#define SCREEN_80x25 1
/* #define SCREEN_80x50 1 */

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

extern paddr_t lo_mem;
extern paddr_t hi_mem;

/*
 * Setup boot information.
 *
 * Note: We already got the memory size via BIOS call in head.S.
 */
static void
bootinfo_setup(void)
{

#ifdef SCREEN_80x25
	bootinfo->video.text_x = 80;
	bootinfo->video.text_y = 25;
#else
	bootinfo->video.text_x = 80;
	bootinfo->video.text_y = 50;
#endif
	/*
	 * Main memory
	 */
	bootinfo->ram[0].base = 0;
	bootinfo->ram[0].size = (size_t)((1024 + hi_mem) * 1024);
	bootinfo->ram[0].type = MT_USABLE;
	bootinfo->nr_rams = 1;

	/*
	 * Add BIOS ROM and VRAM area
	 */
	if (hi_mem != 0) {
		bootinfo->ram[1].base = lo_mem * 1024;
		bootinfo->ram[1].size = (size_t)((1024 - lo_mem) * 1024);
		bootinfo->ram[1].type = MT_MEMHOLE;
		bootinfo->nr_rams++;
	}
}

#ifdef DEBUG
#ifdef CONFIG_DIAG_SERIAL
/*
 * Setup serial port
 */
static void
serial_setup(void)
{

	if (inb(COM_LSR) == 0xff)
		return;		/* Serial port is disabled */

	outb(0x00, COM_IER);	/* Disable interrupt */
	outb(0x80, COM_LCR);	/* Access baud rate */
	outb(0x01, COM_DLL);	/* 115200 baud */
	outb(0x00, COM_DLM);
	outb(0x03, COM_LCR);	/* N, 8, 1 */
	outb(0x03, COM_MCR);	/* Ready */
	outb(0x00, COM_FCR);	/* Disable FIFO */
	inb(COM_PORT);
	inb(COM_PORT);
}

/*
 * Put chracter to serial port
 */
static void
serial_putc(int c)
{

	while (!(inb(COM_LSR) & 0x20))
		;
	outb(c, COM_THR);
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
#endif
#ifdef CONFIG_DIAG_BOCHS
	/*
	 * output to bochs emulater console.
	 */
	if (inb(0xe9) == 0xe9)
		outb((u_char)c, 0xe9);
#endif
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

	bootinfo_setup();

#if defined(DEBUG) && defined(CONFIG_DIAG_SERIAL)
	serial_setup();
#endif
}
