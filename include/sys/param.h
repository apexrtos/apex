/*-
 * Copyright (c) 1982, 1986, 1989, 1993
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
 *	@(#)param.h	8.3 (Berkeley) 4/4/95
 */

#include <conf/config.h>

#ifndef LOCORE
#include <sys/types.h>
#endif

/*
 * Machine-independent constants (some used in following include files).
 * Redefined constants are from POSIX 1003.1 limits file.
 *
 * MAXCOMLEN should be >= sizeof(ac_comm) (see <acct.h>)
 * MAXLOGNAME should be >= UT_NAMESIZE (see <utmp.h>)
 */
#include <sys/syslimits.h>

#define	MAXCOMLEN	16		/* max command name remembered */
#define	MAXINTERP	32		/* max interpreter file name length */
#define	MAXLOGNAME	12		/* max login name length */
#define	MAXUPRC		CHILD_MAX	/* max simultaneous processes */
#define	NCARGS		ARG_MAX		/* max bytes for an exec function */
#define	NGROUPS		NGROUPS_MAX	/* max number groups */
#define	NOFILE		OPEN_MAX	/* max open files per process */
#define	NOGROUP		65535		/* marker for empty group set member */
#define MAXHOSTNAMELEN	32		/* max hostname size */

/* The following name length include a null-terminate charcter */
#define MAXTASKNAME	12		/* max task name */
#define MAXDEVNAME	12		/* max device name */
#define MAXOBJNAME	16		/* max object name */
#define MAXEVTNAME	12		/* max event name */

#define HZ		CONFIG_HZ	/* ticks per second */
#define KSTACK_SIZE	768		/* kernel stack size */
#define USTACK_SIZE	4096		/* user stack size */
#ifdef CONFIG_MMU
#define PAGE_SIZE	4096		/* bytes per page */
#else /* !CONFIG_MMU */
#define PAGE_SIZE	1024		/* bytes per page */
#endif /* !CONFIG_MMU */

#define PRIO_DFLT	200		/* default user priority */

#ifndef	NULL
#if !defined(__cplusplus)
#define	NULL	((void *)0)
#else
#define	NULL	0
#endif
#endif

/* More types and definitions used throughout the kernel. */
#ifdef KERNEL
#include <sys/cdefs.h>
#endif

#ifndef KERNEL
/* Signals. */
#include <sys/signal.h>
#endif

#include <machine/limits.h>

/*
 * Round p (pointer or byte index) up to a correctly-aligned value for all
 * data types (int, long, ...).   The result is u_long and must be cast to
 * any desired pointer type.
 */
#define	ALIGNBYTES	3
#define	ALIGN(p)	(((vaddr_t)(p) + ALIGNBYTES) &~ ALIGNBYTES)

/*
 * Memory page
 */
#define PAGE_MASK	(PAGE_SIZE-1)

#define PAGE_ALIGN(n)	((((vaddr_t)(n)) + PAGE_MASK) & (vaddr_t)~PAGE_MASK)
#define PAGE_TRUNC(n)	(((vaddr_t)(n)) & (vaddr_t)~PAGE_MASK)

/*
 * MAXPATHLEN defines the longest permissable path length after expanding
 * symbolic links. It is used to allocate a temporary buffer from the buffer
 * pool in which to do the name expansion, hence should be a power of two,
 * and must be less than or equal to MAXBSIZE.  MAXSYMLINKS defines the
 * maximum number of symbolic links that may be expanded in a path name.
 * It should be set high enough to allow all legitimate uses, but halt
 * infinite loops reasonably quickly.
 */
#define	MAXPATHLEN	PATH_MAX
#define MAXSYMLINKS	8

/* Bit map related macros. */
#define	setbit(a,i)	((a)[(i)/NBBY] |= 1<<((i)%NBBY))
#define	clrbit(a,i)	((a)[(i)/NBBY] &= ~(1<<((i)%NBBY)))
#define	isset(a,i)	((a)[(i)/NBBY] & (1<<((i)%NBBY)))
#define	isclr(a,i)	(((a)[(i)/NBBY] & (1<<((i)%NBBY))) == 0)

/* Macros for counting and rounding. */
#ifndef howmany
#define	howmany(x, y)	(((x)+((y)-1))/(y))
#endif
#define	roundup(x, y)	((((x)+((y)-1))/(y))*(y))
#define powerof2(x)	((((x)-1)&(x))==0)

/* Macros for min/max. */
#ifndef KERNEL
#define	MIN(a,b) (((a)<(b))?(a):(b))
#define	MAX(a,b) (((a)>(b))?(a):(b))
#endif

#define	BSIZE	512		/* size of secondary block (bytes) */
