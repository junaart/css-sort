/* SPDX-License-Identifier: MIT */
/* Входы для сравнения сортировок: киллеры (killers.c) и распространённые распределения. */
#include <math.h>
#include "killers.h"
static uint64_t in_s;
static uint64_t in_rnd(void) { uint64_t z = (in_s += 0x9e3779b97f4a7c15ULL); z = (z ^ (z >> 30)) * 0xbf58476d1ce4e5b9ULL; z = (z ^ (z >> 27)) * 0x94d049bb133111ebULL; return z ^ (z >> 31); }
static double in_u(void) { return (in_rnd() >> 11) * (1.0 / 9007199254740992.0); }
static double in_nrm(void) { double a = in_u(), b = in_u(); if (a < 1e-300) a = 1e-300; return sqrt(-2 * log(a)) * cos(6.283185307179586 * b); }
static int in_cmp(const void *a, const void *b) { uint64_t x = *(const uint64_t *)a, y = *(const uint64_t *)b; return (x > y) - (x < y); }
static void in_shuffle(uint64_t *A, int64_t n) { for (int64_t i = n - 1; i > 0; i--) { int64_t j = (int64_t)(in_rnd() % (uint64_t)(i + 1)); uint64_t t = A[i]; A[i] = A[j]; A[j] = t; } }
static const char *IN_NAME[] = {
  "random", "random_n", "sorted", "reversed", "nearly_sorted", "pipeorgan", "m3killer", "pipeorganmx",
  "sawtooth", "push_front", "push_middle", "unevenseq", "unevensorted", "0to255", "rep4", "rep64",
  "rep9010", "binary", "ternary", "quintary", "outlier", "powers2", "logbits", "pareto",
  "lognormal", "normal", "clusters8", "sorted95", "dist20", "zipf"};
#define IN_N 30
/* группа: 0 — случайные различные, 1 — упорядоченность и киллеры порядка, 2 — повторы, 3 — густота значений */
static const int IN_GROUP[IN_N] = {0,0,1,1,1,1,1,1, 1,1,1,3,1,2,2,2, 2,2,2,2,3,3,3,3, 3,3,3,1,2,2};
static void in_gen(int p, uint64_t *A, int64_t n, uint64_t seed) {
  in_s = seed; killer_set_seed(seed | 1);
  int ni = (int)n;
  switch (p) {
    case 0: for (int64_t i = 0; i < n; i++) A[i] = in_rnd(); break;
    case 1: for (int64_t i = 0; i < n; i++) A[i] = in_rnd() % (uint64_t)n; break;
    case 2: for (int64_t i = 0; i < n; i++) A[i] = (uint64_t)i * 7 + (seed & 3); break;
    case 3: for (int64_t i = 0; i < n; i++) A[i] = (uint64_t)(n - i) * 7 + (seed & 3); break;
    case 4: for (int64_t i = 0; i < n; i++) A[i] = (uint64_t)i; for (int64_t k = 0; k < n / 100; k++) { int64_t a = (int64_t)(in_rnd() % (uint64_t)n), b = (int64_t)(in_rnd() % (uint64_t)n); uint64_t t = A[a]; A[a] = A[b]; A[b] = t; } break;
    case 5: killer_pipeorgan(A, ni); break;
    case 6: killer_m3killer(A, ni); break;
    case 7: killer_pipeorganmx(A, ni, 1000000000000ULL); break;
    case 8: { int64_t L = (int64_t)log2((double)n) + 1, run = n / L; for (int64_t i = 0; i < n; i++) A[i] = in_rnd(); for (int64_t s = 0; s < n; s += run) qsort(A + s, (size_t)(s + run <= n ? run : n - s), 8, in_cmp); } break;
    case 9: for (int64_t i = 1; i < n; i++) A[i - 1] = (uint64_t)i; A[n - 1] = 0; break;
    case 10: { int64_t k = 0; for (int64_t i = 0; i < n; i++) if (i != n / 2) A[k++] = (uint64_t)i; A[n - 1] = (uint64_t)(n / 2); } break;
    case 11: killer_unevenseq(A, ni, 1000000000000ULL); break;
    case 12: killer_unevensorted(A, ni, 1000000000000ULL); break;
    case 13: killer_0to255(A, ni); break;
    case 14: killer_rep(A, ni, 4); break;
    case 15: killer_rep(A, ni, 64); break;
    case 16: killer_rep9010(A, ni, 1000000000ULL); break;
    case 17: killer_binary(A, ni, 0, 1000000); break;
    case 18: killer_ternary(A, ni, 10, 1000, 100000); break;
    case 19: { static const uint64_t v[5] = {10, 100, 500, 1000, 5000}; killer_quintary(A, ni, v); } break;
    case 20: for (int64_t i = 0; i < n; i++) A[i] = in_rnd() >> 32; A[in_rnd() % (uint64_t)n] = UINT64_MAX; break;
    case 21: for (int64_t i = 0; i < n; i++) A[i] = i % 65 == 64 ? 0 : (uint64_t)1 << (i % 65); in_shuffle(A, n); break;
    case 22: for (int64_t i = 0; i < n; i++) A[i] = in_rnd() >> (in_rnd() % 64); break;
    case 23: for (int64_t i = 0; i < n; i++) { double u = in_u(); A[i] = (uint64_t)(1048576.0 / pow(u < 1e-12 ? 1e-12 : u, 1.5)); } break;
    case 24: for (int64_t i = 0; i < n; i++) A[i] = (uint64_t)exp(20 + 2 * in_nrm()); break;
    case 25: for (int64_t i = 0; i < n; i++) { double x = 549755813888.0 + 17179869184.0 * in_nrm(); A[i] = x < 0 ? 0 : (uint64_t)x; } break;
    case 26: { double c[8]; for (int j = 0; j < 8; j++) c[j] = in_u() * 1099511627776.0;
               for (int64_t i = 0; i < n; i++) { double x = c[in_rnd() % 8] + 16777216.0 * in_nrm(); A[i] = x < 0 ? 0 : (uint64_t)x; } } break;
    case 27: { int64_t s = n * 95 / 100; for (int64_t i = 0; i < n; i++) A[i] = in_rnd(); qsort(A, (size_t)s, 8, in_cmp); } break;
    case 28: for (int64_t i = 0; i < n; i++) A[i] = in_rnd() % 20; break;
    case 29: for (int64_t i = 0; i < n; i++) { double u = in_u(); A[i] = (uint64_t)floor(pow((double)n, u)); } break;   /* Ципф-подобное: плотность ~ 1/x */
  }
}
