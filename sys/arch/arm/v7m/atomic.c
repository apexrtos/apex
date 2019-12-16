#include <stddef.h>

#include <irq.h>
#include <string.h>

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
inline uint64_t
__atomic_load_8(const volatile uint64_t *p, int m)
{
	uint64_t tmp;
	int s;
	asm volatile(
		"mrs %[s], PRIMASK\n"
		"cpsid i\n"
		"ldrd %[r], %H[r], %[p]\n"
		"msr PRIMASK, %[s]\n"
		: [s]"=&lh"(s), [r]"=lh"(tmp)
		: [p]"m"(*p)
	);
	return tmp;
}

inline void
__atomic_load(size_t len, const void *p, void *r, int m)
{
	const int s = irq_disable();
	memcpy(r, p, len);
	irq_restore(s);
}

/*
 * atomic_store
 */
inline void
__atomic_store_8(volatile uint64_t *p, uint64_t v, int m)
{
	int s;
	asm volatile(
		"mrs %[s], PRIMASK\n"
		"cpsid i\n"
		"strd %[v], %H[v], %[p]\n"
		"msr PRIMASK, %[s]\n"
		: [s]"=&lh"(s), [p]"=m"(*p)
		: [v]"lh"(v)
	);
}

inline void
__atomic_store(size_t len, void *p, const void *v, int m)
{
	const int s = irq_disable();
	memcpy(p, v, len);
	irq_restore(s);
}

/*
 * atomic_exchange
 */
inline uint64_t
__atomic_exchange_8(volatile uint64_t *p, uint64_t v, int m)
{
	int s;
	uint64_t ret;
	asm volatile(
		"mrs %[s], PRIMASK\n"
		"cpsid i\n"
		"ldrd %[ret], %H[ret], %[p]\n"
		"strd %[v], %H[v], %[p]\n"
		"msr PRIMASK, %[s]\n"
		: [s]"=&lh"(s), [p]"=m"(*p), [ret]"=&lh"(ret)
		: [v]"lh"(v), "m"(*p)
	);
	return ret;
}

inline void
__atomic_exchange(size_t len, void *p, const void *v, void *r, int m)
{
	const int s = irq_disable();
	memcpy(r, p, len);
	memcpy(p, v, len);
	irq_restore(s);
}

/*
 * atomic_compare_exchange
 */
inline bool
__atomic_compare_exchange_8(volatile uint64_t *p, const uint64_t *e, uint64_t d,
    bool weak, int sm, int fm)
{
	bool ret;
	const int s = irq_disable();
	if ((ret = *p == *e))
		*p = d;
	irq_restore(s);
	return ret;
}

inline bool
__atomic_compare_exchange(size_t len, void *p, const void *e, void *d,
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
inline uint64_t
__atomic_add_fetch_8(volatile uint64_t *p, uint64_t v, int m)
{
	uint64_t ret;
	const int s = irq_disable();
	ret = *p += v;
	irq_restore(s);
	return ret;
}

/*
 * atomic_sub_fetch
 */
inline uint64_t
__atomic_sub_fetch_8(volatile uint64_t *p, uint64_t v, int m)
{
	uint64_t ret;
	const int s = irq_disable();
	ret = *p -= v;
	irq_restore(s);
	return ret;
}

/*
 * atomic_and_fetch
 */
inline uint64_t
__atomic_and_fetch_8(volatile uint64_t *p, uint64_t v, int m)
{
	uint64_t ret;
	const int s = irq_disable();
	ret = *p &= v;
	irq_restore(s);
	return ret;
}

/*
 * atomic_xor_fetch
 */
inline uint64_t
__atomic_xor_fetch_8(volatile uint64_t *p, uint64_t v, int m)
{
	uint64_t ret;
	const int s = irq_disable();
	ret = *p ^= v;
	irq_restore(s);
	return ret;
}

/*
 * atomic_or_fetch
 */
inline uint64_t
__atomic_or_fetch_8(volatile uint64_t *p, uint64_t v, int m)
{
	uint64_t ret;
	const int s = irq_disable();
	ret = *p |= v;
	irq_restore(s);
	return ret;
}

/*
 * atomic_nand_fetch
 */
inline uint64_t
__atomic_nand_fetch_8(volatile uint64_t *p, uint64_t v, int m)
{
	uint64_t ret;
	const int s = irq_disable();
	ret = *p = ~(*p & v);
	irq_restore(s);
	return ret;
}

/*
 * atomic_fetch_add
 */
inline uint64_t
__atomic_fetch_add_8(volatile uint64_t *p, uint64_t v, int m)
{
	uint64_t ret;
	const int s = irq_disable();
	ret = *p;
	*p += v;
	irq_restore(s);
	return ret;
}

/*
 * atomic_fetch_sub
 */
inline uint64_t
__atomic_fetch_sub_8(volatile uint64_t *p, uint64_t v, int m)
{
	uint64_t ret;
	const int s = irq_disable();
	ret = *p;
	*p -= v;
	irq_restore(s);
	return ret;
}

/*
 * atomic_fetch_and
 */
inline uint64_t
__atomic_fetch_and_8(volatile uint64_t *p, uint64_t v, int m)
{
	uint64_t ret;
	const int s = irq_disable();
	ret = *p;
	*p &= v;
	irq_restore(s);
	return ret;
}

/*
 * atomic_fetch_xor
 */
inline uint64_t
__atomic_fetch_xor_8(volatile uint64_t *p, uint64_t v, int m)
{
	uint64_t ret;
	const int s = irq_disable();
	ret = *p;
	*p ^= v;
	irq_restore(s);
	return ret;
}

/*
 * atomic_fetch_or
 */
inline uint64_t
__atomic_fetch_or_8(volatile uint64_t *p, uint64_t v, int m)
{
	uint64_t ret;
	const int s = irq_disable();
	ret = *p;
	*p |= v;
	irq_restore(s);
	return ret;
}

/*
 * atomic_fetch_nand
 */
inline uint64_t
__atomic_fetch_nand_8(volatile uint64_t *p, uint64_t v, int m)
{
	uint64_t ret;
	const int s = irq_disable();
	ret = *p;
	*p = ~(ret & v);
	irq_restore(s);
	return ret;
}
