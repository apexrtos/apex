#pragma once

#include <span>

/*
 * SHA-256 Hash
 */

namespace crypto {

class sha256 final {
public:
	static constexpr auto blocklen = 64;
	static constexpr auto digestlen = 32;

	sha256();

	void clear();
	void process(std::span<const std::byte>);
	std::span<const std::byte, digestlen> complete();

private:
	uint64_t len_;
	std::array<uint32_t, 8> h_;
	std::array<std::byte, 64> buf_;
};

}
