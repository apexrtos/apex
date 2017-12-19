/*-
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

/*
 * fileio.c - file I/O test program
 */

#include <prex/prex.h>
#include <server/stdmsg.h>
#include <server/object.h>

#include <sys/syslog.h>
#include <sys/mount.h>
#include <sys/fcntl.h>

#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>

#define IOBUFSZ	512
#define READ_TARGET	"/boot/LICENSE"
#define WRITE_TARGET	"/tmp/test"

static	char iobuf[IOBUFSZ];

static void
test_write(void)
{
	int fd, i;

	if ((fd = open(WRITE_TARGET, O_CREAT|O_RDWR, 0)) < 0)
		panic("can not open file " WRITE_TARGET);

	for (i = 0; i < 50; i++) {
		memset(iobuf, i, IOBUFSZ);
		write(fd, iobuf, IOBUFSZ);
	}
	close(fd);
}

/*
 * Display file contents
 */
static void
cat_file(void)
{
	int rd, fd;

	if ((fd = open(READ_TARGET, O_RDONLY, 0)) < 0)
		panic("can not open file " READ_TARGET);

	while ((rd = read(fd, iobuf, IOBUFSZ)) > 0)
		write(STDOUT_FILENO, iobuf, (size_t)rd);
	close(fd);
}

/*
 * Test invalid request
 */
static void
test_invalid(void)
{
	object_t fs_obj;
	struct msg m;

	object_lookup(OBJNAME_FS, &fs_obj);
	m.hdr.code = 0x300;
	msg_send(fs_obj, &m, sizeof(m));
}

/*
 * Test open/close
 */
static void
test_open(void)
{
	int fd;

	for (;;) {
		if ((fd = open(READ_TARGET, O_RDONLY, 0)) < 0)
			panic("can not open file " READ_TARGET);
		close(fd);
	}
}

/*
 * Test file read
 */
static void
test_read(void)
{
	int rd, fd;

	if ((fd = open(READ_TARGET, O_RDONLY, 0)) < 0)
		panic("can not open file " READ_TARGET);

	for (;;)
		while ((rd = read(fd, iobuf, IOBUFSZ)) > 0)
			;

	close(fd);
}

/*
 * Main routine
 */
int
main(int argc, char *argv[])
{
	char test_str[] = "test stdout...\n\n";

	syslog(LOG_INFO, "\nfileio: fs test program\n");

	/* Wait 1 sec until loading fs server */
	timer_sleep(1000, 0);

	/*
	 * Prepare to use a file system.
	 */
	fslib_init();

	/*
	 * Mount file systems
	 */
	mount("", "/", "ramfs", 0, NULL);
	mkdir("/dev", 0);
	mount("", "/dev", "devfs", 0, NULL);		/* device */
	mkdir("/boot", 0);
	mount("/dev/ram0", "/boot", "arfs", 0, NULL);	/* archive */
	mkdir("/tmp", 0);
	/*
	 * Prepare stdio
	 */
	open("/dev/tty", O_RDWR);	/* stdin */
	dup(0);				/* stdout */
	dup(0);				/* stderr */

	/* Test device write */
	write(STDOUT_FILENO, test_str, strlen(test_str));

	test_write();

	cat_file();		/* test read/write */

	test_invalid();		/* test invalid request */

	test_read();		/* test read loop */

	test_open();		/* test open/close loop */

	/*
	 * Disconnect from a file system.
	 */
	fslib_exit();
	return 0;
}
