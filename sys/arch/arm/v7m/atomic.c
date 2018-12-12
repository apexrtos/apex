#include <stddef.h>

/*
 * TODO: sometimes gcc doesn't use our implementations, see
 *       https://gcc.gnu.org/bugzilla/show_bug.cgi?id=88456
 */

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
#define ATOMIC_LOAD(N, T) \
inline T \
__atomic_load_##N(const volatile T *p, int m) \
{ \
	return *p; \
}

ATOMIC_LOAD(1, uint8_t)
ATOMIC_LOAD(2, uint16_t)
ATOMIC_LOAD(4, uint32_t)

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
#define ATOMIC_STORE(N, T) \
inline void \
__atomic_store_##N(volatile T *p, T v, int m) \
{ \
	*p = v; \
}

ATOMIC_STORE(1, uint8_t)
ATOMIC_STORE(2, uint16_t)
ATOMIC_STORE(4, uint32_t)

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
#define ATOMIC_EXCHANGE(N, T, S) \
inline T \
__atomic_exchange_##N(volatile T *p, T v, int m) \
{ \
	int ex; \
	T ret; \
	asm volatile( \
		"1: ldrex"S" %[ret], %[p]\n" \
		"strex"S" %[ex], %[v], %[p]\n" \
		"cmp %[ex], 0\n" \
		"bne 1b\n" \
		: [ex]"=&lh"(ex), [p]"=Q"(*p), [ret]"=&lh"(ret) \
		: [v]"lh"(v), "m"(*p) \
		: "cc" \
	); \
	return ret; \
}

ATOMIC_EXCHANGE(1, uint8_t, "b")
ATOMIC_EXCHANGE(2, uint16_t, "h")
ATOMIC_EXCHANGE(4, uint32_t, "")

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
#define ATOMIC_COMPARE_EXCHANGE(N, T, S) \
inline bool \
__atomic_compare_exchange_##N(volatile T *p, const T *e, T d, bool w, int sm, int fm) \
{ \
	int ex; \
	T cur; \
	bool ret; \
	asm volatile( \
		"1: ldrex"S" %[cur], %[p]\n" \
		"cmp %[cur], %[e]\n" \
		"bne 2f\n" \
		"strex"S" %[ex], %[d], %[p]\n" \
		"cmp %[ex], 0\n" \
		"bne 1b\n" \
		"2: ite eq\n" \
		"moveq %[ret], 1\n" \
		"movne %[ret], 0\n" \
		: [ex]"=&lh"(ex), [p]"=Q"(*p), [cur]"=&lh"(cur), [ret]"=lh"(ret) \
		: [e]"lhI"(*e), [d]"lh"(d), "m"(*p) \
		: "cc" \
	); \
	return ret; \
}

ATOMIC_COMPARE_EXCHANGE(1, uint8_t, "b")
ATOMIC_COMPARE_EXCHANGE(2, uint16_t, "h")
ATOMIC_COMPARE_EXCHANGE(4, uint32_t, "")

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
 * ATOMIC_OP_FETCH
 */
#define ATOMIC_OP_FETCH(OP, INS, N, T, S) \
inline T \
__atomic_##OP##_fetch_##N(volatile T *p, T v, int m) \
{ \
	int ex; \
	T ret; \
	asm volatile( \
		"1: ldrex"S" %[ret], %[p]\n" \
		INS \
		"strex"S" %[ex], %[ret], %[p]\n" \
		"cmp %[ex], 0\n" \
		"bne 1b\n" \
		: [ex]"=&lh"(ex), [p]"=Q"(*p), [ret]"=&lh"(ret) \
		: [v]"lhI"(v), "m"(*p) \
		: "cc" \
	); \
	return ret; \
}

/*
 * atomic_add_fetch
 */
ATOMIC_OP_FETCH(add, "add %[ret], %[v]\n", 1, uint8_t, "b")
ATOMIC_OP_FETCH(add, "add %[ret], %[v]\n", 2, uint16_t, "h")
ATOMIC_OP_FETCH(add, "add %[ret], %[v]\n", 4, uint32_t, "")

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
ATOMIC_OP_FETCH(sub, "sub %[ret], %[v]\n", 1, uint8_t, "b")
ATOMIC_OP_FETCH(sub, "sub %[ret], %[v]\n", 2, uint16_t, "h")
ATOMIC_OP_FETCH(sub, "sub %[ret], %[v]\n", 4, uint32_t, "")

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
ATOMIC_OP_FETCH(and, "and %[ret], %[v]\n", 1, uint8_t, "b")
ATOMIC_OP_FETCH(and, "and %[ret], %[v]\n", 2, uint16_t, "h")
ATOMIC_OP_FETCH(and, "and %[ret], %[v]\n", 4, uint32_t, "")

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
ATOMIC_OP_FETCH(xor, "eor %[ret], %[v]\n", 1, uint8_t, "b")
ATOMIC_OP_FETCH(xor, "eor %[ret], %[v]\n", 2, uint16_t, "h")
ATOMIC_OP_FETCH(xor, "eor %[ret], %[v]\n", 4, uint32_t, "")

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
ATOMIC_OP_FETCH(or, "orr %[ret], %[v]\n", 1, uint8_t, "b")
ATOMIC_OP_FETCH(or, "orr %[ret], %[v]\n", 2, uint16_t, "h")
ATOMIC_OP_FETCH(or, "orr %[ret], %[v]\n", 4, uint32_t, "")

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
ATOMIC_OP_FETCH(nand, "and %[ret], %[v]\nmvn %[ret], %[ret]\n", 1, uint8_t, "b")
ATOMIC_OP_FETCH(nand, "and %[ret], %[v]\nmvn %[ret], %[ret]\n", 2, uint16_t, "h")
ATOMIC_OP_FETCH(nand, "and %[ret], %[v]\nmvn %[ret], %[ret]\n", 4, uint32_t, "")

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
 * ATOMIC_FETCH_OP
 */
#define ATOMIC_FETCH_OP(OP, INS, N, T, S) \
inline T \
__atomic_fetch_##OP##_##N(volatile T *p, T v, int m) \
{ \
	int ex; \
	T ret, tmp; \
	asm volatile( \
		"1: ldrex"S" %[ret], %[p]\n" \
		INS \
		"strex"S" %[ex], %[tmp], %[p]\n" \
		"cmp %[ex], 0\n" \
		"bne 1b\n" \
		: [ex]"=&lh"(ex), [p]"=Q"(*p), [ret]"=&lh"(ret), [tmp]"=&lh"(tmp) \
		: [v]"lhI"(v), "m"(*p) \
		: "cc" \
	); \
	return ret; \
}

/*
 * atomic_fetch_add
 */
ATOMIC_FETCH_OP(add, "add %[tmp], %[ret], %[v]\n", 1, uint8_t, "b")
ATOMIC_FETCH_OP(add, "add %[tmp], %[ret], %[v]\n", 2, uint16_t, "h")
ATOMIC_FETCH_OP(add, "add %[tmp], %[ret], %[v]\n", 4, uint32_t, "")

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
ATOMIC_FETCH_OP(sub, "sub %[tmp], %[ret], %[v]\n", 1, uint8_t, "b")
ATOMIC_FETCH_OP(sub, "sub %[tmp], %[ret], %[v]\n", 2, uint16_t, "h")
ATOMIC_FETCH_OP(sub, "sub %[tmp], %[ret], %[v]\n", 4, uint32_t, "")

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
ATOMIC_FETCH_OP(and, "and %[tmp], %[ret], %[v]\n", 1, uint8_t, "b")
ATOMIC_FETCH_OP(and, "and %[tmp], %[ret], %[v]\n", 2, uint16_t, "h")
ATOMIC_FETCH_OP(and, "and %[tmp], %[ret], %[v]\n", 4, uint32_t, "")

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
ATOMIC_FETCH_OP(xor, "eor %[tmp], %[ret], %[v]\n", 1, uint8_t, "b")
ATOMIC_FETCH_OP(xor, "eor %[tmp], %[ret], %[v]\n", 2, uint16_t, "h")
ATOMIC_FETCH_OP(xor, "eor %[tmp], %[ret], %[v]\n", 4, uint32_t, "")

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
ATOMIC_FETCH_OP(or, "orr %[tmp], %[ret], %[v]\n", 1, uint8_t, "b")
ATOMIC_FETCH_OP(or, "orr %[tmp], %[ret], %[v]\n", 2, uint16_t, "h")
ATOMIC_FETCH_OP(or, "orr %[tmp], %[ret], %[v]\n", 4, uint32_t, "")

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
ATOMIC_FETCH_OP(nand, "and %[tmp], %[ret], %[v]\nmvn %[tmp], %[tmp]\n", 1, uint8_t, "b")
ATOMIC_FETCH_OP(nand, "and %[tmp], %[ret], %[v]\nmvn %[tmp], %[tmp]\n", 2, uint16_t, "h")
ATOMIC_FETCH_OP(nand, "and %[tmp], %[ret], %[v]\nmvn %[tmp], %[tmp]\n", 4, uint32_t, "")

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

/*
 * atomic_test_and_set
 */
inline uint8_t
__atomic_test_and_set(uint8_t *p, int m)
{
	return __atomic_exchange_1(p, 1, m);
}

/*
 * atomic_clear
 */
inline void
__atomic_clear(uint8_t *p, int m)
{
	return __atomic_store_1(p, 0, 0);
}

