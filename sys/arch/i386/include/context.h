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

#ifndef _I386_CONTEXT_H
#define _I386_CONTEXT_H

/*
 * Common register frame for trap/interrupt.
 * These cpu state are saved into top of the kernel stack in
 * trap/interrupt entries. Since the arguments of system calls are
 * passed via registers, the system call library is completely
 * dependent on this register format.
 *
 * The value of ss & esp are not valid for kernel mode trap
 * because these are set only when privilege level is changed.
 */
struct cpu_regs {
	uint32_t ebx;		/*  +0 (00) --- s/w trap frame --- */
	uint32_t ecx;		/*  +4 (04) */
	uint32_t edx;		/*  +8 (08) */
	uint32_t esi;		/* +12 (0C) */
	uint32_t edi;		/* +16 (10) */
	uint32_t ebp;		/* +20 (14) */
	uint32_t eax;		/* +24 (18) */
	uint32_t ds;		/* +28 (1C) */
	uint32_t es;		/* +32 (20) */
	uint32_t trap_no;	/* +36 (24) --- h/w trap frame --- */
	uint32_t err_code;	/* +40 (28) */
	uint32_t eip;		/* +44 (2C) */
	uint32_t cs;		/* +48 (30) */
	uint32_t eflags;	/* +52 (34) */
	uint32_t esp;		/* +56 (38) */
	uint32_t ss;		/* +60 (3C) */
};

/*
 * Kernel mode context for context switching.
 */
struct kern_regs {
	uint32_t eip;		/*  +0 (00) */
	uint32_t ebx;		/*  +4 (04) */
	uint32_t edi;		/*  +8 (08) */
	uint32_t esi;		/* +12 (0C) */
	uint32_t ebp;		/* +16 (10) */
	uint32_t esp;		/* +20 (14) */
};

/*
 * FPU register for fsave/frstor
 */
struct fpu_regs {
	uint32_t ctrl_word;
	uint32_t stat_word;
	uint32_t tag_word;
	uint32_t ip_offset;
	uint32_t cs_sel;
	uint32_t op_offset;
	uint32_t op_sel;
	uint32_t st[20];
};

/*
 * Processor context
 */
struct context {
	struct kern_regs kregs;		/* kernel mode registers */
	struct cpu_regs	*uregs;		/* user mode registers */
	struct cpu_regs	*saved_regs;	/* saved user mode registers */
#ifdef CONFIG_FPU
	struct fpu_regs	*fregs;		/* co-processor registers */
#endif
	uint32_t	 esp0;		/* top of kernel stack */
};

typedef struct context *context_t;	/* context id */

#endif /* !_I386_CONTEXT_H */
