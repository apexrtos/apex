/*
 * Copyright (c) 2007, Kohsuke Ohtani
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
 * ball.c - move many balls within screen.
 */

#include <prex/prex.h>

#include <termios.h>
#include <stdio.h>
#include <stdlib.h>

#define NBALLS		30
#define STACKLEN	512

/* Screen size */
static int max_x;
static int max_y;

static char stack[NBALLS][STACKLEN];

static thread_t
thread_run(void (*start)(void), char *stack)
{
	thread_t th;

	if (thread_create(task_self(), &th) != 0)
		return 0;
	if (thread_load(th, start, stack) != 0)
		return 0;
	if (thread_resume(th) != 0)
		return 0;
	return th;
}

/*
 * A thread to move one ball.
 */
static void
move_ball(void)
{
	int x, y;
	int delta_x, delta_y;
	int old_x, old_y;

	/* Get random value for moving speed. */
	old_x = old_y = 0;
	x = random() % max_x;
	y = random() % max_y;
	delta_x = (random() % 10) + 1;
	delta_y = (random() % 10) + 1;

	for (;;) {
		/* Erase ball at old position */
		printf("\33[%d;%dH ", old_y / 10, old_x / 10);

		/* Print ball at new position */
		printf("\33[%d;%dH*", y / 10, x / 10);

		/* Wait 5ms */
		timer_sleep(5, 0);

		/* Update position */
		old_x = x;
		old_y = y;
		x += delta_x;
		y += delta_y;
		if (x < 10 || x >= max_x)
			delta_x = -delta_x;
		if (y < 10 || y >= max_y)
			delta_y = -delta_y;
	}
	/* NOTREACHED */
}

int
main(int argc, char *argv[])
{
	struct winsize ws;
	int rows, cols, i;
	device_t cons;

	/* Get screen size */
	device_open("console", 0, &cons);
	if (device_ioctl(cons, TIOCGWINSZ, &ws) == 0) {
		rows = ws.ws_row;
		cols = ws.ws_col;
	} else {
		/* Use default screen setting */
		rows = 25;
		cols = 80;
	}
	device_close(cons);

	max_x = (cols - 1) * 10;
	max_y = (rows - 2) * 10;

	/* Clear screen */
	printf("\33[2J");

	/* Create threads and run them. */
	for (i = 0; i < NBALLS; i++) {
		if (thread_run(move_ball, stack[i]+STACKLEN) == 0)
			panic("failed to create thread");
	}

	/* Don't return */
	for (;;);
	/* NOTREACHED */
	return 0;
}
