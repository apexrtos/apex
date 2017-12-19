/*
 * Copyright (c) 2008, Kohsuke Ohtani
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
 * pipe.c - test unix pipe
 */

#include <prex/prex.h>
#include <sys/fcntl.h>
#include <sys/stat.h>
#include <sys/errno.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#if 0
static void
test1(void)
{
	int fd[2];
	char str[] = "test1";
	char buf[256];

	printf("pipe test program\n");

	if (pipe(fd) == -1) {
		perror("pipe");
		exit(1);
	}
	write(fd[1], str, sizeof(str));
	read(fd[0], buf, sizeof(buf));
	printf("str=%s\n", buf);
}
#endif

static void
test2(void)
{
	int fd[2];
	char str[] = "test2: hello!";
	char buf[256];

	printf("pipe test program\n");

	if (pipe(fd) == -1) {
		perror("pipe");
		exit(1);
	}
	switch(vfork()) {
	case -1:
		perror("fork");
		break;

	case 0: /* child */
		close(fd[1]);
		read(fd[0], buf, 256);
		close(fd[0]);
		printf("str=%s\n", buf);
		exit(0);
		break;

	default: /* parent */
		close(fd[0]);
		write(fd[1], str, sizeof(str));
		close(fd[1]);
		exit(0);
		break;
	}
}

int
main(int argc, char *argv[])
{
#if 0
	test1();
#endif
	test2();
	return 0;
}
