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

/*
 * args.c - routine to build arguments
 */

#include <prex/prex.h>
#include <server/fs.h>
#include <server/proc.h>
#include <sys/list.h>

#include <limits.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>

#include "exec.h"

/*
 * Build argument on stack.
 *
 * Stack layout:
 *    file name string
 *    env string
 *    arg string
 *    NULL
 *    envp[n]
 *    NULL
 *    argv[n]
 *    argc
 *
 * NOTE: This may depend on processor architecture.
 */
int
build_args(task_t task, void *stack, struct exec_msg *msg, void **new_sp)
{
	int argc, envc;
	char *path, *file;
	char **argv, **envp;
	int i, err;
	u_long arg_top, mapped, sp;

	argc = msg->argc;
	envc = msg->envc;
	path = (char *)&msg->path;

	/*
	 * Map target stack in current task.
	 */
	err = vm_map(task, stack, USTACK_SIZE, (void *)&mapped);
	if (err)
		return ENOMEM;
	memset((void *)mapped, 0, USTACK_SIZE);

	sp = mapped + USTACK_SIZE - sizeof(int) * 3;

	/*
	 * Copy items
	 */

	/* File name */
	*(char *)sp = '\0';
	sp -= strlen(path);
	sp = ALIGN(sp);
	strlcpy((char *)sp, path, USTACK_SIZE);
	file = (char *)sp;

	/* arg/env */
	sp -= ALIGN(msg->bufsz);
	memcpy((char *)sp, (char *)&msg->buf, msg->bufsz);
	arg_top = sp;

	/* envp[] */
	sp -= ((envc + 1) * sizeof(char *));
	envp = (char **)sp;

	/* argv[] */
	sp -= ((argc + 1) * sizeof(char *));
	argv = (char **)sp;

	/* argc */
	sp -= sizeof(int);
	*(int *)(sp) = argc + 1;

	/*
	 * Build argument list
	 */
	argv[0] = (char *)((u_long)stack + (u_long)file - mapped);
	DPRINTF(("exec: argv[0] = %s\n", argv[0]));

	for (i = 1; i <= argc; i++) {
		argv[i] = (char *)((u_long)stack + (arg_top - mapped));
		while ((*(char *)arg_top++) != '\0');
		DPRINTF(("exec: argv[%d] = %s\n", i, argv[i]));
	}
	argv[argc + 1] = NULL;

	for (i = 0; i < envc; i++) {
		envp[i] = (char *)((u_long)stack + (arg_top - mapped));
		while ((*(char *)arg_top++) != '\0');
	}
	envp[envc] = NULL;

	*new_sp = (void *)((u_long)stack + (sp - mapped));
	vm_free(task_self(), (void *)mapped);

	return 0;
}
