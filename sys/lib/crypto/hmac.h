#pragma once

#include <span>

/*
 * HMAC Algorithm
 */

namespace crypto {

template<typename Hash>
class hmac {
public:
	static constexpr auto blocklen = Hash::blocklen;
	static constexpr auto digestlen = Hash::digestlen;

	hmac(std::span<const std::byte> key);

	void clear();
	void process(std::span<const std::byte>);
	std::span<const std::byte, digestlen> complete();

private:
	void setup();

	Hash h_;
	std::array<std::byte, blocklen> key_;
};

/*
 * Implementation
 */
template<typename Hash>
hmac<Hash>::hmac(std::span<const std::byte> key)
{
	if (size(key) > blocklen) {
		h_.process(key);
		memcpy(data(key_), data(h_.complete()), digestlen);
		memset(data(key_) + digestlen, 0, blocklen - digestlen);
		h_.clear();
	} else {
		memcpy(data(key_), data(key), size(key));
		memset(data(key_) + size(key), 0, blocklen - size(key));
	}

	setup();
}

template<typename Hash>
void
hmac<Hash>::clear()
{
	h_.clear();
	setup();
}

template<typename Hash>
void
hmac<Hash>::setup()
{
	std::array<std::byte, blocklen> tmp;
	for (auto i{0}; i != blocklen; ++i)
		tmp[i] = key_[i] ^ static_cast<std::byte>(0x36);
	h_.process(tmp);
}

template<typename Hash>
void
hmac<Hash>::process(std::span<const std::byte> in)
{
	h_.process(in);
}

template<typename Hash>
std::span<const std::byte, hmac<Hash>::digestlen>
hmac<Hash>::complete()
{
	std::array<std::byte, digestlen> inner;
	memcpy(data(inner), data(h_.complete()), size(inner));

	std::array<std::byte, blocklen> tmp;
	for (auto i{0}; i != blocklen; ++i)
		tmp[i] = key_[i] ^ static_cast<std::byte>(0x5c);

	h_.clear();
	h_.process(tmp);
	h_.process(inner);
	return h_.complete();
}

}
