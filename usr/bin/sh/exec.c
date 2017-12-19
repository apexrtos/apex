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

#include <prex/prex.h>
#include <sys/wait.h>

#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <limits.h>
#include <errno.h>
#include <unistd.h>
#include <stdio.h>

#include "sh.h"

/*
 * Fork and execute command.
 */
int
exec_cmd(int argc, char *argv[])
{
	int pid, cpid;
	int status;
	static char **arg;
	char *file;
	char path[PATH_MAX];
	char *env[] = { "PATH=path", "Foo", 0 };

	arg = argc > 1 ? &argv[1] : NULL;
	file = argv[0];
	if (file[0] == '/')
		strlcpy(path, file, sizeof(path));
	else {
		getcwd(path, PATH_MAX);
		if (!(path[0] == '/' && path[1] == '\0'))
			strlcat(path, "/", sizeof(path));
		strlcat(path, file, sizeof(path));
	}
	path[PATH_MAX - 1] = '\0';

	pid = vfork();
	if (pid == -1) {
		fprintf(stderr, "cmdbox: Cannot fork\n");
		return -1;
	}
	if (pid == 0) {
		/* Child only */
		execve(path, arg, env);
		if (errno == ENOENT || errno == ENOTDIR)
			fprintf(stderr, "cmdbox: %s: command not found\n",
				argv[0]);
		else
			fprintf(stderr, "cmdbox: %s cannot execute\n",
				argv[0]);
		exit(1);
	}
	/* Parent */
	while (1) {
		cpid = wait(&status);
		if (cpid == -1 && errno != EINTR)
			break;
		if (cpid == pid)
			break;
	}
	return 0;
}

#ifdef CMDBOX
static void
show_signal(int s)
{
	int signo = WTERMSIG(s);

	signo &= 0x7f;
	if (signo < NSIG && sys_siglist[signo])
		fputs(sys_siglist[signo], stderr);
	else
		fprintf(stderr, "Signal %d", signo);

	fputs("\n", stderr);
}

int
exec_builtin(cmd_func_t cmd, int argc, char *argv[])
{
	int status;
	pid_t cpid;

	cpid = vfork();
	if (cpid == -1) {
		fprintf(stderr, "cmdbox: Cannot fork\n");
		return -1;
	}
	if (cpid == 0) {
		/* Child only */
		task_name(task_self(), argv[0]);
		errno = 0;
		if (cmd(argc, argv) != 0)
			printf("%s: %s\n", argv[0], strerror(errno));
		exit(1);
	}
	/* Parent */
	while (wait(&status) != cpid) ;

	if (status) {
		if (WIFSIGNALED(status))
			show_signal(status);
		else if (WIFEXITED(status))
			return WEXITSTATUS(status);
	}
	return 0;
}
#endif
