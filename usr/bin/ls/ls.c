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

#include <unistd.h>
#include <err.h>
#include <string.h>
#include <stdlib.h>
#include <dirent.h>
#include <limits.h>
#include <stdio.h>
#include <errno.h>

#ifdef CMDBOX
#define main(argc, argv)	ls_main(argc, argv)
#endif

static void print_entry(char *name, struct stat *sp);
static int do_ls(char *path);

/* Flags */
#define LSF_DOT		0x01	/* List files begining with . */
#define LSF_LONG	0x02	/* Long format */
#define LSF_SINGLE	0x04	/* Single column */
#define LSF_TYPE	0x08	/* Add /(dir) and @(symlink) with file name */
#define LSF_ALL		0x10	/* List hidden files */

#define LSF_RECURSIVE	0x20	/* List Subdirectory */
#define LSF_TIMESORT	0x40	/* Sort by time */

static unsigned int ls_flags;

int
main(int argc, char *argv[])
{
	int ch, rc;

	ls_flags = 0;

	while ((ch = getopt(argc, argv, "1ClFaA")) != -1) {
		switch(ch) {
		case '1':
			ls_flags |= LSF_SINGLE;
			ls_flags &= ~LSF_LONG;
			break;
		case 'C':
			ls_flags &= ~LSF_SINGLE;
			ls_flags &= ~LSF_LONG;
			break;
		case 'l':
			ls_flags |= LSF_LONG;
			ls_flags &= ~LSF_SINGLE;
			break;
		case 'F':
			ls_flags |= LSF_TYPE;
			break;
		case 'a':
			ls_flags |= LSF_DOT;
			/* FALLTHROUGH */
		case 'A':
			ls_flags |= LSF_ALL;
			break;
		default:
		case '?':
			fprintf(stderr, "usage: ls [-1CFAal] [file ...]\n");
			exit(1);
		}
	}
	argc -= optind;
	argv += optind;

	if (argc == 0)
		rc = do_ls(".");
	else {
		do {
			rc = do_ls(*argv);
			++argv;
		} while (*argv);
	}
	if (rc)
		err(1, NULL);
	return 0;
}

static void
print_entry(char *name, struct stat *sp)
{
	int color;
	int dot = 0;

	if (name[0] == '.') {
		if ((ls_flags & LSF_DOT) == 0)
			return;
		dot = 1;
	}

	/* set color */
	color = 0;
	if (S_ISCHR(sp->st_mode) || S_ISBLK(sp->st_mode))
		color = 35;  /* magenta */
	else if (S_ISDIR(sp->st_mode))
		color = 36;  /* cyan */
	else if (S_ISFIFO(sp->st_mode))
		color = 34;
	else if (S_ISLNK(sp->st_mode))
		color = 33;  /* yellow */

	if (ls_flags & LSF_LONG) {
		/* print mode */
		if (S_ISDIR(sp->st_mode))
			putchar('d');
		else if (S_ISLNK(sp->st_mode))
			putchar('@');
		else if (S_ISFIFO(sp->st_mode))
			putchar('|');
		else
			putchar('-');

		printf("rw");
		if (sp->st_mode & (S_IXUSR | S_IXGRP | S_IXOTH))
			putchar('x');
		else
			putchar('-');
		printf("------");

		/* print link */
		printf("  1 ");

		/* print owner */
		printf("prex   ");

		/* print date/time */
		printf("%s 12:00 ", __DATE__);

		/* print size */
		printf("%7d ", (int)sp->st_size);
	}

	/* print name */
	printf("\033[%dm", color);
	printf("%s", name);

	/* print type */
	if (!dot && (ls_flags & LSF_TYPE)) {
		switch (sp->st_mode & S_IFMT) {
		case S_IFDIR:
			putchar('/');
			break;
		case S_IFIFO:
			putchar('|');
			break;
		case S_IFLNK:
			putchar('@');
			break;
		case S_IFSOCK:
			putchar('=');
			break;
		case S_IFWHT:
			putchar('%');
			break;
		}
		if (sp->st_mode & (S_IXUSR | S_IXGRP | S_IXOTH))
			putchar('*');
	}
	printf("\033[0m");
	if (ls_flags & LSF_LONG || ls_flags & LSF_SINGLE) {
		putchar('\n');
	} else {
		putchar(' ');
	}
}

static int
do_ls(char *path)
{
	struct stat st;
	int nr_file = 0;
	char buf[PATH_MAX];
	DIR *dir;
	struct dirent *entry;

	if (stat(path, &st) == -1)
		return ENOTDIR;

	if (S_ISDIR(st.st_mode)) {

		dir = opendir(path);
		if (dir == NULL)
			return ENOTDIR;

		for (;;) {
			entry = readdir(dir);
			if (entry == NULL)
				break;
			buf[0] = 0;
			strlcpy(buf, path, sizeof(buf));
			buf[sizeof(buf) - 1] = '\0';
			if (!strcmp(entry->d_name, "."))
				;	/* Do nothing */
			else if (!strcmp(entry->d_name, ".."))
				;	/* Do nothing */
			else {
				strlcat(buf, "/", sizeof(buf));
				strlcat(buf, entry->d_name, sizeof(buf));
			}
			if (stat(buf, &st) == -1)
				break;
			print_entry(entry->d_name, &st);
			nr_file++;
		}
		closedir(dir);
		if (ls_flags & LSF_LONG)
			printf("total %d\n", nr_file);
		else
			putchar('\n');
	} else {
		print_entry(path, &st);
		putchar('\n');
	}
	return 0;
}
