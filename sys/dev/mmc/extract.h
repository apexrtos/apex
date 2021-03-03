#pragma once

#include <cassert>

namespace mmc {

/*
 * bits - extract bits from buffer according to SD/MMC bit numbering
 */
inline unsigned long __attribute__((always_inline))
bits(const void *buf, size_t bufsiz, size_t begin, size_t end)
{
	const size_t size = end - begin + 1;
	const uint32_t mask = (size < 32 ? 1ul << size : 0) - 1;
	const uint32_t *p = static_cast<const uint32_t *>(buf) +
	    (bufsiz - 1 - begin / 8) / 4;
	assert(size <= 32);
	uint32_t tmp, v;

	memcpy(&tmp, p, 4);
	v = be32toh(tmp) >> begin % 32;
	if (begin / 32 != end / 32) {
		memcpy(&tmp, p - 1, 4);
		v |= be32toh(tmp) << (32 - begin % 32);
	}
	return v & mask;
}

template<size_t N>
inline unsigned long
bits(const std::array<std::byte, N> &buf, size_t begin, size_t end)
{
	return bits(buf.data(), buf.size(), begin, end);
}

template<size_t N>
inline unsigned long
bits(const std::array<std::byte, N> &buf, size_t bit)
{
	return bits(buf, bit, bit);
}

inline unsigned long
bits(const uintmax_t r, size_t begin, size_t end)
{
	const size_t size = end - begin + 1;
	const uint32_t mask = (size < 32 ? 1ul << size : 0) - 1;

	return (r >> begin) & mask;
}

inline unsigned long
bits(const uintmax_t r, size_t bit)
{
	return (r >> bit) & 1;
}

/*
 * bytes - extract bytes from buffer according to MMC byte numbering
 */
inline unsigned long __attribute__((always_inline))
bytes(const void *buf, size_t bufsiz, size_t begin, size_t end)
{
	const size_t size = end - begin + 1;
	const uint8_t *p = static_cast<const uint8_t *>(buf) + begin;
	assert(size <= 4);

	if (size == 1)
		return *p;

	uint32_t tmp{0};
	memcpy(&tmp, p, size);
	return le32toh(tmp);
}

template<size_t N>
inline unsigned long
bytes(const std::array<std::byte, N> &buf, size_t begin, size_t end)
{
	return bytes(buf.data(), buf.size(), begin, end);
}

template<size_t N>
inline unsigned long
bytes(const std::array<std::byte, N> &buf, size_t byte)
{
	return bytes(buf.data(), buf.size(), byte, byte);
}

}
