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
 * fifo.c - test FIFO function
 */

#include <prex/prex.h>
#include <sys/fcntl.h>
#include <sys/stat.h>
#include <sys/errno.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

int
main(int argc, char *argv[])
{
	int pid, fd1, fd2, len;
	char buf1[256], buf2[256];

	printf("FIFO test program\n");

	if (mknod("/fifo/test", S_IFIFO | 0666, 0) == -1) {
		perror("mkfifo");
		exit(1);
	}
	pid = vfork();
	if (pid == -1) {
		perror("fork");
		exit(1);
	}
	if (pid == 0) {
		/* child */
		printf("child: task=%x\n", (int)task_self());

		fd1 = open("/fifo/test", O_RDONLY);
		if (fd1 == -1) {
			perror("open");
			exit(1);
		}
		for (;;) {
			printf("child: reading data from FIFO\n");
			len = read(fd1, buf1, sizeof(buf1) - 1);
			if (len == 0)
				break;
			buf1[len] = '\0';
			printf("child: length=%d data=%s\n", len, buf1);
		}
		close(fd1);
		printf("child: exit\n");
		exit(0);
	}
	printf("parent: task=%x\n", (int)task_self());

	fd2 = open("/fifo/test", O_WRONLY);
	if (fd2 == -1) {
		perror("open");
		exit(1);
	}
	for (;;) {
		printf("parent: please input string...\n");
		fgets(buf2, sizeof(buf2) - 1, stdin);
		if (feof(stdin))
			break;
		printf("parent: writing to FIFO\n");
		write(fd2, buf2, strlen(buf2));
	}
	close(fd2);
	printf("parent: exit\n");
	exit(0);
	return 0;
}
