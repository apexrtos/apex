/*
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

#include <sys/stat.h>

#include <errno.h>
#include <err.h>
#include <string.h>
#include <stdio.h>
#include <limits.h>
#include <stdlib.h>

#ifdef CMDBOX
#define main(argc, argv)	mv_main(argc, argv)
#endif

int
main(int argc, char *argv[])
{
	char path[PATH_MAX];
	char *src, *dest, *p;
	struct stat st1, st2;
	int rc;

	if (argc != 3) {
		fprintf(stderr, "usage: mv src dest\n");
		exit(1);
	}
	src = argv[1];
	dest = argv[2];

	/* Check if source exists and it's regular file. */
	if (stat(src, &st1) < 0)
		err(1,"mv");
	if (!S_ISREG(st1.st_mode)) {
		fprintf(stderr, "mv: invalid file type\n");
		exit(1);
	}

	/* Check if target is a directory. */
	rc = stat(dest, &st2);
	if (!rc && S_ISDIR(st2.st_mode)) {
		p = strrchr(src, '/');
		p = p ? p + 1 : src;
		strcpy(path, dest);
		if (strcmp(dest, "/"))
			strcat(path, "/");
		strcat(path, p);
		dest = path;
	}
	if (rename(src, dest) < 0)
		err(1,"rename");
	return 0;
}
