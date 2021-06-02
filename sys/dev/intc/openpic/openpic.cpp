#include "init.h"
#include "openpic.h"

#include <arch/interrupt.h>
#include <arch/mmio.h>
#include <cassert>
#include <cstddef>
#include <debug.h>
#include <irq.h>
#include <lib/bitfield.h>
#include <sections.h>
#include <thread.h>

/*
 * OpenPIC hardware registers
 */
struct openpic {
	/* MSB is bit 0, bit ranges are inclusive */
	template<typename StorageType, typename DataType, unsigned Bit>
	using bit = bitfield::bit<StorageType, DataType, 31 - Bit>;
	template<typename StorageType, typename DataType, unsigned StartBit, unsigned EndBit>
	using bits = bitfield::bits<StorageType, DataType, 31 - EndBit, EndBit - StartBit + 1>;

	/* Interrupt Vector/Priority Register */
	union ivp {
		using S = uint32_t;
		struct { S r; };
		bit<S, bool, 0> MSK;
		bit<S, bool, 1> ACTIVITY;
		enum class polarity {
			Low_Falling,
			High_Rising,
		};
		bit<S, polarity, 8> POLARITY;
		enum class sense {
			Edge,
			Level,
		};
		bit<S, sense, 9> SENSE;
		bits<S, unsigned, 12, 15> PRIORITY;
		bits<S, unsigned, 16, 31> VECTOR;
	};

	/* Registers */
	uint8_t res0[0x40];
	uint32_t IPID0;		    /* Interprocessor Interrupt 0 Dispatch */
	uint8_t res1[0xc];
	uint32_t IPID1;		    /* Interprocessor Interrupt 1 Dispatch */
	uint8_t res2[0xc];
	uint32_t IPID2;		    /* Interprocessor Interrupt 2 Dispatch */
	uint8_t res3[0xc];
	uint32_t IPID3;		    /* Interprocessor Interrupt 3 Dispatch */
	uint8_t res4[0xc];
	uint32_t CTP;		    /* Current Task Priority */
	uint8_t res5[0xc];
	uint32_t WHOAMI;	    /* Who Am I */
	uint8_t res6[0xc];
	uint32_t IACK;		    /* Interrupt Acknowledge */
	uint8_t res7[0xc];
	uint32_t EOI;		    /* End of Interrupt */
	uint8_t res8[0xf4c];
	union fr {
		using S = uint32_t;
		struct { S r; };
		bits<S, unsigned, 5, 15> NIRQ;
		bits<S, unsigned, 19, 23> NCPU;
		bits<S, unsigned, 24, 31> VID;
	} FR;			    /* Feature Reporting */
	uint8_t res9[0x1c];
	uint32_t GC;		    /* Global Configuration */
	uint8_t resA[0x5c];
	uint32_t VI;		    /* Vendor Identification */
	uint8_t resB[0xc];
	uint32_t PI;		    /* Processor Initialization */
	uint8_t resC[0xc];
	uint32_t IPIVP0;	    /* IPI 0 Vector/Priority */
	uint8_t resD[0xc];
	uint32_t IPIVP1;	    /* IPI 1 Vector/Priority */
	uint8_t resE[0xc];
	uint32_t IPIVP2;	    /* IPI 2 Vector/Priority */
	uint8_t resF[0xc];
	uint32_t IPIVP3;	    /* IPI 3 Vector/Priority */
	uint8_t res10[0xc];
	uint32_t SV;		    /* Spurious Vector */
	uint8_t res11[0xef1c];
	struct {
		ivp IVP;	    /* Interrupt Vector/Priority */
		uint8_t res0[0xc];
		uint32_t ID;	    /* Interrupt Destination */
		uint8_t res1[0xc];
	} IRQ[16 + 112];	    /* 16 External + 112 Internal Interrupts */
	uint8_t res12[0xf000];
	struct {
		uint8_t res0[0x40];
		uint32_t IPID0;	    /* Interprocessor Interrupt 0 Dispatch */
		uint8_t res1[0xc];
		uint32_t IPID1;	    /* Interprocessor Interrupt 1 Dispatch */
		uint8_t res2[0xc];
		uint32_t IPID2;	    /* Interprocessor Interrupt 2 Dispatch */
		uint8_t res3[0xc];
		uint32_t IPID3;	    /* Interprocessor Interrupt 3 Dispatch */
		uint8_t res4[0xc];
		uint32_t CTP;	    /* Current Task Priority */
		uint8_t res5[0xc];
		uint32_t WHOAMI;    /* Who Am I */
		uint8_t res6[0xc];
		uint32_t IACK;	    /* Interrupt Acknowledge */
		uint8_t res7[0xc];
		uint32_t EOI;	    /* End of Interrupt */
		uint8_t res8[0xf4c];
	} CPU[4];		    /* Per CPU Registers */
};
static_assert(offsetof(openpic, IPID0) == 0x40);
static_assert(offsetof(openpic, IPID1) == 0x50);
static_assert(offsetof(openpic, IPID2) == 0x60);
static_assert(offsetof(openpic, IPID3) == 0x70);
static_assert(offsetof(openpic, CTP) == 0x80);
static_assert(offsetof(openpic, WHOAMI) == 0x90);
static_assert(offsetof(openpic, IACK) == 0xA0);
static_assert(offsetof(openpic, EOI) == 0xB0);
static_assert(offsetof(openpic, FR) == 0x1000);
static_assert(offsetof(openpic, GC) == 0x1020);
static_assert(offsetof(openpic, VI) == 0x1080);
static_assert(offsetof(openpic, PI) == 0x1090);
static_assert(offsetof(openpic, IPIVP0) == 0x10A0);
static_assert(offsetof(openpic, IPIVP1) == 0x10B0);
static_assert(offsetof(openpic, IPIVP2) == 0x10C0);
static_assert(offsetof(openpic, IPIVP3) == 0x10D0);
static_assert(offsetof(openpic, SV) == 0x10E0);
static_assert(offsetof(openpic, IRQ) == 0x10000);
static_assert(offsetof(openpic, CPU[0]) == 0x20000);
static_assert(offsetof(openpic, CPU[1]) == 0x21000);
static_assert(offsetof(openpic, CPU[2]) == 0x22000);
static_assert(offsetof(openpic, CPU[3]) == 0x23000);

namespace {

__fast_bss openpic *pic;

};

void
interrupt_mask(int vector)
{
	assert(vector <= read32(&pic->FR).NIRQ);

	/* mask vector */
	write32(&pic->IRQ[vector].IVP, {.MSK = true});
}

void
interrupt_unmask(int vector, int prio)
{
	assert(vector <= read32(&pic->FR).NIRQ);
	assert(prio >= IPL_MIN && prio <= IPL_MAX);

	/* unmask vector, set priority and set vector number */
	write32(&pic->IRQ[vector].IVP, [&]{
		auto v = read32(&pic->IRQ[vector].IVP);
		v.MSK = false;
		v.PRIORITY = prio;
		return v;
	}());
}

void
interrupt_setup(int vector, int mode)
{
	assert(vector <= read32(&pic->FR).NIRQ);

	/* set polarity and sense */
	write32(&pic->IRQ[vector].IVP, [&]{
		auto v = read32(&pic->IRQ[vector].IVP);
		switch (mode) {
		case OPENPIC_EDGE_FALLING:
			v.POLARITY = openpic::ivp::polarity::Low_Falling;
			v.SENSE = openpic::ivp::sense::Edge;
			break;
		case OPENPIC_EDGE_RISING:
			v.POLARITY = openpic::ivp::polarity::High_Rising;
			v.SENSE = openpic::ivp::sense::Edge;
			break;
		case OPENPIC_LEVEL_LOW:
			v.POLARITY = openpic::ivp::polarity::Low_Falling;
			v.SENSE = openpic::ivp::sense::Level;
			break;
		case OPENPIC_LEVEL_HIGH:
			v.POLARITY = openpic::ivp::polarity::High_Rising;
			v.SENSE = openpic::ivp::sense::Level;
			break;
		default:
			assert(0);
		}
		return v;
	}());
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

__fast_text void
intc_openpic_irq()
{
	/* get interrupt vector & acknowledge interrupt */
	const auto vector = read32(&pic->IACK);

	/* ok to enable nested interrupts now */
	interrupt_enable();

	/* handle interrupt */
	irq_handler(vector);
}

void
intc_openpic_init(const intc_openpic_desc *d)
{
#warning THIS IS A GIANT HORRIBLE HACK! need to fix translation of d->base
	pic = reinterpret_cast<openpic *>(0xfe040000);

	const auto fr = read32(&pic->FR);

	/* set vector equal to irq number */
	for (auto i = 0u; i < fr.NIRQ; ++i) {
		write32(&pic->IRQ[i].IVP, [&]{
			openpic::ivp v{};
			v.MSK = true;
			v.VECTOR = i;
			return v;
		}());
	}

	dbg("OpenPIC interrupt controller initialised, NIRQ=%d\n",
		static_cast<int>(fr.NIRQ));
}
