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
 * keypad.c - GBA keypad driver
 */

#include <driver.h>
#include <prex/keycode.h>
#include "keypad.h"

/* Parameters */
#define KEYQ_SIZE	32
#define KEYPAD_IRQ	12

/* Registers for keypad control */
#define REG_KEYSTS	(*(volatile uint16_t *)0x4000130)
#define REG_KEYCNT	(*(volatile uint16_t *)0x4000132)

/* KEY_STS/KEY_CNT */
#define KEY_A		0x0001
#define KEY_B		0x0002
#define KEY_SELECT	0x0004
#define KEY_START	0x0008
#define KEY_RIGHT	0x0010
#define KEY_LEFT	0x0020
#define KEY_UP		0x0040
#define KEY_DOWN	0x0080
#define KEY_R		0x0100
#define KEY_L		0x0200

#define KEY_ALL		0x03ff

/* KEY_CNT value */
#define KEYIRQ_EN	0x4000	/* 0=Disable, 1=Enable */
#define KEYIRQ_COND	0x8000  /* 0=Logical OR, 1=Logical AND */

typedef void (*input_func)(u_char);

static int keypad_init(void);
static int keypad_open(device_t, int);
static int keypad_close(device_t);
static int keypad_read(device_t, char *, size_t *, int);

/*
 * Driver structure
 */
struct driver keypad_drv = {
	/* name */	"GBA Keypad",
	/* order */	4,
	/* init */	keypad_init,
};

static struct devio keypad_io = {
	/* open */	keypad_open,
	/* close */	keypad_close,
	/* read */	keypad_read,
	/* write */	NULL,
	/* ioctl */	NULL,
	/* event */	NULL,
};

/*
 * Key map
 */
static const u_char key_map[] = {
	0,
	'A',		/* A */
	'B',		/* B */
	'\t',		/* Select */
	'\n',		/* Start */
	K_RGHT,		/* Right */
	K_LEFT,		/* Left */
	K_UP,		/* Up */
	K_DOWN,		/* Down */
	'R',		/* R */
	'L',		/* L */
};

#define KEY_MAX (sizeof(key_map) / sizeof(u_char))

static device_t keypad_dev;	/* Device object */
static irq_t keypad_irq;	/* Handle for keyboard irq */
static int nr_open;		/* Open count */
static struct event keypad_event;

static u_char keyq[KEYQ_SIZE];	/* Queue for ascii character */
static int q_tail;
static int q_head;

static input_func input_handler;

/*
 * Keyboard queue operation
 */
#define keyq_next(i)    (((i) + 1) & (KEYQ_SIZE - 1))
#define keyq_empty()    (q_tail == q_head)
#define keyq_full()     (keyq_next(q_tail) == q_head)

/*
 * Put one charcter on key queue
 */
static void
keyq_enqueue(u_char c)
{

	/* Forward key to input handker */
	if (input_handler)
		input_handler(c);
	else {
		sched_wakeup(&keypad_event);
		if (keyq_full())
			return;
		keyq[q_tail] = c;
		q_tail = keyq_next(q_tail);
	}
}

/*
 * Get one character from key queue
 */
static u_char
keyq_dequeue(void)
{
	u_char c;

	c = keyq[q_head];
	q_head = keyq_next(q_head);
	return c;
}

/*
 * Interrupt service routine
 */
static int
keypad_isr(int irq)
{
	uint16_t sts;

	sts = ~REG_KEYSTS & KEY_ALL;

	if (sts == (KEY_SELECT|KEY_START))
		machine_reset();

	if (sts & KEY_A)
		keyq_enqueue('A');
	if (sts & KEY_B)
		keyq_enqueue('B');
	if (sts & KEY_SELECT)
		keyq_enqueue('\t');
	if (sts & KEY_START)
		keyq_enqueue('\n');
	if (sts & KEY_RIGHT)
		keyq_enqueue(K_RGHT);
	if (sts & KEY_LEFT)
		keyq_enqueue(K_LEFT);
	if (sts & KEY_UP)
		keyq_enqueue(K_UP);
	if (sts & KEY_DOWN)
		keyq_enqueue(K_DOWN);
	if (sts & KEY_R)
		keyq_enqueue('R');
	if (sts & KEY_L)
		keyq_enqueue('L');

	return 0;
}

/*
 * Open
 */
static int
keypad_open(device_t dev, int mode)
{

	if (input_handler)
		return EBUSY;
	if (nr_open > 0)
		return EBUSY;
	nr_open++;
	return 0;
}

/*
 * Close
 */
static int
keypad_close(device_t dev)
{

	if (input_handler)
		return EBUSY;
	if (nr_open != 1)
		return EINVAL;
	nr_open--;
	return 0;
}

/*
 * Read
 */
static int
keypad_read(device_t dev, char *buf, size_t *nbyte, int blkno)
{
	int rc, c;
	size_t count;

	if (input_handler)
		return EBUSY;
	if (*nbyte == 0)
		return 0;
	if (keyq_empty()) {
		rc = sched_sleep(&keypad_event);
		if (rc == SLP_INTR)
			return EINTR;
	}
	for (count = 0; count < *nbyte; count++) {
		if (keyq_empty())
			break;
		c = keyq_dequeue();
		if (umem_copyout(&c, buf, 1))
			return EFAULT;
		buf++;
	}
	*nbyte = count;
	return 0;
}

/*
 * Attach input handler for another driver.
 *
 * After an input handler is attached, all key event is forwarded
 * to that handler.
 */
void
keypad_attach(void (*handler)(u_char))
{

	input_handler = handler;
}

/*
 * Initialize
 */
int
keypad_init(void)
{
	input_handler = NULL;

	keypad_dev = device_create(&keypad_io, "keypad", DF_CHR);
	ASSERT(keypad_dev);

	event_init(&keypad_event, "keypad");

	/* Disable keypad interrupt */
	REG_KEYCNT = 0;

	/* Setup isr */
	keypad_irq = irq_attach(KEYPAD_IRQ, IPL_INPUT, 0, keypad_isr, NULL);
	ASSERT(keypad_irq != IRQ_NULL);

	/* Enable interrupt for all key */
	REG_KEYCNT = KEY_ALL | KEYIRQ_EN;
	return 0;
}

