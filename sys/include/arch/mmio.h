#pragma once

/*
 * arch/mmio.h - architecture specific memory mapped i/o
 */

#include <cstdint>
#include <type_traits>

uint8_t mmio_read8(const void *);
uint16_t mmio_read16(const void *);
uint32_t mmio_read32(const void *);
#if UINTPTR_MAX == 0xffffffffffffffff
uint64_t mmio_read64(const void *);
#endif
void mmio_write8(void *, uint8_t);
void mmio_write16(void *, uint16_t);
void mmio_write32(void *, uint32_t);
#if UINTPTR_MAX == 0xffffffffffffffff
void mmio_write64(void *, uint64_t);
#endif

template<typename T>
inline T
readN(const T *p)
{
	static_assert(sizeof(T) <= sizeof(uintptr_t));
	static_assert(std::is_trivially_copyable_v<T>);

	T tmp;

	switch (sizeof(T)) {
	case 1:
		typedef uint8_t __attribute__((__may_alias__)) u8a;
		*(u8a *)&tmp = mmio_read8(p);
		break;
	case 2:
		typedef uint16_t __attribute__((__may_alias__)) u16a;
		*(u16a *)&tmp = mmio_read16(p);
		break;
	case 4:
		typedef uint32_t __attribute__((__may_alias__)) u32a;
		*(u32a *)&tmp = mmio_read32(p);
		break;
#if UINTPTR_MAX == 0xffffffffffffffff
	case 8:
		typedef uint64_t __attribute__((__may_alias__)) u64a;
		*(u64a *)&tmp = mmio_read64(p);
		break;
#endif
	}

	return tmp;
}

template<typename T>
inline T
read8(const T *p)
{
	static_assert(sizeof(T) == 1);
	return readN(p);
}

template<typename T>
inline T
read16(const T *p)
{
	static_assert(sizeof(T) == 2);
	return readN(p);
}

template<typename T>
inline T
read32(const T *p)
{
	static_assert(sizeof(T) == 4);
	return readN(p);
}

template<typename T>
inline T
read64(const T *p)
{
	static_assert(sizeof(T) == 8);
	return readN(p);
}

template<typename T>
inline void
writeN(void *p, const T &v)
{
	static_assert(sizeof(T) <= sizeof(uintptr_t));
	static_assert(std::is_trivially_copyable_v<T>);

	switch (sizeof(T)) {
	case 1:
		typedef uint8_t __attribute__((__may_alias__)) u8a;
		mmio_write8(p, *(const u8a *)&v);
		break;
	case 2:
		typedef uint16_t __attribute__((__may_alias__)) u16a;
		mmio_write16(p, *(const u16a *)&v);
		break;
	case 4:
		typedef uint32_t __attribute__((__may_alias__)) u32a;
		mmio_write32(p, *(const u32a *)&v);
		break;
#if UINTPTR_MAX == 0xffffffffffffffff
	case 8:
		typedef uint64_t __attribute__((__may_alias__)) u64a;
		mmio_write64(p, *(const u64a *)&v);
		break;
#endif
	}
}

template<typename P, typename T>
inline void
write8(P *p, const T &v)
{
	static_assert(sizeof(P) == 1);
	static_assert(sizeof(T) == 1);
	writeN(p, v);
}

template<typename P, typename T>
inline void
write16(P *p, const T &v)
{
	static_assert(sizeof(P) == 2);
	static_assert(sizeof(T) == 2);
	writeN(p, v);
}

template<typename P, typename T>
inline void
write32(P *p, const T &v)
{
	static_assert(sizeof(P) == 4);
	static_assert(sizeof(T) == 4);
	writeN(p, v);
}

template<typename P, typename T>
inline void
write64(P *p, const T &v)
{
	static_assert(sizeof(P) == 8);
	static_assert(sizeof(T) == 8);
	writeN(p, v);
}
