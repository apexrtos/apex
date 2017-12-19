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

#ifndef _ARM_SYSTRAP_H
#define _ARM_SYSTRAP_H

#if defined(__gba__)

/*
 * Note:
 * GBA BIOS does not allow to an install SWI handler by user. So, the system
 * calls will jump to the system call entry point (0x200007c) in the kernel
 * code. Then, the kernel SWI handler will switch the CPU mode to SVC mode.
 * However, CPU mode can be changed only in privileged CPU mode. By this
 * reason, all applications run in SYS mode with GBA.
 *
 */

#define SYSCALL0(name) \
	.global name; .align; \
name##: \
	stmfd sp!, {r4, r5, lr}; \
	mov r4, #SYS_##name; \
	ldr r5, =0x200007c; \
	add lr, pc, #2; \
	mov pc, r5; \
	ldmfd sp!, {r4, r5, pc};

#else

#define SYSCALL0(name) \
	.global name; .align; \
name##: swi #SYS_##name; \
	mov pc, lr

#endif


#define SYSCALL1(name) SYSCALL0(name)
#define SYSCALL2(name) SYSCALL0(name)
#define SYSCALL3(name) SYSCALL0(name)
#define SYSCALL4(name) SYSCALL0(name)

#endif /* _ARM_SYSTRAP_H */
