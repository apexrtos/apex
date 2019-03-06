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

#ifndef kernel_h
#define kernel_h

#include <assert.h>
#include <conf/config.h>
#include <sys/include/types.h>

/*
 * Global variables in the kernel
 */
#if defined(KERNEL)
extern struct task	kern_task;	/* kernel task */
extern struct bootinfo	bootinfo;	/* boot information */
#endif

/*
 * Address translation
 */
static inline void *
phys_to_virt(const phys *pa)
{
#if defined(CONFIG_MMU)
	assert((uintptr_t)pa < CONFIG_PAGE_OFFSET);
#endif
	return (void *)((uintptr_t)pa + CONFIG_PAGE_OFFSET);
}

static inline phys *
virt_to_phys(const void *va)
{
#if defined(CONFIG_MMU)
	assert((uintptr_t)va >= CONFIG_PAGE_OFFSET);
#endif
	return (phys *)((uintptr_t)va - CONFIG_PAGE_OFFSET);
}

/*
 * Memory page
 */
#define PAGE_MASK	(CONFIG_PAGE_SIZE-1)
#define PAGE_ALIGN(n)	((__typeof__(n))(((((uintptr_t)(n)) + PAGE_MASK) & (uintptr_t)~PAGE_MASK)))
#define PAGE_TRUNC(n)	((__typeof__(n))((((uintptr_t)(n)) & (uintptr_t)~PAGE_MASK)))

/*
 * Align or truncate a pointer to a power of 2 boundary.
 */
#define ALIGNn(p, n)	((__typeof__(p))(((uintptr_t)(p) + ((n) - 1)) & (-n)))
#define	TRUNCn(p, n)	((__typeof__(p))(((uintptr_t)(p)) & -(n)))

/*
 * Align or truncate a pointer to a boundary suitable for storing all native
 * data types.
 */
#if UINTPTR_MAX == 0xffffffff
#define ALIGN(p)    ALIGNn(p, 4)
#define TRUNC(p)    TRUNCn(p, 4)
#else
#define ALIGN(p)    ALIGNn(p, 8)
#define TRUNC(p)    TRUNCn(p, 8)
#endif

/*
 * Calculate integer logarithm of an integer
 */
static inline unsigned
floor_log2(unsigned long n)
{
	assert(n != 0);
	return sizeof(n) * 8 - __builtin_clzl(n) - 1;
}

static inline unsigned
ceil_log2(unsigned long n)
{
	return floor_log2(n) + (n & (n - 1) ? 1 : 0);
}

/*
 * Integer division to closest integer
 */
static inline long
div_closest(long n, long d)
{
	if ((n > 0) == (d > 0))
		return (n + d / 2) / d;
	else
		return (n - d / 2) / d;
}

/*
 * Integer division to ceiling of quotient
 */
static inline long
div_ceil(long n, long d)
{
	if (n % d)
		return n / d + 1;
	else
		return n / d;
}

/*
 * Determine if integer is a non-zero power of 2.
 */
static inline bool
is_pow2(unsigned long v)
{
	return v && !(v & (v - 1));
}

#endif /* !kernel_h */
