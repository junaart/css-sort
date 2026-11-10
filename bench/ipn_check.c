/* SPDX-License-Identifier: MIT */
/* Сверка ipnsort из rivals/ipnsort со slice::sort_unstable стандартной библиотеки Rust:
   число сравнений и результат на 30 входах (n = 10^6) и отношение времени. */
#define _GNU_SOURCE
#include "pin_cpu.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <time.h>
#include <sched.h>
#include "inputs.h"
void ipn_sort_u64(uint64_t*,size_t); void ipnstd_sort_u64(uint64_t*,size_t);
void ipn_sort_u64_cnt(uint64_t*,size_t,int64_t*); void ipnstd_sort_u64_cnt(uint64_t*,size_t,int64_t*);
static double now(void){struct timespec t;clock_gettime(CLOCK_MONOTONIC_RAW,&t);return t.tv_sec+1e-9*t.tv_nsec;}
int main(){css_pin_cpu();
 int64_t n=1000000; uint64_t*A=malloc(n*8),*W=malloc(n*8),*V=malloc(n*8); int same=0; double ta=0,tb=0;
 for(int p=0;p<IN_N;p++){ in_gen(p,A,n,12345); int64_t c1=0,c2=0;
  memcpy(W,A,n*8); ipn_sort_u64_cnt(W,n,&c1); memcpy(V,A,n*8); ipnstd_sort_u64_cnt(V,n,&c2);
  int eq=c1==c2&&!memcmp(W,V,n*8); same+=eq;
  double a=1e9,b=1e9; for(int r=0;r<5;r++){memcpy(W,A,n*8);double t=now();ipn_sort_u64(W,n);t=now()-t;if(t<a)a=t;memcpy(W,A,n*8);t=now();ipnstd_sort_u64(W,n);t=now()-t;if(t<b)b=t;}
  ta+=log(a);tb+=log(b);
  printf("%-14s cmp %lld %lld %s  t %.2f %.2f\n",IN_NAME[p],(long long)c1,(long long)c2,eq?"=":"РАЗНЫЕ",a*1e9/n,b*1e9/n);}
 printf("совпали %d/30; ср.геом. время vendored/std = %.3f\n",same,exp((ta-tb)/IN_N));}
