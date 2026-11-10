/* SPDX-License-Identifier: MIT */
#include <stdint.h>
#include <stdlib.h>
#include <stddef.h>
#include "fluxsort.h"
void flux_u64(uint64_t *Q, size_t n) { fluxsort_prim(Q, n, 9); }
static long long *fcnt;
static int fcmp(const void *a, const void *b) { (*fcnt)++; uint64_t x = *(const uint64_t *)a, y = *(const uint64_t *)b; return (x > y) - (x < y); }
void flux_u64_cnt(uint64_t *Q, size_t n, long long *c) { fcnt = c; fluxsort(Q, n, 8, fcmp); }
