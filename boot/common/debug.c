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
 * debug.c - loader debug functions
 */

#include <boot.h>

#ifdef DEBUG
/*
 * printf - print formated string
 */
void
printf(const char *fmt, ...)
{
	static const char digits[] = "0123456789abcdef";
	va_list ap;
	char buf[10];
	char *s;
	unsigned u;
	int c, i, pad;

	va_start(ap, fmt);
	while ((c = *fmt++)) {
		if (c == '%') {
			c = *fmt++;
			switch (c) {
			case 'c':
				machine_putc(va_arg(ap, int));
				continue;
			case 's':
				s = va_arg(ap, char *);
				if (s == NULL)
					s = "<NULL>";
				for (; *s; s++) {
					machine_putc((int)*s);
				}
				continue;
			case 'd':
				c = 'u';
			case 'u':
			case 'x':
				u = va_arg(ap, unsigned);
				s = buf;
				if (c == 'u') {
					do
						*s++ = digits[u % 10U];
					while (u /= 10U);
				} else {
					pad = 0;
					for (i = 0; i < 8; i++) {
						if (pad)
							*s++ = '0';
						else {
							*s++ = digits[u % 16U];
							if ((u /= 16U) == 0)
								pad = 1;
						}
					}
				}
				while (--s >= buf)
					machine_putc((int)*s);
				continue;
			}
		}
		machine_putc((int)c);
	}
	va_end(ap);
}
#endif /* DEBUG */

/*
 * Show error and hang up.
 */
void
panic(const char *msg)
{

#ifdef DEBUG
	printf("Panic: %s\n", msg);
#endif
	machine_panic();
	/* NOTREACHED */
}
