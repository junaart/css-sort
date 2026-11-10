/* SPDX-License-Identifier: MIT */
#include "killers.h"

#include <stdlib.h>

static uint64_t R = 42;

void killer_set_seed(uint64_t seed) { R = seed; }

static uint64_t krnd(void) {
  R = R * 6364136223846793005ULL + 1442695040888963407ULL;
  return R;
}

static int cmpu64(const void *a, const void *b) {
  uint64_t x = *(const uint64_t *)a, y = *(const uint64_t *)b;
  return (x > y) - (x < y);
}

void killer_pipeorgan(uint64_t *A, int n) {
  uint64_t mid = (uint64_t)n / 2;
  for (int i = 0; i < mid; i++) A[i] = (uint64_t)i + 1;
  for (int i = mid; i < n; i++) A[i] = (uint64_t)n - i;
}

void killer_m3killer(uint64_t *A, int n) {
  uint64_t k = (uint64_t)n / 2;
  int i;
  for (i = 0; i < n; i++)
    A[i] = (i % 2 == 0) ? (uint64_t)(i / 2) + 1 : k + (uint64_t)(i / 2) + 1;
  for (i = 1; i < k; i += 2)
    if (i + 1 < n) { uint64_t tmp = A[i]; A[i] = A[i + 1]; A[i + 1] = tmp; }
}

void killer_pipeorganmx(uint64_t *A, int n, uint64_t max_val) {
  uint64_t mid = (uint64_t)n / 2;
  double step = (double)max_val / (double)mid;
  for (int i = 0; i < mid; i++) {
    A[i] = (uint64_t)((double)(i + 1) * step);
    if (A[i] > max_val) A[i] = max_val;
  }
  for (int i = mid; i < n; i++) {
    A[i] = (uint64_t)((double)(n - i) * step);
    if (A[i] > max_val) A[i] = max_val;
  }
}

void killer_unevenseq(uint64_t *A, int n, uint64_t max_val) {
  uint64_t mid_val = max_val / 2;
  uint64_t delta = max_val / 10000;
  if (delta == 0) delta = 1;
  for (int i = 0; i < n; i++) {
    if (krnd() % 100 < 90) {
      A[i] = mid_val - delta + (krnd() % (2 * delta + 1));
    } else {
      if (krnd() % 2 == 0) {
        A[i] = krnd() % (mid_val - delta);
      } else {
        uint64_t upper = max_val - (mid_val + delta);
        A[i] = (mid_val + delta + 1) + (upper > 0 ? (krnd() % upper) : 0);
      }
    }
  }
}

void killer_unevensorted(uint64_t *A, int n, uint64_t max_val) {
  killer_unevenseq(A, n, max_val);
  qsort(A, n, sizeof(uint64_t), cmpu64);
}

void killer_0to255(uint64_t *A, int n) {
  for (int i = 0; i < n; i++) A[i] = krnd() % 256;
}

void killer_rep(uint64_t *A, int n, uint64_t k) {
  for (int i = 0; i < n; i++) A[i] = krnd() % k;
}

void killer_rep9010(uint64_t *A, int n, uint64_t max_val) {
  uint64_t pop = max_val / 2;
  for (int i = 0; i < n; i++)
    A[i] = (krnd() % 100 < 90) ? pop : (krnd() % max_val);
}

void killer_binary(uint64_t *A, int n, uint64_t vlo, uint64_t vhi) {
  for (int i = 0; i < n; i++) A[i] = (krnd() % 2 == 0) ? vlo : vhi;
}

void killer_ternary(uint64_t *A, int n, uint64_t v0, uint64_t v1, uint64_t v2) {
  for (int i = 0; i < n; i++) {
    uint64_t r = krnd() % 3;
    A[i] = (r == 0) ? v0 : (r == 1) ? v1 : v2;
  }
}

void killer_quintary(uint64_t *A, int n, const uint64_t *values) {
  for (int i = 0; i < n; i++) A[i] = values[krnd() % 5];
}

void killer_mag(uint64_t *A, int n, int shift) {
  uint64_t v = 1ULL << shift;
  for (int i = 0; i < n; i++) A[i] = v;
}
