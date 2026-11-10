/* SPDX-License-Identifier: MIT */
// std::sort и последовательный IPS4o для bench_std.
#include <algorithm>
#include <cstdint>
#include <cstddef>
#include "ips4o/include/ips4o.hpp"
extern "C" {
void std_u64(uint64_t *a, size_t n) { std::sort(a, a + n); }
void ips4o_u64(uint64_t *a, size_t n) { ips4o::sort(a, a + n); }
}
