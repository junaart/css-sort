/* SPDX-License-Identifier: MIT */
#define CSS_NO_MAIN
#define CSS_COUNT
#define CSS_STATS
#include "../css.c"
#include <stdio.h>
#include "inputs.h"
int main(void){ int64_t n=1000000; uint64_t *A=malloc(n*8);
  printf("%-14s %6s | на элемент:  хор.знач хор.выб  серии  перекос выроЖд запасн неуд.вст | badmax\n","вход","C/nlog");
  for(int p=0;p<IN_N;p++){ in_gen(p,A,n,12345);
    css_st_badmax=0; css_st_bisect=css_st_tryfail=css_st_good_v=css_st_good_s=css_st_eq=css_st_bad=css_st_deg=0;
    css_cmp_reset(); css_sort(A,n);
    printf("%-14s %6.3f | %8.2f %7.2f %6.2f %7.2f %6.2f %6.2f %8.2f | %d\n",IN_NAME[p],(double)css_cmp_count()/n/log2(n),
      (double)css_st_good_v/n,(double)css_st_good_s/n,(double)css_st_eq/n,(double)css_st_bad/n,(double)css_st_deg/n,(double)css_st_bisect/n,(double)css_st_tryfail/n,
      (int)css_st_badmax);
  }
}
