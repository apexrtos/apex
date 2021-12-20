#pragma once

/*
 * csrr - read CSR
 */
template<typename CSR>
CSR
csrr()
{
	CSR v;
	static_assert(sizeof v == 4);
	asm(
		"csrr %0, %1"
		: "=r"(v.r)
		: "i"(CSR::CSRN)
	);
	return v;
}

/*
 * csrw - write CSR
 */
template<typename CSR>
void
csrw(CSR v)
{
	static_assert(sizeof v == 4);
	asm(
		"csrw %0, %1"
		:
		: "i"(CSR::CSRN), "r"(v.r)
	);
}

/*
 * csrs - set bits in CSR
 */
template<typename CSR>
void
csrs(CSR v)
{
	static_assert(sizeof v == 4);
	if (__builtin_constant_p(v.r) && v.r < 32) {
		asm(
			"csrsi %0, %1"
			:
			: "i"(CSR::CSRN), "i"(v.r)
		);
	} else {
		asm(
			"csrs %0, %1"
			:
			: "i"(CSR::CSRN), "r"(v.r)
		);
	}
}

/*
 * csrc - clear bits in CSR
 */
template<typename CSR>
void
csrc(CSR v)
{
	static_assert(sizeof v == 4);
	if (__builtin_constant_p(v.r) && v.r < 32) {
		asm(
			"csrci %0, %1"
			:
			: "i"(CSR::CSRN), "i"(v.r)
		);
	} else {
		asm(
			"csrc %0, %1"
			:
			: "i"(CSR::CSRN), "r"(v.r)
		);
	}
}

/*
 * csrrc - read CSR then clear bits, return old value
 */
template<typename CSR>
CSR
csrrc(CSR v)
{
	static_assert(sizeof v == 4);
	if (__builtin_constant_p(v.r) && v.r < 32) {
		asm volatile(
			"csrrci %0, %1, %2"
			: "=r"(v)
			: "i"(CSR::CSRN), "i"(v.r)
		);
	} else {
		asm volatile(
			"csrrc %0, %1, %2"
			: "=r"(v)
			: "i"(CSR::CSRN), "r"(v.r)
		);
	}
	return v;
}
