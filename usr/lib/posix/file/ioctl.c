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
#include <prex/posix.h>
#include <server/fs.h>
#include <sys/ioctl.h>

#include <errno.h>
#include <stdarg.h>
#include <string.h>

int
ioctl(int fd, unsigned long request, ...)
{
	struct ioctl_msg m;
	char *argp;
	va_list args;
	size_t size;
	struct stat st;
	int err;

	va_start(args, request);
	argp = va_arg(args, char *);
	va_end(args);

	size = IOCPARM_LEN(request);
	if (size > IOCPARM_MAX) {
		errno = ENOTTY;
		return -1;
	}

	if (fstat(fd, &st) == -1)
		return -1;
	if (S_ISCHR(st.st_mode) || S_ISBLK(st.st_mode)) {
		/*
		 * Note: The file system server can not handle the
		 * poniter type arguent for ioctl(). So, we have to
		 * invoke the ioctl here if the target file is a device.
		 */
		err = device_ioctl(st.st_rdev, request, argp);
		if (err != 0) {
			errno = err;
			return -1;
		}
		return 0;
	}
	/*
	 * Note:
	 *  We can not know if the argument is pointer or int value.
	 *  So, if it is an input request and its length is sizeof(int),
	 *  we handle it as direct value.
	 */
	if (((request & IOC_DIRMASK) == IOC_IN) && (size == sizeof(int)))
		*((int *)m.buf) = (int)argp;
	else if ((request & IOC_IN) && size && argp)
		memcpy(&m.buf, argp, size);

	m.hdr.code = FS_IOCTL;
	m.fd = fd;
	m.request = request;
	if (__posix_call(__fs_obj, &m, sizeof(m), 0) != 0)
		return -1;
	if ((request & IOC_OUT) && size && argp)
		memcpy(argp, &m.buf, size);
	return 0;
}
