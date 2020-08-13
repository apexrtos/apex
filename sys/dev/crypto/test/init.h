#pragma once

#include "crypto_test_impl.h"
#include <lib/crypto/hmac.h>
#include <lib/crypto/pbkdf2.h>
#include <lib/crypto/sha256.h>

/*
 * Cryptography Self Test
 *
 * For example:
 *  driver sys/dev/crypto/test<crypto::sha256>("software sha256", sha256_tests)
 *  driver sys/dev/crypto/test<crypto::hmac<crypto::sha256>>("software hmac(sha256)", hmac_sha256_tests)
 *  driver sys/dev/crypto/test(crypto::pbkdf2<crypto::sha256>, "software pbkdf2(sha256)", pbkdf2_sha256_tests)
 */

template<typename Class, typename Test>
void crypto_test_init(const char *name, std::span<const Test>);

template<typename Function, typename Test>
void crypto_test_init(Function, const char *name, std::span<const Test>);
