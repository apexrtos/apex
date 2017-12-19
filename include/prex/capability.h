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

#ifndef _PREX_CAPABILITY_H
#define _PREX_CAPABILITY_H

/*
 * Type of capability
 */
typedef uint32_t	cap_t;

/*
 * Task capabilities
 *
 * Bit 0:  Allow setting capability
 * Bit 1:  Allow controlling another task's execution
 * Bit 2:  Allow touching another task's memory
 * Bit 3:  Allow raising exception to another task
 * Bit 4:  Allow accessing another task's semaphore
 * Bit 5:  Allow changing scheduling parameter
 * Bit 6:  Allow accessing another task's IPC object
 * Bit 7:  Allow device I/O operation
 * Bit 8:  Allow power control including shutdown
 * Bit 9:  Allow setting system time
 * Bit 10: Allow direct I/O access
 * Bit 11: Allow debugging requests
 *
 * Bit 16: Allow mount,umount,sethostname,setdomainname,etc
 * Bit 17: Allow executing any file
 * Bit 18: Allow reading any file
 * Bit 19: Allow writing any file
 */
#define CAP_SETPCAP	0x00000001
#define CAP_TASK	0x00000002
#define CAP_MEMORY	0x00000004
#define CAP_KILL	0x00000008
#define CAP_SEMAPHORE	0x00000010
#define CAP_NICE	0x00000020
#define CAP_IPC		0x00000040
#define CAP_DEVIO	0x00000080
#define CAP_POWER	0x00000100
#define CAP_TIME	0x00000200
#define CAP_RAWIO	0x00000400
#define CAP_DEBUG	0x00000800

#define CAP_ADMIN	0x00010000
#define CAP_EXEC	0x00020000
#define CAP_FS_READ	0x00040000
#define CAP_FS_WRITE	0x00080000

#endif /* !_PREX_CAPABILITY_H */
