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
 *	@(#)screen.c	8.1 (Berkeley) 5/31/93
 */

/* Modified for Prex by Kohsuke Ohtani. */

/*
 * Tetris screen control.
 */

#include <sys/tty.h>
#include <sys/ioctl.h>

#include <setjmp.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "screen.h"
#include "tetris.h"

static cell curscreen[B_SIZE];	/* 1 => standout (or otherwise marked) */
static int curscore;
static int isset;		/* true => terminal is in game mode */
static struct termios oldtt;

/*
 * putstr() is for unpadded strings (either as in termcap(5) or
 * simply literal strings); putpad() is for padded strings with
 * count=1.  (See screen.h for putpad().)
 */
#define	putstr(s)	printf("%s", s)
#define	moveto(r, c)	printf("\33[%d;%dH", r, c)

/*
 * Set up from termcap.
 */
void
scr_init()
{
}

/*
 * Set up screen mode.
 */
void
scr_set(void)
{
	struct winsize ws;
	struct termios newtt;

	Rows = 0, Cols = 0;
	if (ioctl(0, TIOCGWINSZ, &ws) == 0) {
		Rows = ws.ws_row;
		Cols = ws.ws_col;
	}
	if (Rows == 0)
		Rows = 25;
	if (Cols == 0)
		Cols = 80;

	if (tcgetattr(0, &oldtt) < 0)
		stop("tcgetattr() fails");
	newtt = oldtt;
	newtt.c_lflag &= ~(ICANON|ECHO);
	newtt.c_oflag &= ~OXTABS;
	if (tcsetattr(0, TCSADRAIN, &newtt) < 0)
		stop("tcsetattr() fails");

	isset = 1;
	scr_clear();
}

/*
 * End screen mode.
 */
void
scr_end(void)
{

	moveto(Rows - 1, 0);

	/* exit screen mode */
	(void) fflush(stdout);
	(void) tcsetattr(0, TCSADRAIN, &oldtt);
	isset = 0;
}

void
stop(char *why)
{

	if (isset)
		scr_end();
	(void) fprintf(stderr, "aborting: %s\n", why);
	exit(1);
}

/*
 * Clear the screen, forgetting the current contents in the process.
 */
void
scr_clear(void)
{

	printf("\33[2J");
	curscore = -1;
	bzero((char *)curscreen, sizeof(curscreen));
}

#if !__GNUC__
typedef int regcell;	/* pcc is bad at `register char', etc */
#else
typedef cell regcell;
#endif

/*
 * Update the screen.
 */
void
scr_update(void)
{
	cell *bp, *sp;
	regcell so, cur_so = 0;
	int i, ccol, j;

	/* always leave cursor after last displayed point */
	curscreen[D_LAST * B_COLS - 1] = -1;

	if (score != curscore) {
		moveto(0, 0);
		printf("%d", score);
		curscore = score;
	}

	bp = &board[D_FIRST * B_COLS];
	sp = &curscreen[D_FIRST * B_COLS];
	for (j = D_FIRST; j < D_LAST; j++) {
		ccol = -1;
		for (i = 0; i < B_COLS; bp++, sp++, i++) {
			if (*sp == (so = *bp))
				continue;
			*sp = so;
			if (i != ccol) {
				if (cur_so) {
					cur_so = 0;
				}
				moveto(RTOD(j), CTOD(i));
			}
			if (so != cur_so) {
				cur_so = so;
			}
			putstr(so ? "XX" : "  ");

			ccol = i + 1;
			/*
			 * Look ahead a bit, to avoid extra motion if
			 * we will be redrawing the cell after the next.
			 * Motion probably takes four or more characters,
			 * so we save even if we rewrite two cells
			 * `unnecessarily'.  Skip it all, though, if
			 * the next cell is a different color.
			 */
#define	STOP (B_COLS - 3)
			if (i > STOP || sp[1] != bp[1] || so != bp[1])
				continue;
			if (sp[2] != bp[2])
				sp[1] = -1;
			else if (i < STOP && so == bp[2] && sp[3] != bp[3]) {
				sp[2] = -1;
				sp[1] = -1;
			}
		}
	}
	(void) fflush(stdout);
}

/*
 * Write a message (set!=0), or clear the same message (set==0).
 * (We need its length in case we have to overwrite with blanks.)
 */
void
scr_msg(char *s, int set)
{

	int l = strlen(s);

	moveto(Rows - 2, ((Cols - l) >> 1) - 1);
	if (set)
		putstr(s);
	else
		while (--l >= 0)
			(void) putchar(' ');
}
