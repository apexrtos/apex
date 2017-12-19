/*
 * Copyright (c) 1983, 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
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

#include <sys/mount.h>

#include <unistd.h>
#include <err.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>

#ifdef CMDBOX
#define main(argc, argv)	mkdir_main(argc, argv)
#endif

static int	build(char *);
static void	usage(void);

int
main(int argc, char *argv[])
{
	int ch, exitval, pflag;

	pflag = 0;
	while ((ch = getopt(argc, argv, "p")) != EOF)
		switch(ch) {
		case 'p':
			pflag = 1;
			break;
		case '?':
		default:
			usage();
		}
	argc -= optind;
	argv += optind;

	if (argv[0] == NULL)
		usage();

	for (exitval = 0; *argv != NULL; ++argv) {
		if (pflag && build(*argv)) {
			exitval = 1;
			continue;
		}
		if (mkdir(*argv, S_IRWXU | S_IRWXG | S_IRWXO) < 0) {
			warn("%s", *argv);
			exitval = 1;
		}
	}
	exit(exitval);
	/* NOTREACHED */
}

static int
build(path)
	char *path;
{
	struct stat sb;
	char *p;

	p = path;
	if (p[0] == '/')		/* Skip leading '/'. */
		++p;
	for (;; ++p) {
		if (p[0] == '\0' || (p[0] == '/' && p[1] == '\0'))
			break;
		if (p[0] != '/')
			continue;
		*p = '\0';
		if (stat(path, &sb)) {
			if (errno != ENOENT ||
			    mkdir(path, S_IRWXU | S_IRWXG | S_IRWXO) < 0) {
				warn("%s", path);
				return 1;
			}
		}
		*p = '/';
	}
	return 0;
}

static void
usage()
{
	(void)fprintf(stderr, "usage: mkdir [-p] directory ...\n");
	exit (1);
}
