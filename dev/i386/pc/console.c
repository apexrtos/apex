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
 * console.c - pc console driver
 */

#include <driver.h>
#include <cpufunc.h>
#include <console.h>
#include <sys/tty.h>

#define CRTC_INDEX	0x3d4
#define CRTC_DATA	0x3d5
#define GRAC_INDEX	0x3ce
#define GRAC_DATA	0x3cf

#define VID_RAM		0xB8000

static int console_init(void);
static int console_read(device_t, char *, size_t *, int);
static int console_write(device_t, char *, size_t *, int);
static int console_ioctl(device_t, u_long, void *);

/*
 * Driver structure
 */
struct driver console_drv = {
	/* name */	"Console",
	/* order */	4,
	/* init */	console_init,
};

/*
 * Device I/O table
 */
static struct devio console_io = {
	/* open */	NULL,
	/* close */	NULL,
	/* read */	console_read,
	/* write */	console_write,
	/* ioctl */	console_ioctl,
	/* event */	NULL,
};

static device_t console_dev;
static struct tty console_tty;

static short *vram;
static int pos_x;
static int pos_y;
static int cols;
static int rows;
static u_short attrib;

static int esc_index;
static int esc_arg1;
static int esc_arg2;
static int esc_argc;
static int esc_saved_x;
static int esc_saved_y;

static u_short ansi_colors[] = {0, 4, 2, 6, 1, 5, 3, 7};

static void
scroll_up(void)
{
	int i;

	memcpy(vram, vram + cols, (size_t)(cols * (rows - 1) * 2));
	for (i = 0; i < cols; i++)
		vram[cols * (rows - 1) + i] = ' ' | (attrib << 8);
}

static void
move_cursor(void)
{
	int pos = pos_y * cols + pos_x;

	irq_lock();
	outb(0x0e, CRTC_INDEX);
	outb((pos >> 8) & 0xff, CRTC_DATA);
	outb(0x0f, CRTC_INDEX);
	outb(pos & 0xff, CRTC_DATA);
	irq_unlock();
}

static void
reset_cursor(void)
{
	u_int offset;

	irq_lock();
	outb(0x0e, CRTC_INDEX);
	offset = (u_int)inb(CRTC_DATA);
	offset <<= 8;
	outb(0x0f, CRTC_INDEX);
	offset += (u_int)inb(CRTC_DATA);
	pos_x = offset % cols;
	pos_y = offset / cols;
	irq_unlock();
}

static void
new_line(void)
{

	pos_x = 0;
	pos_y++;
	if (pos_y >= rows) {
		pos_y = rows - 1;
		scroll_up();
	}
}

static void
clear_screen(void)
{
	int i;

	for (i = 0; i < cols * rows; i++)
		vram[i] = ' ' | (attrib << 8);
	pos_x = 0;
	pos_y = 0;
	move_cursor();
}

/*
 * Check for escape code sequence.
 * Rreturn true if escape
 *
 * <Support list>
 *  ESC[#;#H or	: moves cursor to line #, column #
 *  ESC[#;#f
 *  ESC[#A	: moves cursor up # lines
 *  ESC[#B	: moves cursor down # lines
 *  ESC[#C	: moves cursor right # spaces
 *  ESC[#D	: moves cursor left # spaces
 *  ESC[#;#R	: reports current cursor line & column
 *  ESC[s	: save cursor position for recall later
 *  ESC[u	: return to saved cursor position
 *  ESC[2J	: clear screen and home cursor
 *  ESC[K	: clear to end of line
 *  ESC[#m	: attribute (0=attribure off, 4=underline, 5=blink)
 */
static int
check_escape(char c)
{
	int move = 0;
	int val;
	u_short color;

	if (c == 033) {
		esc_index = 1;
		esc_argc = 0;
		return 1;
	}
	if (esc_index == 0)
		return 0;

	if (c >= '0' && c <= '9') {
		val = c - '0';
		switch (esc_argc) {
		case 0:
			esc_arg1 = val;
			esc_index++;
			break;
		case 1:
			esc_arg1 = esc_arg1 * 10 + val;
			break;
		case 2:
			esc_arg2 = val;
			esc_index++;
			break;
		case 3:
			esc_arg2 = esc_arg2 * 10 + val;
			break;
		default:
			goto reset;
		}
		esc_argc++;
		return 1;
	}

	esc_index++;

	switch (esc_index) {
        case 2:
		if (c != '[')
			goto reset;
		return 1;
	case 3:
		switch (c) {
		case 's':	/* Save cursor position */
			esc_saved_x = pos_x;
			esc_saved_y = pos_y;
			break;
		case 'u':	/* Return to saved cursor position */
			pos_x = esc_saved_x;
			pos_y = esc_saved_y;
			move_cursor();
			break;
		case 'K':	/* Clear to end of line */
			break;
		}
		goto reset;
	case 4:
		switch (c) {
		case 'A':	/* Move cursor up # lines */
			pos_y -= esc_arg1;
			if (pos_y < 0)
				pos_y = 0;
			move = 1;
			break;
		case 'B':	/* Move cursor down # lines */
			pos_y += esc_arg1;
			if (pos_y >= rows)
				pos_y = rows - 1;
			move = 1;
			break;
		case 'C':	/* Move cursor forward # spaces */
			pos_x += esc_arg1;
			if (pos_x >= cols)
				pos_x = cols - 1;
			move = 1;
			break;
		case 'D':	/* Move cursor back # spaces */
			pos_x -= esc_arg1;
			if (pos_x < 0)
				pos_x = 0;
			move = 1;
			break;
		case ';':
			if (esc_argc == 1)
				esc_argc = 2;
			return 1;
		case 'J':
			if (esc_arg1 == 2)	/* Clear screen */
				clear_screen();
			break;
		case 'm':	/* Change attribute */
			switch (esc_arg1) {
			case 0:		/* reset */
				attrib = 0x0F;
				break;
			case 1:		/* bold */
				attrib = 0x0F;
				break;
			case 4:		/* under line */
				break;
			case 5:		/* blink */
				attrib |= 0x80;
				break;
			case 30: case 31: case 32: case 33:
			case 34: case 35: case 36: case 37:
				color = ansi_colors[esc_arg1 - 30];
				attrib = (attrib & 0xf0) | color;
				break;
			case 40: case 41: case 42: case 43:
			case 44: case 45: case 46: case 47:
				color = ansi_colors[esc_arg1 - 40];
				attrib = (attrib & 0x0f) | (color << 4);
				break;
			}
			break;

		}
		if (move)
			move_cursor();
		goto reset;
	case 6:
		switch (c) {
		case 'H':
		case 'f':
			pos_y = esc_arg1;
			pos_x = esc_arg2;
			if (pos_y >= rows)
				pos_y = rows - 1;
			if (pos_x >= cols)
				pos_x = cols - 1;
			move_cursor();
			break;
		case 'R':
			/* XXX */
			break;
		}
		goto reset;
	default:
		goto reset;
	}
	return 1;
 reset:
	esc_index = 0;
	esc_argc = 0;
	return 1;
}

static void
console_putc(char c)
{

	if (check_escape(c))
		return;

	switch (c) {
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

	vram[pos_y * cols + pos_x] = c | (attrib << 8);
	pos_x++;
	if (pos_x >= cols) {
		pos_x = 0;
		pos_y++;
		if (pos_y >= rows) {
			pos_y = rows - 1;
			scroll_up();
		}
	}
}

/*
 * Start output operation.
 */
static void
console_start(struct tty *tp)
{
	int c;

	sched_lock();
	while ((c = ttyq_getc(&tp->t_outq)) >= 0)
		console_putc(c);
	move_cursor();
	tty_done(tp);
	sched_unlock();
}

/*
 * Read
 */
static int
console_read(device_t dev, char *buf, size_t *nbyte, int blkno)
{

	return tty_read(&console_tty, buf, nbyte);
}

/*
 * Write
 */
static int
console_write(device_t dev, char *buf, size_t *nbyte, int blkno)
{

	return tty_write(&console_tty, buf, nbyte);
}

/*
 * I/O control
 */
static int
console_ioctl(device_t dev, u_long cmd, void *arg)
{

	return tty_ioctl(&console_tty, cmd, arg);
}

/*
 * Attach input device.
 */
void
console_attach(struct tty **tpp)
{

	*tpp = &console_tty;
}

#if defined(DEBUG) && defined(CONFIG_DIAG_SCREEN)
/*
 * Diag print handler
 */
static void
console_puts(char *str)
{
	size_t count;
	char c;

	sched_lock();
	for (count = 0; count < 128; count++) {
		c = *str;
		if (c == '\0')
			break;
		console_putc(c);
#if defined(CONFIG_DIAG_BOCHS)
		if (inb(0xe9) == 0xe9) {
			if (c == '\n')
				outb((int)'\r', 0xe9);
			outb(c, 0xe9);
		}
#endif
		str++;
	}
	move_cursor();
	esc_index = 0;
	sched_unlock();
}
#endif

/*
 * Init
 */
static int
console_init(void)
{
	struct bootinfo *bootinfo;

	machine_bootinfo(&bootinfo);
	cols = bootinfo->video.text_x;
	rows = bootinfo->video.text_y;

	esc_index = 0;
	attrib = 0x0F;

	vram = phys_to_virt((void *)VID_RAM);
	console_dev = device_create(&console_io, "console", DF_CHR);
	reset_cursor();
#if defined(DEBUG) && defined(CONFIG_DIAG_SCREEN)
	debug_attach(console_puts);
#endif
	tty_attach(&console_io, &console_tty);
	console_tty.t_oproc = console_start;
	console_tty.t_winsize.ws_row = (u_short)rows;
	console_tty.t_winsize.ws_col = (u_short)cols;
	return 0;
}
