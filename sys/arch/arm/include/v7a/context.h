/*-
 * Copyright (c) 2005-2008, Kohsuke Ohtani
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

#ifndef context_h
#define context_h

#ifndef __ASSEMBLY__
#include <stdint.h>

/**
 * ARM register reference:
 *
 *  Name    Number	ARM Procedure Calling Standard Role
 *
 *  a1	    r0		argument 1 / integer result / scratch register / argc
 *  a2	    r1		argument 2 / scratch register / argv
 *  a3	    r2		argument 3 / scratch register / envp
 *  a4	    r3		argument 4 / scratch register
 *  v1	    r4		register variable
 *  v2	    r5		register variable
 *  v3	    r6		register variable
 *  v4	    r7		register variable
 *  v5	    r8		register variable
 *  sb/v6   r9		static base / register variable
 *  sl/v7   r10		stack limit / stack chunk handle / reg. variable
 *  fp	    r11		frame pointer
 *  ip	    r12		scratch register / new-sb in inter-link-unit calls
 *  sp	    r13		lower end of current stack frame
 *  lr	    r14		link address / scratch register
 *  pc	    r15		program counter
 */

/*
 * Common register frame for trap/interrupt.
 * These cpu state are saved into top of the kernel stack in
 * trap/interrupt entries. Since the arguments of system calls are
 * passed via registers, the system call library is completely
 * dependent on this register format.
 */
struct cpu_regs {
	uint32_t	r0;	/*  +0 (00) */
	uint32_t	r1;	/*  +4 (04) */
	uint32_t	r2;	/*  +8 (08) */
	uint32_t	r3;	/* +12 (0C) */
	uint32_t	r4;	/* +16 (10) */
	uint32_t	r5;	/* +20 (14) */
	uint32_t	r6;	/* +24 (18) */
	uint32_t	r7;	/* +28 (1C) */
	uint32_t	r8;	/* +32 (20) */
	uint32_t	r9;	/* +36 (24) */
	uint32_t	r10;	/* +40 (28) */
	uint32_t	r11;	/* +44 (2C) */
	uint32_t	r12;	/* +48 (30) */
	uint32_t	sp;	/* +52 (34) */
	uint32_t	lr;	/* +56 (38) */
	uint32_t	cpsr;	/* +60 (3C) */
	uint32_t	svc_sp;	/* +64 (40) */
	uint32_t	svc_lr;	/* +68 (44) */
	uint32_t	pc;	/* +72 (48) */
};

/*
 * Kernel mode context for context switching.
 */
struct kern_regs {
	uint32_t	r4;
	uint32_t	r5;
	uint32_t	r6;
	uint32_t	r7;
	uint32_t	r8;
	uint32_t	r9;
	uint32_t	r10;
	uint32_t	r11;
	uint32_t	sp;
	uint32_t	lr;
};

/*
 * Processor context
 */
struct context {
	struct kern_regs kregs;		/* kernel mode registers */
	struct cpu_regs	*uregs;		/* user mode registers */
	struct cpu_regs	*saved_regs;	/* saved user mode registers */
};

#endif /* !__ASSEMBLY__ */

/*
 * Register offset in cpu_regs
 */
#define REG_R0		0x00
#define REG_R1		0x04
#define REG_R2		0x08
#define REG_R3		0x0c
#define REG_R4		0x10
#define REG_R5		0x14
#define REG_R6		0x18
#define REG_R7		0x1c
#define REG_R8		0x20
#define REG_R9		0x24
#define REG_R10		0x28
#define REG_R11		0x2c
#define REG_R12		0x30
#define REG_SP		0x34
#define REG_LR		0x38
#define REG_CPSR	0x3c
#define REG_SVCSP	0x40
#define REG_SVCLR	0x44
#define REG_PC		0x48

#define CTXREGS		(4*19)

#endif /* !context_h */
