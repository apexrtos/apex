/*
 * Copyright (c) 2006, Kohsuke Ohtani
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
 * ramdisk.c - ramdisk driver test program.
 */

#include <prex/prex.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>

static char test_msg[] = "Hello. This is a test message.";

int
test_read(int sector)
{
	device_t ramdev;
	size_t size;
	int err, i, j;
	static unsigned char disk_buf[512];
	unsigned char ch;
	
	printf("open ram0\n");
	err = device_open("ram0", 0, &ramdev);
	if (err) {
		printf("open failed\n");
		return 0;
	}
	printf("opened\n");

	printf("ramdisk read: sector=%d buf=%x\n", sector, (u_int)disk_buf);
	size = 512;
	err = device_read(ramdev, disk_buf, &size, sector);
	if (err) {
		printf("read failed\n");
		device_close(ramdev);
		return 0;
	}
	printf("read comp: sector=%d buf=%x\n", sector, (u_int)disk_buf);
	
	for (i = 0; i < (512 / 16); i++) {
		for (j = 0; j < 16; j++)
			printf("%02x ", disk_buf[i * 16 + j]);
		printf("    ");
		for (j = 0; j < 16; j++) {
			ch = disk_buf[i * 16 + j];
			if (isprint(ch))
				putchar(ch);
			else
				putchar('.');

		}
		printf("\n");
	}
	printf("\n");
	err = device_close(ramdev);
	if (err)
		printf("close failed\n");
	
	return 0;
}

int
test_write(int sector)
{
	device_t ramdev;
	size_t size;
	int err;
	static unsigned char disk_buf[512];

	printf("open ram0\n");
	err = device_open("ram0", 0, &ramdev);
	if (err) {
		printf("open failed\n");
		return 0;
	}
	printf("opened\n");

	size = 512;
	err = device_read(ramdev, disk_buf, &size, sector);
	if (err) {
		printf("read failed\n");
		device_close(ramdev);
		return 0;
	}
	printf("read comp sector=%d\n", sector);

	strcpy((char *)disk_buf, test_msg);

	size = 512;
	err = device_write(ramdev, disk_buf, &size, sector);
	if (err) {
		printf("write failed\n");
		device_close(ramdev);
		return 0;
	}
	printf("write comp sector=%d\n", sector);

	err = device_close(ramdev);
	if (err)
		printf("close failed\n");
	return 0;
}

int
main(int argc, char *argv[])
{
	test_read(0);
	test_read(1);
	test_write(1);
	test_read(1);
	return 0;
}
