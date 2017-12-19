/*-
 * Copyright (c) 2005, Kohsuke Ohtani
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
 * vsprintf.c - Format and output data to buffer
 */

#include <kernel.h>
#include <sys/types.h>

#define isdigit(c)  ((u_int)((c) - '0') < 10)

static u_long
divide(long *n, int base)
{
	u_long res;

	res = ((u_long)*n) % base;
	*n = ((u_long)*n) / base;
	return res;
}

static int
atoi(const char **s)
{
	int i = 0;
	while (isdigit((int)**s))
		i = i * 10 + *((*s)++) - '0';
	return i;
}

/*
 * Print formatted output - scaled down version
 *
 * Identifiers:
 *  %d - Decimal signed int
 *  %x - Hex integer
 *  %u - Unsigned integer
 *  %c - Character
 *  %s - String
 *
 * Flags:
 *   0 - Zero pad
 */
int
vsprintf(char *buf, const char *fmt, va_list args)
{
	char *p, *str;
	const char *digits = "0123456789abcdef";
	char pad, tmp[16];
	int width, base, sign, i;
	long num;

	for (p = buf; *fmt; fmt++) {
		if (*fmt != '%') {
			*p++ = *fmt;
			continue;
		}
		/* get flags */
		++fmt;
		pad = ' ';
		if (*fmt == '0') {
			pad = '0';
			fmt++;
		}
		/* get width */
		width = -1;
		if (isdigit(*fmt)) {
			width = atoi(&fmt);
		}
		base = 10;
		sign = 0;
		switch (*fmt) {
		case 'c':
			*p++ = (char)va_arg(args, int);
			continue;
		case 's':
			str = va_arg(args, char *);
			if (str == NULL)
				str = "<NULL>";
			for (; *str && width != 0; str++, width--) {
				*p++ = *str;
			}
			while (width-- > 0)
				*p++ = pad;
			continue;
		case 'X':
		case 'x':
			base = 16;
			break;
		case 'd':
			sign = 1;
			break;
		case 'u':
			break;
		default:
			continue;
		}
		num = va_arg(args, long);
		if (sign && num < 0) {
			num = -num;
			*p++ = '-';
			width--;
		}
		i = 0;
		if (num == 0)
			tmp[i++] = '0';
		else
			while (num != 0)
				tmp[i++] = digits[divide(&num, base)];
		width -= i;
		while (width-- > 0)
			*p++ = pad;
		while (i-- > 0)
			*p++ = tmp[i];
	}
	*p = '\0';
	return 0;
}
