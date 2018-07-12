/*-
 * Copyright (c) 1982, 1986, 1990, 1991, 1993
 *	The Regents of the University of California.  All rights reserved.
 * (c) UNIX System Laboratories, Inc.
 * All or some portions of this file are derived from material licensed
 * to the University of California by American Telephone and Telegraph
 * Co. or Unix System Laboratories, Inc. and are reproduced herein with
 * the permission of UNIX System Laboratories, Inc.
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
 *	@(#)tty.c	8.13 (Berkeley) 1/9/95
 */

/* Modified for Prex by Kohsuke Ohtani. */

#include "tty.h"

#include <debug.h>
#include <device.h>
#include <errno.h>
#include <event.h>
#include <fcntl.h>
#include <fs.h>
#include <fs/file.h>
#include <fs/util.h>
#include <fs/vnode.h>
#include <ioctl.h>
#include <irq.h>
#include <kmem.h>
#include <sch.h>
#include <signal.h>
#include <stdatomic.h>
#include <string.h>
#include <sys/ttydefaults.h>
#include <task.h>
#include <termios.h>
#include <unistd.h>

#define ttydbg(...)

#define TTYQ_SIZE	512
#define TTYQ_HIWAT	502

struct tty_queue {
	char	tq_buf[TTYQ_SIZE];
	int	tq_head;
	int	tq_tail;
};

/*
 * TTY queue operations
 */
#define ttyq_index(i)	((i) & (TTYQ_SIZE - 1))
#define ttyq_p(q, loc)	((q)->tq_buf + ttyq_index((q)->tq_ ## loc))

/*
 * Per-tty structure.
 */
struct tty {
	struct tty_queue    t_rawq;	/* raw input queue */
	struct tty_queue    t_canq;	/* canonical queue */
	struct tty_queue    t_outq;	/* ouput queue */
	struct termios	    t_termios;	/* termios state */
	struct winsize	    t_winsize;	/* window size */
	struct event	    t_input;	/* event for input data ready */
	struct event	    t_output;	/* event for output completion */
	atomic_int	    t_state;	/* driver state */
	int		    t_column;	/* tty output column */
	pid_t		    t_pgid;	/* foreground process group. */
	int		    t_open;	/* open count */
	void		   *t_data;	/* instance data */
	void (*t_oproc)(struct tty *);	/* routine to start output */
	int  (*t_tproc)(struct tty *);	/* routine to configure tty */
};

/* These flags are kept in t_state. */
#define	TS_TTSTOP	    0x00001	/* Output paused. */

static void tty_output(struct tty *tp, int c);

#define is_ctrl(c)   ((c) < 32 || (c) == 0x7f)

/*
 * Test if tty queue is empty
 */
static bool
ttyq_empty(struct tty_queue *tq)
{
	return tq->tq_tail == tq->tq_head;
}

/*
 * Get size of tty queue
 */
static size_t
ttyq_count(struct tty_queue *tq)
{
	return tq->tq_tail - tq->tq_head;
}

/*
 * Test if tty queue is full
 */
static bool
ttyq_full(struct tty_queue *tq)
{
	return ttyq_count(tq) >= TTYQ_SIZE;
}

/*
 * Get a character from a queue.
 */
static int
ttyq_getc(struct tty_queue *tq)
{
	int c;

	const int s = irq_disable();
	if (ttyq_empty(tq)) {
		irq_restore(s);
		return -1;
	}
	c = *ttyq_p(tq, head);
	tq->tq_head++;
	irq_restore(s);
	return c;
}

/*
 * Put a character into a queue.
 */
static void
ttyq_putc(int c, struct tty_queue *tq)
{
	const int s = irq_disable();
	if (ttyq_full(tq)) {
		irq_restore(s);
		return;
	}
	*ttyq_p(tq, tail) = c;
	tq->tq_tail++;
	irq_restore(s);
}

/*
 * Remove the last character in a queue and return it.
 */
static int
ttyq_unputc(struct tty_queue *tq)
{
	int c;

	if (ttyq_empty(tq))
		return -1;
	const int s = irq_disable();
	tq->tq_tail--;
	c = *ttyq_p(tq, tail);
	irq_restore(s);
	return c;
}

/*
 * Put the chars in the from queue on the end of the to queue.
 */
static void
tty_catq(struct tty_queue *from, struct tty_queue *to)
{
	int c;

	while ((c = ttyq_getc(from)) != -1)
		ttyq_putc(c, to);
}

/*
 * Rubout one character from the rawq of tp
 */
static void
tty_rubout(struct tty *tp)
{
	if (!(tp->t_termios.c_lflag & ECHO))
		return;
	if (tp->t_termios.c_lflag & ECHOE) {
		tty_output(tp, '\b');
		tty_output(tp, ' ');
		tty_output(tp, '\b');
	} else
		tty_output(tp, tp->t_termios.c_cc[VERASE]);
}

/*
 * Echo char
 */
static void
tty_echo(int c, struct tty *tp)
{
	if (!(tp->t_termios.c_lflag & ECHO)) {
		if (c == '\n' && (tp->t_termios.c_lflag & ECHONL))
			tty_output(tp, '\n');
		return;
	}
	if (is_ctrl(c) && c != '\n' && c != '\t' && c != '\b') {
		tty_output(tp, '^');
		tty_output(tp, c + 'A' - 1);
	} else
		tty_output(tp, c);
}

/*
 * Start output.
 */
static void
tty_start(struct tty *tp)
{
	ttydbg("tty_start\n");

	if (tp->t_state & TS_TTSTOP)
		return;
	if (tp->t_oproc)
		(*tp->t_oproc)(tp);
}

/*
 * Flush tty read and/or write queues, notifying anyone waiting.
 */
static void
tty_flush(struct tty *tp, int io)
{
	ttydbg("tty_flush io=%d\n", rw);
	switch (io) {
	case TCIFLUSH:
		while (ttyq_getc(&tp->t_canq) != -1)
			;
		while (ttyq_getc(&tp->t_rawq) != -1)
			;
		break;
	case TCOFLUSH:
		tp->t_state &= ~TS_TTSTOP;
		tty_start(tp);
		break;
	}
}

/*
 * Wait for output to drain.
 */
static int
tty_wait(struct tty *tp)
{
	ttydbg("tty_wait\n");

	if ((!ttyq_empty(&tp->t_outq)) && tp->t_oproc) {
		while (1) {
			/* REVISIT: Is this really meant to smash TS_TTSTOP? */
			(*tp->t_oproc)(tp);
			if (ttyq_empty(&tp->t_outq))
				break;
			if (SLP_INTR == sch_sleep(&tp->t_output))
				return DERR(-EINTR);
		}
	}
	return 0;
}

/*
 * Get character from output queue
 */
int
tty_oq_getc(struct tty *tp)
{
	return ttyq_getc(&tp->t_outq);
}

/*
 * Get length of output queue
 */
size_t
tty_oq_count(struct tty *tp)
{
	return ttyq_count(&tp->t_outq);
}

/*
 * Test if output queue is empty
 */
bool
tty_oq_empty(struct tty *tp)
{
	return ttyq_empty(&tp->t_outq);
}

/*
 * Get size of linear tty buffer
 */
size_t
tty_oq_dmalen(struct tty *tp)
{
	struct tty_queue *tq = &tp->t_outq;
	const size_t count = ttyq_count(tq);
	const size_t lin = TTYQ_SIZE - ttyq_index(tq->tq_head);
	return count < lin ? count : lin;
}

/*
 * Output is completed. May be called from an ISR
 */
void
tty_oq_done(struct tty *tp)
{
	sch_wakeup(&tp->t_output, SLP_SUCCESS);
}

/*
 * Process input of a single character received on a tty.
 * echo if required.
 * This may be called with interrupt level.
 */
void
tty_input(struct tty *tp, int c)
{
	unsigned char *cc;
	tcflag_t iflag, lflag;

	lflag = tp->t_termios.c_lflag;
	iflag = tp->t_termios.c_iflag;

	if (!tp->t_open)
		return;

	cc = tp->t_termios.c_cc;

	/* IGNCR, ICRNL, INLCR */
	if (c == '\r') {
		if (iflag & IGNCR)
			goto endcase;
		else if (iflag & ICRNL)
			c = '\n';
	} else if (c == '\n' && (iflag & INLCR))
		c = '\r';

	if (iflag & IXON) {
		/* stop (^S) */
		if (c == cc[VSTOP]) {
			if (!(tp->t_state & TS_TTSTOP)) {
				tp->t_state |= TS_TTSTOP;
				return;
			}
			if (c != cc[VSTART])
				return;
			/* if VSTART == VSTOP then toggle */
			goto endcase;
		}
		/* start (^Q) */
		if (c == cc[VSTART])
			goto restartoutput;
	}
	if (lflag & ICANON) {
		/* erase (^H / ^?) or backspace */
		if (c == cc[VERASE] || c == '\b') {
			if (!ttyq_empty(&tp->t_rawq)) {
				ttyq_unputc(&tp->t_rawq);
				tty_rubout(tp);
			}
			goto endcase;
		}
		/* kill (^U) */
		if (c == cc[VKILL]) {
			while (!ttyq_empty(&tp->t_rawq)) {
				ttyq_unputc(&tp->t_rawq);
				tty_rubout(tp);
			}
			goto endcase;
		}
	}
	if (lflag & ISIG) {
		/* quit (^C) */
		if (c == cc[VINTR] || c == cc[VQUIT]) {
			if (!(lflag & NOFLSH)) {
				tty_flush(tp, TCIFLUSH);
				tty_flush(tp, TCOFLUSH);
			}
			tty_echo(c, tp);
			const int sig = (c == cc[VINTR]) ? SIGINT : SIGQUIT;
			if (tp->t_pgid > 0)
				kill(-tp->t_pgid, sig);
			goto endcase;
		}
		/* suspend (^Z) */
		if (c == cc[VSUSP]) {
			if (!(lflag & NOFLSH)) {
				tty_flush(tp, TCIFLUSH);
				tty_flush(tp, TCOFLUSH);
			}
			tty_echo(c, tp);
			const int sig = SIGTSTP;
			if (tp->t_pgid > 0)
				kill(-tp->t_pgid, sig);
			goto endcase;
		}
	}

	/*
	 * Check for input buffer overflow
	 */
	if (ttyq_full(&tp->t_rawq)) {
		dbg("tty overflow\n");
		tty_flush(tp, TCIFLUSH);
		tty_flush(tp, TCOFLUSH);
		goto endcase;
	}
	ttyq_putc(c, &tp->t_rawq);

	if (lflag & ICANON) {
		if (c == '\n' || c == cc[VEOF] || c == cc[VEOL]) {
			tty_catq(&tp->t_rawq, &tp->t_canq);
			sch_wakeone(&tp->t_input);
		}
	} else
		sch_wakeone(&tp->t_input);

	if (lflag & ECHO)
		tty_echo(c, tp);
endcase:
	/*
	 * IXANY means allow any character to restart output.
	 */
	if ((tp->t_state & TS_TTSTOP) && (iflag & IXANY) == 0 &&
	    cc[VSTART] != cc[VSTOP])
		return;
restartoutput:
	tp->t_state &= ~TS_TTSTOP;

	tty_start(tp);
}

/*
 * Output a single character on a tty, doing output processing
 * as needed (expanding tabs, newline processing, etc.).
 */
static void
tty_output(struct tty *tp, int c)
{
	int i, col;

	if ((tp->t_termios.c_lflag & ICANON) == 0) {
		ttyq_putc(c, &tp->t_outq);
		return;
	}
	/* Expand tab */
	if (c == '\t' && (tp->t_termios.c_oflag & TAB3)) {
		i = 8 - (tp->t_column & 7);
		tp->t_column += i;
		do {
			ttyq_putc(' ', &tp->t_outq);
		} while (--i > 0);
		return;
	}
	/* Translate newline into "\r\n" */
	if (c == '\n' && (tp->t_termios.c_oflag & ONLCR))
		ttyq_putc('\r', &tp->t_outq);

	ttyq_putc(c, &tp->t_outq);

	col = tp->t_column;
	switch (c) {
	case '\b':	/* back space */
		if (col > 0)
			--col;
		break;
	case '\t':	/* tab */
		col = (col + 8) & ~7;
		break;
	case '\n':	/* newline */
		col = 0;
		break;
	case '\r':	/* return */
		col = 0;
		break;
	default:
		if (!is_ctrl(c))
		    ++col;
		break;
	}
	tp->t_column = col;
	return;
}

/*
 * Process a open call on a tty device.
 */
static int
tty_open(struct file *f)
{
	int err;
	struct tty *tp = f->f_data;
	struct task *t = task_cur();

	if (!(f->f_flags & O_NOCTTY) && t->sid == task_pid(t))
		tp->t_pgid = task_pid(t);

	if ((tp->t_open++ == 0) && tp->t_tproc)
		if ((err = tp->t_tproc(tp)) < 0)
			return err;

	return 0;
}

/*
 * Process a close call on a tty device.
 */
static int
tty_close(struct file *file)
{
	struct tty *tp = file->f_data;

	if (--tp->t_open)
		return 0;

	tp->t_pgid = 0;

	return 0;
}

/*
 * Process a read call on a tty device.
 */
static ssize_t
tty_read(struct file *file, void *buf, size_t len)
{
	struct tty *tp = file->f_data;
	unsigned char *cc;
	struct tty_queue *qp;
	int rc, c;
	size_t count = 0;
	tcflag_t lflag;
	char *cbuf = buf;

	lflag = tp->t_termios.c_lflag;
	cc = tp->t_termios.c_cc;
	qp = (lflag & ICANON) ? &tp->t_canq : &tp->t_rawq;

	/* tstchar calls with a NULL buffer so need to
	 * do this before checking buf */
	if ((file->f_flags & O_NONBLOCK) && ttyq_empty(qp))
		return DERR(-EAGAIN);

	/* If there is no input, wait it */
	while (ttyq_empty(qp)) {
		vn_unlock(file->f_vnode);
		rc = sch_sleep(&tp->t_input);
		vn_lock(file->f_vnode);
		if (rc == SLP_INTR)
			return -EINTR;
	}
	while (count < len) {
		if ((c = ttyq_getc(qp)) == -1)
			break;
		if (c == cc[VEOF] && (lflag & ICANON))
			break;
		count++;
		*cbuf = c;
		if ((lflag & ICANON) && (c == '\n' || c == cc[VEOL]))
			break;
		cbuf++;
	}
	return count;
}

static ssize_t tty_read_iov(struct file *file, const struct iovec *iov,
    size_t count)
{
	return for_each_iov(file, iov, count, tty_read);
}

/*
 * Process a write call on a tty device.
 */
static ssize_t
tty_write(struct file *file, void *buf, size_t len)
{
	struct tty *tp = file->f_data;
	size_t remain, count = 0;
	const char *cbuf = buf;

	ttydbg("tty_write\n");

	remain = len;
	while (remain > 0) {
		if (ttyq_count(&tp->t_outq) > TTYQ_HIWAT) {
			tty_start(tp);
			if (ttyq_count(&tp->t_outq) <= TTYQ_HIWAT)
				continue;
			if (file->f_flags & O_NONBLOCK) {
				if (count == 0)
					return DERR(-EAGAIN);
				else
					break;
			}
			int rc = sch_sleep(&tp->t_output);
			if (rc == SLP_INTR) {
				if (count == 0)
					count = DERR(-EINTR);
				/* still do tty_start() if EINTR */
				break;
			}
			continue;
		}
		tty_output(tp, *cbuf);
		cbuf++;
		remain--;
		count++;
	}
	tty_start(tp);
	return count;
}

static ssize_t tty_write_iov(struct file *file, const struct iovec *iov,
    size_t count)
{
	return for_each_iov(file, iov, count, tty_write);
}

/*
 * Ioctls for all tty devices.
 */
static int
tty_ioctl(struct file *file, u_long cmd, void *data)
{
	struct tty *tp = file->f_data;
	int *intarg = data;
	int ret;
	char c;

	ttydbg("tty_ioctl\n");

	switch (cmd) {
	case TCGETS:
		memcpy(data, &tp->t_termios, sizeof(struct termios));
		break;
	case TCSETSW:
	case TCSETSF:
		if (tty_wait(tp) < 0)
			return DERR(-EINTR);
		if (cmd == TCSETSF)
			tty_flush(tp, TCIFLUSH);
		/* FALLTHROUGH */
	case TCSETS:
		memcpy(&tp->t_termios, data, sizeof(struct termios));

		/* reconfigure the driver */
		if (tp->t_tproc)
			return tp->t_tproc(tp);
		break;
	case TIOCSPGRP:			/* set pgrp of tty */
		memcpy(&tp->t_pgid, data, sizeof(pid_t));
		break;
	case TIOCGPGRP:
		memcpy(data, &tp->t_pgid, sizeof(pid_t));
		break;
	case TCFLSH:
		tty_flush(tp, *intarg);
		break;
	case TCXONC:
		switch (*intarg) {
		case TCOOFF:
			/* suspend output */
			tp->t_state |= TS_TTSTOP;
			break;
		case TCOON:
			/* restart output */
			if (tp->t_state & TS_TTSTOP) {
				tp->t_state &= ~TS_TTSTOP;
				tty_start(tp);
			}
			break;
		case TCIOFF:
			/* transmit stop character */
		case TCION:
			/* transmit start character */
			c = tp->t_termios.c_cc[*intarg == TCIOFF ? VSTOP : VSTART];
			if (c == _POSIX_VDISABLE)
				break;
			if ((ret = tty_write(file, &c, sizeof(c))) < 0)
				return ret;
			break;
		default:
			return DERR(-EINVAL);
		}
		break;
	case TIOCGWINSZ:
		memcpy(data, &tp->t_winsize, sizeof(struct winsize));
		break;
	case TIOCSWINSZ:
		memcpy(&tp->t_winsize, data, sizeof(struct winsize));
		break;
	case TIOCINQ:
		*intarg = ttyq_count((tp->t_termios.c_lflag & ICANON) ? &tp->t_canq : &tp->t_rawq);
		break;
	case TIOCOUTQ:
		*intarg = ttyq_count(&tp->t_outq);
		break;
	}
	return 0;
}

/*
 * Create TTY device
 */
struct tty *
tty_create(const char *name, int (*tproc)(struct tty *),
    void (*oproc)(struct tty *), void *data)
{
	struct tty *tp = kmem_alloc(sizeof(struct tty), MEM_NORMAL);
	memset(tp, 0, sizeof(struct tty));

	event_init(&tp->t_input, "TTY input", ev_IO);
	event_init(&tp->t_output, "TTY output", ev_IO);

	tp->t_termios.c_iflag = TTYDEF_IFLAG;
	tp->t_termios.c_oflag = TTYDEF_OFLAG;
	tp->t_termios.c_cflag = TTYDEF_CFLAG | TTYDEF_SPEED;
	tp->t_termios.c_lflag = TTYDEF_LFLAG;
	tp->t_termios.c_cc[VINTR] = CINTR;
	tp->t_termios.c_cc[VQUIT] = CQUIT;
	tp->t_termios.c_cc[VERASE] = CERASE;
	tp->t_termios.c_cc[VKILL] = CKILL;
	tp->t_termios.c_cc[VEOF] = CEOF;
	tp->t_termios.c_cc[VTIME] = CTIME;
	tp->t_termios.c_cc[VMIN] = CMIN;
/*	tp->t_termios.c_cc[VSWTC] = REVISIT: is there a default here? */
	tp->t_termios.c_cc[VSTART] = CSTART;
	tp->t_termios.c_cc[VSTOP] = CSTOP;
	tp->t_termios.c_cc[VSUSP] = CSUSP;
	tp->t_termios.c_cc[VEOL] = CEOL;
	tp->t_termios.c_cc[VREPRINT] = CREPRINT;
	tp->t_termios.c_cc[VDISCARD] = CDISCARD;
	tp->t_termios.c_cc[VWERASE] = CWERASE;
	tp->t_termios.c_cc[VLNEXT] = CLNEXT;
/*	tp->t_termios.c_cc[VEOL2] = REVISIT: is there a default here? */
	tp->t_data = data;
	tp->t_tproc = tproc;
	tp->t_oproc = oproc;

	static struct devio tty_io = {
		.open = tty_open,
		.close = tty_close,
		.read = tty_read_iov,
		.write = tty_write_iov,
		.ioctl = tty_ioctl,
	};
	device_create(&tty_io, name, DF_CHR, tp);

	return tp;
}

/*
 * Retrieve TTY driver data
 */
void *
tty_data(struct tty *tp)
{
	return tp->t_data;
}

/*
 * Retrieve termios struct for TTY
 */
const struct termios *
tty_termios(struct tty *tp)
{
	return &tp->t_termios;
}

/*
 * Convert enumerated baud rate to real speed
 */
long
tty_speed(tcflag_t b)
{
	switch (b & CBAUD) {
	case B0: return 0;
	case B50: return 50;
	case B75: return 75;
	case B110: return 110;
	case B134: return 134;
	case B150: return 150;
	case B200: return 200;
	case B300: return 300;
	case B600: return 600;
	case B1200: return 1200;
	case B1800: return 1800;
	case B2400: return 2400;
	case B4800: return 4800;
	case B9600: return 9600;
	case B19200: return 19200;
	case B38400: return 38400;
	case B57600: return 57600;
	case B115200: return 115200;
	case B230400: return 230400;
	case B460800: return 460800;
	case B500000: return 500000;
	case B576000: return 576000;
	case B921600: return 921600;
	case B1000000: return 1000000;
	case B1152000: return 1152000;
	case B1500000: return 1500000;
	case B2000000: return 2000000;
	case B2500000: return 2500000;
	case B3000000: return 3000000;
	case B3500000: return 3500000;
	case B4000000: return 4000000;
	default: return -1;
	}
}
