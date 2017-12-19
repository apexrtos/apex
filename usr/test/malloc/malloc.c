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
 * malloc.c - malloc test program.
 */

#include <stdlib.h>
#include <stdio.h>

#define NR_ALLOCS	30

static void *ptr[NR_ALLOCS];

static char *
alloc(int buflen)
{
	char *p, *q;
	int i;

	printf("Allocate %d bytes - ", buflen);

	p = malloc(buflen);
	if (p == NULL) {
		printf("Error: malloc() returns NULL!\n");
		return NULL;
	}
	for (q = p, i = 0; i < buflen; i++)
		*(q++) = '@';
#if 0
	for (q = p, i = 0; i < buflen; i++)
		putchar(*(q++));
#endif
	printf("OK!\n");
	return p;
}

static void
test_1(void)
{
	char *p;

	printf("test_1 - start\n");

	p = alloc(1);
	free(p);
	p = alloc(2);
	free(p);
	p = alloc(256);
	free(p);
	p = alloc(1024);
	free(p);
	p = alloc(8096);
	free(p);
	p = alloc(-1);
	free(p);

	printf("test_1 - done\n");
}

static void
test_2(void)
{
	int i, j;

	printf("test_2 - start\n");

	for (i = 0; i < NR_ALLOCS; i++)
		ptr[i] = alloc(random() & 0xf);
	for (i = 0; i < NR_ALLOCS; i++)
		free(ptr[i]);

	for (i = 0; i < NR_ALLOCS; i++)
		ptr[i] = alloc(random() & 0xff);
	for (i = 0; i < NR_ALLOCS; i++)
		free(ptr[i]);

	for (i = 0; i < NR_ALLOCS; i++)
		ptr[i] = alloc(random() & 0xfff);
	for (i = 0; i < NR_ALLOCS; i++)
		free(ptr[i]);

	for (i = 0; i < NR_ALLOCS; i++)
		ptr[i] = alloc(random() & 0xfff);
	for (i = 0; i < 10000; i++) {
		j = random() % NR_ALLOCS;
		if (ptr[j] != NULL) {
			free(ptr[j]);
			ptr[j] = NULL;
		}
	}
	printf("test_2 - done\n");
}

static void
test_3(void)
{
	char *p;

	printf("test_3 - start\n");

	p = alloc(256);
	free(p);

	printf("test_3 - try to free invalid area...\n");
	free(p); /* invalid */

	printf("test_3 - done!?\n");
}

int
main(int argc, char *argv[])
{
	printf("Malloc test program.\n");

	test_1();
	test_2();
	test_3();

	return 0;
}
