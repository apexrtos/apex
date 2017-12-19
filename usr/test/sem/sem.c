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
 * sem.c - test program for semaphore.
 */

#include <prex/prex.h>
#include <stdio.h>

int
main(int argc, char *argv[])
{
	sem_t sem;
	int err;
	u_int val;

	printf("Semaphore test program\n");

	/*
	 * Test initialize
	 */
	sem_init(&sem, 3);

	/*
	 * Test wait
	 */
	err = sem_getvalue(&sem, &val);
	printf("Semaphore value=%d\n", val);

	printf("1) Wait semahore\n");
	err = sem_wait(&sem, 0);
	if (err)
		panic("wait err");

	err = sem_getvalue(&sem, &val);
	printf("Semaphore value=%d\n", val);

	printf("2) Wait semahore\n");
	err = sem_wait(&sem, 0);
	if (err)
		panic("wait err");

	err = sem_getvalue(&sem, &val);
	printf("Semaphore value=%d\n", val);

	printf("3) Wait semahore\n");
	err = sem_wait(&sem, 0);
	if (err)
		panic("wait err");

	err = sem_getvalue(&sem, &val);
	printf("Semaphore value=%d\n", val);

#if 0
	/*
	 * Sleep by wait
	 */
	printf("4) Wait semahore\n");
	err = sem_wait(&sem, 0);
	if (err)
		panic("wait err");
#endif
	/*
	 * trywait must be error.
	 */
	printf("4e) Try to wait semahore\n");
	err = sem_trywait(&sem);
	printf("Try wait err=%d\n", err);

	/*
	 * Test post
	 */
	printf("5) Post semahore\n");
	err = sem_post(&sem);
	if (err)
		panic("post err");
	err = sem_getvalue(&sem, &val);
	printf("Semaphore value=%d\n", val);

	printf("Test complete\n");
	return 0;
}
