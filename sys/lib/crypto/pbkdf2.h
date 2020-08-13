#pragma once

#include "hmac.h"
#include <endian.h>

/*
 * PBKDF2 (PKCS #5 Algorithm #2)
 */

namespace crypto {

template<typename Hash>
void
pbkdf2(std::span<const std::byte> password,
       std::span<const std::byte> salt,
       int iterations,
       std::span<std::byte> result)
{
	using HMAC = hmac<Hash>;
	constexpr auto len{HMAC::digestlen};

	HMAC h{password};
	uint32_t block{0};
	while (!empty(result)) {
		h.clear();
		h.process(salt);
		const uint32_t b{htobe32(++block)};
		h.process({reinterpret_cast<const std::byte *>(&b), 4});

		std::array<std::byte, len> b0, b1;
		memcpy(data(b0), data(h.complete()), len);
		b1 = b0;
		for (auto i{1}; i < iterations; ++i) {
			h.clear();
			h.process(b0);
			memcpy(data(b0), data(h.complete()), len);
			for (auto j{0}; j != len; ++j)
				b1[j] ^= b0[j];
		}

		const auto sz{std::min(ssize(result), len)};
		memcpy(data(result), data(b1), sz);
		result = result.subspan(sz);
	}
}

}
