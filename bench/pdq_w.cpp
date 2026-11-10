/* SPDX-License-Identifier: MIT */
#include <cstdint>
#include <cstddef>
#include "pdqsort.h"
extern "C" void pdq_u64(uint64_t *Q, size_t n) { pdqsort(Q, Q + n); }
extern "C" void pdq_u64_cnt(uint64_t *Q, size_t n, long long *c) { pdqsort(Q, Q + n, [c](uint64_t a, uint64_t b) { (*c)++; return a < b; }); }
