/* SPDX-License-Identifier: MIT */
/* CSS против ipnsort (slice::sort_unstable из Rust) и опорных pdqsort, crumsort, fluxsort в одном прогоне.
   ./bench_ipn time N R   — время, нс на элемент: медиана R повторов, порядок алгоритмов случайный, 30 входов (inputs.h)
   ./bench_ipn hash N R   — то же на равномерных 64-битных ключах (splitmix64)
   ./bench_ipn cmp N      — число сравнений ipnsort на элемент (компаратор со счётчиком) на 30 входах
   Таблица прошлого массива для css+tab — 4096 квантилей массива того же вида с другим зерном. */
#define _GNU_SOURCE
#include "pin_cpu.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <sched.h>
#include <time.h>
#include "inputs.h"
void css_sort(uint64_t *, uint64_t); void css_sort_tab(uint64_t *, uint64_t, const uint64_t *, uint64_t);
void css_table_from_sorted(const uint64_t *, uint64_t, uint64_t *, uint64_t);
void crum_u64(uint64_t *, size_t); void flux_u64(uint64_t *, size_t); void pdq_u64(uint64_t *, size_t);
void ipn_sort_u64(uint64_t *, size_t); void ipn_sort_u64_cnt(uint64_t *, size_t, int64_t *);
static double now(void) { struct timespec t; clock_gettime(CLOCK_MONOTONIC_RAW, &t); return t.tv_sec + 1e-9 * t.tv_nsec; }
static int dcmp(const void *a, const void *b) { double x = *(const double *)a, y = *(const double *)b; return (x > y) - (x < y); }
static int ucmp(const void *a, const void *b) { uint64_t x = *(const uint64_t *)a, y = *(const uint64_t *)b; return (x > y) - (x < y); }
static uint64_t TAB[4096];
#define NV 6
static const char *VN[NV] = {"css", "css+tab", "pdqsort", "crumsort", "fluxsort", "ipnsort"};
static void run(int v, uint64_t *W, int64_t n) {
  switch (v) {
  case 0: css_sort(W, n); break;   case 1: css_sort_tab(W, n, TAB, 4096); break;
  case 2: pdq_u64(W, n); break;    case 3: crum_u64(W, n); break;
  case 4: flux_u64(W, n); break;   case 5: ipn_sort_u64(W, n); break;
  }
}
static uint64_t sm(uint64_t *s) { uint64_t z = (*s += 0x9e3779b97f4a7c15ULL); z = (z ^ (z >> 30)) * 0xbf58476d1ce4e5b9ULL; z = (z ^ (z >> 27)) * 0x94d049bb133111ebULL; return z ^ (z >> 31); }
static int bad;
static void measure(const char *name, int g, const uint64_t *A, const uint64_t *Ref, uint64_t *W, int64_t n, int R) {
  double t[NV][64], res[NV]; int ord[NV];
  for (int r = 0; r < R; r++) {
    for (int v = 0; v < NV; v++) ord[v] = v;
    for (int v = NV - 1; v > 0; v--) { int j = rand() % (v + 1); int x = ord[v]; ord[v] = ord[j]; ord[j] = x; }
    for (int k = 0; k < NV; k++) {
      int v = ord[k]; memcpy(W, A, n * 8);
      double t0 = now(); run(v, W, n); t[v][r] = (now() - t0) * 1e9 / n;
      if (r == 0 && memcmp(W, Ref, n * 8)) { bad++; fprintf(stderr, "ошибка %s %s\n", name, VN[v]); }
    }
  }
  for (int v = 0; v < NV; v++) { qsort(t[v], R, sizeof(double), dcmp); res[v] = t[v][R / 2]; }
  printf("%s,%d", name, g); for (int v = 0; v < NV; v++) printf(",%.3f", res[v]); printf("\n"); fflush(stdout);
}
int main(int argc, char **argv) {
  css_pin_cpu();
  const char *mode = argc > 1 ? argv[1] : "time";
  int64_t n = argc > 2 ? atoll(argv[2]) : 1000000;
  int R = argc > 3 ? atoi(argv[3]) : 5; if (R > 64) R = 64;
  uint64_t *A = malloc(n * 8), *W = malloc(n * 8), *Ref = malloc(n * 8);
  if (!strcmp(mode, "cmp")) {
    printf("cmp,n=%lld\nвход,группа,ipnsort\n", (long long)n);
    for (int p = 0; p < IN_N; p++) {
      in_gen(p, A, n, 12345); memcpy(Ref, A, n * 8); qsort(Ref, n, 8, in_cmp);
      int64_t c = 0; ipn_sort_u64_cnt(A, n, &c);
      if (memcmp(A, Ref, n * 8)) { bad++; fprintf(stderr, "ошибка %s\n", IN_NAME[p]); }
      printf("%s,%d,%.3f\n", IN_NAME[p], IN_GROUP[p], (double)c / n); fflush(stdout);
    }
    return bad;
  }
  printf("%s,n=%lld\nвход,группа", mode, (long long)n); for (int v = 0; v < NV; v++) printf(",%s", VN[v]); printf("\n");
  if (!strcmp(mode, "hash")) {
    uint64_t s = 777; for (int64_t i = 0; i < n; i++) A[i] = sm(&s);
    qsort(A, n, 8, ucmp); css_table_from_sorted(A, n, TAB, 4096);
    s = 12345; for (int64_t i = 0; i < n; i++) A[i] = sm(&s);
    memcpy(Ref, A, n * 8); qsort(Ref, n, 8, ucmp);
    measure("hash", 0, A, Ref, W, n, R);
  } else for (int p = 0; p < IN_N; p++) {
    in_gen(p, Ref, n, 777); qsort(Ref, n, 8, in_cmp); css_table_from_sorted(Ref, n, TAB, 4096);
    in_gen(p, A, n, 12345); memcpy(Ref, A, n * 8); qsort(Ref, n, 8, in_cmp);
    measure(IN_NAME[p], IN_GROUP[p], A, Ref, W, n, R);
  }
  if (bad) fprintf(stderr, "ошибок: %d\n", bad);
  return bad;
}
