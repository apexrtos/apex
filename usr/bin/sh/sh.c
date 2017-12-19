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

#include <prex/prex.h>
#include <prex/keycode.h>
#include <sys/syslog.h>

#include <limits.h>
#include <termios.h>
#include <ctype.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>

#include "sh.h"

#define	LINELEN		256
#define ARGMAX		32

#ifdef CMDBOX
extern const struct cmd_entry builtin_cmds[];
extern int exec_builtin(cmd_func_t, int argc, char *argv[]);
#define main(argc, argv)	sh_main(argc, argv)
#endif

extern int exec_cmd(int argc, char *argv[]);
extern const struct cmd_entry internal_cmds[];

static char *args[ARGMAX];

/*
 * Find command from the specifid table.
 *
 * Returns 0 if command is processed.
 * Returns -1 if no command is found.
 */
static cmd_func_t
find_cmd(const struct cmd_entry cmds[], char *cmd)
{
	int i = 0;

	while (cmds[i].cmd != NULL) {
		if (!strcmp(cmd, cmds[i].cmd))
			return cmds[i].func;
		i++;
	}
	return NULL;
}

/*
 * Parse an entire given line.
 * Break it down into commands.
 */
static void
parse_line(char *line)
{
	char *p = line;
	int i, argc;
	cmd_func_t cmd;

	/*
	 * Build argument list
	 */
	i = 0;
	do {
		/* Ignore whitespace at beginning */
		while (*p && isspace((int)*p))
			p++;
		if (*p == '\0')
			break;
		args[i] = p;
		i++;

		/* Skip word */
		while (*p && !isspace((int)*p))
			p++;
		if (*p == '\0')
			break;
		*p++ = '\0';
	} while (i < ARGMAX - 1);

	if (i == ARGMAX) {
		fprintf(stderr, "Too many args\n");
		return;
	}
	args[i] = NULL;
	argc = i;

	/*
	 * Dispatch command
	 */
	if (argc > 0) {
		optind = 1;

		/* Run it as internal command */
		cmd  = find_cmd(internal_cmds, args[0]);
		if (cmd != NULL) {
			if ((*cmd)(argc, args) != 0) {
				fprintf(stderr, "%s: %s\n", args[0],
					strerror(errno));
			}
			return;
		}
#ifdef CMDBOX
		/* Run it as shell built-in command */
		cmd  = find_cmd(builtin_cmds, args[0]);
		if (cmd != NULL) {
			exec_builtin(cmd, argc, args);
			return;
		}
#endif
		/* Next, run it as external command */
		exec_cmd(argc, args);
	}
}

/*
 * Read command string.
 */
static char *
read_line(char *line, int len, int fd)
{
	char *p, *end;
	int c;

	end = line + len;
	for (p = line; p < end; p++) {
		c = getchar();
		if (c == EOF) {
			if (p == line)
				return NULL;
			*p = '\0';
			return p;
		}
		if (c == '\n') {
			*p = '\0';
			return p;
		}
		*p = (char)c;
	}
	fprintf(stderr, "Command line overflow\n");
	return (char *)-1;
}

int
main(int argc, char **argv)
{
	static char line[LINELEN];
	static char cwd[PATH_MAX];
	char *p;

	signal(SIGINT, SIG_IGN);
	signal(SIGQUIT, SIG_IGN);
	signal(SIGTERM, SIG_IGN);
	signal(SIGTSTP, SIG_IGN);	/* temporary... */

	/*
	 * Command loop
	 */
	for (;;) {
 next:
		/* Display prompt */
		getcwd(cwd, PATH_MAX);
		printf("\033[32m");
		printf("[prex:%s]", cwd);
		printf("\033[0m# ");

		/* Read and parse user input */
		p = read_line(line, LINELEN, 0);
		if (p == NULL)
			break;
		if (p == (char *)-1)
			continue;
		while (*(p - 1) == '\\' && (p - 2) >= line
			&& *(p - 2) != '\\') {
			*(p - 1) = ' ';
			p = read_line(p, LINELEN - (p - line), 0);
			if (p == NULL)
				break;
			if (p == (char *)-1)
				goto next;
		}
		parse_line(line);
	}
	printf("bye!\n");
	exit(0);
	return 0;
}
