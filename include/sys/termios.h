/*
 * Copyright (c) 1988, 1989, 1993, 1994
 *	The Regents of the University of California.  All rights reserved.
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
 *	@(#)termios.h	8.3 (Berkeley) 3/28/94
 */

#ifndef _SYS_TERMIOS_H_
#define _SYS_TERMIOS_H_

#include <sys/ioctl.h>

/*
 * Special Control Characters
 *
 * Index into c_cc[] character array.
 *
 *	Name	     Subscript	Enabled by
 */
#define	VEOF		0	/* ICANON */
#define	VEOL		1	/* ICANON */
#define	VEOL2		2	/* ICANON */
#define	VERASE		3	/* ICANON */
#define VWERASE 	4	/* ICANON */
#define VKILL		5	/* ICANON */
#define	VREPRINT 	6	/* ICANON */
/*			7	   spare 1 */
#define VINTR		8	/* ISIG */
#define VQUIT		9	/* ISIG */
#define VSUSP		10	/* ISIG */
#define VDSUSP		11	/* ISIG */
#define VSTART		12	/* IXON, IXOFF */
#define VSTOP		13	/* IXON, IXOFF */
#define	VLNEXT		14	/* IEXTEN */
#define	VDISCARD	15	/* IEXTEN */
#define VMIN		16	/* !ICANON */
#define VTIME		17	/* !ICANON */
#define VSTATUS		18	/* ICANON */
/*			19	   spare 2 */
#define	NCCS		20

#define _POSIX_VDISABLE	((unsigned char)'\377')

#define CCEQ(val, c)	(c == val ? val != _POSIX_VDISABLE : 0)

/*
 * Input flags - software input processing
 */
#define	IGNBRK		0x00000001	/* ignore BREAK condition */
#define	BRKINT		0x00000002	/* map BREAK to SIGINTR */
#define	IGNPAR		0x00000004	/* ignore (discard) parity errors */
#define	PARMRK		0x00000008	/* mark parity and framing errors */
#define	INPCK		0x00000010	/* enable checking of parity errors */
#define	ISTRIP		0x00000020	/* strip 8th bit off chars */
#define	INLCR		0x00000040	/* map NL into CR */
#define	IGNCR		0x00000080	/* ignore CR */
#define	ICRNL		0x00000100	/* map CR to NL (ala CRMOD) */
#define	IXON		0x00000200	/* enable output flow control */
#define	IXOFF		0x00000400	/* enable input flow control */
#define	IXANY		0x00000800	/* any char will restart after stop */
#define IMAXBEL		0x00002000	/* ring bell on input queue full */

/*
 * Output flags - software output processing
 */
#define	OPOST		0x00000001	/* enable following output processing */
#define ONLCR		0x00000002	/* map NL to CR-NL (ala CRMOD) */
#define OXTABS		0x00000004	/* expand tabs to spaces */
#define ONOEOT		0x00000008	/* discard EOT's (^D) on output) */

/*
 * Control flags - hardware control of terminal
 */
#define	CIGNORE		0x00000001	/* ignore control flags */
#define CSIZE		0x00000300	/* character size mask */
#define     CS5		    0x00000000	    /* 5 bits (pseudo) */
#define     CS6		    0x00000100	    /* 6 bits */
#define     CS7		    0x00000200	    /* 7 bits */
#define     CS8		    0x00000300	    /* 8 bits */
#define CSTOPB		0x00000400	/* send 2 stop bits */
#define CREAD		0x00000800	/* enable receiver */
#define PARENB		0x00001000	/* parity enable */
#define PARODD		0x00002000	/* odd parity, else even */
#define HUPCL		0x00004000	/* hang up on last close */
#define CLOCAL		0x00008000	/* ignore modem status lines */
#define CCTS_OFLOW	0x00010000	/* CTS flow control of output */
#define CRTSCTS		CCTS_OFLOW	/* ??? */
#define CRTS_IFLOW	0x00020000	/* RTS flow control of input */
#define	MDMBUF		0x00100000	/* flow control output via Carrier */


/*
 * "Local" flags - dumping ground for other state
 *
 * Warning: some flags in this structure begin with
 * the letter "I" and look like they belong in the
 * input flag.
 */

#ifndef _POSIX_SOURCE
#define	ECHOKE		0x00000001	/* visual erase for line kill */
#endif  /*_POSIX_SOURCE */
#define	ECHOE		0x00000002	/* visually erase chars */
#define	ECHOK		0x00000004	/* echo NL after line kill */
#define ECHO		0x00000008	/* enable echoing */
#define	ECHONL		0x00000010	/* echo NL even if ECHO is off */
#ifndef _POSIX_SOURCE
#define	ECHOPRT		0x00000020	/* visual erase mode for hardcopy */
#define ECHOCTL  	0x00000040	/* echo control chars as ^(Char) */
#endif  /*_POSIX_SOURCE */
#define	ISIG		0x00000080	/* enable signals INTR, QUIT, [D]SUSP */
#define	ICANON		0x00000100	/* canonicalize input lines */
#ifndef _POSIX_SOURCE
#define ALTWERASE	0x00000200	/* use alternate WERASE algorithm */
#endif  /*_POSIX_SOURCE */
#define	IEXTEN		0x00000400	/* enable DISCARD and LNEXT */
#define EXTPROC         0x00000800      /* external processing */
#define TOSTOP		0x00400000	/* stop background jobs from output */
#ifndef _POSIX_SOURCE
#define FLUSHO		0x00800000	/* output being flushed (state) */
#define	NOKERNINFO	0x02000000	/* no kernel output from VSTATUS */
#define PENDIN		0x20000000	/* XXX retype pending input (state) */
#endif  /*_POSIX_SOURCE */
#define	NOFLSH		0x80000000	/* don't flush after interrupt */

typedef unsigned long	tcflag_t;
typedef unsigned char	cc_t;
typedef long		speed_t;

struct termios {
	tcflag_t	c_iflag;	/* input flags */
	tcflag_t	c_oflag;	/* output flags */
	tcflag_t	c_cflag;	/* control flags */
	tcflag_t	c_lflag;	/* local flags */
	cc_t		c_cc[NCCS];	/* control chars */
	long		c_ispeed;	/* input speed */
	long		c_ospeed;	/* output speed */
};

/*
 * Commands passed to tcsetattr() for setting the termios structure.
 */
#define	TCSANOW		0		/* make change immediate */
#define	TCSADRAIN	1		/* drain output, then change */
#define	TCSAFLUSH	2		/* drain output, flush input */
#ifndef _POSIX_SOURCE
#define TCSASOFT	0x10		/* flag - don't alter h.w. state */
#endif

/*
 * Standard speeds
 */
#define B0	0
#define B50	50
#define B75	75
#define B110	110
#define B134	134
#define B150	150
#define B200	200
#define B300	300
#define B600	600
#define B1200	1200
#define	B1800	1800
#define B2400	2400
#define B4800	4800
#define B9600	9600
#define B19200	19200
#define B38400	38400

#ifndef KERNEL

#define	TCIFLUSH	1
#define	TCOFLUSH	2
#define TCIOFLUSH	3
#define	TCOOFF		1
#define	TCOON		2
#define TCIOFF		3
#define TCION		4

#include <sys/cdefs.h>

__BEGIN_DECLS
speed_t	cfgetispeed(const struct termios *);
speed_t	cfgetospeed(const struct termios *);
int	cfsetispeed(struct termios *, speed_t);
int	cfsetospeed(struct termios *, speed_t);
int	tcgetattr(int, struct termios *);
int	tcsetattr(int, int, const struct termios *);
int	tcdrain(int);
int	tcflow(int, int);
int	tcflush(int, int);
int	tcsendbreak(int, int);

#ifndef _POSIX_SOURCE
void	cfmakeraw(struct termios *);
int	cfsetspeed(struct termios *, speed_t);
#endif /* !_POSIX_SOURCE */
__END_DECLS

#endif /* !KERNEL */


/*
 * Tty ioctl's except for those supported only for backwards compatibility
 * with the old tty driver.
 */

/*
 * Window/terminal size structure.  This information is stored by the kernel
 * in order to provide a consistent interface, but is not used by the kernel.
 */
struct winsize {
	unsigned short	ws_row;		/* rows, in characters */
	unsigned short	ws_col;		/* columns, in characters */
	unsigned short	ws_xpixel;	/* horizontal size, pixels */
	unsigned short	ws_ypixel;	/* vertical size, pixels */
};

#define	TIOCMODG	_IOR('t', 3, int)	/* get modem control state */
#define	TIOCMODS	_IOW('t', 4, int)	/* set modem control state */
#define		TIOCM_LE	0001		/* line enable */
#define		TIOCM_DTR	0002		/* data terminal ready */
#define		TIOCM_RTS	0004		/* request to send */
#define		TIOCM_ST	0010		/* secondary transmit */
#define		TIOCM_SR	0020		/* secondary receive */
#define		TIOCM_CTS	0040		/* clear to send */
#define		TIOCM_CAR	0100		/* carrier detect */
#define		TIOCM_CD	TIOCM_CAR
#define		TIOCM_RNG	0200		/* ring */
#define		TIOCM_RI	TIOCM_RNG
#define		TIOCM_DSR	0400		/* data set ready */
						/* 8-10 compat */
#define	TIOCEXCL	 _IO('t', 13)		/* set exclusive use of tty */
#define	TIOCNXCL	 _IO('t', 14)		/* reset exclusive use of tty */
						/* 15 unused */
#define	TIOCFLUSH	_IOW('t', 16, int)	/* flush buffers */
						/* 17-18 compat */
#define	TIOCGETA	_IOR('t', 19, struct termios) /* get termios struct */
#define	TIOCSETA	_IOW('t', 20, struct termios) /* set termios struct */
#define	TIOCSETAW	_IOW('t', 21, struct termios) /* drain output, set */
#define	TIOCSETAF	_IOW('t', 22, struct termios) /* drn out, fls in, set */
#define	TIOCGETD	_IOR('t', 26, int)	/* get line discipline */
#define	TIOCSETD	_IOW('t', 27, int)	/* set line discipline */
						/* 127-124 compat */
#define	TIOCSBRK	 _IO('t', 123)		/* set break bit */
#define	TIOCCBRK	 _IO('t', 122)		/* clear break bit */
#define	TIOCSDTR	 _IO('t', 121)		/* set data terminal ready */
#define	TIOCCDTR	 _IO('t', 120)		/* clear data terminal ready */
#define	TIOCGPGRP	_IOR('t', 119, int)	/* get pgrp of tty */
#define	TIOCSPGRP	_IOW('t', 118, int)	/* set pgrp of tty */
						/* 117-116 compat */
#define	TIOCOUTQ	_IOR('t', 115, int)	/* output queue size */
#define	TIOCSTI		_IOW('t', 114, char)	/* simulate terminal input */
#define	TIOCNOTTY	 _IO('t', 113)		/* void tty association */
#define	TIOCPKT		_IOW('t', 112, int)	/* pty: set/clear packet mode */
#define		TIOCPKT_DATA		0x00	/* data packet */
#define		TIOCPKT_FLUSHREAD	0x01	/* flush packet */
#define		TIOCPKT_FLUSHWRITE	0x02	/* flush packet */
#define		TIOCPKT_STOP		0x04	/* stop output */
#define		TIOCPKT_START		0x08	/* start output */
#define		TIOCPKT_NOSTOP		0x10	/* no more ^S, ^Q */
#define		TIOCPKT_DOSTOP		0x20	/* now do ^S ^Q */
#define		TIOCPKT_IOCTL		0x40	/* state change of pty driver */
#define	TIOCSTOP	 _IO('t', 111)		/* stop output, like ^S */
#define	TIOCSTART	 _IO('t', 110)		/* start output, like ^Q */
#define	TIOCMSET	_IOW('t', 109, int)	/* set all modem bits */
#define	TIOCMBIS	_IOW('t', 108, int)	/* bis modem bits */
#define	TIOCMBIC	_IOW('t', 107, int)	/* bic modem bits */
#define	TIOCMGET	_IOR('t', 106, int)	/* get all modem bits */
#define	TIOCREMOTE	_IOW('t', 105, int)	/* remote input editing */
#define	TIOCGWINSZ	_IOR('t', 104, struct winsize)	/* get window size */
#define	TIOCSWINSZ	_IOW('t', 103, struct winsize)	/* set window size */
#define	TIOCUCNTL	_IOW('t', 102, int)	/* pty: set/clr usr cntl mode */
#define		UIOCCMD(n)	_IO('u', n)	/* usr cntl op "n" */
#define	TIOCCONS	_IOW('t', 98, int)	/* become virtual console */
#define	TIOCSCTTY	 _IO('t', 97)		/* become controlling tty */
#define	TIOCEXT		_IOW('t', 96, int)	/* pty: external processing */
#define	TIOCSIG		 _IO('t', 95)		/* pty: generate signal */
#define	TIOCDRAIN	 _IO('t', 94)		/* wait till output drained */

#define	TTYDISC		0		/* termios tty line discipline */
#define	TABLDISC	3		/* tablet discipline */
#define	SLIPDISC	4		/* serial IP discipline */

#define	TIOCSETSIGT	_IOW('t', 200, int)	/* set signal task */
#define TIOCINQ		_IOR('t', 201, int)	/* input queue size */

/*
 * Defaults on "first" open.
 */
#define	TTYDEF_IFLAG	(tcflag_t)(BRKINT | ICRNL | IXON | IXANY)
#define TTYDEF_OFLAG	(tcflag_t)(OPOST | ONLCR | OXTABS)
#define TTYDEF_LFLAG	(tcflag_t)(ECHO | ICANON | ISIG | ECHOE|ECHOK|ECHONL)
#define TTYDEF_CFLAG	(tcflag_t)(CREAD | CS8 | HUPCL)
#define TTYDEF_SPEED	(B9600)

/*
 * Control Character Defaults
 */
#define CTRL(x)	(cc_t)(x&037)
#define	CEOF		CTRL('d')
#define	CEOL		((unsigned char)'\377')	/* XXX avoid _POSIX_VDISABLE */
#define	CERASE		0177
#define	CINTR		CTRL('c')
#define	CSTATUS		((unsigned char)'\377')	/* XXX avoid _POSIX_VDISABLE */
#define	CKILL		CTRL('u')
#define	CMIN		1
#define	CQUIT		034		/* FS, ^\ */
#define	CSUSP		CTRL('z')
#define	CTIME		0
#define	CDSUSP		CTRL('y')
#define	CSTART		CTRL('q')
#define	CSTOP		CTRL('s')
#define	CLNEXT		CTRL('v')
#define	CDISCARD 	CTRL('o')
#define	CWERASE 	CTRL('w')
#define	CREPRINT 	CTRL('r')
#define	CEOT		CEOF
/* compat */
#define	CBRK		CEOL
#define CRPRNT		CREPRINT
#define	CFLUSH		CDISCARD

#ifdef KERNEL
/*
 * #define TTYDEFCHARS to include an array of default control characters.
 */
#define TTYDEFCHARS \
{ \
	CEOF,	CEOL,	CEOL,	CERASE, CWERASE, CKILL, CREPRINT, \
	_POSIX_VDISABLE, CINTR,	CQUIT,	CSUSP,	CDSUSP,	CSTART,	CSTOP,	CLNEXT, \
	CDISCARD, CMIN,	CTIME,  CSTATUS, _POSIX_VDISABLE \
}
#endif /* KERNEL */

/*
 * END OF PROTECTED INCLUDE.
 */
#endif /* !_SYS_TERMIOS_H_ */
