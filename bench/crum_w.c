/* SPDX-License-Identifier: MIT */
#include <stdint.h>
#include <stdlib.h>
#include <stddef.h>
#include "crumsort.h"
void crum_u64(uint64_t *Q, size_t n) { crumsort_prim(Q, n, 9); }
static long long *ccnt;
static int ccmp(const void *a, const void *b) { (*ccnt)++; uint64_t x = *(const uint64_t *)a, y = *(const uint64_t *)b; return (x > y) - (x < y); }
void crum_u64_cnt(uint64_t *Q, size_t n, long long *c) { ccnt = c; crumsort(Q, n, 8, ccmp); }
