/* SPDX-License-Identifier: MIT */
/* Таблица чужого распределения: css без таблицы / со своей / с чужой (вход (p+11) mod 30).
   ./bench_mism N R — сравнения на элемент и время (медиана R) */
#define _GNU_SOURCE
#include "pin_cpu.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <sched.h>
#include <time.h>
#include "inputs.h"
void css_sort_tab(uint64_t *, uint64_t, const uint64_t *, uint64_t);
void css_table_from_sorted(const uint64_t *, uint64_t, uint64_t *, uint64_t);
void cssc_sort_tab(uint64_t *, uint64_t, const uint64_t *, uint64_t);
int64_t css_cmp_count(void); void css_cmp_reset(void);
static double now(void) { struct timespec t; clock_gettime(CLOCK_MONOTONIC_RAW, &t); return t.tv_sec + 1e-9 * t.tv_nsec; }
static int dcmp(const void *a, const void *b) { double x = *(const double *)a, y = *(const double *)b; return (x > y) - (x < y); }
static uint64_t OWN[4096], FOR[4096];
int main(int argc, char **argv) {
  css_pin_cpu();
  int64_t n = argc > 1 ? atoll(argv[1]) : 1000000; int R = argc > 2 ? atoi(argv[2]) : 7;
  uint64_t *A = malloc(n * 8), *W = malloc(n * 8);
  printf("вход,группа,cmp_none,cmp_own,cmp_foreign,t_none,t_own,t_foreign\n");
  for (int p = 0; p < IN_N; p++) {
    in_gen(p, W, n, 777); qsort(W, n, 8, in_cmp); css_table_from_sorted(W, n, OWN, 4096);
    in_gen((p + 11) % IN_N, W, n, 777); qsort(W, n, 8, in_cmp); css_table_from_sorted(W, n, FOR, 4096);
    in_gen(p, A, n, 12345);
    const uint64_t *tb[3] = {NULL, OWN, FOR}; double c[3], t[3][64];
    for (int v = 0; v < 3; v++) { memcpy(W, A, n * 8); css_cmp_reset(); cssc_sort_tab(W, n, tb[v], 4096); c[v] = (double)css_cmp_count() / n; }
    for (int r = 0; r < R; r++) for (int k = 0; k < 3; k++) { int v = (k + r) % 3; memcpy(W, A, n * 8); double t0 = now(); css_sort_tab(W, n, tb[v], 4096); t[v][r] = (now() - t0) * 1e9 / n; }
    for (int v = 0; v < 3; v++) qsort(t[v], R, sizeof(double), dcmp);
    printf("%s,%d,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f\n", IN_NAME[p], IN_GROUP[p], c[0], c[1], c[2], t[0][R/2], t[1][R/2], t[2][R/2]); fflush(stdout);
  }
}
