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

/*
 * cmd.c - command processor
 */

#include <prex/prex.h>
#include <sys/ioctl.h>
#include <sys/types.h>

#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <stdio.h>

int cmd_help(int argc, char **argv);
int cmd_ver(int argc, char **argv);
int cmd_mem(int argc, char **argv);
int cmd_clear(int argc, char **argv);
int cmd_kill(int argc, char **argv);
#ifdef DEBUG
int cmd_thread(int argc, char **argv);
int cmd_task(int argc, char **argv);
int cmd_object(int argc, char **argv);
int cmd_timer(int argc, char **argv);
int cmd_irq(int argc, char **argv);
int cmd_device(int argc, char **argv);
int cmd_vm(int argc, char **argv);
#endif
int cmd_reboot(int argc, char **argv);
int cmd_shutdown(int argc, char **argv);

static const char *err_msg[] = {
	"Syntax error",
};

struct cmd_entry {
	char *cmd;
	int (*func) (int, char **);
	char *usage;
};

static struct cmd_entry cmd_table[] = {
	{ "help"	,cmd_help	,"help     - This help" },
	{ "ver"	 	,cmd_ver	,"ver      - Kernel version information" },
	{ "mem"	 	,cmd_mem	,"mem      - Show memory usage" },
	{ "clear"	,cmd_clear	,"clear    - Clear screen" },
	{ "kill"	,cmd_kill	,"kill     - Terminate thread" },
#ifdef DEBUG
	{ "thread"	,cmd_thread	,"thread   - Dump threads" },
	{ "task"	,cmd_task	,"task     - Dump tasks" },
	{ "vm"		,cmd_vm		,"vm       - Dump virtual memory information" },
#endif
	{ "reboot"	,cmd_reboot	,"reboot   - Reboot system" },
	{ "shutdown"	,cmd_shutdown	,"shutdown - Shutdown system" },
	{ NULL		,NULL		,NULL },
};

int
cmd_help(int argc, char **argv)
{
	int i = 0;

	while (cmd_table[i].cmd != NULL) {
		puts(cmd_table[i].usage);
		i++;
	}
	return 0;
}

int
cmd_ver(int argc, char **argv)
{
	struct info_kernel info;

	sys_info(INFO_KERNEL, &info);

	printf("Kernel version:\n");
	printf("%s version %s for %s\n",
	       info.sysname, info.version, info.machine);
	return 0;
}

int
cmd_mem(int argc, char **argv)
{
	struct info_memory info;

	sys_info(INFO_MEMORY, &info);

	printf("Memory usage:\n");
	printf("    total     used     free bootdisk\n");
	printf(" %8d %8d %8d %d\n", (u_int)info.total,
	       (u_int)(info.total - info.free), (u_int)info.free,
	       (u_int)info.bootdisk);
	return 0;
}

int
cmd_clear(int argc, char **argv)
{
	printf("\33[2J");
	return 0;
}

int
cmd_kill(int argc, char **argv)
{
	thread_t th;
	char *ep;

	if (argc < 2)
		return 1;
	th = (thread_t)strtoul(argv[1], &ep, 16);
	printf("Kill thread id:%x\n", (u_int)th);
	if (thread_terminate(th)) {
		printf("Thread %x does not exist\n", (u_int)th);
		return 1;
	}
	return 0;
}

#ifdef DEBUG
int
cmd_thread(int argc, char **argv)
{
	int item = DUMP_THREAD;

	sys_debug(DCMD_DUMP, &item);
	return 0;
}

int
cmd_task(int argc, char **argv)
{
	int item = DUMP_TASK;

	sys_debug(DCMD_DUMP, &item);
	return 0;
}

int
cmd_vm(int argc, char **argv)
{
	int item = DUMP_VM;

	sys_debug(DCMD_DUMP, &item);
	return 0;
}
#endif /* DEBUG */

int
cmd_reboot(int argc, char **argv)
{
	device_t pm_dev;
	int err, state;

	if ((err = device_open("pm", 0, &pm_dev)) == 0) {
		state = POWER_REBOOT;
		err = device_ioctl(pm_dev, PMIOC_SET_POWER, &state);
		device_close(pm_dev);
	}
	return err;
}

int
cmd_shutdown(int argc, char **argv)
{
	device_t pm_dev;
	int err, state;

	if ((err = device_open("pm", 0, &pm_dev)) == 0) {
		state = POWER_OFF;
		err = device_ioctl(pm_dev, PMIOC_SET_POWER, &state);
		device_close(pm_dev);
	}
	return err;
}

int
dispatch_cmd(int argc, char **argv)
{
	int i = 0;
	int err = 0;

	while (cmd_table[i].cmd != NULL) {
		if (!strcmp(argv[0], cmd_table[i].cmd)) {
			err = (cmd_table[i].func)(argc, argv);
			break;
		}
		i++;
	}
	if (cmd_table[i].cmd == NULL)
		printf("%s: command not found\n", argv[0]);
	if (err)
		printf("Error %d:%s\n", err, err_msg[err - 1]);
	return 0;
}
