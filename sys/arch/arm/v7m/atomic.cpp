#include <cstddef>
#include <cstdint>
#include <cstring>
#include <irq.h>

/*
 * The atomic implementations below are valid on uniprocessor systems only:
 * 1. they do not include memory barriers
 * 2. they use interrupt masking as a fallback for unsupported operations
 */
#if defined(CONFIG_SMP)
#error ARMv7-M atomics do not support SMP
#endif

/*
 * atomic_load
 */
extern "C" uint64_t
__atomic_load_8(const volatile void *p, int m)
{
	/* ldrd is restarted on interrupt */
	const volatile uint64_t *p64 = static_cast<const volatile uint64_t *>(p);
	uint64_t tmp;
	asm volatile(
		"ldrd %Q[r], %R[r], %[p]\n"
		: [r]"=r"(tmp)
		: [p]"m"(*p64)
		: "memory"
	);
	return tmp;
}

#pragma redefine_extname atomic_load __atomic_load
extern "C" void
atomic_load(size_t len, const void *p, void *r, int m)
{
	const int s = irq_disable();
	memcpy(r, p, len);
	irq_restore(s);
}

/*
 * atomic_store
 */
extern "C" void
__atomic_store_8(volatile void *p, uint64_t v, int m)
{
	volatile uint64_t *p64 = static_cast<volatile uint64_t *>(p);
	int s;
	asm volatile(
		"mrs %[s], PRIMASK\n"
		"cpsid i\n"
		"strd %Q[v], %R[v], %[p]\n"
		"msr PRIMASK, %[s]\n"
		: [s]"=&lh"(s), [p]"=m"(*p64)
		: [v]"lh"(v)
		: "memory"
	);
}

#pragma redefine_extname atomic_store __atomic_store
extern "C" void
atomic_store(size_t len, void *p, const void *v, int m)
{
	const int s = irq_disable();
	memcpy(p, v, len);
	irq_restore(s);
}

/*
 * atomic_exchange
 */
extern "C" uint64_t
__atomic_exchange_8(volatile void *p, uint64_t v, int m)
{
	volatile uint64_t *p64 = static_cast<volatile uint64_t *>(p);
	int s;
	uint64_t ret;
	asm volatile(
		"mrs %[s], PRIMASK\n"
		"cpsid i\n"
		"ldrd %Q[ret], %R[ret], %[p]\n"
		"strd %Q[v], %R[v], %[p]\n"
		"msr PRIMASK, %[s]\n"
		: [s]"=&lh"(s), [p]"=m"(*p64), [ret]"=&lh"(ret)
		: [v]"lh"(v), "m"(*p64)
		: "memory"
	);
	return ret;
}

#pragma redefine_extname atomic_exchange __atomic_exchange
extern "C" void
atomic_exchange(size_t len, void *p, const void *v, void *r, int m)
{
	const int s = irq_disable();
	memcpy(r, p, len);
	memcpy(p, v, len);
	irq_restore(s);
}

/*
 * atomic_compare_exchange
 */
extern "C" bool
__atomic_compare_exchange_8(volatile void *p, void *e, uint64_t d,
    bool weak, int sm, int fm)
{
	volatile uint64_t *p64 = static_cast<volatile uint64_t *>(p);
	const uint64_t *e64 = static_cast<const uint64_t *>(e);
	bool ret;
	const int s = irq_disable();
	if ((ret = *p64 == *e64))
		*p64 = d;
	irq_restore(s);
	return ret;
}

#pragma redefine_extname atomic_compare_exchange __atomic_compare_exchange
extern "C" bool
atomic_compare_exchange(size_t len, void *p, const void *e, void *d,
    int sm, int fm)
{
	bool ret;
	const int s = irq_disable();
	if ((ret = !memcmp(p, e, len)))
		memcpy(p, d, len);
	irq_restore(s);
	return ret;
}

/*
 * atomic_add_fetch
 */
extern "C" uint64_t
__atomic_add_fetch_8(volatile void *p, uint64_t v, int m)
{
	volatile uint64_t *p64 = static_cast<volatile uint64_t *>(p);
	uint64_t ret;
	const int s = irq_disable();
	ret = *p64;
	ret += v;
	*p64 = ret;
	irq_restore(s);
	return ret;
}

/*
 * atomic_sub_fetch
 */
extern "C" uint64_t
__atomic_sub_fetch_8(volatile void *p, uint64_t v, int m)
{
	volatile uint64_t *p64 = static_cast<volatile uint64_t *>(p);
	uint64_t ret;
	const int s = irq_disable();
	ret = *p64;
	ret -= v;
	*p64 = ret;
	irq_restore(s);
	return ret;
}

/*
 * atomic_and_fetch
 */
extern "C" uint64_t
__atomic_and_fetch_8(volatile void *p, uint64_t v, int m)
{
	volatile uint64_t *p64 = static_cast<volatile uint64_t *>(p);
	uint64_t ret;
	const int s = irq_disable();
	ret = *p64;
	ret &= v;
	*p64 = ret;
	irq_restore(s);
	return ret;
}

/*
 * atomic_xor_fetch
 */
extern "C" uint64_t
__atomic_xor_fetch_8(volatile void *p, uint64_t v, int m)
{
	volatile uint64_t *p64 = static_cast<volatile uint64_t *>(p);
	uint64_t ret;
	const int s = irq_disable();
	ret = *p64;
	ret ^= v;
	*p64 = ret;
	irq_restore(s);
	return ret;
}

/*
 * atomic_or_fetch
 */
extern "C" uint64_t
__atomic_or_fetch_8(volatile void *p, uint64_t v, int m)
{
	volatile uint64_t *p64 = static_cast<volatile uint64_t *>(p);
	uint64_t ret;
	const int s = irq_disable();
	ret = *p64;
	ret |= v;
	*p64 = ret;
	irq_restore(s);
	return ret;
}

/*
 * atomic_nand_fetch
 */
extern "C" uint64_t
__atomic_nand_fetch_8(volatile void *p, uint64_t v, int m)
{
	volatile uint64_t *p64 = static_cast<volatile uint64_t *>(p);
	uint64_t ret;
	const int s = irq_disable();
	ret = *p64;
	ret = ~(ret & v);
	*p64 = ret;
	irq_restore(s);
	return ret;
}

/*
 * atomic_fetch_add
 */
extern "C" uint64_t
__atomic_fetch_add_8(volatile void *p, uint64_t v, int m)
{
	volatile uint64_t *p64 = static_cast<volatile uint64_t *>(p);
	uint64_t ret;
	const int s = irq_disable();
	ret = *p64;
	*p64 = ret + v;
	irq_restore(s);
	return ret;
}

/*
 * atomic_fetch_sub
 */
extern "C" uint64_t
__atomic_fetch_sub_8(volatile void *p, uint64_t v, int m)
{
	volatile uint64_t *p64 = static_cast<volatile uint64_t *>(p);
	uint64_t ret;
	const int s = irq_disable();
	ret = *p64;
	*p64 = ret - v;
	irq_restore(s);
	return ret;
}

/*
 * atomic_fetch_and
 */
extern "C" uint64_t
__atomic_fetch_and_8(volatile void *p, uint64_t v, int m)
{
	volatile uint64_t *p64 = static_cast<volatile uint64_t *>(p);
	uint64_t ret;
	const int s = irq_disable();
	ret = *p64;
	*p64 = ret & v;
	irq_restore(s);
	return ret;
}

/*
 * atomic_fetch_xor
 */
extern "C" uint64_t
__atomic_fetch_xor_8(volatile void *p, uint64_t v, int m)
{
	volatile uint64_t *p64 = static_cast<volatile uint64_t *>(p);
	uint64_t ret;
	const int s = irq_disable();
	ret = *p64;
	*p64 = ret ^ v;
	irq_restore(s);
	return ret;
}

/*
 * atomic_fetch_or
 */
extern "C" uint64_t
__atomic_fetch_or_8(volatile void *p, uint64_t v, int m)
{
	volatile uint64_t *p64 = static_cast<volatile uint64_t *>(p);
	uint64_t ret;
	const int s = irq_disable();
	ret = *p64;
	*p64 = ret | v;
	irq_restore(s);
	return ret;
}

/*
 * atomic_fetch_nand
 */
extern "C" uint64_t
__atomic_fetch_nand_8(volatile void *p, uint64_t v, int m)
{
	volatile uint64_t *p64 = static_cast<volatile uint64_t *>(p);
	uint64_t ret;
	const int s = irq_disable();
	ret = *p64;
	*p64 = ~(ret & v);
	irq_restore(s);
	return ret;
}
