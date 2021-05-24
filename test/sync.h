#pragma once

/*
 * Wire up locks to standard library for test harness.
 */

#include <mutex>

namespace a {

using spinlock = std::mutex;

};

struct rwlock {};
