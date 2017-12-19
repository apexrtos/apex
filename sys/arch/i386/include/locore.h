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

#ifndef _I386_LOCORE_H
#define _I386_LOCORE_H

#include <sys/cdefs.h>
#include <arch.h>

__BEGIN_DECLS
void	intr_0(void);
void	intr_1(void);
void	intr_2(void);
void	intr_3(void);
void	intr_4(void);
void	intr_5(void);
void	intr_6(void);
void	intr_7(void);
void	intr_8(void);
void	intr_9(void);
void	intr_10(void);
void	intr_11(void);
void	intr_12(void);
void	intr_13(void);
void	intr_14(void);
void	intr_15(void);
void	trap_default(void);
void	trap_0(void);
void	trap_1(void);
void	trap_2(void);
void	trap_3(void);
void	trap_4(void);
void	trap_5(void);
void	trap_6(void);
void	trap_7(void);
void	trap_8(void);
void	trap_9(void);
void	trap_10(void);
void	trap_11(void);
void	trap_12(void);
void	trap_13(void);
void	trap_14(void);
void	trap_15(void);
void	trap_16(void);
void	trap_17(void);
void	trap_18(void);
void	syscall_entry(void);
void	syscall_ret(void);
void	cpu_switch(struct kern_regs *, struct kern_regs *);
void	known_fault1(void);
void	known_fault2(void);
void	known_fault3(void);
void	umem_fault(void);
void	cpu_reset(void);
__END_DECLS

#define cache_init()	do {} while (0)

#endif /* !_I386_LOCORE_H */
