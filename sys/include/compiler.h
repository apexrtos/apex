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

#endif
