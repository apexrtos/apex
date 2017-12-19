/*-
 * Copyright (c) 2005-2007, Kohsuke Ohtani
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
 * diag.c - diagnostic message support
 */

#include <kernel.h>
#include <page.h>
#include <cpu.h>
#include <cpufunc.h>

#ifdef DEBUG
#ifdef CONFIG_DIAG_SCREEN

#define VID_ATTR	0x0F00
#define VID_PORT	0x03d4
#define VID_RAM		0xB8000

/* Screen info */
static short *vram;
static int pos_x;
static int pos_y;
static int screen_x;
static int screen_y;

static void
screen_scrollup(void)
{
	int i;

	memcpy(vram, vram + screen_x, (size_t)(screen_x * (screen_y - 1) * 2));
	for (i = 0; i < screen_x; i++)
		vram[screen_x * (screen_y - 1) + i] = ' ';
}

static void
screen_update(void)
{
	int pos = pos_y * screen_x + pos_x;

	outb(0x0e, VID_PORT);
	outb((u_int)pos >> 8, VID_PORT + 1);

	outb(0x0f, VID_PORT);
	outb((u_int)pos & 0xff, VID_PORT + 1);
}

static void
screen_newline(void)
{

	pos_x = 0;
	pos_y++;
	if (pos_y >= screen_y) {
		pos_y = screen_y - 1;
		screen_scrollup();
	}
	screen_update();
}

static void
screen_putc(char c)
{

	switch (c) {
	case '\n':
		screen_newline();
		return;
	case '\r':
		pos_x = 0;
		screen_update();
		return;
	case '\b':
		if (pos_x == 0)
			return;
		pos_x--;
		screen_update();
		return;
	}
	vram[pos_y * screen_x + pos_x] = c | VID_ATTR;
	pos_x++;
	if (pos_x >= screen_x) {
		pos_x = 0;
		pos_y++;
		if (pos_y >= screen_y) {
			pos_y = screen_y - 1;
			screen_scrollup();
		}
	}
	screen_update();
}

static int
screen_init(void)
{

	vram = (short *)phys_to_virt(VID_RAM);
	pos_x = 0;
	pos_y = 0;
	screen_x = bootinfo->video.text_x;
	screen_y = bootinfo->video.text_y;
	return 0;
}
#endif /* CONFIG_DIAG_SCREEN */


#ifdef CONFIG_DIAG_SERIAL

#define COM_PORT	0x3F8

/* Register offsets */
#define COM_THR		(COM_PORT + 0x00)	/* transmit holding register */
#define COM_LSR		(COM_PORT + 0x05)	/* line status register */

static void
serial_putc(char c)
{

	while (!(inb(COM_LSR) & 0x20))
		;
	outb(c, COM_THR);
}
#endif /* CONFIG_DIAG_SERIAL */


#ifdef CONFIG_DIAG_BOCHS
static void
bochs_putc(char c)
{
	/*
	 * Bochs debug port
	 */
	if (inb(0xe9) == 0xe9)
		outb((u_char)c, 0xe9);
}
#endif /* CONFIG_DIAG_BOCHS */

void
diag_print(char *buf)
{

	while (*buf) {
#ifdef CONFIG_DIAG_SCREEN
		screen_putc(*buf);
#endif
#ifdef CONFIG_DIAG_SERIAL
		if (*buf == '\n')
			serial_putc('\r');
		serial_putc(*buf);
#endif
#ifdef CONFIG_DIAG_BOCHS
		bochs_putc(*buf);
#endif
		++buf;
	}
}
#endif	/* DEBUG */

void
diag_init(void)
{
#ifdef DEBUG
#ifdef CONFIG_DIAG_SCREEN
	screen_init();
#endif
#endif
}
