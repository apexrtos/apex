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
 * swkbd.c - GBA software keyboard driver
 */

/**
 * This driver emulates a generic keyboard by using GBA keypad.
 *
 * <Key assign>
 *
 * On-screen keyboard:
 *
 *  A        : Select pointed key
 *  B        : Enter key
 *  Select   : Hide keyboard
 *  Start    :
 *  Right    : Move keyboard cursor right
 *  Left     : Move keyboard cursor left
 *  Up       : Move keyboard cursor up
 *  Down     : Move keyboard cursor down
 *  Button R : Toggle shift state
 *  Button L : Toggle shift state
 *
 * No on-screen keyboard:
 *
 *  A        : A key
 *  B        : B key
 *  Select   : Show keyboard
 *  Start    : Enter key
 *  Right    : Right key
 *  Left     : Left key
 *  Up       : Up key
 *  Down     : Down key
 *  Button R : R key
 *  Button L : L Key
 *
 */

#include <driver.h>
#include <prex/keycode.h>
#include <sys/tty.h>
#include <console.h>
#include "lcd.h"
#include "kbd_img.h"
#include "keymap.h"
#include "keypad.h"

/*
 * Since GBA does not kick interrupt for the button release, we have
 * to wait for a while after button is pressed. Otherwise, many key
 * events are queued by one button press.
 */
#define CURSOR_WAIT	100	/* 100 msec */
#define BUTTON_WAIT	200	/* 200 msec */

static int kbd_init(void);
static int kbd_ioctl(device_t, u_long, void *);

static void move_cursor(void);

/*
 * Driver structure
 */
struct driver kbd_drv = {
	/* name */	"GBA S/W Keyboard",
	/* order */	11,
	/* init */	kbd_init,
};

static struct devio kbd_io = {
	/* open */	NULL,
	/* close */	NULL,
	/* read */	NULL,
	/* write */	NULL,
	/* ioctl */	kbd_ioctl,
	/* event */	NULL,
};

static device_t kbd_dev;	/* Device object */
static struct tty *tty;
static struct timer kbd_tmr;

/* Meta key status */
static int shift;
static int alt;
static int ctrl;
static int capslk;

static int kbd_on;
static int kbd_page;
static volatile int ignore_key;
static u_int pos_x, pos_y;
static int cur_obj;		/* Current object for cursor */

/*
 * Select keyboard page.
 *
 *   Page0 ... Text only
 *   Page1 ... Text & Normal keyboard
 *   Page2 ... Text & Shifted keyboard
 */
static void
kbd_select(int page)
{

	if (page == 0)
		REG_DISPCNT = 0x0840;	/* only BG3 */
	else if (page == 1) {
		REG_DISPCNT = 0x1A40;	/* use BG1&3 */
		move_cursor();
	} else {
		REG_DISPCNT = 0x1C40;	/* use BG2&3 */
		move_cursor();
	}
	kbd_page = page;
}

/*
 * Toggle keyboard type: normal or shift.
 */
static void
kbd_toggle(void)
{
	int page;

	if (kbd_page == 0)
		return;
	if (capslk)
		page = shift ? 1 : 2;
	else
		page = shift ? 2 : 1;
	kbd_select(page);
}

/*
 * Timer call back handler.
 * Just clear ignoring flag.
 */
static void
kbd_timeout(void *arg)
{

	ignore_key = 0;
}

/*
 * Move cursor to point key.
 */
static void
move_cursor(void)
{
	uint16_t *oam = OAM;
	struct _key_info *ki;
	int i, x, y;

	ki = (struct _key_info *)&key_info[pos_y][pos_x];
	x = ki->pos_x + 108;
	y = pos_y * 8 + 11;
	i = 0;
	switch (ki->width) {
	case 9: i = 0; break;
	case 11: i = 1; break;
	case 12: i = 2; break;
	case 13: i = 3; break;
	case 15: i = 4; break;
	case 17: i = 5; break;
	case 19: i = 6; break;
	case 53: i = 7; break;
	}
	if (i != cur_obj) {
		oam[cur_obj * 4] = (uint16_t)((oam[cur_obj * 4] & 0xff00) | 160);
		oam[cur_obj * 4 + 1] = (uint16_t)((oam[cur_obj * 4 + 1] & 0xfe00) | 240);
		cur_obj = i;
	}
	oam[i * 4] = (uint16_t)((oam[i * 4] & 0xff00) | y);
	oam[i * 4 + 1] = (uint16_t)((oam[i * 4 + 1] & 0xfe00) | x);
}

/*
 * Process key press
 */
static void
key_press(void)
{
	struct _key_info *ki;
	u_char ac;

	ki = (struct _key_info *)&key_info[pos_y][pos_x];
	ac = ki->normal;

	/* Check meta key */
	switch (ac) {
	case K_SHFT:
		shift = !shift;
		kbd_toggle();
		return;
	case K_CTRL:
		ctrl = !ctrl;
		return;
	case K_ALT:
		alt = !alt;
		return;
	case K_CAPS:
		capslk = !capslk;
		kbd_toggle();
		return;
	}
	/* Check ctrl & shift state */
	if (ctrl) {
		if (ac >= 'a' && ac <= 'z')
			ac = ac - 'a' + 0x01;
		else if (ac == '\\')
			ac = 0x1c;
		else
			ac = 0;
	} else if (kbd_page == 2)
		ac = ki->shifted;

	if (ac == 0)
		return;

	/* Check caps lock state */
	if (capslk) {
		if (ac >= 'A' && ac <= 'Z')
			ac += 'a' - 'A';
		else if (ac >= 'a' && ac <= 'z')
			ac -= 'a' - 'A';
	}

	/* Check alt key */
	if (alt)
		ac |= 0x80;

	/* Insert key to queue */
	tty_input(ac, tty);

	/* Reset meta status */
	if (shift) {
		shift = 0;
		kbd_toggle();
	}
	if (ctrl)
		ctrl = 0;
	if (alt)
		alt = 0;
}

/*
 * Input handler
 * This routine will be called by keypad ISR.
 */
static void
kbd_isr(u_char c)
{
	int move = 0;
	int timeout = BUTTON_WAIT;

	if (ignore_key)
		return;

	/* Select key */
	if (c == '\t') {
		kbd_on = !kbd_on;
		kbd_select(kbd_on);

		/* Reset meta status */
		shift = 0;
		alt = 0;
		ctrl = 0;
		capslk = 0;
		goto out;
	}

	/* Direct input */
	if (!kbd_on) {
		tty_input(c, tty);
		goto out;
	}

	switch (c) {
	case K_LEFT:
		if (pos_x > 0) {
			if (pos_y == 4 && pos_x >=4 && pos_x <= 8) /* Space */
				pos_x = 3;
			pos_x--;
			move = 1;
		}
		break;
	case K_RGHT:
		if (pos_x < max_x[pos_y]) {
			if (pos_y == 4 && pos_x > 3 && pos_x <= 7) /* Space */
				pos_x = 8;
			pos_x++;
			move = 1;
		}
		break;
	case K_UP:
		if (pos_y > 0 ) {
			pos_y--;
			move = 1;
			if (pos_x > max_x[pos_y])
				pos_x = max_x[pos_y];
		}
		break;
	case K_DOWN:
		if (pos_y < 4) {
			pos_y++;
			move = 1;
			if (pos_x > max_x[pos_y])
				pos_x = max_x[pos_y];
		}
		break;
	case 'A':
		key_press();
		break;
	case 'B':
		tty_input('\n', tty);
		break;
	case 'R':
	case 'L':
		shift = shift ? 0 : 1;
		kbd_toggle();
		break;
	case '\n':
#ifdef DEBUG
		debug_dump(DUMP_THREAD);
		debug_dump(DUMP_TASK);
		debug_dump(DUMP_VM);
#endif
		break;
	}
	if (move) {
		timeout = CURSOR_WAIT;
		move_cursor();
	}
out:
	ignore_key = 1;
	timer_callout(&kbd_tmr, timeout, &kbd_timeout, NULL);
	return;
}

/*
 * I/O control
 */
static int
kbd_ioctl(device_t dev, u_long cmd, void *arg)
{

	return 0;
}

/*
 * Init font
 */
static void
init_kbd_image(void)
{
	uint8_t bit;
	uint16_t val1, val2;
	uint16_t *pal = BG_PALETTE;
	uint16_t *tile1 = KBD1_TILE;
	uint16_t *tile2 = KBD2_TILE;
	uint16_t *map1 = KBD1_MAP;
	uint16_t *map2 = KBD2_MAP;
	int i, j, row, col;

	/* Load tiles for keyboard image */
	for (i = 0; i < 32; i++)
		tile1[i] = 0;

	for (i = 0; i < 64 * 12; i++) {
		bit = 0x01;
		for (j = 0; j < 4; j++) {
			val1 = kbd1_bitmap[i] & bit ? 0xff : 0x03;
			val2 = kbd2_bitmap[i] & bit ? 0xff : 0x03;
			bit = bit << 1;
			val1 |= kbd1_bitmap[i] & bit ? 0xff00 : 0x0300;
			val2 |= kbd2_bitmap[i] & bit ? 0xff00 : 0x0300;
			bit = bit << 1;
			tile1[i * 4 + 32 + j] = val1;
			tile2[i * 4 + j] = val2;
		}
	}


	/* Setup map */
	i = 1;
	for (row = 1; row < 7; row++) {
		for (col = 13; col < 29; col++) {
			map1[row * 32 + col] = (uint16_t)i;
			map2[row * 32 + col] = (uint16_t)(i + 127);
			i++;
		}
	}

	pal[3] = RGB(0,0,31);	/* Kbd bg color */
	pal[255] = RGB(28,28,28);	/* Kbd color */

	/* Setup video */
	REG_BG1CNT = 0x1284;	/* Size0, 256color, priority0 */
	REG_BG2CNT = 0x1484;	/* Size0, 256color, priority0 */
	kbd_select(1);
}

/*
 * Initialize keyboard cursor
 */
static void
init_cursor(void)
{
	int i, j;
	uint8_t bit;
	uint16_t val;
	uint16_t *oam = OAM;
	uint16_t *cur = CURSOR_DATA;
	uint16_t *pal = SPL_PALETTE;

	/* Move out all objects */
	for (i = 0; i < 128; i++) {
		oam[0] = 160;
		oam[1] = 240;
		oam += 4;
	}
	/* Load cursor splite */
	for (i = 0; i < 64 * 7 + 64 * 8; i++) {
		bit = 0x01;
		for (j = 0; j < 4; j++) {
			val = cursor_bitmap[i] & bit ? 0xff : 0;
			bit = bit << 1;
			val |= cursor_bitmap[i] & bit ? 0xff00 : 0;
			bit = bit << 1;
			cur[i * 4 + j] = val;
		}
	}

	/* Setup cursors */
	oam = OAM;
	for (i = 0; i < 7; i++) {
		oam[0] = (uint16_t)(0x6000 + 160); /* 256 color, Horizontal */
		oam[1] = (uint16_t)(0x8000 + 240); /* 32x16 */
		oam[2] = (uint16_t)(i * 16); /* Tile number */
		oam += 4;
	}
	/* for space key */
	oam[0] = 0x6000 + 160; /* 256 color, Horizontal */
	oam[1] = 0xC000 + 240; /* 64x32 */
	oam[2] = 112;

	pal[255] = RGB(31,0,0);	/* cursor color */
}

/*
 * Initialize
 */
static int
kbd_init(void)
{

	kbd_dev = device_create(&kbd_io, "kbd", DF_CHR);
	ASSERT(kbd_dev);

	ignore_key = 0;
	cur_obj = 0;
	kbd_on = 1;
	kbd_page = 0;
	pos_x = 0;
	pos_y = 0;

	timer_init(&kbd_tmr);
	init_cursor();
	init_kbd_image();
	move_cursor();

	/* Hook keypad isr */
	keypad_attach(kbd_isr);

	/* Attach to console */
	console_attach(&tty);
	return 0;
}

