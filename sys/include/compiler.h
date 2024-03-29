#pragma once

#include <cstdint>
#include <cstring>
#include <type_traits>

/*
 * Inform the compiler that it must not reorder.
 */
static inline void
compiler_barrier()
{
	asm("" ::: "memory");
}

/*
 * Ask the compiler to read a value.
 */
template<typename T>
inline T
read_once(const T *p)
{
	static_assert(std::is_trivially_copyable_v<T>);

	T tmp;

	switch (sizeof(T)) {
	case 1:
		typedef uint8_t __attribute__((__may_alias__)) u8a;
		*(u8a *)&tmp = *(const volatile u8a *)p;
		break;
	case 2:
		typedef uint16_t __attribute__((__may_alias__)) u16a;
		*(u16a *)&tmp = *(const volatile u16a *)p;
		break;
	case 4:
		typedef uint32_t __attribute__((__may_alias__)) u32a;
		*(u32a *)&tmp = *(const volatile u32a *)p;
		break;
	case 8:
		typedef uint64_t __attribute__((__may_alias__)) u64a;
		*(u64a *)&tmp = *(const volatile u64a *)p;
		break;
	default:
		compiler_barrier();
		memcpy(&tmp, p, sizeof(T));
		compiler_barrier();
	}

	return tmp;
}

/*
 * Ask the compiler to write a value.
 */
template<typename T>
void
write_once(T *p, const T &v)
{
	static_assert(std::is_trivially_copyable_v<T>);

	switch (sizeof(T)) {
	case 1:
		typedef uint8_t __attribute__((__may_alias__)) u8a;
		*(volatile u8a *)p = *(u8a *)&v;
		break;
	case 2:
		typedef uint16_t __attribute__((__may_alias__)) u16a;
		*(volatile u16a *)p = *(u16a *)&v;
		break;
	case 4:
		typedef uint32_t __attribute__((__may_alias__)) u32a;
		*(volatile u32a *)p = *(u32a *)&v;
		break;
	case 8:
		typedef uint64_t __attribute__((__may_alias__)) u64a;
		*(volatile u64a *)p = *(u64a *)&v;
		break;
	default:
		compiler_barrier();
		memcpy(p, &v, sizeof(T));
		compiler_barrier();
	}
}

/*
 * ARRAY_SIZE
 */
template<class T, size_t N>
constexpr size_t ARRAY_SIZE(const T(&)[N])
{
	return N;
}

/*
 * Copy symbol attributes (GCC 9+)
 */
#if __has_attribute(copy)
#define ATTR_COPY(x) __attribute__((copy(x)))
#else
#define ATTR_COPY(x)
#endif

/*
 * Create create a weak alias (from musl).
 */
#define weak_alias(old, new) \
	extern "C" __typeof(old) new __attribute__((weak, alias(#old))) ATTR_COPY(old)
