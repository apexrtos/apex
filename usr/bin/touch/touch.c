/*
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

#include <sys/mount.h>
#include <sys/stat.h>
#include <sys/fcntl.h>

#include <unistd.h>
#include <err.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <limits.h>
#include <stdio.h>

#ifdef CMDBOX
#define main(argc, argv)	touch_main(argc, argv)
#endif

/* Flags */
#define TF_NOCREAT	0x01	/* Do not create file */

static void usage(void);
static int do_touch(char *, unsigned int);

int
main(int argc, char *argv[])
{
	unsigned int flags;
	int ch;

	flags = 0;
	while ((ch = getopt(argc, argv, "")) != -1)
		switch(ch) {
		case 'c':
			flags |= TF_NOCREAT;
			break;
		case '?':
		default:
			usage();
		}
	argc -= optind;
	argv += optind;

	if (argc == 0)
		usage();

	do {
		if (do_touch(*argv, flags))
			err(1, NULL);
		++argv;
	} while (*argv);
	exit(1);
}

static void
usage(void)
{
	fprintf(stderr, "usage: touch [-c] file...\n");
	exit(1);
}

static int
do_touch(char *file, unsigned int flags)
{
	struct stat st;
	int fd;

	if (stat(file, &st) < 0) {
		if (!(flags & TF_NOCREAT)) {
			if ((fd = creat(file, 0666)) < 0)
				return -1;
			close(fd);
		}
		return 0;
	}
	if ((fd = open(file, 2)) < 0)
		return -1;
	/* utime(file, NULL) */
	close(fd);
	return 0;
}
