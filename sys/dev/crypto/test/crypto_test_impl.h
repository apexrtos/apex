#pragma once

#include <debug.h>
#include <initializer_list>
#include <span>

/*
 * Helpers for test functions
 */
namespace crypto_test {

inline
std::span<const std::byte>
as_bytes(const char *s)
{
	return {reinterpret_cast<const std::byte *>(s), strlen(s)};
}

inline
std::span<const std::byte>
as_bytes(std::span<const unsigned char> v)
{
	return {reinterpret_cast<const std::byte *>(data(v)), size(v)};
}

inline
bool
equal(std::span<const std::byte> l, std::span<const unsigned char> r)
{
	return size(l) == size(r) && !memcmp(data(l), data(r), size(l));
}

struct hash_test_vector {
	const char *input;
	int repeat;
	std::initializer_list<unsigned char> digest;
};

struct mac_test_vector {
	const char *input;
	std::initializer_list<unsigned char> key;
	std::initializer_list<unsigned char> mac;
};

struct kdf_test_vector {
	const char *password;
	const char *salt;
	unsigned iterations;
	std::initializer_list<unsigned char> key;
};

}

/*
 * Test Vectors
 */
extern const std::span<const crypto_test::hash_test_vector> sha256_tests;
extern const std::span<const crypto_test::mac_test_vector> hmac_sha256_tests;
extern const std::span<const crypto_test::kdf_test_vector> pbkdf2_sha256_tests;

/*
 * Hash Test
 */
template<typename Algorithm>
void
crypto_test_init(const char *name,
		 std::span<const crypto_test::hash_test_vector> tests)
{
	for (const auto &t : tests) {
		/* verify that test passes after initialisation */
		Algorithm a;
		for (auto i{0}; i != t.repeat; ++i)
			a.process(crypto_test::as_bytes(t.input));
		if (!crypto_test::equal(a.complete(), t.digest)) {
			dbg("*** crypto test: %s failed\n", name);
			return;
		}

		/* verify that test passes after a clear operation */
		a.clear();
		for (auto i{0}; i != t.repeat; ++i)
			a.process(crypto_test::as_bytes(t.input));
		if (!crypto_test::equal(a.complete(), t.digest)) {
			dbg("*** crypto test: %s failed\n", name);
			return;
		}
	}
	dbg("crypto test: %s passed\n", name);
}

/*
 * MAC Test
 */
template<typename Algorithm>
void
crypto_test_init(const char *name,
		 std::span<const crypto_test::mac_test_vector> tests)
{
	for (const auto &t : tests) {
		/* verify that test passes after initialisation */
		Algorithm a(crypto_test::as_bytes(t.key));
		a.process(crypto_test::as_bytes(t.input));
		if (!crypto_test::equal(a.complete(), t.mac)) {
			dbg("*** crypto test: %s failed\n", name);
			return;
		}

		/* verify that test passes after a clear operation */
		a.clear();
		a.process(crypto_test::as_bytes(t.input));
		if (!crypto_test::equal(a.complete(), t.mac)) {
			dbg("*** crypto test: %s failed\n", name);
			return;
		}
	}
	dbg("crypto test: %s passed\n", name);
}

/*
 * KDF Test
 */
template<typename Function>
void
crypto_test_init(Function kdf,
		 const char *name,
		 std::span<const crypto_test::kdf_test_vector> tests)
{
	std::array<std::byte, 64> buf;
	for (const auto &t : tests) {
		assert(size(t.key) <= size(buf));
		std::span<std::byte> result{data(buf), size(t.key)};
		kdf(crypto_test::as_bytes(t.password),
		    crypto_test::as_bytes(t.salt),
		    t.iterations, result);
		if (!crypto_test::equal(result, t.key)) {
			dbg("*** crypto test: %s failed\n", name);
			return;
		}
	}

	dbg("crypto test: %s passed\n", name);
}

