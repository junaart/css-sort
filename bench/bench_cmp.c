/* SPDX-License-Identifier: MIT */
/* CSS против pdqsort, crumsort, fluxsort на 30 входах (inputs.h).
   ./bench_cmp cmp N       — число сравнений ключей на элемент (через компаратор у соперников)
   ./bench_cmp time N R    — время, нс на элемент: медиана R повторов, порядок алгоритмов случайный
   Таблица «прошлого массива» — 4096 квантилей массива того же вида с другим зерном. */
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
void css_sort_ex(uint64_t *, uint64_t, const uint64_t *, uint64_t, int);
void css_table_from_sorted(const uint64_t *, uint64_t, uint64_t *, uint64_t);
void cssc_sort(uint64_t *, uint64_t); void cssc_sort_tab(uint64_t *, uint64_t, const uint64_t *, uint64_t);
int64_t css_cmp_count(void); void css_cmp_reset(void);
void crum_u64(uint64_t *, size_t); void flux_u64(uint64_t *, size_t); void pdq_u64(uint64_t *, size_t);
void crum_u64_cnt(uint64_t *, size_t, long long *); void flux_u64_cnt(uint64_t *, size_t, long long *); void pdq_u64_cnt(uint64_t *, size_t, long long *);
static double now(void) { struct timespec t; clock_gettime(CLOCK_MONOTONIC_RAW, &t); return t.tv_sec + 1e-9 * t.tv_nsec; }
static int dcmp(const void *a, const void *b) { double x = *(const double *)a, y = *(const double *)b; return (x > y) - (x < y); }
static uint64_t TAB[4096];
#define NV 6
static const char *VN[NV] = {"css", "css+tab", "css+tab/h", "pdqsort", "crumsort", "fluxsort"};
static void run(int v, uint64_t *W, int64_t n) {
  switch (v) {
  case 0: css_sort(W, n); break;
  case 1: css_sort_tab(W, n, TAB, 4096); break;
  case 2: css_sort_ex(W, n, TAB, 4096, 1); break;
  case 3: pdq_u64(W, n); break;
  case 4: crum_u64(W, n); break;
  case 5: flux_u64(W, n); break;
  }
}
int main(int argc, char **argv) {
  css_pin_cpu();
  const char *mode = argc > 1 ? argv[1] : "time";
  int64_t n = argc > 2 ? atoll(argv[2]) : 1000000;
  int R = argc > 3 ? atoi(argv[3]) : 5;
  uint64_t *A = malloc(n * 8), *W = malloc(n * 8), *Ref = malloc(n * 8);
  int bad = 0;
  printf("%s,n=%lld\nвход,группа", mode, (long long)n); for (int v = 0; v < NV; v++) printf(",%s", VN[v]); printf("\n");
  for (int p = 0; p < IN_N; p++) {
    in_gen(p, Ref, n, 777); qsort(Ref, n, 8, in_cmp); css_table_from_sorted(Ref, n, TAB, 4096);
    in_gen(p, A, n, 12345); memcpy(Ref, A, n * 8); qsort(Ref, n, 8, in_cmp);
    double res[NV];
    if (!strcmp(mode, "cmp")) {
      for (int v = 0; v < NV; v++) {
        if (v == 2) { res[v] = res[1]; continue; }
        long long c = 0; memcpy(W, A, n * 8);
        if (v == 0) { css_cmp_reset(); cssc_sort(W, n); c = css_cmp_count(); }
        else if (v == 1) { css_cmp_reset(); cssc_sort_tab(W, n, TAB, 4096); c = css_cmp_count(); }
        else if (v == 3) pdq_u64_cnt(W, n, &c);
        else if (v == 4) crum_u64_cnt(W, n, &c);
        else flux_u64_cnt(W, n, &c);
        if (memcmp(W, Ref, n * 8)) { bad++; fprintf(stderr, "ошибка %s %s\n", IN_NAME[p], VN[v]); }
        res[v] = (double)c / n;
      }
    } else {
      double t[NV][64]; int ord[NV];
      for (int r = 0; r < R; r++) {
        for (int v = 0; v < NV; v++) ord[v] = v;
        for (int v = NV - 1; v > 0; v--) { int j = rand() % (v + 1); int x = ord[v]; ord[v] = ord[j]; ord[j] = x; }
        for (int k = 0; k < NV; k++) {
          int v = ord[k]; memcpy(W, A, n * 8);
          double t0 = now(); run(v, W, n); t[v][r] = (now() - t0) * 1e9 / n;
          if (r == 0 && memcmp(W, Ref, n * 8)) { bad++; fprintf(stderr, "ошибка %s %s\n", IN_NAME[p], VN[v]); }
        }
      }
      for (int v = 0; v < NV; v++) { qsort(t[v], R, sizeof(double), dcmp); res[v] = t[v][R / 2]; }
    }
    printf("%s,%d", IN_NAME[p], IN_GROUP[p]); for (int v = 0; v < NV; v++) printf(",%.3f", res[v]); printf("\n");
    fflush(stdout);
  }
  if (bad) fprintf(stderr, "ошибок: %d\n", bad);
  return bad;
}
