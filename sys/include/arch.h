/*
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

#ifndef arch_h
#define arch_h

#include <conf/config.h>
#include <elf.h>
#include <signal.h>
#include <stdnoreturn.h>
#include <sys/include/types.h>

struct as;
struct context;
struct pgd;
struct thread;

/*
 * Virtual/physical address mapping
 */
struct mmumap {
#if defined(CONFIG_MMU)
	void	       *vaddr;	/* virtual address */
#endif
	phys	       *paddr;	/* physical address */
	size_t		size;	/* size */
	int		flags;	/* machine specific flags */
};

#if defined(__cplusplus)
#define noreturn [[noreturn]]
extern "C" {
#endif

void		context_init_idle(struct context *, void *kstack_top);
void		context_init_kthread(struct context *, void *kstack_top,
				     void (*entry)(void *), void *arg);
void		context_init_uthread(struct context *, void *kstack_top,
				     void *ustack_top, void (*entry)(void),
				     long retval);
void		context_alloc_siginfo(struct context *, siginfo_t **,
				      ucontext_t **);
void		context_init_ucontext(struct context *, const k_sigset_t *,
				      ucontext_t *);
void		context_set_signal(struct context *, const k_sigset_t *,
				   void (*handler)(int), void (*restorer)(void),
				   int, const siginfo_t *, const ucontext_t *,
				   int);
void		context_set_tls(struct context *, void *);
bool		context_in_signal(struct context *);
void		context_switch(struct thread *, struct thread *);
int		context_restore(struct context *, k_sigset_t *);
int		context_siginfo_restore(struct context *, k_sigset_t *);
void		context_cleanup(struct context *);
void		interrupt_enable(void);
void		interrupt_disable(void);
void		interrupt_save(int *);
void		interrupt_restore(int);
void		interrupt_mask(int);
void		interrupt_unmask(int, int);
void		interrupt_setup(int, int);
void		interrupt_init(void);
int		interrupt_to_ist_priority(int);
bool		interrupt_from_userspace(void);
void		early_console_init(void);
void		early_console_print(const char *, size_t);
void		machine_memory_init(void);
void		machine_init(void);
void		machine_driver_init(void);
void		machine_ready(void);
void		machine_idle(void);
void		machine_reset(void);
void		machine_poweroff(void);
void		machine_suspend(void);
noreturn void	machine_panic(void);
void		arch_backtrace(struct thread *);
bool		arch_check_elfhdr(const Elf32_Ehdr *);
unsigned	arch_elf_hwcap(void);
void	       *arch_stack_align(void *);
void		cache_coherent_exec(const void *, size_t);

#if defined(CONFIG_MMU)
void		mmu_init(struct mmumap *, size_t);
struct pgd     *mmu_newmap(pid_t);
void		mmu_delmap(struct pgd *);
int		mmu_map(struct pgd *, phys *, void *, size_t, int);
void		mmu_premap(void *, void *);
void		mmu_switch(struct pgd *);
void		mmu_preload(void *, void *, void *);
phys	       *mmu_extract(struct pgd *, void *, size_t, int);
#endif

#if defined(CONFIG_MPU)
void		mpu_init(const struct mmumap*, size_t, int);
void		mpu_switch(const struct as *);
void		mpu_unmap(const void *, size_t);
void		mpu_map(const void *, size_t, int);
void		mpu_protect(const void *, size_t, int);
void		mpu_fault(const void *);
void		mpu_dump(void);
#endif

#if defined(__cplusplus)
} /* extern "C" */
#undef noreturn
#endif

#endif /* !arch_h */
