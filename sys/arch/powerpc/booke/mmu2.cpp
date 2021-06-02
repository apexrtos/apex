/*
 * Two level MMU (TLB0 and TLB1)
 *
 * TLB1 usually has a few entries (16) and supports many page sizes
 * TLB0 usually has many entries (256) and supports limited page sizes
 *
 * We use TLB1 for kernel mappings and TLB0 for user mappings.
 */

#include <arch/mmu.h>

#include <cpu.h>
#include <debug.h>
#include <kernel.h>
#include <sys/mman.h>

#if defined(POWER_MAV_2)
constexpr auto size_power = 1;
#else
constexpr auto size_power = 2;
#endif

/*
 * mmu_init
 */
void
mmu_init(const mmumap *maps, size_t nmaps)
{
	TLB1CFG cfg = mfspr<TLB1CFG>();

	assert(nmaps <= cfg.NENTRY);

	/* write mappings */
	size_t ent = 0;
	for (; ent < nmaps; ++ent) {
		const auto &m = maps[ent];

		/* mmu_init only supports exact tlb sizes for now */
		assert(is_pow2(m.size));
		assert(!(floor_log2(m.size / 1024) % size_power));

		MAS0 mas0 = {};
		mas0.TLBSEL = 1;
		mas0.ESEL = ent;
		mtspr<MAS0>(mas0);

		MAS1 mas1 = {};
		mas1.V = true;
		mas1.IPROT = true;
		mas1.TID = 0;
		mas1.TS = 0;
		mas1.TSIZE = floor_log2(m.size / 1024);
		mtspr<MAS1>(mas1);

		MAS2 mas2 = {};
		mas2.EPN = reinterpret_cast<uintptr_t>(m.vaddr) >> mas2.EPN.offset;
		mas2.r |= m.flags;
		mtspr<MAS2>(mas2);

		MAS3 mas3 = {};
		mas3.RPN = m.paddr.phys() >> mas3.RPN.offset & mas3.RPN.max;
		mas3.SR = m.prot & PROT_READ;
		mas3.SW = m.prot & PROT_WRITE;
		mas3.SX = m.prot & PROT_EXEC;
		mtspr<MAS3>(mas3);

#if defined(CONFIG_PAE)
		MAS7 mas7 = {};
		mas7.RPNU = m.paddr.phys() >> 32;
		mtspr<MAS7>(mas7);
#endif

		tlbwe();
	}

	/* invalidate all other mappings */
	for (; ent < cfg.NENTRY; ++ent) {
		MAS0 mas0 = {};
		mas0.TLBSEL = 1;
		mas0.ESEL = ent;
		mtspr<MAS0>(mas0);

		MAS1 mas1 = {};
		mas1.V = false;
		mtspr<MAS1>(mas1);

		tlbwe();
	}
}

/*
 * mmu_newmap
 */
unique_ptr_err<pgd>
mmu_newmap(pid_t)
{
	return nullptr;
}

/*
 * mmu_map
 */
int
mmu_map(as & phys, void *, size_t, int)
{
	#warning mmu_map not implemented
	assert(0);
}

/*
 * mmu_early_map - some I/O memory to assist debugging
 */
void
mmu_early_map(phys phys, void *virt, size_t len, unsigned flags)
{
#warning check if already mapped?

	TLB1CFG cfg = mfspr<TLB1CFG>();

	/* We expect TLB1 to be fully associative */
	assert(cfg.NENTRY > 0);
	assert(!cfg.ASSOC || cfg.ASSOC == cfg.NENTRY);

	/* mmu_early_map only creates one entry */
	assert(is_pow2(len));
	assert(!(floor_log2(len / 1024) % size_power));

	/* find an unused entry in TLB1 */
	size_t ent = 0;
	while (ent < cfg.NENTRY) {
		MAS0 mas0 = {};
		mas0.TLBSEL = 1;
		mas0.ESEL = ent;
		mtspr<MAS0>(mas0);

		tlbre();

		if (!mfspr<MAS1>().V)
			break;

		++ent;
	}
	assert(ent < cfg.NENTRY);

	/* write entry */
	MAS1 mas1 = {};
	mas1.V = true;
	mas1.IPROT = true;
	mas1.TID = 0;
	mas1.TS = 0;
	mas1.TSIZE = floor_log2(len / 1024);
	mtspr<MAS1>(mas1);

	MAS2 mas2 = {};
	mas2.EPN = reinterpret_cast<uintptr_t>(virt) >> mas2.EPN.offset;
	mas2.r |= flags;
	mtspr<MAS2>(mas2);

	MAS3 mas3 = {};
	mas3.RPN = phys.phys() >> mas3.RPN.offset & mas3.RPN.max;
	mas3.SR = true;
	mas3.SW = true;
	mtspr<MAS3>(mas3);

#if defined(CONFIG_PAE)
	MAS7 mas7 = {};
	mas7.RPNU = phys.phys() >> 32;
	mtspr<MAS7>(mas7);
#endif

	tlbwe();
}

/*
 * mmu_switch
 */
void
mmu_switch(const as &)
{
	#warning mmu_switch not implemented
	assert(0);
}

/*
 * mmu_extract
 */
phys
mmu_extract(const as &, void *, size_t, int)
{
	#warning mmu_extract not implemented
	assert(0);
}

/*
 * mmu_dump
 */
void
mmu_dump()
{
	TLB1CFG cfg = mfspr<TLB1CFG>();

	dbg("MMU Dump\n");
	for (size_t ent = 0; ent < cfg.NENTRY; ++ent) {
		MAS0 mas0 = {};
		mas0.TLBSEL = 1;
		mas0.ESEL = ent;
		mtspr<MAS0>(mas0);

		tlbre();

		MAS1 mas1 = mfspr<MAS1>();
		MAS2 mas2 = mfspr<MAS2>();
		MAS3 mas3 = mfspr<MAS3>();

		if (!mas1.V) {
			dbg("%2d: INVALID\n", ent);
			continue;
		}

		dbg("%2d: mas1 %x\n", ent, mas1.r);
		dbg("            V %x\n", (unsigned)mas1.V);
		dbg("            IPROT %x\n", (unsigned)mas1.IPROT);
		dbg("            TID %x\n", (unsigned)mas1.TID);
		dbg("            TS %x\n", (unsigned)mas1.TS);
		dbg("            TSIZE %x\n", (unsigned)mas1.TSIZE);
		dbg("     mas2 %x\n", mas2.r);
		dbg("            EPN %x\n", (unsigned)mas2.EPN);
		dbg("            W %x\n", (unsigned)mas2.W);
		dbg("            I %x\n", (unsigned)mas2.I);
		dbg("            G %x\n", (unsigned)mas2.G);
		dbg("            M %x\n", (unsigned)mas2.M);
		dbg("            E %x\n", (unsigned)mas2.E);
		dbg("     mas3 %x\n", mas3.r);
		dbg("            RPN %x\n", (unsigned)mas3.RPN);
		dbg("            U %x\n", (unsigned)mas3.U);
		dbg("            UX %x\n", (unsigned)mas3.UX);
		dbg("            SX %x\n", (unsigned)mas3.SX);
		dbg("            UW %x\n", (unsigned)mas3.UW);
		dbg("            SW %x\n", (unsigned)mas3.SW);
		dbg("            UR %x\n", (unsigned)mas3.UR);
		dbg("            SR %x\n", (unsigned)mas3.SR);
	}
}

