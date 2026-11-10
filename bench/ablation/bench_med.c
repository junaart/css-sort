/* SPDX-License-Identifier: MIT */
/* Деление пополам против медианы выборки на 30 входах (../inputs.h).
   ./bench_med cmp N | ./bench_med time N R. Столбцы: c2 — CSS (деление пополам), c1.9 — медиана выборки
   вместо деления пополам в режиме значений (CSS_MED=1), c2.1 — только медиана, без режима значений (CSS_MED=2);
   «+tab» — то же с таблицей прошлого массива. */
#define _GNU_SOURCE
#include "../pin_cpu.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <sched.h>
#include <time.h>
#include "../inputs.h"
#define D(t) void cs##t##_sort(uint64_t*,uint64_t); void cs##t##_sort_tab(uint64_t*,uint64_t,const uint64_t*,uint64_t); \
 void cc##t##_sort(uint64_t*,uint64_t); void cc##t##_sort_tab(uint64_t*,uint64_t,const uint64_t*,uint64_t); int64_t cc##t##_cmp_count(void); void cc##t##_cmp_reset(void);
D(19) D(20) D(21)
void cs20_table(const uint64_t*,uint64_t,uint64_t*,uint64_t);
static double now(void){struct timespec t;clock_gettime(CLOCK_MONOTONIC_RAW,&t);return t.tv_sec+1e-9*t.tv_nsec;}
static int dcmp(const void*a,const void*b){double x=*(const double*)a,y=*(const double*)b;return (x>y)-(x<y);}
static uint64_t TAB[4096];
#define NV 6
static const char*VN[NV]={"c2","c1.9","c2.1","c2+tab","c1.9+tab","c2.1+tab"};
static void run(int v,uint64_t*W,int64_t n){switch(v){case 0:cs20_sort(W,n);break;case 1:cs19_sort(W,n);break;case 2:cs21_sort(W,n);break;
 case 3:cs20_sort_tab(W,n,TAB,4096);break;case 4:cs19_sort_tab(W,n,TAB,4096);break;case 5:cs21_sort_tab(W,n,TAB,4096);break;}}
static long long cnt(int v,uint64_t*W,int64_t n){switch(v){
 case 0:cc20_cmp_reset();cc20_sort(W,n);return cc20_cmp_count(); case 1:cc19_cmp_reset();cc19_sort(W,n);return cc19_cmp_count();
 case 2:cc21_cmp_reset();cc21_sort(W,n);return cc21_cmp_count(); case 3:cc20_cmp_reset();cc20_sort_tab(W,n,TAB,4096);return cc20_cmp_count();
 case 4:cc19_cmp_reset();cc19_sort_tab(W,n,TAB,4096);return cc19_cmp_count(); default:cc21_cmp_reset();cc21_sort_tab(W,n,TAB,4096);return cc21_cmp_count();}}
int main(int argc,char**argv){css_pin_cpu();
 const char*mode=argv[1];int64_t n=atoll(argv[2]);int R=argc>3?atoi(argv[3]):5;
 uint64_t*A=malloc(n*8),*W=malloc(n*8),*Ref=malloc(n*8);int bad=0;
 printf("%s,n=%lld\nвход,группа",mode,(long long)n);for(int v=0;v<NV;v++)printf(",%s",VN[v]);printf("\n");
 for(int p=0;p<IN_N;p++){in_gen(p,Ref,n,777);qsort(Ref,n,8,in_cmp);cs20_table(Ref,n,TAB,4096);
  in_gen(p,A,n,12345);memcpy(Ref,A,n*8);qsort(Ref,n,8,in_cmp);double res[NV];
  if(!strcmp(mode,"cmp")){for(int v=0;v<NV;v++){memcpy(W,A,n*8);res[v]=(double)cnt(v,W,n)/n;if(memcmp(W,Ref,n*8))bad++;}}
  else{double t[NV][64];int ord[NV];for(int r=0;r<R;r++){for(int v=0;v<NV;v++)ord[v]=v;
    for(int v=NV-1;v>0;v--){int j=rand()%(v+1);int x=ord[v];ord[v]=ord[j];ord[j]=x;}
    for(int k=0;k<NV;k++){int v=ord[k];memcpy(W,A,n*8);double t0=now();run(v,W,n);t[v][r]=(now()-t0)*1e9/n;if(r==0&&memcmp(W,Ref,n*8))bad++;}}
   for(int v=0;v<NV;v++){qsort(t[v],R,sizeof(double),dcmp);res[v]=t[v][R/2];}}
  printf("%s,%d",IN_NAME[p],IN_GROUP[p]);for(int v=0;v<NV;v++)printf(",%.3f",res[v]);printf("\n");fflush(stdout);}
 if(bad)fprintf(stderr,"ошибок: %d\n",bad);return bad;}
