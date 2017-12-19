/*
 * Copyright (c) 2007, Kohsuke Ohtani
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
#include <server/object.h>
#include <server/stdmsg.h>
#include <server/proc.h>

#include <unistd.h>
#include <stdlib.h>
#include <errno.h>
#include <stdio.h>

#ifdef CMDBOX
#define main(argc, argv)	ps_main(argc, argv)
#endif

#define PSFX	0x01
#define PSFL	0x02

struct info_proc {
	pid_t	pid;
	pid_t	ppid;
	int	stat;
};

static object_t procobj;

static int
pstat(task_t task, struct info_proc *ip)
{
	static struct msg m;
	int rc;

	do {
		m.hdr.code = PS_PSTAT;
		m.data[0] = task;
		rc = msg_send(procobj, &m, sizeof(m));
	} while (rc == EINTR);

	if (rc || m.hdr.status) {
		ip->pid = -1;
		ip->ppid = -1;
		ip->stat = 1;
		return -1;
	}

	ip->pid = m.data[0];
	ip->ppid = m.data[1];
	ip->stat = m.data[2];
	return 0;
}

int
main(int argc, char *argv[])
{
	int ch, rc, ps_flag = 0;
	static struct info_thread it;
	static struct info_proc ip;
	pid_t last_pid = -2;
	char stat[][2] = { "R", "Z", "S" };
	char pol[][5] = { "FIFO", "RR  " };

	while ((ch = getopt(argc, argv, "lx")) != -1)
		switch(ch) {
		case 'x':
			ps_flag |= PSFX;
			break;
		case 'l':
			ps_flag |= PSFL;
			break;

		case '?':
		default:
			fprintf(stderr, "usage: ps [-lx]\n");
			exit(1);
		}
	argc -= optind;
	argv += optind;

	if (object_lookup(OBJNAME_PROC, &procobj))
		exit(1);

	if (ps_flag & PSFL)
		printf("  PID  PPID PRI STAT POL      TIME WCHAN       CMD\n");
	else
		printf("  PID     TIME CMD\n");

	rc = 0;
	it.cookie = 0;
	do {
		/*
		 * Get thread info from kernel.
		 */
		rc = sys_info(INFO_THREAD, &it);
		if (!rc) {
			/*
			 * Get process info from server.
			 */
			if (pstat(it.task, &ip) && !(ps_flag & PSFX))
				continue;

			if (ps_flag & PSFL) {
				if (ip.pid == -1)
					printf("    -     -"); /* kernel */
				else
					printf("%5d %5d", ip.pid, ip.ppid);

				printf(" %3d %s    %s %8d "
				       "%-11s %-11s\n",
				       it.prio, stat[ip.stat-1],
				       pol[it.policy],
				       it.time, it.slpevt, it.taskname);
			} else {
				if (!(ps_flag & PSFX) && (ip.pid == last_pid))
					continue;
				if (ip.pid == -1)
					printf("    -"); /* kernel */
				else
					printf("%5d", ip.pid);

				printf(" %8d %-11s\n", it.time, it.taskname);
				last_pid = ip.pid;
			}
		}
	} while (rc == 0);
	exit(0);
}
