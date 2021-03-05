/*-
 * Copyright (c) 1982, 1986, 1993
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
 *	@(#)tty.h	8.7 (Berkeley) 1/9/95
 */

#pragma once

#include <cstddef>
#include <types.h>

struct tty;
typedef unsigned int tcflag_t;


typedef int (*tty_tproc)(struct tty *, tcflag_t);
typedef void (*tty_oproc)(struct tty *);
typedef void (*tty_iproc)(struct tty *);
typedef void (*tty_fproc)(struct tty *, int);

struct tty *tty_create(const char *, long attr, size_t bufsiz, size_t bufmin,
		       tty_tproc, tty_oproc, tty_iproc, tty_fproc, void *);
void	    tty_destroy(struct tty *);
void	   *tty_data(struct tty *);
long	    tty_speed(tcflag_t);

char	   *tty_rx_getbuf(struct tty *);
void	    tty_rx_putbuf(struct tty *, char *, size_t);
void	    tty_rx_putc(struct tty *, char);
void	    tty_rx_overflow(struct tty *);

int	    tty_tx_getc(struct tty *);
size_t	    tty_tx_getbuf(struct tty *, size_t, const void **);
bool	    tty_tx_empty(struct tty *);
void	    tty_tx_advance(struct tty *, size_t);
void	    tty_tx_complete(struct tty *);

