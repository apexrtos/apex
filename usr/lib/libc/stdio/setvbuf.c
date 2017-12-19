/*-
 * Copyright (c) 1990, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Chris Torek.
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
 */

#include <stdio.h>
#include <stdlib.h>
#include "local.h"

/*
 * Set one of the three kinds of buffering, optionally including
 * a buffer.
 */
int
setvbuf(fp, buf, mode, size)
	FILE *fp;
	char *buf;
	int mode;
	size_t size;
{
	int ret, flags;
	/*	int ttyflag; */

	/*
	 * Verify arguments.  The `int' limit on `size' is due to this
	 * particular implementation.  Note, buf and size are ignored
	 * when setting _IONBF.
	 */
	if (mode != _IONBF)
		if ((mode != _IOFBF && mode != _IOLBF) || (int)size < 0)
			return (EOF);

	/*
	 * Write current buffer, if any.  Discard unread input (including
	 * ungetc data), cancel line buffering, and free old buffer if
	 * malloc()ed.  We also clear any eof condition, as if this were
	 * a seek.
	 */
	ret = 0;
	(void)__sflush(fp);
	if (HASUB(fp))
		FREEUB(fp);
	fp->_r = 0;
	flags = fp->_flags;
	if (buf != NULL && (flags & __SMBF))
		free((void *)fp->_bf._base);
	flags &= ~(__SLBF | __SNBF | __SMBF | __SEOF);

	/*
	 * Fix up the FILE fields, and set __cleanup for output flush on
	 * exit (since we are buffered in some way).
	 */
	if (mode == _IOLBF)
		flags |= __SLBF;
	fp->_flags = flags;

	/*
	 * If buffer is NULL, we change only buffering mode.
	 */
	if (buf == NULL)
		return 0;

	fp->_bf._base = fp->_p = (unsigned char *)buf;
	fp->_bf._size = size;
	fp->_w = 0;
	__cleanup = _cleanup;

	return (ret);
}
