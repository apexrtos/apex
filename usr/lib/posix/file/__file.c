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
#include <server/object.h>
#include <server/stdmsg.h>
#include <server/fs.h>

#include <stddef.h>

object_t __fs_obj;

/*
 * This is called first when task is started
 */
void
__file_init(void)
{
	int err;

	/*
	 * Look up file system server
	 */
	err = object_lookup(OBJNAME_FS, &__fs_obj);
	if (err)
		__fs_obj = 0;
}

/*
 * Clean up
 */
void
__file_exit(void)
{
	struct msg m;

	/* Notify to file system server */
	if (__fs_obj != 0) {
		m.hdr.code = FS_EXIT;
		msg_send(__fs_obj, &m, sizeof(m));
	}
}

/*
 * Notify to file system server.
 * This routine is used by native task which uses file system.
 */
void
fslib_init(void)
{
	struct msg m;

	if (__fs_obj == 0)
		__file_init();

	m.hdr.code = FS_REGISTER;
	msg_send(__fs_obj, &m, sizeof(m));
}


/*
 * Clean up (native task)
 */
void
fslib_exit(void)
{

	__file_exit();
}

