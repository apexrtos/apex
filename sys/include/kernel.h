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

#pragma once

#include <bit>
#include <cassert>
#include <conf/config.h>
#include <sys/include/types.h>
#include <type_traits>

/*
 * Global variables in the kernel
 */
#if defined(KERNEL)
extern struct task	kern_task;	/* kernel task */
#endif

/*
 * Memory page
 */
#define PAGE_SIZE	((unsigned)(CONFIG_PAGE_SIZE))
#define PAGE_MASK	((uintptr_t)PAGE_SIZE - 1)
#define PAGE_OFF(n)	((unsigned)((intmax_t)(n) & PAGE_MASK))
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
 * Test if pointer has approprate alignment for type
 */
#define ALIGNEDn(p, n) (!((uintptr_t)(p) & (n - 1)))
#define ALIGNED(p, t) ALIGNEDn(p, alignof(t))

/*
 * Calculate integer logarithm of an integer
 */
template<class T>
constexpr unsigned
floor_log2(T n)
{
	assert(n > 0);
	return std::bit_width(n) - 1;
}

template<class T>
constexpr unsigned
ceil_log2(T n)
{
	assert(n > 0);
	return std::bit_width(n) - std::has_single_bit(n);
}

/*
 * Integer division to closest integer
 */
template<class T>
constexpr std::enable_if_t<std::is_integral_v<T>, T>
div_closest(T n, T d)
{
	if ((n > 0) == (d > 0))
		return (n + d / 2) / d;
	else
		return (n - d / 2) / d;
}

/*
 * Integer division to ceiling of quotient
 */
template<class T>
constexpr std::enable_if_t<std::is_integral_v<T>, T>
div_ceil(T n, T d)
{
	if (n % d)
		return n / d + 1;
	else
		return n / d;
}

/*
 * Determine if integer is a non-zero power of 2.
 */
template<class T>
constexpr bool
is_pow2(T v)
{
	return std::has_single_bit(v);
}
