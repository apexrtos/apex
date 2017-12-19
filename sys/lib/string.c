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

/*
 * string.c - Minimum string library for kernel
 */

#include <sys/types.h>
#include <kernel.h>

/*
 * Safer version of strncpy
 * The destination string is always terminated with NULL character.
 */
size_t
strlcpy(char *dest, const char *src, size_t count)
{
	char *d = dest;
	const char *s = src;
	size_t n = count;

	/* Copy as many bytes as will fit */
	if (n != 0 && --n != 0) {
		do {
			if ((*d++ = *s++) == 0)
				break;
		} while (--n != 0);
	}

	/* Not enough room in dst, add NUL and traverse rest of src */
	if (n == 0) {
		if (count != 0)
			*d = '\0';		/* NUL-terminate dst */
		while (*s++)
			;
	}

	return (size_t)(s - src - 1);	/* count does not include NUL */
}

int
strncmp(const char *src, const char *tgt, size_t count)
{
	signed char res = 0;

	while (count) {
		if ((res = *src - *tgt++) != 0 || !*src++)
			break;
		count--;
	}
	return res;
}

/* The returned size does not include the last NULL char */
size_t
strnlen(const char *str, size_t count)
{
	const char *tmp;

	for (tmp = str; count-- && *tmp != '\0'; ++tmp);
	return (size_t)(tmp - str);
}

void *
memcpy(void *dest, const void *src, size_t count)
{
	char *tmp = (char *)dest, *s = (char *)src;

	while (count--)
		*tmp++ = *s++;

	return dest;
}

void *
memset(void *dest, int ch, size_t count)
{
	char *p = (char *)dest;

	while (count--)
		*p++ = (char)ch;

	return dest;
}
