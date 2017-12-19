/*
 * Copyright (c) 2005, Kohsuke Ohtani
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
 * console.c - test program for console driver
 */

#include <prex/prex.h>
#include <string.h>

int
main(int argc, char *argv[])
{
	device_t console_dev;
	char buf[] = "ABCDEFGHIJKLMN";
	int i, err;
	size_t len;

	sys_log("console test program\n");
	err = device_open("console", 0, &console_dev);
	if (err)
		sys_log("device open err!\n");

	/*
	 * Display 'ABCDE'
	 */
	len = 5;
	device_write(console_dev, buf, &len, 0);

	/*
	 * Display 'AAAA...'
	 */
	len = 1;
	for (i = 0; i < 100; i++)
		device_write(console_dev, buf, &len, 0);

	/*
	 * Try to pass invalid pointer.
	 * It will cause a fault, and get EFAULT.
	 */
	sys_log("\ntest an invalid pointer.\n");
	len = 100;
	err = device_write(console_dev, 0, &len, 0);
	if (err)
		sys_log("OK!\n");
	else
		sys_log("Bad!\n");

	err = device_close(console_dev);
	if (err)
		sys_log("device close err!\n");

       	sys_log("Completed\n");
	return 0;
}
