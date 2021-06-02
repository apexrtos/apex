#pragma once

/*
 * intrinsics.h
 */

#ifndef __ASSEMBLY__

template<typename SPR>
void
mtspr(SPR v)
{
	static_assert(sizeof v == 4);
	asm volatile(
		"mtspr %0, %1"
		:
		: "K"(SPR::SPRN), "r"(v.r)
	);
}

template<typename SPR>
SPR
mfspr()
{
	SPR v;
	static_assert(sizeof v == 4);
	asm volatile(
		"mfspr %0, %1"
		: "=r"(v.r)
		: "K"(SPR::SPRN)
	);
	return v;
}

static inline
MSR
mfmsr()
{
	MSR v;
	static_assert(sizeof v == 4);
	asm volatile(
		"mfmsr %0"
		: "=r"(v.r)
	);
	return v;
}

static inline
void
mtmsr(MSR v)
{
	static_assert(sizeof v == 4);
	asm volatile(
		"mtmsr %0\n"
		"isync\n"
		: : "r"(v.r)
	);
}

static inline
void
tlbre()
{
	asm volatile(
		"tlbre"
	);
}

static inline
void
tlbwe()
{
	asm volatile(
		"isync\n"
		"tlbwe\n"
		"isync\n"
	);

}

#endif
