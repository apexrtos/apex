#include "sha256.h"

#include <endian.h>

namespace crypto {

namespace {

inline uint32_t
ror(uint32_t n, int k)
{
	return (n >> k) | (n << (32 - k));
}

inline uint32_t
Ch(uint32_t x, uint32_t y, uint32_t z)
{
	return z ^ (x & (y ^ z));
}

inline uint32_t
Maj(uint32_t x, uint32_t y, uint32_t z)
{
	return (x & y) | (z & (x | y));
}

inline uint32_t
S0(uint32_t x)
{
	return ror(x, 2) ^ ror(x, 13) ^ ror(x, 22);
}

inline uint32_t
S1(uint32_t x)
{
	return ror(x, 6) ^ ror(x, 11) ^ ror(x, 25);
}

inline uint32_t
R0(uint32_t x)
{
	return ror(x, 7) ^ ror(x, 18) ^ (x >> 3);
}

inline uint32_t
R1(uint32_t x)
{
	return ror(x, 17) ^ ror(x, 19) ^ (x >> 10);
}

constexpr std::array<uint32_t, 64> K{
	0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1,
	0x923f82a4, 0xab1c5ed5, 0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3,
	0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174, 0xe49b69c1, 0xefbe4786,
	0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
	0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147,
	0x06ca6351, 0x14292967, 0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13,
	0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85, 0xa2bfe8a1, 0xa81a664b,
	0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
	0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a,
	0x5b9cca4f, 0x682e6ff3, 0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208,
	0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2
};

constexpr std::array<uint32_t, 8> init{
	0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a, 0x510e527f, 0x9b05688c,
	0x1f83d9ab, 0x5be0cd19
};

void
compress(std::span<uint32_t, 8> h, std::span<const std::byte, 64> in)
{
	std::array<uint32_t, 8> S;
	std::array<uint32_t, 64> W;

	memcpy(data(W), data(in), 64);
	for (auto i{0}; i < 16; ++i)
		W[i] = be32toh(W[i]);
	for (auto i{16}; i < 64; i++)
		W[i] = R1(W[i - 2]) + W[i - 7] + R0(W[i - 15]) + W[i - 16];

	memcpy(data(S), data(h), 32);
	for (auto i{0}; i < 64; i++) {
		uint32_t t1 = S[7] + S1(S[4]) + Ch(S[4], S[5], S[6]) + K[i] + W[i];
		uint32_t t2 = S0(S[0]) + Maj(S[0], S[1], S[2]);
		S[7] = S[6];
		S[6] = S[5];
		S[5] = S[4];
		S[4] = S[3] + t1;
		S[3] = S[2];
		S[2] = S[1];
		S[1] = S[0];
		S[0] = t1 + t2;
	}
	for (auto i{0}; i < 8; ++i)
		h[i] += S[i];
}

}

sha256::sha256()
{
	clear();
}

void
sha256::clear()
{
	len_ = 0;
	h_ = init;
}

void
sha256::process(std::span<const std::byte> in)
{
        const auto off{len_ % 64};
        len_ += size(in);
        if (off) {
		const auto rem{64 - off};
                if (size(in) < rem) {
                        memcpy(data(buf_) + off, data(in), size(in));
                        return;
                }
                memcpy(data(buf_) + off, data(in), rem);
                compress(h_, buf_);
		in = in.subspan(rem);
        }
	while (size(in) >= 64) {
		compress(h_, in);
		in = in.subspan(64);
	}
        memcpy(data(buf_), data(in), size(in));
}

std::span<const std::byte, sha256::digestlen>
sha256::complete()
{
        auto off{len_ % 64};

        buf_[off++] = static_cast<std::byte>(0x80);
        if (off > 56) {
                memset(data(buf_) + off, 0, 64 - off);
                compress(h_, buf_);
                off = 0;
        }
        memset(data(buf_) + off, 0, 56 - off);
	const uint64_t l{htobe64(len_ * 8)};
	memcpy(data(buf_) + 56, &l, 8);
        compress(h_, buf_);

        for (auto &v : h_)
		v = htobe32(v);

	return {reinterpret_cast<std::byte *>(data(h_)), size(h_)};
}

}
