/*-
 * Copyright (c) 2005-2006, Kohsuke Ohtani
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

#ifndef _DEBUG_H
#define _DEBUG_H

#include <sys/cdefs.h>

#define LOGBUF_SIZE	2048	/* size of log buffer */ 
#define DBGMSG_SIZE	128	/* Size of one kernel message */

#ifdef DEBUG
#define DPRINTF(a)	printf a
#define ASSERT(exp)	do { if (!(exp)) \
			     assert(__FILE__, __LINE__, #exp); } while (0)
#else
#define DPRINTF(a)	do {} while (0)
#define ASSERT(exp)	do {} while (0)
#define panic(x)	machine_reset()
#endif

/*
 * Command for sys_debug()
 */
#define DCMD_DUMP	0
#define DCMD_LOGSIZE	1
#define DCMD_GETLOG	2

/*
 * Items for debug_dump()
 */
#define DUMP_THREAD	1
#define DUMP_TASK	2
#define DUMP_VM		3

#ifdef DEBUG
__BEGIN_DECLS
void	printf(const char *, ...);
void	panic(const char *);
int	debug_dump(int);
void	debug_attach(void (*)(char *));
void	assert(const char *, int, const char *);
int	debug_getlog(char *);
__END_DECLS
#endif

#endif /* !_DEBUG_H */
