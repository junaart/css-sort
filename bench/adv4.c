/* SPDX-License-Identifier: MIT */
/* Противник режима значений, согласованный с корнем: выборка корня (255 позиций) задаёт медиану 2^63
   и почти равномерна (таблица не берётся); остальные значения — дерево делений пополам с долей a сверху. */
#define CSS_NO_MAIN
#define CSS_STATS
#define CSS_COUNT
#include "../css.c"
#include <stdio.h>
static uint64_t st=7; static uint64_t rnd(void){uint64_t z=(st+=0x9e3779b97f4a7c15ULL);z=(z^(z>>30))*0xbf58476d1ce4e5b9ULL;z=(z^(z>>27))*0x94d049bb133111ebULL;return z^(z>>31);}
static uint64_t *O; static int64_t k; static double A_;
static void gen(int64_t m, uint64_t lo, uint64_t hi){
  if(m<=0) return;
  if(m<24 || hi-lo < (uint64_t)m*4){ uint64_t d=(hi-lo)/(uint64_t)m; for(int64_t i=0;i<m;i++) O[k++]=lo+(uint64_t)i*d; return; }
  uint64_t w=hi-lo, t=lo+(w-w/2);
  int64_t q=(int64_t)ceil(A_*m); if(q<1)q=1;
  gen(m-q, lo, t-1); gen(q, t, hi);
}
int main(int argc,char**argv){
  int64_t n=argc>1?atoll(argv[1]):1<<20; O=malloc(n*8);
  double as[]={0.1,0.11,0.125,0.15,0.2,0.3,0.5};
  for(int j=0;j<7;j++){ A_=as[j]; k=0;
    /* корень: две половины [0,2^63) и [2^63, 2^64) с долей a у верхней */
    int64_t q=(int64_t)ceil(A_*n); gen(n-q,0,(1ULL<<63)-1); gen(q,1ULL<<63,UINT64_MAX);
    for(int64_t i=n-1;i>0;i--){int64_t r=rnd()%(i+1); uint64_t t=O[i];O[i]=O[r];O[r]=t;}
    int S=255; int64_t step=n/S;
    for(int i=0;i<S;i++){ uint64_t hh=(uint64_t)(i+1)*0x9e3779b97f4a7c15ULL;
      int64_t pos=(int64_t)i*step+(int64_t)(((unsigned __int128)(hh>>32)*(uint64_t)step)>>32);
      uint64_t v = i<127 ? (uint64_t)i*((1ULL<<63)/127) : i==127 ? (1ULL<<63) : (1ULL<<63)+(uint64_t)(i-127)*((1ULL<<63)/128);
      if(i==254) v=UINT64_MAX; O[pos]=v; }
    css_st_badmax=0; css_st_bisect=css_st_tryfail=css_st_good_v=css_st_good_s=css_st_eq=css_st_bad=css_st_deg=0;
    css_cmp_reset(); css_sort(O,n);
    for(int64_t i=1;i<n;i++) if(O[i-1]>O[i]){printf("ERR\n");break;}
    printf("a=%.3f  %.3f n log n | знач %.2f выб %.2f серии %.2f перекос %.2f выр %.2f запасн %.2f badmax %d\n",A_,(double)css_cmp_count()/n/log2((double)n),(double)css_st_good_v/n,(double)css_st_good_s/n,(double)css_st_eq/n,(double)css_st_bad/n,(double)css_st_deg/n,(double)css_st_bisect/n,(int)css_st_badmax);
  }
}
