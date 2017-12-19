/*-
 * Copyright (c) 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Chris Torek and Darren F. Provine.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	@(#)tetris.c	8.1 (Berkeley) 5/31/93
 */

/* Modified for Prex by Kohsuke Ohtani. */

/*
 * Tetris (or however it is spelled).
 */

#include <sys/time.h>
#include <sys/param.h>
#ifdef __gba__
#include <prex/keycode.h>
#endif

#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "input.h"
#include "screen.h"
#include "tetris.h"

#ifdef __gba__
static char keys[6] = {K_LEFT, 'A', K_RGHT, K_DOWN, '\n', '\n'};
#else
static char *keys = "jkl pq";
#endif

cell	board[B_SIZE];		/* 1 => occupied, 0 => empty */
int	Rows, Cols;		/* current screen size */
int	score;			/* the obvious thing */
char	key_msg[100];
long	fallrate;		/* less than 1 million; smaller => faster */

void onintr(int);
void usage(void);

/*
 * Set up the initial board.  The bottom display row is completely set,
 * along with another (hidden) row underneath that.  Also, the left and
 * right edges are set.
 */
static void
setup_board(void)
{
	int i;
	cell *p;

	p = board;
	for (i = B_SIZE; i; i--)
		*p++ = i <= (2 * B_COLS) || (i % B_COLS) < 2;
}

/*
 * Elide any full active rows.
 */
static void
elide(void)
{
	int i, j, base;
	cell *p;

	for (i = A_FIRST; i < A_LAST; i++) {
		base = i * B_COLS + 1;
		p = &board[base];
		for (j = B_COLS - 2; *p++ != 0;) {
			if (--j <= 0) {
				/* this row is to be elided */
				bzero(&board[base], B_COLS - 2);
				scr_update();
				tsleep();
				while (--base != 0)
					board[base + B_COLS] = board[base];
				scr_update();
				tsleep();
				break;
			}
		}
	}
}

int
main(int argc, char *argv[])
{
	int pos, c;
	const struct shape *curshape;
	int level = 2;
#ifndef __gba__
	char key_write[6][10];
	int i;
#endif
	int ch;

	while ((ch = getopt(argc, argv, "l")) != EOF)
		switch(ch) {
		case 'l':
			level = atoi(optarg);
			if (level < MINLEVEL || level > MAXLEVEL) {
				(void)fprintf(stderr,
				    "tetris: level must be from %d to %d",
				    MINLEVEL, MAXLEVEL);
				exit(1);
			}
			break;
		case '?':
		default:
			usage();
		}

	argc -= optind;
	argv += optind;

	if (argc)
		usage();

	fallrate = 1000000 / level;

#ifdef __gba__
	/* No help line... */
#else
	for (i = 0; i <= 5; i++) {
		if (keys[i] == ' ')
			strcpy(key_write[i], "<space>");
		else {
			key_write[i][0] = keys[i];
			key_write[i][1] = '\0';
		}
	}

	sprintf(key_msg,
"%s - left   %s - rotate   %s - right   %s - drop   %s - pause   %s - quit",
		key_write[0], key_write[1], key_write[2], key_write[3],
		key_write[4], key_write[5]);
#endif

	(void)signal(SIGINT, onintr);
	scr_init();
	setup_board();

	srandom(getpid());
	scr_set();

	pos = A_FIRST*B_COLS + (B_COLS/2)-1;
	curshape = randshape();

	scr_msg(key_msg, 1);

	for (;;) {
		place(curshape, pos, 1);
		scr_update();

		place(curshape, pos, 0);
		c = tgetchar();
		if (c < 0) {
			/*
			 * Timeout.  Move down if possible.
			 */
			if (fits_in(curshape, pos + B_COLS)) {
				pos += B_COLS;
				continue;
			}

			/*
			 * Put up the current shape `permanently',
			 * bump score, and elide any full rows.
			 */
			place(curshape, pos, 1);
			score++;
			elide();

			/*
			 * Choose a new shape.  If it does not fit,
			 * the game is over.
			 */
			curshape = randshape();
			pos = A_FIRST*B_COLS + (B_COLS/2)-1;
			if (!fits_in(curshape, pos))
				break;
			continue;
		}

		/*
		 * Handle command keys.
		 */
		if (c == keys[5]) {
			/* quit */
			break;
		}
		if (c == keys[4]) {
			static char msg[] =
			    "paused - press RETURN to continue";

			place(curshape, pos, 1);
			do {
				scr_update();
				scr_msg(key_msg, 0);
				scr_msg(msg, 1);
				(void) fflush(stdout);
			} while (rwait((struct timeval *)NULL) == -1);
			scr_msg(msg, 0);
			scr_msg(key_msg, 1);
			place(curshape, pos, 0);
			continue;
		}
		if (c == keys[0]) {
			/* move left */
			if (fits_in(curshape, pos - 1))
				pos--;
			continue;
		}
		if (c == keys[1]) {
			/* turn */
			const struct shape *new = &shapes[curshape->rot];

			if (fits_in(new, pos))
				curshape = new;
			continue;
		}
		if (c == keys[2]) {
			/* move right */
			if (fits_in(curshape, pos + 1))
				pos++;
			continue;
		}
		if (c == keys[3]) {
			/* move to bottom */
			while (fits_in(curshape, pos + B_COLS)) {
				pos += B_COLS;
				score++;
			}
			continue;
		}
		if (c == '\f')
			scr_clear();
	}

	scr_clear();
	scr_end();

	(void)printf("Your score:  %d point%s  x  level %d  =  %d\n",
	    score, score == 1 ? "" : "s", level, score * level);
	exit(0);
}

void
onintr(int signo)
{
	scr_clear();
	scr_end();
	exit(0);
}

void
usage()
{
	(void)fprintf(stderr, "usage: tetris [-l level]\n");
	exit(1);
}
