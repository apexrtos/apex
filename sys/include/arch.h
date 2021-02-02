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

#include <assert.h>
#include <conf/config.h>
#include <elf.h>
#include <signal.h>
#include <stdalign.h>
#include <stdnoreturn.h>
#include <sys/include/compiler.h>
#include <sys/include/types.h>

struct as;
struct bootargs;
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
	unsigned	flags;	/* machine specific flags */
};

#if defined(__cplusplus)
#define noreturn [[noreturn]]
extern "C" {
#endif

void		context_init_idle(struct context *, void *kstack_top);
void		context_init_kthread(struct context *, void *kstack_top,
				     void (*entry)(void *), void *arg);
int		context_init_uthread(struct context *, struct as *,
				     void *kstack_top, void *ustack_top,
				     void (*entry)(void), long retval);
void		context_restore_vfork(struct context *, struct as *);
bool		context_set_signal(struct context *, const k_sigset_t *,
				   void (*handler)(int), void (*restorer)(void),
				   int, const siginfo_t *, int);
void		context_set_tls(struct context *, void *);
void		context_switch(struct thread *, struct thread *);
bool		context_restore(struct context *, k_sigset_t *, int *, bool);
void		context_terminate(struct thread *);
void		context_free(struct context *);
void		interrupt_enable(void);
void		interrupt_disable(void);
void		interrupt_save(int *);
void		interrupt_restore(int);
bool		interrupt_enabled(void);
void		interrupt_mask(int);
void		interrupt_unmask(int, int);
void		interrupt_setup(int, int);
void		interrupt_init(void);
int		interrupt_to_ist_priority(int);
bool		interrupt_from_userspace(void);
bool		interrupt_running(void);
void		early_console_init(void);
void		early_console_print(const char *, size_t);
void		machine_init(struct bootargs *);
void		machine_driver_init(struct bootargs *);
void		machine_idle(void);
noreturn void	machine_reset(void);
void		machine_poweroff(void);
void		machine_suspend(void);
noreturn void	machine_panic(void);
void		arch_schedule(void);
void		arch_backtrace(struct thread *);
bool		arch_check_elfhdr(const Elf32_Ehdr *);
unsigned	arch_elf_hwcap(void);
void	       *arch_ustack_align(void *);
void	       *arch_kstack_align(void *);
void		cache_coherent_exec(const void *, size_t);
void		cache_flush(const void *, size_t);
void		cache_invalidate(const void *, size_t);
void		cache_flush_invalidate(const void *, size_t);
bool		cache_coherent_range(const void *, size_t);
bool		cache_aligned(const void *, size_t);
void		memory_barrier(void);
void		read_memory_barrier(void);
void		write_memory_barrier(void);
uint8_t		mmio_read8(const void *);
uint16_t	mmio_read16(const void *);
uint32_t	mmio_read32(const void *);
#if UINTPTR_MAX == 0xffffffffffffffff
uint64_t	mmio_read64(const void *);
#endif
void		mmio_write8(void *, uint8_t);
void		mmio_write16(void *, uint16_t);
void		mmio_write32(void *, uint32_t);
#if UINTPTR_MAX == 0xffffffffffffffff
void		mmio_write64(void *, uint64_t);
#endif

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
void		mpu_fault(const void *, size_t);
void		mpu_dump(void);
#endif

#if defined(CONFIG_SMP)
static inline void
smp_memory_barrier()
{
	memory_barrier();
}

static inline void
smp_read_memory_barrier()
{
	read_memory_barrier();
}

static inline void
smp_write_memory_barrier()
{
	write_memory_barrier();
}
#else
static inline void
smp_memory_barrier()
{
	compiler_barrier();
}

static inline void
smp_read_memory_barrier()
{
	compiler_barrier();
}

static inline void
smp_write_memory_barrier()
{
	compiler_barrier();
}
#endif

#if defined(__cplusplus)
} /* extern "C" */
#undef noreturn

template<typename T>
inline T
readN(const T *p)
{
	static_assert(sizeof(T) <= sizeof(uintptr_t));
	static_assert(std::is_trivially_copyable_v<T>);

	T tmp;

	switch (sizeof(T)) {
	case 1:
		typedef uint8_t __attribute__((__may_alias__)) u8a;
		*(u8a *)&tmp = mmio_read8(p);
		break;
	case 2:
		typedef uint16_t __attribute__((__may_alias__)) u16a;
		*(u16a *)&tmp = mmio_read16(p);
		break;
	case 4:
		typedef uint32_t __attribute__((__may_alias__)) u32a;
		*(u32a *)&tmp = mmio_read32(p);
		break;
#if UINTPTR_MAX == 0xffffffffffffffff
	case 8:
		typedef uint64_t __attribute__((__may_alias__)) u64a;
		*(u64a *)&tmp = mmio_read64(p);
		break;
#endif
	}

	return tmp;
}

template<typename T>
inline T
read8(const T *p)
{
	static_assert(sizeof(T) == 1);
	return readN(p);
}

template<typename T>
inline T
read16(const T *p)
{
	static_assert(sizeof(T) == 2);
	return readN(p);
}

template<typename T>
inline T
read32(const T *p)
{
	static_assert(sizeof(T) == 4);
	return readN(p);
}

template<typename T>
inline T
read64(const T *p)
{
	static_assert(sizeof(T) == 8);
	return readN(p);
}

#else

#define read8(p) ({ \
	static_assert(sizeof(*p) == 1, ""); \
	(__typeof__(*p))mmio_read8(p);})
#define read16(p) ({ \
	static_assert(sizeof(*p) == 2, ""); \
	static_assert(alignof(__typeof(*p)) >= 2, ""); \
	(__typeof__(*p))mmio_read16(p);})
#define read32(p) ({ \
	static_assert(sizeof(*p) == 4, ""); \
	static_assert(alignof(__typeof(*p)) >= 4, ""); \
	(__typeof__(*p))mmio_read32(p);})
#if UINTPTR_MAX == 0xffffffffffffffff
#define read64(p) ({ \
	static_assert(sizeof(*p) == 8, ""); \
	static_assert(alignof(__typeof(*p)) >= 8); \
	(__typeof__(*p))mmio_read64(p);})
#endif

#endif /* __cplusplus */

#define write8(p, ...) ({ \
	static_assert(sizeof(*p) == 1, ""); \
	mmio_write8(p, __VA_ARGS__);})
#define write16(p, ...) ({ \
	static_assert(sizeof(*p) == 2, ""); \
	static_assert(alignof(__typeof(*p)) >= 2, ""); \
	mmio_write16(p, __VA_ARGS__);})
#define write32(p, ...) ({ \
	static_assert(sizeof(*p) == 4, ""); \
	static_assert(alignof(__typeof(*p)) >= 4, ""); \
	mmio_write32(p, __VA_ARGS__);})
#if UINTPTR_MAX == 0xffffffffffffffff
#define write64(p, ...) ({ \
	static_assert(sizeof(*p) == 8, ""); \
	static_assert(alignof(__typeof(*p)) >= 8, ""); \
	mmio_write64(p, __VA_ARGS__);})
#endif

#endif /* !arch_h */
