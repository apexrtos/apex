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
 * diag.c - diagnostic message support
 */

#include <kernel.h>

#ifdef DEBUG

#ifdef CONFIG_DIAG_SCREEN
#include "font.h"

#define VSCR_COLS	32
#define SCR_COLS	30
#define SCR_ROWS	20

/* Registers for keypad control */
#define REG_DISPCNT	(*(volatile u_short *)0x4000000)
#define REG_BG0CNT	(*(volatile u_short *)0x4000008)

#define BG_PALETTE	(u_short *)0x5000000
#define VRAM_TILE	(u_short *)0x6000000
#define VRAM_MAP	(u_short *)0x6008000

#define	RGB(r, g, b)	(((b) << 10) + ((g) << 5) + (r))

static u_short *vram = VRAM_MAP;
static int pos_x;
static int pos_y;

static void
scroll_up(void)
{
	int i;

	for (i = 0; i < VSCR_COLS * (SCR_ROWS - 1); i++)
		vram[i] = vram[i + VSCR_COLS];
	for (i = 0; i < VSCR_COLS; i++)
		vram[VSCR_COLS * (SCR_ROWS - 1) + i] = ' ';
}

static void
new_line(void)
{

	pos_x = 0;
	pos_y++;
	if (pos_y >= SCR_ROWS) {
		pos_y = SCR_ROWS - 1;
		scroll_up();
	}
}

static void
screen_putc(char ch)
{

	switch (ch) {
	case '\n':
		new_line();
		return;
	case '\r':
		pos_x = 0;
		return;
	case '\b':
		if (pos_x == 0)
			return;
		pos_x--;
		return;
	}

	vram[pos_y * VSCR_COLS + pos_x] = ch;
	pos_x++;
	if (pos_x >= SCR_COLS) {
		pos_x = 0;
		pos_y++;
		if (pos_y >= SCR_ROWS) {
			pos_y = SCR_ROWS - 1;
			scroll_up();
		}
	}
}

/*
 * Init font
 */
static void
init_font(void)
{
	int i, row, col, bit, val = 0;
	u_short *tile = VRAM_TILE;

	for (i = 0; i < 256; i++) {
		for (row = 0; row < 8; row++) {
			for (col = 7; col >= 0; col--) {
				bit = (font_bitmap[i][row] &
				       (1 << col)) ? 2 : 1;
				if (col % 2)
					val = bit;
				else {
					tile[(i * 32) +
					     (row * 4) + ((7 - col) / 2)] =
						val + (bit << 8);
				}
			}
		}
	}
}

/*
 * Init screen
 */
static void
init_screen(void)
{
	u_short *pal = BG_PALETTE;

	/* Initialize palette */
	pal[0] = 0;		/* Transparent */
	pal[1] = RGB(0,0,0);	/* Black */
	pal[2] = RGB(31,31,31);	/* White */

	/* Setup video */
	REG_DISPCNT = 0x0100;	/* Mode0, BG0 */
	REG_BG0CNT = 0x1080;	/* Size0, 256color */
}
#endif /* CONFIG_DIAG_SCREEN */


void
diag_print(char *buf)
{
#ifdef CONFIG_DIAG_SCREEN
	char *p = buf;

	while (*p)
		screen_putc(*p++);
#endif
}

#endif /* DEBUG */

/*
 * Init
 */
void
diag_init(void)
{

#ifdef DEBUG
#ifdef CONFIG_DIAG_SCREEN
	init_font();
	init_screen();
#endif
#endif
}
