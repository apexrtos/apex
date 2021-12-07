#include "init.h"
#include "plic.h"

#include <arch/interrupt.h>
#include <arch/mmio.h>
#include <debug.h>
#include <irq.h>
#include <sections.h>
#include <sync.h>
#include <thread.h>

/*
 * SiFive PLIC hardware registers
 */
struct plic {
	uint32_t priority[1024];
	uint32_t pending[32];
	uint8_t res1[3968];
	uint32_t hart0_m_enable[32];
	uint8_t res2[2088832];
	uint32_t hart0_m_priority_threshold;
	uint32_t hart0_m_claim;
};
static_assert(offsetof(plic, pending) == 0x1000);
static_assert(offsetof(plic, hart0_m_enable) == 0x2000);
static_assert(offsetof(plic, hart0_m_priority_threshold) == 0x200000);
static_assert(offsetof(plic, hart0_m_claim) == 0x200004);

namespace {

__fast_bss plic *inst;
a::spinlock_irq lock;

}

void
interrupt_mask(int vector)
{
	assert(vector > 0 && vector < 1024);

	auto reg = inst->hart0_m_enable + vector / 32;
	uint32_t bit = 1ul << vector % 32;

	std::lock_guard l(lock);
	write32(reg, read32(reg) & ~bit);
}

void
interrupt_unmask(int vector, int level)
{
	assert(level > 0 && level <= IPL_MAX);
	assert(vector > 0 && vector < 1024);

	auto reg = inst->hart0_m_enable + vector / 32;
	uint32_t bit = 1ul << vector % 32;

	std::lock_guard l(lock);
	write32(inst->priority + vector, static_cast<uint32_t>(level));
	write32(reg, read32(reg) | bit);
}

void
interrupt_setup(int vector, int mode)
{
	/* nothing to do */
}

void
interrupt_init()
{
	/* nothing to do */
}

int
interrupt_to_ist_priority(int prio)
{
	static_assert(PRI_IST_MIN - PRI_IST_MAX > IPL_MAX - IPL_MIN);
	assert(prio >= IPL_MIN && prio <= IPL_MAX);
	return PRI_IST_MIN - prio;
}

__fast_text
void
intc_sifive_plic_irq()
{
	/* handle interrupts */
	while (auto vector = read32(&inst->hart0_m_claim)) {
		/* plic supports nested interrupts */
		interrupt_enable();

		/* run interrupt handler */
		irq_handler(vector);

		/* acknowledge interrupt */
		write32(&inst->hart0_m_claim, vector);
	}
}

void
intc_sifive_plic_init(const intc_sifive_plic_desc *d)
{
	inst = static_cast<plic *>(phys_to_virt(d->base));

	/* allow all interrupts to hart 0 M mode */
	write32(&inst->hart0_m_priority_threshold, 0u);

	dbg("SiFive PLIC initialised\n");
}
