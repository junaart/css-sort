/* SPDX-License-Identifier: MIT */
/* Закрепление замера за одним логическим процессором.
   Номер берётся из переменной окружения CSS_CPU (по умолчанию 1): CSS_CPU=6 ./bench_ipn time 1000000 11.
   На гибридных процессорах Intel выбирайте поток производительного ядра (cat /sys/devices/cpu_core/cpus),
   лучше с наибольшей частотой (lscpu -e=CPU,CORE,MAXMHZ), и следите, чтобы второй поток того же ядра
   был свободен. В stderr печатаются номер процессора и его частота после короткого разогрева. */
#ifndef CSS_PIN_CPU_H
#define CSS_PIN_CPU_H
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <sched.h>
#include <stdio.h>
#include <stdlib.h>
static void css_pin_cpu(void) {
  const char *e = getenv("CSS_CPU");
  int c = e && *e ? atoi(e) : 1;
  cpu_set_t cs; CPU_ZERO(&cs); CPU_SET(c, &cs);
  if (sched_setaffinity(0, sizeof cs, &cs)) { fprintf(stderr, "не удалось закрепить за процессором %d\n", c); return; }
  volatile unsigned long x = 0; for (unsigned long i = 0; i < 300000000UL; i++) x += i;   /* ~0.1 с разогрева */
  char p[96]; snprintf(p, sizeof p, "/sys/devices/system/cpu/cpu%d/cpufreq/scaling_cur_freq", c);
  FILE *f = fopen(p, "r"); long khz = 0;
  if (f) { if (fscanf(f, "%ld", &khz) != 1) khz = 0; fclose(f); }
  if (khz) fprintf(stderr, "процессор %d, частота %ld МГц\n", c, khz / 1000);
  else fprintf(stderr, "процессор %d\n", c);
}
#endif
