#ifndef compiler_h
#define compiler_h

/*
 * Inform the compiler that it must not reorder.
 */
static inline void
compiler_barrier()
{
	asm volatile("" ::: "memory");
}

/*
 * Ask the compiler to read a value.
 */
#define read_once(p) ({ \
	typeof(*p) tmp; \
	switch (sizeof(*p)) { \
	case 1 ... 8: \
		tmp = *(volatile typeof(*p) *)p; \
		break; \
	default: \
		compiler_barrier(); \
		memcpy(&tmp, p, sizeof(*p)); \
		compiler_barrier(); \
	} \
	tmp; \
})

/*
 * Ask the compiler to write a value.
 */
#define write_once(p, v) ({ \
	switch (sizeof(*p)) { \
	case 1 ... 8: \
		*(volatile typeof(*p) *)p = v; \
		break; \
	default: \
		compiler_barrier(); \
		memcpy(p, &v, sizeof(*p)); \
		compiler_barrier(); \
	} \
})

/*
 * ARRAY_SIZE
 */
#define ARRAY_SIZE(arr) \
    (sizeof(arr) / sizeof((arr)[0]) + sizeof(typeof(int[1 - 2 * \
    !!__builtin_types_compatible_p(typeof(arr), typeof(&arr[0]))])) * 0)

/*
 * Optimiser hints.
 */
#define likely(x) __builtin_expect((!!(x)),1)
#define unlikely(x) __builtin_expect((!!(x)),0)

/*
 * Create create a weak alias (from musl).
 */
#define weak_alias(old, new) \
	extern __typeof(old) new __attribute__((weak, alias(#old)))

#endif
