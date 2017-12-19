/*
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
 * zero.c - test zero device driver.
 */

#include <prex/prex.h>
#include <stdio.h>
#include <string.h>

static char buf[100];

void
dump_buf(void)
{
	int i;

	for (i = 0; i < 100; i++)
		putchar(buf[i]);
	putchar('\n');
}

int
main(int argc, char *argv[])
{
	device_t zero_dev;
	int i, err;
	size_t len;

	printf("zero test\n");

	/* 0000000000111111111122222.... */
	for (i = 0; i < 100; i++)
		buf[i] = '0' + i / 10;
	dump_buf();

	err = device_open("zero", 0, &zero_dev);
	if (err)
		printf("device open error!\n");

	/* zero fill 50 character */
	len = 50;
	err = device_read(zero_dev, buf, &len, 0);
	if (err)
		printf("device read error!\n");	

	/* 5555555555666666666777... */
	dump_buf();

	return 0;
}
