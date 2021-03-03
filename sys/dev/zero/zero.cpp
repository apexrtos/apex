/*-
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

#include "zero.h"

#include <assert.h>
#include <device.h>
#include <fs.h>
#include <fs/util.h>
#include <string.h>
#include <sys/uio.h>

/*
 * All data read from this device is zero
 */
static ssize_t
zero_read(struct file *file, void *buf, size_t len, off_t offset)
{
	memset(buf, 0, len);
	return len;
}

static ssize_t
zero_read_iov(struct file *file, const struct iovec *iov, size_t count,
    off_t offset)
{
	return for_each_iov(file, iov, count, offset, zero_read);
}

/*
 * Writing data to this device is ignored.
 */
static ssize_t
zero_write_iov(struct file *file, const struct iovec *iov, size_t count,
    off_t offset)
{
	ssize_t res = 0;
	while (count--) {
		res += iov->iov_len;
		++iov;
	}

	return res;
}

/*
 * Device I/O table
 */
static struct devio zero_io = {
	.read = zero_read_iov,
	.write = zero_write_iov,
};

/*
 * Initialize
 */
void
zero_init()
{
	/* Create device object */
	struct device *d = device_create(&zero_io, "zero", DF_CHR, NULL);
	assert(d);
}
