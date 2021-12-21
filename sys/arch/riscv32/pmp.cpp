#include <arch/mmu.h>

#include <cassert>
#include <conf/config.h>
#include <cpu.h>
#include <debug.h>
#include <intrinsics.h>
#include <irq.h>
#include <kernel.h>
#include <locore.h>
#include <sections.h>
#include <seg.h>
#include <sig.h>
#include <sys/mman.h>
#include <task.h>
#include <thread.h>

namespace {

__fast_bss size_t victim;		    /* next victim to evict */
__fast_bss const thread *mapped_thread;	    /* currently mapped thread */
__fast_bss const void *fault_addr;	    /* last fault address */

void
clear()
{
	mapped_thread = nullptr;
	csrw(pmpcfg0{.r = 0});
	csrw(pmpcfg1{.r = 0});
	csrw(pmpcfg2{.r = 0});
	csrw(pmpcfg3{.r = 0});
}

#ifndef CONFIG_PMP_MISSING_TOR
void
map_range_tor(const void *begin, const void *end, int prot)
{
	pmpcfg cfg{};
	cfg.A = pmpcfg::Match::TOR;
	cfg.X = prot & PROT_EXEC;
	cfg.W = prot & PROT_WRITE;
	cfg.R = prot & PROT_READ;
	auto sa = reinterpret_cast<uint32_t>(begin) >> 2;
	auto ea = reinterpret_cast<uint32_t>(end) >> 2;

	switch (victim) {
	case 0:
		csrw([&] {
			auto v = csrr<pmpcfg0>();
			v.pmp1cfg = cfg;
			return v;
		}());
		csrw(pmpaddr0{sa});
		csrw(pmpaddr1{ea});
		break;
	case 1:
		csrw([&] {
			auto v = csrr<pmpcfg0>();
			v.pmp3cfg = cfg;
			return v;
		}());
		csrw(pmpaddr2{sa});
		csrw(pmpaddr3{ea});
		break;
	case 2:
		csrw([&] {
			auto v = csrr<pmpcfg1>();
			v.pmp5cfg = cfg;
			return v;
		}());
		csrw(pmpaddr4{sa});
		csrw(pmpaddr5{ea});
		break;
	case 3:
		csrw([&] {
			auto v = csrr<pmpcfg1>();
			v.pmp7cfg = cfg;
			return v;
		}());
		csrw(pmpaddr6{sa});
		csrw(pmpaddr7{ea});
		break;
	case 4:
		csrw([&] {
			auto v = csrr<pmpcfg2>();
			v.pmp9cfg = cfg;
			return v;
		}());
		csrw(pmpaddr8{sa});
		csrw(pmpaddr9{ea});
		break;
	case 5:
		csrw([&] {
			auto v = csrr<pmpcfg2>();
			v.pmp11cfg = cfg;
			return v;
		}());
		csrw(pmpaddr10{sa});
		csrw(pmpaddr11{ea});
		break;
	case 6:
		csrw([&] {
			auto v = csrr<pmpcfg3>();
			v.pmp13cfg = cfg;
			return v;
		}());
		csrw(pmpaddr12{sa});
		csrw(pmpaddr13{ea});
		break;
	case 7:
		csrw([&] {
			auto v = csrr<pmpcfg3>();
			v.pmp15cfg = cfg;
			return v;
		}());
		csrw(pmpaddr14{sa});
		csrw(pmpaddr15{ea});
		break;
	}

	if (++victim == 8)
		victim = 0;
}
#else
void
map_range_napot(uintptr_t base, size_t order, int prot)
{
	pmpcfg cfg{};
	cfg.A = pmpcfg::Match::NAPOT;
	cfg.X = prot & PROT_EXEC;
	cfg.W = prot & PROT_WRITE;
	cfg.R = prot & PROT_READ;
	auto a = base >> 2 | 0xffffffffu >> (35 - order);

	switch (victim) {
	case 0:
		csrw([&] {
			auto v = csrr<pmpcfg0>();
			v.pmp0cfg = cfg;
			return v;
		}());
		csrw(pmpaddr0{a});
		break;
	case 1:
		csrw([&] {
			auto v = csrr<pmpcfg0>();
			v.pmp1cfg = cfg;
			return v;
		}());
		csrw(pmpaddr1{a});
		break;
	case 2:
		csrw([&] {
			auto v = csrr<pmpcfg0>();
			v.pmp2cfg = cfg;
			return v;
		}());
		csrw(pmpaddr2{a});
		break;
	case 3:
		csrw([&] {
			auto v = csrr<pmpcfg0>();
			v.pmp3cfg = cfg;
			return v;
		}());
		csrw(pmpaddr3{a});
		break;
	case 4:
		csrw([&] {
			auto v = csrr<pmpcfg1>();
			v.pmp4cfg = cfg;
			return v;
		}());
		csrw(pmpaddr4{a});
		break;
	case 5:
		csrw([&] {
			auto v = csrr<pmpcfg1>();
			v.pmp5cfg = cfg;
			return v;
		}());
		csrw(pmpaddr5{a});
		break;
	case 6:
		csrw([&] {
			auto v = csrr<pmpcfg1>();
			v.pmp6cfg = cfg;
			return v;
		}());
		csrw(pmpaddr6{a});
		break;
	case 7:
		csrw([&] {
			auto v = csrr<pmpcfg1>();
			v.pmp7cfg = cfg;
			return v;
		}());
		csrw(pmpaddr7{a});
		break;
	case 8:
		csrw([&] {
			auto v = csrr<pmpcfg2>();
			v.pmp8cfg = cfg;
			return v;
		}());
		csrw(pmpaddr8{a});
		break;
	case 9:
		csrw([&] {
			auto v = csrr<pmpcfg2>();
			v.pmp9cfg = cfg;
			return v;
		}());
		csrw(pmpaddr9{a});
		break;
	case 10:
		csrw([&] {
			auto v = csrr<pmpcfg2>();
			v.pmp10cfg = cfg;
			return v;
		}());
		csrw(pmpaddr10{a});
		break;
	case 11:
		csrw([&] {
			auto v = csrr<pmpcfg2>();
			v.pmp11cfg = cfg;
			return v;
		}());
		csrw(pmpaddr11{a});
		break;
	case 12:
		csrw([&] {
			auto v = csrr<pmpcfg3>();
			v.pmp12cfg = cfg;
			return v;
		}());
		csrw(pmpaddr12{a});
		break;
	case 13:
		csrw([&] {
			auto v = csrr<pmpcfg3>();
			v.pmp13cfg = cfg;
			return v;
		}());
		csrw(pmpaddr13{a});
		break;
	case 14:
		csrw([&] {
			auto v = csrr<pmpcfg3>();
			v.pmp14cfg = cfg;
			return v;
		}());
		csrw(pmpaddr14{a});
		break;
	case 15:
		csrw([&] {
			auto v = csrr<pmpcfg3>();
			v.pmp15cfg = cfg;
			return v;
		}());
		csrw(pmpaddr15{a});
		break;
	}

	if (++victim == 16)
		victim = 0;
}
#endif

}

/*
 * mpu_init - initialise memory protection unit
 */
void
mpu_init(const mmumap*, size_t, int)
{
	/* check PMP granularity */
	csrw(pmpcfg0{.r = 0});
	csrw(pmpaddr0{.r = 0xffffffff});
	auto gran = csrr<pmpaddr0>().r << 2;
	gran &= ~(gran << 1);

	if (gran > PAGE_SIZE)
		panic("PMP granularity is larger than page size");

	dbg("PMP initialised, %u byte granularity\n", gran);
}

/*
 * mpu_switch - switch mpu to new address space
 */
void
mpu_switch(const as *)
{
	/* clear all regions, fault handler will map in as required */
	const int s = irq_disable();
	clear();
	irq_restore(s);
}

/*
 * mpu_unmap - unmap region from currently active address space
 */
void
mpu_unmap(const void *, size_t)
{
	/* clear all regions, fault handler will map in as required */
	const int s = irq_disable();
	clear();
	irq_restore(s);
}

/*
 * mpu_map - map region into currently active address space
 */
void
mpu_map(const void *, size_t, int)
{
	/* fault handler will map in as required */
}

/*
 * mpu_protect - change protection flags on address range in currently active
 *		 address space
 */
void
mpu_protect(const void *, size_t, int)
{
	/* clear all regions, fault handler will map in as required */
	const int s = irq_disable();
	clear();
	irq_restore(s);
}

/*
 * mpu_fault - handle mpu fault
 */
__fast_text
void
mpu_fault(const void *addr, size_t len)
{
	/* double fault at the same address means that last time we faulted in
	   a region the access still didn't satisfy the PMP permissions */
	const seg *seg;
	if (auto r = as_find_seg(task_cur()->as, addr);
	   !r.ok() || seg_prot(r.val()) == PROT_NONE || addr == fault_addr ||
	    (len && (std::byte *)addr + len > (std::byte *)seg_end(r.val()))) {
		sig_thread(thread_cur(), SIGSEGV);
		return;
	} else seg = r.val();
	fault_addr = addr;

#ifndef CONFIG_PMP_MISSING_TOR
	map_range_tor(seg_begin(seg), seg_end(seg), seg_prot(seg));
#else
	/* find largest power-of-2 sized region containing addr within seg */
	const size_t order = std::min(
	    floor_log2((uintptr_t)addr ^ (uintptr_t)seg_end(seg)),
	    floor_log2((-(uintptr_t)addr - 1) ^ -(uintptr_t)seg_begin(seg)));
	const uintptr_t region_base = (uintptr_t)addr & -(1ul << order);
	map_range_napot(region_base, order, seg_prot(seg));
#endif
}

/*
 * mpu_dump - dump mpu state
 */
void
mpu_dump()
{
#ifdef CONFIG_DEBUG
	auto print = [](int n, pmpcfg cfg, uint32_t addr) {
		const char *str[] = {"OFF", "TOR", "NA4", "NAPOT"};
		dbg("region %x: %5s L:%d R:%d W:%x X:%x ADDR:%08x\n",
			n, str[static_cast<int>(cfg.A.value())],
			cfg.L.value(), cfg.R.value(), cfg.W.value(),
			cfg.X.value(), addr);
	};

	dbg("*** MPU dump ***\n");
	dbg("victim:%x fault_addr:%8p\n", victim, fault_addr);
	print(0, csrr<pmpcfg0>().pmp0cfg, csrr<pmpaddr0>().r);
	print(1, csrr<pmpcfg0>().pmp1cfg, csrr<pmpaddr1>().r);
	print(2, csrr<pmpcfg0>().pmp2cfg, csrr<pmpaddr2>().r);
	print(3, csrr<pmpcfg0>().pmp3cfg, csrr<pmpaddr3>().r);
	print(4, csrr<pmpcfg1>().pmp4cfg, csrr<pmpaddr4>().r);
	print(5, csrr<pmpcfg1>().pmp5cfg, csrr<pmpaddr5>().r);
	print(6, csrr<pmpcfg1>().pmp6cfg, csrr<pmpaddr6>().r);
	print(7, csrr<pmpcfg1>().pmp7cfg, csrr<pmpaddr7>().r);
	print(8, csrr<pmpcfg2>().pmp8cfg, csrr<pmpaddr8>().r);
	print(9, csrr<pmpcfg2>().pmp9cfg, csrr<pmpaddr9>().r);
	print(10, csrr<pmpcfg2>().pmp10cfg, csrr<pmpaddr10>().r);
	print(11, csrr<pmpcfg2>().pmp11cfg, csrr<pmpaddr11>().r);
	print(12, csrr<pmpcfg3>().pmp12cfg, csrr<pmpaddr12>().r);
	print(13, csrr<pmpcfg3>().pmp13cfg, csrr<pmpaddr13>().r);
	print(14, csrr<pmpcfg3>().pmp14cfg, csrr<pmpaddr14>().r);
	print(15, csrr<pmpcfg3>().pmp15cfg, csrr<pmpaddr15>().r);
#endif
}

/*
 * mpu_user_thread_switch - switch mpu userspace thread context
 */
__fast_text
void
mpu_user_thread_switch()
{
	thread *t = thread_cur();

	assert(t->task != &kern_task);

	if (t == mapped_thread)
		return;
	if (mapped_thread && mapped_thread->task != t->task)
		mpu_switch(t->task->as);

	/* REVISIT: QEMU PMP implementation throws an illegal instruction
		    trap if mret instruction is run with no PMP regions? */
	trap_frame *tf = reinterpret_cast<trap_frame *>(t->ctx.kstack - sizeof *tf);
	/* map stack */
	mpu_fault(reinterpret_cast<const void *>(tf->sp), 4);
	/* map return address */
	mpu_fault(reinterpret_cast<const void *>(tf->xepc), 4);

	mapped_thread = t;
}
