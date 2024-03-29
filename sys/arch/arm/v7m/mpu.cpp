#include "mpu.h"
#include <arch/mmu.h>

#include <arch/mmio.h>
#include <cpu.h>
#include <debug.h>
#include <irq.h>
#include <kernel.h>
#include <sections.h>
#include <seg.h>
#include <sig.h>
#include <sys/mman.h>
#include <sys/param.h>
#include <task.h>
#include <thread.h>

/* Tell gcc not to use FPU registers */
#if defined(__arm__) && defined(__GNUC__) && !defined(__clang__)
#pragma GCC target("general-regs-only")
#endif

__fast_bss size_t fixed;		    /* number of fixed regions */
__fast_bss size_t stack;		    /* number of stack regions */
__fast_bss size_t victim;		    /* next victim to evict */
__fast_bss const void *fault_addr;	    /* last fault address */
__fast_bss const thread *mapped_thread;	    /* currently mapped thread */

static void
clear_dynamic()
{
	stack = 0;
	mapped_thread = nullptr;
	fault_addr = 0;

	const size_t regions = read32(&MPU->TYPE).DREGION;
	for (size_t i = fixed; i < regions; ++i) {
		write32(&MPU->RNR, i);
		write32(&MPU->RASR, {});
	}
}

static void
static_region(const mmumap *map, size_t i)
{
	if (!is_pow2(map->size))
		panic("region must be power-of-2 sized");
	if (map->paddr.phys() & (map->size - 1))
		panic("region must be aligned on size boundary");
	write32(&MPU->RBAR, [&] {
		mpu::rbar r{};
		r.REGION = i;
		r.VALID = 1;
		r.ADDR = map->paddr.phys() >> 5;
		return r;
	}());
	write32(&MPU->RASR, [&] {
		mpu::rasr r{};
		r.r = map->flags;
		r.ENABLE = 1;
		r.SIZE = floor_log2(map->size) - 1;
		return r;
	}());
}

__fast_text static mpu::rasr
prot_to_rasr(int prot)
{
	switch (prot & (PROT_READ | PROT_WRITE | PROT_EXEC)) {
	case PROT_READ:
		return RASR_USER_R_WBWA;
	case PROT_READ | PROT_EXEC:
		return RASR_USER_RX_WBWA;
	case PROT_READ | PROT_WRITE:
		return RASR_USER_RW_WBWA;
	case PROT_READ | PROT_WRITE | PROT_EXEC:
		return RASR_USER_RWX_WBWA;
	default:
		panic("bad prot");
	}
}

/*
 * mpu_init - initialise memory protection unit
 */
void
mpu_init(const mmumap *map, size_t count, int flags)
{
	const size_t regions = read32(&MPU->TYPE).DREGION;

	if (!regions)
		panic("MPU not implemented");
	if (regions > 16)
		panic("MPU not supported"); /* RBAR.REGION supports 0 to 15 */
	if (count >= regions - 2)
		panic("invalid");

	/* all regions must be initialised before enabling */
	for (size_t i = 0; i < count; ++i)
		static_region(map + i, i);
	fixed = victim = count;
	clear_dynamic();

	write32(&MPU->CTRL, [&]{
		mpu::ctrl r{};
		r.ENABLE = 1;
		r.PRIVDEFENA = flags & MPU_ENABLE_DEFAULT_MAP;
		return r;
	}());

	dbg("PMSAv7 MPU initialised, %d dynamic regions\n", regions - fixed);
}

/*
 * mpu_switch - switch mpu to new address space
 */
void
mpu_switch(const as *as)
{
	const int s = irq_disable();
	clear_dynamic();
	mpu_user_thread_switch();
	irq_restore(s);
}

/*
 * mpu_user_thread_switch - switch mpu userspace thread context
 */
__fast_text void
mpu_user_thread_switch()
{
	thread *t = thread_cur();

	assert(t->task != &kern_task);

	if (t == mapped_thread)
		return;
	if (mapped_thread && mapped_thread->task != t->task)
		mpu_switch(t->task->as);

	/* zombies have no ustack */
	if (!t->ctx.usp)
		return;

	/* map stack */
	const size_t regions = read32(&MPU->TYPE).DREGION;
	const seg *seg;
	if (auto r = as_find_seg(t->task->as, (void *)t->ctx.usp);
	    !r.ok() || seg_prot(r.val()) == PROT_NONE) {
		sig_thread(t, SIGSEGV);
		return;
	} else seg = r.val();
	stack = 0;
	const auto rasr_prot = prot_to_rasr(seg_prot(seg));
	for (std::byte *a = (std::byte *)seg_begin(seg); a < seg_end(seg);) {
		const size_t size = (std::byte *)seg_end(seg) - a;
		size_t o = MIN(std::countr_zero((uintptr_t)a), floor_log2(size));
		write32(&MPU->RNR, fixed + stack);
		write32(&MPU->RASR, {});
		write32(&MPU->RBAR, {.ADDR = (uintptr_t)a >> 5});
		write32(&MPU->RASR, [&] {
			mpu::rasr r{rasr_prot};
			r.ENABLE = 1;
			r.SIZE = o - 1;
			return r;
		}());
		a += 1 << o;
		++stack;

		if (fixed + stack == regions) {
			sig_thread(t, SIGSEGV);
			return;
		}
	}

	fault_addr = 0;
	mapped_thread = t;
	if (victim < fixed + stack)
		victim = fixed + stack;
}

/*
 * mpu_thread_terminate - notify mpu of terminated thread
 */
void
mpu_thread_terminate(thread *th)
{
	const int s = irq_disable();
	if (th == mapped_thread)
		clear_dynamic();
	irq_restore(s);
}

/*
 * mpu_unmap - unmap region from currently active address space
 */
void
mpu_unmap(const void *addr, size_t len)
{
	const int s = irq_disable();
	clear_dynamic();
	mpu_user_thread_switch();
	irq_restore(s);
}

/*
 * mpu_map - map region into currently active address space
 */
void
mpu_map(const void *addr, size_t len, int prot)
{
	/* nothing to do, rely on fault handler */
}

/*
 * mpu_protect - change protection flags on address range in currently active
 *		 address space
 */
void
mpu_protect(const void *addr, size_t len, int prot)
{
	const int s = irq_disable();
	clear_dynamic();
	mpu_user_thread_switch();
	irq_restore(s);
}

/*
 * mpu_fault - handle mpu fault
 */
__fast_text void
mpu_fault(const void *addr, size_t len)
{
	/* double fault at the same address means that last time we faulted in
	   a region it didn't satisfy the MPU */
	const seg *seg;
	if (auto r = as_find_seg(task_cur()->as, addr);
	   !r.ok() || seg_prot(r.val()) == PROT_NONE || addr == fault_addr ||
	    (len && (std::byte *)addr + len > (std::byte *)seg_end(r.val()))) {
		sig_thread(thread_cur(), SIGSEGV);
		return;
	} else seg = r.val();
	fault_addr = addr;

again:;
	/* find largest power-of-2 sized region containing addr within seg */
	const size_t order = MIN(
	    floor_log2((uintptr_t)addr ^ (uintptr_t)seg_end(seg)),
	    floor_log2((-(uintptr_t)addr - 1) ^ -(uintptr_t)seg_begin(seg)));

	/* configure MPU */
	const uintptr_t region_base = (uintptr_t)addr & -(1UL << order);
	write32(&MPU->RNR, victim);
	write32(&MPU->RASR, {});
	write32(&MPU->RBAR, {.ADDR = region_base >> 5});
	write32(&MPU->RASR, [&] {
		mpu::rasr r{prot_to_rasr(seg_prot(seg))};
		r.ENABLE = 1;
		r.SIZE = order - 1;
		return r;
	}());

	if (++victim == read32(&MPU->TYPE).DREGION)
		victim = fixed + stack;

	/* multiple mappings if access crosses mapping boundary */
	if (len) {
		std::byte *region_end = (std::byte *)region_base + (1UL << order);
		if ((std::byte *)addr + len > region_end) {
			addr = region_end;
			len = (std::byte *)addr + len - region_end;
			goto again;
		}
	}
}

/*
 * mpu_dump - dump mpu state
 */
void
mpu_dump()
{
#if defined(CONFIG_DEBUG)
	dbg("*** MPU dump ***\n");
	dbg("fixed:%x stack:%x victim:%x fault_addr:%8p\n",
	    fixed, stack, victim, fault_addr);

	const mpu::type type = read32(&MPU->TYPE);
	dbg("MPU_TYPE %08x: SEPARATE:%d IREGION:%d DREGION:%d\n",
	    type.r, type.SEPARATE.value(), type.IREGION.value(), type.DREGION.value());

	const mpu::ctrl ctrl = read32(&MPU->CTRL);
	dbg("MPU_CTRL %08x: ENABLE:%d HFNMIENA:%d PRIVDEFENA:%d\n",
	    ctrl.r, ctrl.ENABLE.value(), ctrl.HFNMIENA.value(), ctrl.PRIVDEFENA.value());

	for (size_t i = 0; i < read32(&MPU->TYPE).DREGION; ++i) {
		write32(&MPU->RNR, i);

		const mpu::rbar rbar = read32(&MPU->RBAR);
		const mpu::rasr rasr = read32(&MPU->RASR);

		assert(rbar.REGION == i);

		if (rasr.ENABLE)
			dbg("Region %x: ADDR:%08x SIZE:%08x SRD:%02x TEX:%x C:%d B:%d S:%d AP:%x XN:%d\n",
			    i, rbar.ADDR << 5, 1 << (rasr.SIZE + 1), rasr.SRD.value(), rasr.TEX.value(), rasr.C.value(), rasr.B.value(), rasr.S.value(), static_cast<int>(rasr.AP.value()), static_cast<int>(rasr.XN.value()));
		else
			dbg("Region %x: disabled\n", i);
	}
#endif
}
