/*-
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
 * main.c - bootstrap server
 */

/*
 * A bootstrap server works to setup the POSIX environment for
 * 'init' process. It sends a setup message to other servers in
 * order to let them know that this task becomes 'init' process.
 * The bootstrap server is gone after it launches (exec) the
 * 'init' process.
 */

#include <prex/prex.h>
#include <sys/mount.h>
#include <server/fs.h>
#include <server/object.h>
#include <server/exec.h>
#include <server/proc.h>
#include <server/stdmsg.h>

#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <signal.h>
#include <errno.h>
#include <fstab.h>

#ifdef DEBUG
#define DPRINTF(a) sys_log a
#else
#define DPRINTF(a)
#endif

extern const struct fstab fstab[];
extern const int fstab_size;

#define PRIO_BOOT	131		/* priority of boot server */

/* forward declarations */
static void	wait_server(const char *);
static void	process_init(void);
static int	run_init(char *);
static void	mount_fs(void);

static object_t proc_obj;

static char *init_argv[] = { "arg", NULL };
static char *init_envp[] = { "HOME=/", NULL };

/*
 * Base directories
 */
static char *base_dir[] = {
	"/bin",		/* essential user commands */
	"/boot",	/* static files for boot */
	"/dev",		/* device files */
	"/etc",		/* system conifguration */
	"/mnt",		/* mount point for file systems */
	"/mnt/floppy",	/* mount point for floppy */
	"/mnt/cdrom",	/* mount point for cdrom */
	"/fifo",	/* mount point for fifo */
	"/tmp",		/* temporary files */
	"/usr",		/* shareable read-only data */
	"/var",		/* log files, spool data */
	NULL
};

/*
 * Main routine for boot strap.
 */
int
main(int argc, char *argv[])
{

	sys_log("Starting Bootstrap Server\n");

	/*
	 * Boost current priority.
	 */
	thread_setprio(thread_self(), PRIO_BOOT);

	/*
	 * Wait until required system servers
	 * become available.
	 */
	wait_server(OBJNAME_PROC);
	wait_server(OBJNAME_FS);
	wait_server(OBJNAME_EXEC);

	/*
	 * Register this task to other servers.
	 */
	process_init();
	fslib_init();

	/*
	 * Mount file systems
	 */
	mount_fs();

	/*
	 * Run init process
	 */
	run_init("/boot/init");

	sys_panic("boot: failed to run init");
	/* NOTREACHED */
	return 0;
}

/*
 * Wait until specified server starts.
 */
static void
wait_server(const char *name)
{
	int i, err = 0;
	object_t obj = 0;

	thread_yield();

	/*
	 * Wait for server loading. timeout is 2 sec.
	 */
	for (i = 0; i < 200; i++) {
		err = object_lookup((char *)name, &obj);
		if (err == 0)
			break;

		/* Wait 10msec */
		timer_sleep(10, 0);
		thread_yield();
	}
	if (err)
		sys_panic("boot: server not found");
}

/*
 * Notify the process server.
 */
static void
process_init(void)
{
	struct msg m;

	/*
	 * We will become an init process later.
	 */
	object_lookup(OBJNAME_PROC, &proc_obj);
	m.hdr.code = PS_SETINIT;
	msg_send(proc_obj, &m, sizeof(m));
}

/*
 * Run init process
 */
static int
run_init(char *path)
{
	struct exec_msg *msg;
	int err, i, argc, envc;
	size_t bufsz;
	char *dest, *src;
	object_t obj;

	DPRINTF(("boot: Run init process\n"));

	/*
	 * Allocate a message buffer with arg/env data.
	 */
	bufsz = 0;
	argc = 0;
	while (init_argv[argc] != NULL) {
		bufsz += (strlen(init_argv[argc]) + 1);
		argc++;
	}
	envc = 0;
	while (init_envp[envc] != NULL) {
		bufsz += (strlen(init_envp[envc]) + 1);
		envc++;
	}
	msg = malloc(sizeof(struct exec_msg) + bufsz);
	if (msg == NULL)
		return -1;

	/*
	 * Build exec message.
	 */
	dest = (char *)&msg->buf;
	for (i = 0; i < argc; i++) {
		src = init_argv[i];
		while ((*dest++ = *src++) != 0);
	}
	for (i = 0; i < envc; i++) {
		src = init_envp[i];
		while ((*dest++ = *src++) != 0);
	}
	msg->argc = argc;
	msg->envc = envc;
	msg->bufsz = bufsz;
	strcpy((char *)&msg->path, path);

	/*
	 * Request exec() to exec server
	 */
	object_lookup(OBJNAME_EXEC, &obj);
	do {
		msg->hdr.code = EX_EXEC;
		err = msg_send(obj, msg,
			       sizeof(struct exec_msg) + bufsz);
		/*
		 * If exec server can execute new process
		 * properly, it will terminate the caller task
		 * automatically. So, the control never comes
		 * here in that case.
		 */
	} while (err == EINTR);
	return -1;
}

static void
mount_fs(void)
{
	int i;

	DPRINTF(("boot: Mounting file systems\n"));

	/*
	 * Mount RAMFS as root file system.
	 */
	if (mount("", "/", "ramfs", 0, NULL) < 0)
		sys_panic("boot: mount failed");

	/*
	 * Create some default directories on RAMFS.
	 */
	i = 0;
	while (base_dir[i] != NULL) {
		mkdir(base_dir[i], 0);
		i++;
	}

	/*
	 * Mount other file systems.
	 */
	for (i = 0; i < fstab_size; i++) {
		mount(fstab[i].fs_spec, fstab[i].fs_file,
		      fstab[i].fs_vfstype, 0, (void *)fstab[i].fs_mntops);
	}
}
