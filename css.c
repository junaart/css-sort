/*==== css.c — CSS: сортировка сравнениями построением порождающей последовательности на лету ====

  SPDX-License-Identifier: MIT
  Copyright (c) 2026 Ю. Н. Артамонов (Yu. N. Artamonov) А.Д. Степанов (A.D.Stepanov)

  Алгоритм статьи: Ю. Н. Артамонов, А.Д. Степанов «Гибридный алгоритм сортировки натуральных чисел на основе
  порождающей последовательности, построенной на лету» (IEEE REEPE-2027). Только сравнения ключей.
  Ключи — uint64_t, порядок — по возрастанию, сортировка неустойчивая; память — O(log n) стека и
  стековый буфер слияния 32 КБ (css_sort_ex с heap = 1 — буфер n элементов в куче).
  Не потокобезопасна (таблица прошлого массива передаётся через статическую переменную).

  Устройство (сверху вниз):
  * слой упорядоченных участков: монотонные участки с выбросами, модель стоимости,
    развороты, слияние в порядке powersort; на входах без длинных участков не включается;
  * корень ядра: подсчёт частых значений по выборке или первое разбиение по медиане
    выборки с попутными min/max;
  * ядро: в каждом узле [lo, hi] строится первый элемент порождающей последовательности —
    порог t; режим значений (деление пополам, c = 2, или медиана массы по таблице густоты),
    при перекосе — режим выборки (медиана 3/9/31/127 элементов), серии равных, запас
    перекосов и запасное деление пополам; блочное разбиение без ветвлений, проверка
    упорядоченности, листья до 23 элементов — сети сортировки Батчера;
  * густота: порог узла в режиме значений — медиана массы по таблице густоты. Таблица — либо
    квантили прошлого массива (css_sort_tab), если они согласны с выборкой текущего сегмента
    по расстоянию Колмогорова, либо сама выборка корня, если она не почти равномерна; иначе
    деление пополам.
  Сложность: в среднем на равномерных ключах n log2 n + O(n) сравнений; в худшем случае
  не больше n (2.14 log2 n + 2.29 w + 63), w = ceil(log2(max - min + 1)) <= 64;
  на упорядоченном массиве n - 1 сравнение. Ради этой оценки: пороги перекоса без деления
  (10q < m, 8q < m), запас перекосов min(log2 n, 2), вырожденный проход по медиане расходует его, неудачных
  досортировок вставками на пути не больше CSS_TRYK = 4, узлы короче CSS_TAB_MIN = 1024
  таблицей не пользуются. -DCSS_STATS — статистика проходов по видам.

  Функции:
    void css_sort(uint64_t *Q, uint64_t n);                         — без таблицы прошлого массива
    void css_sort_tab(uint64_t *Q, uint64_t n, const uint64_t *T, uint64_t S);
                                                                    — с таблицей T[0..S) (может быть NULL)
    void css_sort_ex(uint64_t *Q, uint64_t n, const uint64_t *T, uint64_t S, int heap);
                                                                    — heap = 1: буфер слияния n элементов в куче
    void css_table_from_sorted(const uint64_t *sorted, uint64_t n, uint64_t *T, uint64_t S);
                                                                    — S квантилей отсортированного массива
    int64_t css_cmp_count(void); void css_cmp_reset(void);          — счётчик сравнений (-DCSS_COUNT)

  Сборка: gcc -O2 css.c -lm -o css (нужен unsigned __int128 — gcc или clang).
  ./css — самопроверка; ./css 1000000 — то же и время.
  Как библиотека: -DCSS_NO_MAIN или #define CSS_NO_MAIN перед #include "css.c".
  Параметры — макросы CPA_*, CPR_*, CPD_*, CSS_*; значения по умолчанию — те, что в статье. */

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

/* ============================================================================
   1. Сети сортировки для листьев (сгенерированы gen_net.py)
   ============================================================================ */

/* Сгенерировано gen_net.py: сети Батчера, усечённые до n входов, n = 2..23.
   Лишние сравнения удалены; каждая сеть проверена исчерпывающе по принципу 0-1. */
/* Счётчик сравнений ключей (-DCSS_COUNT): все сравнения ключей, включая компараторы сетей в листьях,
   двоичные поиски по таблицам и выборки. По умолчанию пусто. */
#if defined(CSS_COUNT) && !defined(CPA_COUNT)
#define CPA_COUNT
#endif
#ifdef CPA_COUNT
static int64_t cpa_cmp_count;
#define CPA_C(k) (cpa_cmp_count += (k))
#define CPA_CNT1 (cpa_cmp_count++)
#else
#define CPA_C(k) ((void)0)
#define CPA_CNT1 ((void)0)
#endif
#define CPA_CX(a, b) do { uint64_t _x = (a), _y = (b); CPA_CNT1; (a) = _x < _y ? _x : _y; (b) = _x < _y ? _y : _x; } while (0)
static void cpa_net_sort(uint64_t *q, int n) {
  switch (n) {
    case 2: { uint64_t v0 = q[0], v1 = q[1];
      CPA_CX(v0, v1);
      q[0] = v0; q[1] = v1; } break;
    case 3: { uint64_t v0 = q[0], v1 = q[1], v2 = q[2];
      CPA_CX(v0, v1); CPA_CX(v0, v2); CPA_CX(v1, v2);
      q[0] = v0; q[1] = v1; q[2] = v2; } break;
    case 4: { uint64_t v0 = q[0], v1 = q[1], v2 = q[2], v3 = q[3];
      CPA_CX(v0, v1); CPA_CX(v2, v3); CPA_CX(v0, v2); CPA_CX(v1, v3); CPA_CX(v1, v2);
      q[0] = v0; q[1] = v1; q[2] = v2; q[3] = v3; } break;
    case 5: { uint64_t v0 = q[0], v1 = q[1], v2 = q[2], v3 = q[3], v4 = q[4];
      CPA_CX(v0, v1); CPA_CX(v2, v3); CPA_CX(v0, v2); CPA_CX(v1, v3); CPA_CX(v1, v2); CPA_CX(v0, v4); CPA_CX(v2, v4); CPA_CX(v1, v2); CPA_CX(v3, v4);
      q[0] = v0; q[1] = v1; q[2] = v2; q[3] = v3; q[4] = v4; } break;
    case 6: { uint64_t v0 = q[0], v1 = q[1], v2 = q[2], v3 = q[3], v4 = q[4], v5 = q[5];
      CPA_CX(v0, v1); CPA_CX(v2, v3); CPA_CX(v0, v2); CPA_CX(v1, v3); CPA_CX(v1, v2); CPA_CX(v4, v5); CPA_CX(v0, v4); CPA_CX(v2, v4); CPA_CX(v1, v5); CPA_CX(v3, v5); CPA_CX(v1, v2); CPA_CX(v3, v4);
      q[0] = v0; q[1] = v1; q[2] = v2; q[3] = v3; q[4] = v4; q[5] = v5; } break;
    case 7: { uint64_t v0 = q[0], v1 = q[1], v2 = q[2], v3 = q[3], v4 = q[4], v5 = q[5], v6 = q[6];
      CPA_CX(v0, v1); CPA_CX(v2, v3); CPA_CX(v0, v2); CPA_CX(v1, v3); CPA_CX(v1, v2); CPA_CX(v4, v5); CPA_CX(v4, v6); CPA_CX(v5, v6); CPA_CX(v0, v4); CPA_CX(v2, v6); CPA_CX(v2, v4); CPA_CX(v1, v5); CPA_CX(v3, v5); CPA_CX(v1, v2); CPA_CX(v3, v4); CPA_CX(v5, v6);
      q[0] = v0; q[1] = v1; q[2] = v2; q[3] = v3; q[4] = v4; q[5] = v5; q[6] = v6; } break;
    case 8: { uint64_t v0 = q[0], v1 = q[1], v2 = q[2], v3 = q[3], v4 = q[4], v5 = q[5], v6 = q[6], v7 = q[7];
      CPA_CX(v0, v1); CPA_CX(v2, v3); CPA_CX(v0, v2); CPA_CX(v1, v3); CPA_CX(v1, v2); CPA_CX(v4, v5); CPA_CX(v6, v7); CPA_CX(v4, v6); CPA_CX(v5, v7); CPA_CX(v5, v6); CPA_CX(v0, v4); CPA_CX(v2, v6); CPA_CX(v2, v4); CPA_CX(v1, v5); CPA_CX(v3, v7); CPA_CX(v3, v5); CPA_CX(v1, v2); CPA_CX(v3, v4); CPA_CX(v5, v6);
      q[0] = v0; q[1] = v1; q[2] = v2; q[3] = v3; q[4] = v4; q[5] = v5; q[6] = v6; q[7] = v7; } break;
    case 9: { uint64_t v0 = q[0], v1 = q[1], v2 = q[2], v3 = q[3], v4 = q[4], v5 = q[5], v6 = q[6], v7 = q[7], v8 = q[8];
      CPA_CX(v0, v1); CPA_CX(v2, v3); CPA_CX(v0, v2); CPA_CX(v1, v3); CPA_CX(v1, v2); CPA_CX(v4, v5); CPA_CX(v6, v7); CPA_CX(v4, v6); CPA_CX(v5, v7); CPA_CX(v5, v6); CPA_CX(v0, v4); CPA_CX(v2, v6); CPA_CX(v2, v4); CPA_CX(v1, v5); CPA_CX(v3, v7); CPA_CX(v3, v5); CPA_CX(v1, v2); CPA_CX(v3, v4); CPA_CX(v5, v6); CPA_CX(v0, v8); CPA_CX(v4, v8); CPA_CX(v2, v4); CPA_CX(v6, v8); CPA_CX(v1, v2); CPA_CX(v3, v4); CPA_CX(v5, v6); CPA_CX(v7, v8);
      q[0] = v0; q[1] = v1; q[2] = v2; q[3] = v3; q[4] = v4; q[5] = v5; q[6] = v6; q[7] = v7; q[8] = v8; } break;
    case 10: { uint64_t v0 = q[0], v1 = q[1], v2 = q[2], v3 = q[3], v4 = q[4], v5 = q[5], v6 = q[6], v7 = q[7], v8 = q[8], v9 = q[9];
      CPA_CX(v0, v1); CPA_CX(v2, v3); CPA_CX(v0, v2); CPA_CX(v1, v3); CPA_CX(v1, v2); CPA_CX(v4, v5); CPA_CX(v6, v7); CPA_CX(v4, v6); CPA_CX(v5, v7); CPA_CX(v5, v6); CPA_CX(v0, v4); CPA_CX(v2, v6); CPA_CX(v2, v4); CPA_CX(v1, v5); CPA_CX(v3, v7); CPA_CX(v3, v5); CPA_CX(v1, v2); CPA_CX(v3, v4); CPA_CX(v5, v6); CPA_CX(v8, v9); CPA_CX(v0, v8); CPA_CX(v4, v8); CPA_CX(v2, v4); CPA_CX(v6, v8); CPA_CX(v1, v9); CPA_CX(v5, v9); CPA_CX(v3, v5); CPA_CX(v7, v9); CPA_CX(v1, v2); CPA_CX(v3, v4); CPA_CX(v5, v6); CPA_CX(v7, v8);
      q[0] = v0; q[1] = v1; q[2] = v2; q[3] = v3; q[4] = v4; q[5] = v5; q[6] = v6; q[7] = v7; q[8] = v8; q[9] = v9; } break;
    case 11: { uint64_t v0 = q[0], v1 = q[1], v2 = q[2], v3 = q[3], v4 = q[4], v5 = q[5], v6 = q[6], v7 = q[7], v8 = q[8], v9 = q[9], v10 = q[10];
      CPA_CX(v0, v1); CPA_CX(v2, v3); CPA_CX(v0, v2); CPA_CX(v1, v3); CPA_CX(v1, v2); CPA_CX(v4, v5); CPA_CX(v6, v7); CPA_CX(v4, v6); CPA_CX(v5, v7); CPA_CX(v5, v6); CPA_CX(v0, v4); CPA_CX(v2, v6); CPA_CX(v2, v4); CPA_CX(v1, v5); CPA_CX(v3, v7); CPA_CX(v3, v5); CPA_CX(v1, v2); CPA_CX(v3, v4); CPA_CX(v5, v6); CPA_CX(v8, v9); CPA_CX(v8, v10); CPA_CX(v9, v10); CPA_CX(v0, v8); CPA_CX(v4, v8); CPA_CX(v2, v10); CPA_CX(v6, v10); CPA_CX(v2, v4); CPA_CX(v6, v8); CPA_CX(v1, v9); CPA_CX(v5, v9); CPA_CX(v3, v5); CPA_CX(v7, v9); CPA_CX(v1, v2); CPA_CX(v3, v4); CPA_CX(v5, v6); CPA_CX(v7, v8); CPA_CX(v9, v10);
      q[0] = v0; q[1] = v1; q[2] = v2; q[3] = v3; q[4] = v4; q[5] = v5; q[6] = v6; q[7] = v7; q[8] = v8; q[9] = v9; q[10] = v10; } break;
    case 12: { uint64_t v0 = q[0], v1 = q[1], v2 = q[2], v3 = q[3], v4 = q[4], v5 = q[5], v6 = q[6], v7 = q[7], v8 = q[8], v9 = q[9], v10 = q[10], v11 = q[11];
      CPA_CX(v0, v1); CPA_CX(v2, v3); CPA_CX(v0, v2); CPA_CX(v1, v3); CPA_CX(v1, v2); CPA_CX(v4, v5); CPA_CX(v6, v7); CPA_CX(v4, v6); CPA_CX(v5, v7); CPA_CX(v5, v6); CPA_CX(v0, v4); CPA_CX(v2, v6); CPA_CX(v2, v4); CPA_CX(v1, v5); CPA_CX(v3, v7); CPA_CX(v3, v5); CPA_CX(v1, v2); CPA_CX(v3, v4); CPA_CX(v5, v6); CPA_CX(v8, v9); CPA_CX(v10, v11); CPA_CX(v8, v10); CPA_CX(v9, v11); CPA_CX(v9, v10); CPA_CX(v0, v8); CPA_CX(v4, v8); CPA_CX(v2, v10); CPA_CX(v6, v10); CPA_CX(v2, v4); CPA_CX(v6, v8); CPA_CX(v1, v9); CPA_CX(v5, v9); CPA_CX(v3, v11); CPA_CX(v7, v11); CPA_CX(v3, v5); CPA_CX(v7, v9); CPA_CX(v1, v2); CPA_CX(v3, v4); CPA_CX(v5, v6); CPA_CX(v7, v8); CPA_CX(v9, v10);
      q[0] = v0; q[1] = v1; q[2] = v2; q[3] = v3; q[4] = v4; q[5] = v5; q[6] = v6; q[7] = v7; q[8] = v8; q[9] = v9; q[10] = v10; q[11] = v11; } break;
    case 13: { uint64_t v0 = q[0], v1 = q[1], v2 = q[2], v3 = q[3], v4 = q[4], v5 = q[5], v6 = q[6], v7 = q[7], v8 = q[8], v9 = q[9], v10 = q[10], v11 = q[11], v12 = q[12];
      CPA_CX(v0, v1); CPA_CX(v2, v3); CPA_CX(v0, v2); CPA_CX(v1, v3); CPA_CX(v1, v2); CPA_CX(v4, v5); CPA_CX(v6, v7); CPA_CX(v4, v6); CPA_CX(v5, v7); CPA_CX(v5, v6); CPA_CX(v0, v4); CPA_CX(v2, v6); CPA_CX(v2, v4); CPA_CX(v1, v5); CPA_CX(v3, v7); CPA_CX(v3, v5); CPA_CX(v1, v2); CPA_CX(v3, v4); CPA_CX(v5, v6); CPA_CX(v8, v9); CPA_CX(v10, v11); CPA_CX(v8, v10); CPA_CX(v9, v11); CPA_CX(v9, v10); CPA_CX(v8, v12); CPA_CX(v10, v12); CPA_CX(v9, v10); CPA_CX(v11, v12); CPA_CX(v0, v8); CPA_CX(v4, v12); CPA_CX(v4, v8); CPA_CX(v2, v10); CPA_CX(v6, v10); CPA_CX(v2, v4); CPA_CX(v6, v8); CPA_CX(v10, v12); CPA_CX(v1, v9); CPA_CX(v5, v9); CPA_CX(v3, v11); CPA_CX(v7, v11); CPA_CX(v3, v5); CPA_CX(v7, v9); CPA_CX(v1, v2); CPA_CX(v3, v4); CPA_CX(v5, v6); CPA_CX(v7, v8); CPA_CX(v9, v10); CPA_CX(v11, v12);
      q[0] = v0; q[1] = v1; q[2] = v2; q[3] = v3; q[4] = v4; q[5] = v5; q[6] = v6; q[7] = v7; q[8] = v8; q[9] = v9; q[10] = v10; q[11] = v11; q[12] = v12; } break;
    case 14: { uint64_t v0 = q[0], v1 = q[1], v2 = q[2], v3 = q[3], v4 = q[4], v5 = q[5], v6 = q[6], v7 = q[7], v8 = q[8], v9 = q[9], v10 = q[10], v11 = q[11], v12 = q[12], v13 = q[13];
      CPA_CX(v0, v1); CPA_CX(v2, v3); CPA_CX(v0, v2); CPA_CX(v1, v3); CPA_CX(v1, v2); CPA_CX(v4, v5); CPA_CX(v6, v7); CPA_CX(v4, v6); CPA_CX(v5, v7); CPA_CX(v5, v6); CPA_CX(v0, v4); CPA_CX(v2, v6); CPA_CX(v2, v4); CPA_CX(v1, v5); CPA_CX(v3, v7); CPA_CX(v3, v5); CPA_CX(v1, v2); CPA_CX(v3, v4); CPA_CX(v5, v6); CPA_CX(v8, v9); CPA_CX(v10, v11); CPA_CX(v8, v10); CPA_CX(v9, v11); CPA_CX(v9, v10); CPA_CX(v12, v13); CPA_CX(v8, v12); CPA_CX(v10, v12); CPA_CX(v9, v13); CPA_CX(v11, v13); CPA_CX(v9, v10); CPA_CX(v11, v12); CPA_CX(v0, v8); CPA_CX(v4, v12); CPA_CX(v4, v8); CPA_CX(v2, v10); CPA_CX(v6, v10); CPA_CX(v2, v4); CPA_CX(v6, v8); CPA_CX(v10, v12); CPA_CX(v1, v9); CPA_CX(v5, v13); CPA_CX(v5, v9); CPA_CX(v3, v11); CPA_CX(v7, v11); CPA_CX(v3, v5); CPA_CX(v7, v9); CPA_CX(v11, v13); CPA_CX(v1, v2); CPA_CX(v3, v4); CPA_CX(v5, v6); CPA_CX(v7, v8); CPA_CX(v9, v10); CPA_CX(v11, v12);
      q[0] = v0; q[1] = v1; q[2] = v2; q[3] = v3; q[4] = v4; q[5] = v5; q[6] = v6; q[7] = v7; q[8] = v8; q[9] = v9; q[10] = v10; q[11] = v11; q[12] = v12; q[13] = v13; } break;
    case 15: { uint64_t v0 = q[0], v1 = q[1], v2 = q[2], v3 = q[3], v4 = q[4], v5 = q[5], v6 = q[6], v7 = q[7], v8 = q[8], v9 = q[9], v10 = q[10], v11 = q[11], v12 = q[12], v13 = q[13], v14 = q[14];
      CPA_CX(v0, v1); CPA_CX(v2, v3); CPA_CX(v0, v2); CPA_CX(v1, v3); CPA_CX(v1, v2); CPA_CX(v4, v5); CPA_CX(v6, v7); CPA_CX(v4, v6); CPA_CX(v5, v7); CPA_CX(v5, v6); CPA_CX(v0, v4); CPA_CX(v2, v6); CPA_CX(v2, v4); CPA_CX(v1, v5); CPA_CX(v3, v7); CPA_CX(v3, v5); CPA_CX(v1, v2); CPA_CX(v3, v4); CPA_CX(v5, v6); CPA_CX(v8, v9); CPA_CX(v10, v11); CPA_CX(v8, v10); CPA_CX(v9, v11); CPA_CX(v9, v10); CPA_CX(v12, v13); CPA_CX(v12, v14); CPA_CX(v13, v14); CPA_CX(v8, v12); CPA_CX(v10, v14); CPA_CX(v10, v12); CPA_CX(v9, v13); CPA_CX(v11, v13); CPA_CX(v9, v10); CPA_CX(v11, v12); CPA_CX(v13, v14); CPA_CX(v0, v8); CPA_CX(v4, v12); CPA_CX(v4, v8); CPA_CX(v2, v10); CPA_CX(v6, v14); CPA_CX(v6, v10); CPA_CX(v2, v4); CPA_CX(v6, v8); CPA_CX(v10, v12); CPA_CX(v1, v9); CPA_CX(v5, v13); CPA_CX(v5, v9); CPA_CX(v3, v11); CPA_CX(v7, v11); CPA_CX(v3, v5); CPA_CX(v7, v9); CPA_CX(v11, v13); CPA_CX(v1, v2); CPA_CX(v3, v4); CPA_CX(v5, v6); CPA_CX(v7, v8); CPA_CX(v9, v10); CPA_CX(v11, v12); CPA_CX(v13, v14);
      q[0] = v0; q[1] = v1; q[2] = v2; q[3] = v3; q[4] = v4; q[5] = v5; q[6] = v6; q[7] = v7; q[8] = v8; q[9] = v9; q[10] = v10; q[11] = v11; q[12] = v12; q[13] = v13; q[14] = v14; } break;
    case 16: { uint64_t v0 = q[0], v1 = q[1], v2 = q[2], v3 = q[3], v4 = q[4], v5 = q[5], v6 = q[6], v7 = q[7], v8 = q[8], v9 = q[9], v10 = q[10], v11 = q[11], v12 = q[12], v13 = q[13], v14 = q[14], v15 = q[15];
      CPA_CX(v0, v1); CPA_CX(v2, v3); CPA_CX(v0, v2); CPA_CX(v1, v3); CPA_CX(v1, v2); CPA_CX(v4, v5); CPA_CX(v6, v7); CPA_CX(v4, v6); CPA_CX(v5, v7); CPA_CX(v5, v6); CPA_CX(v0, v4); CPA_CX(v2, v6); CPA_CX(v2, v4); CPA_CX(v1, v5); CPA_CX(v3, v7); CPA_CX(v3, v5); CPA_CX(v1, v2); CPA_CX(v3, v4); CPA_CX(v5, v6); CPA_CX(v8, v9); CPA_CX(v10, v11); CPA_CX(v8, v10); CPA_CX(v9, v11); CPA_CX(v9, v10); CPA_CX(v12, v13); CPA_CX(v14, v15); CPA_CX(v12, v14); CPA_CX(v13, v15); CPA_CX(v13, v14); CPA_CX(v8, v12); CPA_CX(v10, v14); CPA_CX(v10, v12); CPA_CX(v9, v13); CPA_CX(v11, v15); CPA_CX(v11, v13); CPA_CX(v9, v10); CPA_CX(v11, v12); CPA_CX(v13, v14); CPA_CX(v0, v8); CPA_CX(v4, v12); CPA_CX(v4, v8); CPA_CX(v2, v10); CPA_CX(v6, v14); CPA_CX(v6, v10); CPA_CX(v2, v4); CPA_CX(v6, v8); CPA_CX(v10, v12); CPA_CX(v1, v9); CPA_CX(v5, v13); CPA_CX(v5, v9); CPA_CX(v3, v11); CPA_CX(v7, v15); CPA_CX(v7, v11); CPA_CX(v3, v5); CPA_CX(v7, v9); CPA_CX(v11, v13); CPA_CX(v1, v2); CPA_CX(v3, v4); CPA_CX(v5, v6); CPA_CX(v7, v8); CPA_CX(v9, v10); CPA_CX(v11, v12); CPA_CX(v13, v14);
      q[0] = v0; q[1] = v1; q[2] = v2; q[3] = v3; q[4] = v4; q[5] = v5; q[6] = v6; q[7] = v7; q[8] = v8; q[9] = v9; q[10] = v10; q[11] = v11; q[12] = v12; q[13] = v13; q[14] = v14; q[15] = v15; } break;
    case 17: { uint64_t v0 = q[0], v1 = q[1], v2 = q[2], v3 = q[3], v4 = q[4], v5 = q[5], v6 = q[6], v7 = q[7], v8 = q[8], v9 = q[9], v10 = q[10], v11 = q[11], v12 = q[12], v13 = q[13], v14 = q[14], v15 = q[15], v16 = q[16];
      CPA_CX(v0, v1); CPA_CX(v2, v3); CPA_CX(v0, v2); CPA_CX(v1, v3); CPA_CX(v1, v2); CPA_CX(v4, v5); CPA_CX(v6, v7); CPA_CX(v4, v6); CPA_CX(v5, v7); CPA_CX(v5, v6); CPA_CX(v0, v4); CPA_CX(v2, v6); CPA_CX(v2, v4); CPA_CX(v1, v5); CPA_CX(v3, v7); CPA_CX(v3, v5); CPA_CX(v1, v2); CPA_CX(v3, v4); CPA_CX(v5, v6); CPA_CX(v8, v9); CPA_CX(v10, v11); CPA_CX(v8, v10); CPA_CX(v9, v11); CPA_CX(v9, v10); CPA_CX(v12, v13); CPA_CX(v14, v15); CPA_CX(v12, v14); CPA_CX(v13, v15); CPA_CX(v13, v14); CPA_CX(v8, v12); CPA_CX(v10, v14); CPA_CX(v10, v12); CPA_CX(v9, v13); CPA_CX(v11, v15); CPA_CX(v11, v13); CPA_CX(v9, v10); CPA_CX(v11, v12); CPA_CX(v13, v14); CPA_CX(v0, v8); CPA_CX(v4, v12); CPA_CX(v4, v8); CPA_CX(v2, v10); CPA_CX(v6, v14); CPA_CX(v6, v10); CPA_CX(v2, v4); CPA_CX(v6, v8); CPA_CX(v10, v12); CPA_CX(v1, v9); CPA_CX(v5, v13); CPA_CX(v5, v9); CPA_CX(v3, v11); CPA_CX(v7, v15); CPA_CX(v7, v11); CPA_CX(v3, v5); CPA_CX(v7, v9); CPA_CX(v11, v13); CPA_CX(v1, v2); CPA_CX(v3, v4); CPA_CX(v5, v6); CPA_CX(v7, v8); CPA_CX(v9, v10); CPA_CX(v11, v12); CPA_CX(v13, v14); CPA_CX(v0, v16); CPA_CX(v8, v16); CPA_CX(v4, v8); CPA_CX(v12, v16); CPA_CX(v2, v4); CPA_CX(v6, v8); CPA_CX(v10, v12); CPA_CX(v14, v16); CPA_CX(v1, v2); CPA_CX(v3, v4); CPA_CX(v5, v6); CPA_CX(v7, v8); CPA_CX(v9, v10); CPA_CX(v11, v12); CPA_CX(v13, v14); CPA_CX(v15, v16);
      q[0] = v0; q[1] = v1; q[2] = v2; q[3] = v3; q[4] = v4; q[5] = v5; q[6] = v6; q[7] = v7; q[8] = v8; q[9] = v9; q[10] = v10; q[11] = v11; q[12] = v12; q[13] = v13; q[14] = v14; q[15] = v15; q[16] = v16; } break;
    case 18: { uint64_t v0 = q[0], v1 = q[1], v2 = q[2], v3 = q[3], v4 = q[4], v5 = q[5], v6 = q[6], v7 = q[7], v8 = q[8], v9 = q[9], v10 = q[10], v11 = q[11], v12 = q[12], v13 = q[13], v14 = q[14], v15 = q[15], v16 = q[16], v17 = q[17];
      CPA_CX(v0, v1); CPA_CX(v2, v3); CPA_CX(v0, v2); CPA_CX(v1, v3); CPA_CX(v1, v2); CPA_CX(v4, v5); CPA_CX(v6, v7); CPA_CX(v4, v6); CPA_CX(v5, v7); CPA_CX(v5, v6); CPA_CX(v0, v4); CPA_CX(v2, v6); CPA_CX(v2, v4); CPA_CX(v1, v5); CPA_CX(v3, v7); CPA_CX(v3, v5); CPA_CX(v1, v2); CPA_CX(v3, v4); CPA_CX(v5, v6); CPA_CX(v8, v9); CPA_CX(v10, v11); CPA_CX(v8, v10); CPA_CX(v9, v11); CPA_CX(v9, v10); CPA_CX(v12, v13); CPA_CX(v14, v15); CPA_CX(v12, v14); CPA_CX(v13, v15); CPA_CX(v13, v14); CPA_CX(v8, v12); CPA_CX(v10, v14); CPA_CX(v10, v12); CPA_CX(v9, v13); CPA_CX(v11, v15); CPA_CX(v11, v13); CPA_CX(v9, v10); CPA_CX(v11, v12); CPA_CX(v13, v14); CPA_CX(v0, v8); CPA_CX(v4, v12); CPA_CX(v4, v8); CPA_CX(v2, v10); CPA_CX(v6, v14); CPA_CX(v6, v10); CPA_CX(v2, v4); CPA_CX(v6, v8); CPA_CX(v10, v12); CPA_CX(v1, v9); CPA_CX(v5, v13); CPA_CX(v5, v9); CPA_CX(v3, v11); CPA_CX(v7, v15); CPA_CX(v7, v11); CPA_CX(v3, v5); CPA_CX(v7, v9); CPA_CX(v11, v13); CPA_CX(v1, v2); CPA_CX(v3, v4); CPA_CX(v5, v6); CPA_CX(v7, v8); CPA_CX(v9, v10); CPA_CX(v11, v12); CPA_CX(v13, v14); CPA_CX(v16, v17); CPA_CX(v0, v16); CPA_CX(v8, v16); CPA_CX(v4, v8); CPA_CX(v12, v16); CPA_CX(v2, v4); CPA_CX(v6, v8); CPA_CX(v10, v12); CPA_CX(v14, v16); CPA_CX(v1, v17); CPA_CX(v9, v17); CPA_CX(v5, v9); CPA_CX(v13, v17); CPA_CX(v3, v5); CPA_CX(v7, v9); CPA_CX(v11, v13); CPA_CX(v15, v17); CPA_CX(v1, v2); CPA_CX(v3, v4); CPA_CX(v5, v6); CPA_CX(v7, v8); CPA_CX(v9, v10); CPA_CX(v11, v12); CPA_CX(v13, v14); CPA_CX(v15, v16);
      q[0] = v0; q[1] = v1; q[2] = v2; q[3] = v3; q[4] = v4; q[5] = v5; q[6] = v6; q[7] = v7; q[8] = v8; q[9] = v9; q[10] = v10; q[11] = v11; q[12] = v12; q[13] = v13; q[14] = v14; q[15] = v15; q[16] = v16; q[17] = v17; } break;
    case 19: { uint64_t v0 = q[0], v1 = q[1], v2 = q[2], v3 = q[3], v4 = q[4], v5 = q[5], v6 = q[6], v7 = q[7], v8 = q[8], v9 = q[9], v10 = q[10], v11 = q[11], v12 = q[12], v13 = q[13], v14 = q[14], v15 = q[15], v16 = q[16], v17 = q[17], v18 = q[18];
      CPA_CX(v0, v1); CPA_CX(v2, v3); CPA_CX(v0, v2); CPA_CX(v1, v3); CPA_CX(v1, v2); CPA_CX(v4, v5); CPA_CX(v6, v7); CPA_CX(v4, v6); CPA_CX(v5, v7); CPA_CX(v5, v6); CPA_CX(v0, v4); CPA_CX(v2, v6); CPA_CX(v2, v4); CPA_CX(v1, v5); CPA_CX(v3, v7); CPA_CX(v3, v5); CPA_CX(v1, v2); CPA_CX(v3, v4); CPA_CX(v5, v6); CPA_CX(v8, v9); CPA_CX(v10, v11); CPA_CX(v8, v10); CPA_CX(v9, v11); CPA_CX(v9, v10); CPA_CX(v12, v13); CPA_CX(v14, v15); CPA_CX(v12, v14); CPA_CX(v13, v15); CPA_CX(v13, v14); CPA_CX(v8, v12); CPA_CX(v10, v14); CPA_CX(v10, v12); CPA_CX(v9, v13); CPA_CX(v11, v15); CPA_CX(v11, v13); CPA_CX(v9, v10); CPA_CX(v11, v12); CPA_CX(v13, v14); CPA_CX(v0, v8); CPA_CX(v4, v12); CPA_CX(v4, v8); CPA_CX(v2, v10); CPA_CX(v6, v14); CPA_CX(v6, v10); CPA_CX(v2, v4); CPA_CX(v6, v8); CPA_CX(v10, v12); CPA_CX(v1, v9); CPA_CX(v5, v13); CPA_CX(v5, v9); CPA_CX(v3, v11); CPA_CX(v7, v15); CPA_CX(v7, v11); CPA_CX(v3, v5); CPA_CX(v7, v9); CPA_CX(v11, v13); CPA_CX(v1, v2); CPA_CX(v3, v4); CPA_CX(v5, v6); CPA_CX(v7, v8); CPA_CX(v9, v10); CPA_CX(v11, v12); CPA_CX(v13, v14); CPA_CX(v16, v17); CPA_CX(v16, v18); CPA_CX(v17, v18); CPA_CX(v0, v16); CPA_CX(v8, v16); CPA_CX(v4, v8); CPA_CX(v12, v16); CPA_CX(v2, v18); CPA_CX(v10, v18); CPA_CX(v6, v10); CPA_CX(v14, v18); CPA_CX(v2, v4); CPA_CX(v6, v8); CPA_CX(v10, v12); CPA_CX(v14, v16); CPA_CX(v1, v17); CPA_CX(v9, v17); CPA_CX(v5, v9); CPA_CX(v13, v17); CPA_CX(v3, v5); CPA_CX(v7, v9); CPA_CX(v11, v13); CPA_CX(v15, v17); CPA_CX(v1, v2); CPA_CX(v3, v4); CPA_CX(v5, v6); CPA_CX(v7, v8); CPA_CX(v9, v10); CPA_CX(v11, v12); CPA_CX(v13, v14); CPA_CX(v15, v16); CPA_CX(v17, v18);
      q[0] = v0; q[1] = v1; q[2] = v2; q[3] = v3; q[4] = v4; q[5] = v5; q[6] = v6; q[7] = v7; q[8] = v8; q[9] = v9; q[10] = v10; q[11] = v11; q[12] = v12; q[13] = v13; q[14] = v14; q[15] = v15; q[16] = v16; q[17] = v17; q[18] = v18; } break;
    case 20: { uint64_t v0 = q[0], v1 = q[1], v2 = q[2], v3 = q[3], v4 = q[4], v5 = q[5], v6 = q[6], v7 = q[7], v8 = q[8], v9 = q[9], v10 = q[10], v11 = q[11], v12 = q[12], v13 = q[13], v14 = q[14], v15 = q[15], v16 = q[16], v17 = q[17], v18 = q[18], v19 = q[19];
      CPA_CX(v0, v1); CPA_CX(v2, v3); CPA_CX(v0, v2); CPA_CX(v1, v3); CPA_CX(v1, v2); CPA_CX(v4, v5); CPA_CX(v6, v7); CPA_CX(v4, v6); CPA_CX(v5, v7); CPA_CX(v5, v6); CPA_CX(v0, v4); CPA_CX(v2, v6); CPA_CX(v2, v4); CPA_CX(v1, v5); CPA_CX(v3, v7); CPA_CX(v3, v5); CPA_CX(v1, v2); CPA_CX(v3, v4); CPA_CX(v5, v6); CPA_CX(v8, v9); CPA_CX(v10, v11); CPA_CX(v8, v10); CPA_CX(v9, v11); CPA_CX(v9, v10); CPA_CX(v12, v13); CPA_CX(v14, v15); CPA_CX(v12, v14); CPA_CX(v13, v15); CPA_CX(v13, v14); CPA_CX(v8, v12); CPA_CX(v10, v14); CPA_CX(v10, v12); CPA_CX(v9, v13); CPA_CX(v11, v15); CPA_CX(v11, v13); CPA_CX(v9, v10); CPA_CX(v11, v12); CPA_CX(v13, v14); CPA_CX(v0, v8); CPA_CX(v4, v12); CPA_CX(v4, v8); CPA_CX(v2, v10); CPA_CX(v6, v14); CPA_CX(v6, v10); CPA_CX(v2, v4); CPA_CX(v6, v8); CPA_CX(v10, v12); CPA_CX(v1, v9); CPA_CX(v5, v13); CPA_CX(v5, v9); CPA_CX(v3, v11); CPA_CX(v7, v15); CPA_CX(v7, v11); CPA_CX(v3, v5); CPA_CX(v7, v9); CPA_CX(v11, v13); CPA_CX(v1, v2); CPA_CX(v3, v4); CPA_CX(v5, v6); CPA_CX(v7, v8); CPA_CX(v9, v10); CPA_CX(v11, v12); CPA_CX(v13, v14); CPA_CX(v16, v17); CPA_CX(v18, v19); CPA_CX(v16, v18); CPA_CX(v17, v19); CPA_CX(v17, v18); CPA_CX(v0, v16); CPA_CX(v8, v16); CPA_CX(v4, v8); CPA_CX(v12, v16); CPA_CX(v2, v18); CPA_CX(v10, v18); CPA_CX(v6, v10); CPA_CX(v14, v18); CPA_CX(v2, v4); CPA_CX(v6, v8); CPA_CX(v10, v12); CPA_CX(v14, v16); CPA_CX(v1, v17); CPA_CX(v9, v17); CPA_CX(v5, v9); CPA_CX(v13, v17); CPA_CX(v3, v19); CPA_CX(v11, v19); CPA_CX(v7, v11); CPA_CX(v15, v19); CPA_CX(v3, v5); CPA_CX(v7, v9); CPA_CX(v11, v13); CPA_CX(v15, v17); CPA_CX(v1, v2); CPA_CX(v3, v4); CPA_CX(v5, v6); CPA_CX(v7, v8); CPA_CX(v9, v10); CPA_CX(v11, v12); CPA_CX(v13, v14); CPA_CX(v15, v16); CPA_CX(v17, v18);
      q[0] = v0; q[1] = v1; q[2] = v2; q[3] = v3; q[4] = v4; q[5] = v5; q[6] = v6; q[7] = v7; q[8] = v8; q[9] = v9; q[10] = v10; q[11] = v11; q[12] = v12; q[13] = v13; q[14] = v14; q[15] = v15; q[16] = v16; q[17] = v17; q[18] = v18; q[19] = v19; } break;
    case 21: { uint64_t v0 = q[0], v1 = q[1], v2 = q[2], v3 = q[3], v4 = q[4], v5 = q[5], v6 = q[6], v7 = q[7], v8 = q[8], v9 = q[9], v10 = q[10], v11 = q[11], v12 = q[12], v13 = q[13], v14 = q[14], v15 = q[15], v16 = q[16], v17 = q[17], v18 = q[18], v19 = q[19], v20 = q[20];
      CPA_CX(v0, v1); CPA_CX(v2, v3); CPA_CX(v0, v2); CPA_CX(v1, v3); CPA_CX(v1, v2); CPA_CX(v4, v5); CPA_CX(v6, v7); CPA_CX(v4, v6); CPA_CX(v5, v7); CPA_CX(v5, v6); CPA_CX(v0, v4); CPA_CX(v2, v6); CPA_CX(v2, v4); CPA_CX(v1, v5); CPA_CX(v3, v7); CPA_CX(v3, v5); CPA_CX(v1, v2); CPA_CX(v3, v4); CPA_CX(v5, v6); CPA_CX(v8, v9); CPA_CX(v10, v11); CPA_CX(v8, v10); CPA_CX(v9, v11); CPA_CX(v9, v10); CPA_CX(v12, v13); CPA_CX(v14, v15); CPA_CX(v12, v14); CPA_CX(v13, v15); CPA_CX(v13, v14); CPA_CX(v8, v12); CPA_CX(v10, v14); CPA_CX(v10, v12); CPA_CX(v9, v13); CPA_CX(v11, v15); CPA_CX(v11, v13); CPA_CX(v9, v10); CPA_CX(v11, v12); CPA_CX(v13, v14); CPA_CX(v0, v8); CPA_CX(v4, v12); CPA_CX(v4, v8); CPA_CX(v2, v10); CPA_CX(v6, v14); CPA_CX(v6, v10); CPA_CX(v2, v4); CPA_CX(v6, v8); CPA_CX(v10, v12); CPA_CX(v1, v9); CPA_CX(v5, v13); CPA_CX(v5, v9); CPA_CX(v3, v11); CPA_CX(v7, v15); CPA_CX(v7, v11); CPA_CX(v3, v5); CPA_CX(v7, v9); CPA_CX(v11, v13); CPA_CX(v1, v2); CPA_CX(v3, v4); CPA_CX(v5, v6); CPA_CX(v7, v8); CPA_CX(v9, v10); CPA_CX(v11, v12); CPA_CX(v13, v14); CPA_CX(v16, v17); CPA_CX(v18, v19); CPA_CX(v16, v18); CPA_CX(v17, v19); CPA_CX(v17, v18); CPA_CX(v16, v20); CPA_CX(v18, v20); CPA_CX(v17, v18); CPA_CX(v19, v20); CPA_CX(v0, v16); CPA_CX(v8, v16); CPA_CX(v4, v20); CPA_CX(v12, v20); CPA_CX(v4, v8); CPA_CX(v12, v16); CPA_CX(v2, v18); CPA_CX(v10, v18); CPA_CX(v6, v10); CPA_CX(v14, v18); CPA_CX(v2, v4); CPA_CX(v6, v8); CPA_CX(v10, v12); CPA_CX(v14, v16); CPA_CX(v18, v20); CPA_CX(v1, v17); CPA_CX(v9, v17); CPA_CX(v5, v9); CPA_CX(v13, v17); CPA_CX(v3, v19); CPA_CX(v11, v19); CPA_CX(v7, v11); CPA_CX(v15, v19); CPA_CX(v3, v5); CPA_CX(v7, v9); CPA_CX(v11, v13); CPA_CX(v15, v17); CPA_CX(v1, v2); CPA_CX(v3, v4); CPA_CX(v5, v6); CPA_CX(v7, v8); CPA_CX(v9, v10); CPA_CX(v11, v12); CPA_CX(v13, v14); CPA_CX(v15, v16); CPA_CX(v17, v18); CPA_CX(v19, v20);
      q[0] = v0; q[1] = v1; q[2] = v2; q[3] = v3; q[4] = v4; q[5] = v5; q[6] = v6; q[7] = v7; q[8] = v8; q[9] = v9; q[10] = v10; q[11] = v11; q[12] = v12; q[13] = v13; q[14] = v14; q[15] = v15; q[16] = v16; q[17] = v17; q[18] = v18; q[19] = v19; q[20] = v20; } break;
    case 22: { uint64_t v0 = q[0], v1 = q[1], v2 = q[2], v3 = q[3], v4 = q[4], v5 = q[5], v6 = q[6], v7 = q[7], v8 = q[8], v9 = q[9], v10 = q[10], v11 = q[11], v12 = q[12], v13 = q[13], v14 = q[14], v15 = q[15], v16 = q[16], v17 = q[17], v18 = q[18], v19 = q[19], v20 = q[20], v21 = q[21];
      CPA_CX(v0, v1); CPA_CX(v2, v3); CPA_CX(v0, v2); CPA_CX(v1, v3); CPA_CX(v1, v2); CPA_CX(v4, v5); CPA_CX(v6, v7); CPA_CX(v4, v6); CPA_CX(v5, v7); CPA_CX(v5, v6); CPA_CX(v0, v4); CPA_CX(v2, v6); CPA_CX(v2, v4); CPA_CX(v1, v5); CPA_CX(v3, v7); CPA_CX(v3, v5); CPA_CX(v1, v2); CPA_CX(v3, v4); CPA_CX(v5, v6); CPA_CX(v8, v9); CPA_CX(v10, v11); CPA_CX(v8, v10); CPA_CX(v9, v11); CPA_CX(v9, v10); CPA_CX(v12, v13); CPA_CX(v14, v15); CPA_CX(v12, v14); CPA_CX(v13, v15); CPA_CX(v13, v14); CPA_CX(v8, v12); CPA_CX(v10, v14); CPA_CX(v10, v12); CPA_CX(v9, v13); CPA_CX(v11, v15); CPA_CX(v11, v13); CPA_CX(v9, v10); CPA_CX(v11, v12); CPA_CX(v13, v14); CPA_CX(v0, v8); CPA_CX(v4, v12); CPA_CX(v4, v8); CPA_CX(v2, v10); CPA_CX(v6, v14); CPA_CX(v6, v10); CPA_CX(v2, v4); CPA_CX(v6, v8); CPA_CX(v10, v12); CPA_CX(v1, v9); CPA_CX(v5, v13); CPA_CX(v5, v9); CPA_CX(v3, v11); CPA_CX(v7, v15); CPA_CX(v7, v11); CPA_CX(v3, v5); CPA_CX(v7, v9); CPA_CX(v11, v13); CPA_CX(v1, v2); CPA_CX(v3, v4); CPA_CX(v5, v6); CPA_CX(v7, v8); CPA_CX(v9, v10); CPA_CX(v11, v12); CPA_CX(v13, v14); CPA_CX(v16, v17); CPA_CX(v18, v19); CPA_CX(v16, v18); CPA_CX(v17, v19); CPA_CX(v17, v18); CPA_CX(v20, v21); CPA_CX(v16, v20); CPA_CX(v18, v20); CPA_CX(v17, v21); CPA_CX(v19, v21); CPA_CX(v17, v18); CPA_CX(v19, v20); CPA_CX(v0, v16); CPA_CX(v8, v16); CPA_CX(v4, v20); CPA_CX(v12, v20); CPA_CX(v4, v8); CPA_CX(v12, v16); CPA_CX(v2, v18); CPA_CX(v10, v18); CPA_CX(v6, v10); CPA_CX(v14, v18); CPA_CX(v2, v4); CPA_CX(v6, v8); CPA_CX(v10, v12); CPA_CX(v14, v16); CPA_CX(v18, v20); CPA_CX(v1, v17); CPA_CX(v9, v17); CPA_CX(v5, v21); CPA_CX(v13, v21); CPA_CX(v5, v9); CPA_CX(v13, v17); CPA_CX(v3, v19); CPA_CX(v11, v19); CPA_CX(v7, v11); CPA_CX(v15, v19); CPA_CX(v3, v5); CPA_CX(v7, v9); CPA_CX(v11, v13); CPA_CX(v15, v17); CPA_CX(v19, v21); CPA_CX(v1, v2); CPA_CX(v3, v4); CPA_CX(v5, v6); CPA_CX(v7, v8); CPA_CX(v9, v10); CPA_CX(v11, v12); CPA_CX(v13, v14); CPA_CX(v15, v16); CPA_CX(v17, v18); CPA_CX(v19, v20);
      q[0] = v0; q[1] = v1; q[2] = v2; q[3] = v3; q[4] = v4; q[5] = v5; q[6] = v6; q[7] = v7; q[8] = v8; q[9] = v9; q[10] = v10; q[11] = v11; q[12] = v12; q[13] = v13; q[14] = v14; q[15] = v15; q[16] = v16; q[17] = v17; q[18] = v18; q[19] = v19; q[20] = v20; q[21] = v21; } break;
    case 23: { uint64_t v0 = q[0], v1 = q[1], v2 = q[2], v3 = q[3], v4 = q[4], v5 = q[5], v6 = q[6], v7 = q[7], v8 = q[8], v9 = q[9], v10 = q[10], v11 = q[11], v12 = q[12], v13 = q[13], v14 = q[14], v15 = q[15], v16 = q[16], v17 = q[17], v18 = q[18], v19 = q[19], v20 = q[20], v21 = q[21], v22 = q[22];
      CPA_CX(v0, v1); CPA_CX(v2, v3); CPA_CX(v0, v2); CPA_CX(v1, v3); CPA_CX(v1, v2); CPA_CX(v4, v5); CPA_CX(v6, v7); CPA_CX(v4, v6); CPA_CX(v5, v7); CPA_CX(v5, v6); CPA_CX(v0, v4); CPA_CX(v2, v6); CPA_CX(v2, v4); CPA_CX(v1, v5); CPA_CX(v3, v7); CPA_CX(v3, v5); CPA_CX(v1, v2); CPA_CX(v3, v4); CPA_CX(v5, v6); CPA_CX(v8, v9); CPA_CX(v10, v11); CPA_CX(v8, v10); CPA_CX(v9, v11); CPA_CX(v9, v10); CPA_CX(v12, v13); CPA_CX(v14, v15); CPA_CX(v12, v14); CPA_CX(v13, v15); CPA_CX(v13, v14); CPA_CX(v8, v12); CPA_CX(v10, v14); CPA_CX(v10, v12); CPA_CX(v9, v13); CPA_CX(v11, v15); CPA_CX(v11, v13); CPA_CX(v9, v10); CPA_CX(v11, v12); CPA_CX(v13, v14); CPA_CX(v0, v8); CPA_CX(v4, v12); CPA_CX(v4, v8); CPA_CX(v2, v10); CPA_CX(v6, v14); CPA_CX(v6, v10); CPA_CX(v2, v4); CPA_CX(v6, v8); CPA_CX(v10, v12); CPA_CX(v1, v9); CPA_CX(v5, v13); CPA_CX(v5, v9); CPA_CX(v3, v11); CPA_CX(v7, v15); CPA_CX(v7, v11); CPA_CX(v3, v5); CPA_CX(v7, v9); CPA_CX(v11, v13); CPA_CX(v1, v2); CPA_CX(v3, v4); CPA_CX(v5, v6); CPA_CX(v7, v8); CPA_CX(v9, v10); CPA_CX(v11, v12); CPA_CX(v13, v14); CPA_CX(v16, v17); CPA_CX(v18, v19); CPA_CX(v16, v18); CPA_CX(v17, v19); CPA_CX(v17, v18); CPA_CX(v20, v21); CPA_CX(v20, v22); CPA_CX(v21, v22); CPA_CX(v16, v20); CPA_CX(v18, v22); CPA_CX(v18, v20); CPA_CX(v17, v21); CPA_CX(v19, v21); CPA_CX(v17, v18); CPA_CX(v19, v20); CPA_CX(v21, v22); CPA_CX(v0, v16); CPA_CX(v8, v16); CPA_CX(v4, v20); CPA_CX(v12, v20); CPA_CX(v4, v8); CPA_CX(v12, v16); CPA_CX(v2, v18); CPA_CX(v10, v18); CPA_CX(v6, v22); CPA_CX(v14, v22); CPA_CX(v6, v10); CPA_CX(v14, v18); CPA_CX(v2, v4); CPA_CX(v6, v8); CPA_CX(v10, v12); CPA_CX(v14, v16); CPA_CX(v18, v20); CPA_CX(v1, v17); CPA_CX(v9, v17); CPA_CX(v5, v21); CPA_CX(v13, v21); CPA_CX(v5, v9); CPA_CX(v13, v17); CPA_CX(v3, v19); CPA_CX(v11, v19); CPA_CX(v7, v11); CPA_CX(v15, v19); CPA_CX(v3, v5); CPA_CX(v7, v9); CPA_CX(v11, v13); CPA_CX(v15, v17); CPA_CX(v19, v21); CPA_CX(v1, v2); CPA_CX(v3, v4); CPA_CX(v5, v6); CPA_CX(v7, v8); CPA_CX(v9, v10); CPA_CX(v11, v12); CPA_CX(v13, v14); CPA_CX(v15, v16); CPA_CX(v17, v18); CPA_CX(v19, v20); CPA_CX(v21, v22);
      q[0] = v0; q[1] = v1; q[2] = v2; q[3] = v3; q[4] = v4; q[5] = v5; q[6] = v6; q[7] = v7; q[8] = v8; q[9] = v9; q[10] = v10; q[11] = v11; q[12] = v12; q[13] = v13; q[14] = v14; q[15] = v15; q[16] = v16; q[17] = v17; q[18] = v18; q[19] = v19; q[20] = v20; q[21] = v21; q[22] = v22; } break;
    default: break;
  }
}


/* ============================================================================
   2. Ядро: порождающая последовательность на лету в каждом узле
   ============================================================================ */

#define CPA_BLOCK 64

#ifndef CPA_INS
#define CPA_INS 24
#endif

/* Лист (участок короче CPA_INS): net = 1 — сеть без ветвлений,
   net = 0 — вставками (так листья сортируются в cs_otf, cs_otfp и cs_pdq без сетей). */
static void cpa_insertion(uint64_t *Q, int64_t x, int64_t y, int net) {
  if (net && y - x + 1 <= 23) { cpa_net_sort(Q + x, (int)(y - x + 1)); return; }
  if (x > 0) {
    /* Слева от участка стоит элемент строго меньше всех его значений (разбиение
       по порогу), он служит ограничителем: проверка j >= x не нужна. */
    for (int64_t i = x + 1; i <= y; i++) {
      uint64_t v = Q[i];
      int64_t j = i - 1;
      while ((CPA_C(1), Q[j] > v)) { Q[j + 1] = Q[j]; j--; }
      Q[j + 1] = v;
    }
    return;
  }
  for (int64_t i = x + 1; i <= y; i++) {
    uint64_t v = Q[i];
    int64_t j = i - 1;
    while (j >= x && (CPA_C(1), Q[j] > v)) { Q[j + 1] = Q[j]; j--; }
    Q[j + 1] = v;
  }
}

/* Вставки с лимитом: 1, если участок досортирован, сделав не больше limit перемещений.
   При неудаче участок остаётся перестановкой исходного (корректность не страдает). */
static int cpa_partial_insertion_lim(uint64_t *Q, int64_t x, int64_t y, int64_t limit) {
  int64_t moved = 0;
  for (int64_t i = x + 1; i <= y; i++) {
    CPA_C(1);
    if (Q[i] < Q[i - 1]) {
      uint64_t v = Q[i];
      int64_t j = i;
      do { Q[j] = Q[j - 1]; j--; } while (j > x && (CPA_C(1), Q[j - 1] > v));
      Q[j] = v;
      moved += i - j;
      if (moved > limit) return 0;
    }
  }
  return 1;
}

/* Почти упорядоченный участок: разбиение переставило не больше CPA_FEW пар
   (подобрано: 1 — больше даёт лишние неудачные попытки на случайных перестановках).
   Тогда каждая половина пробует досортироваться вставками; лимит перемещений —
   её длина (при 0 перестановок — 8, как в pdqsort), так что неудачная попытка
   стоит O(длины) и бывает только на таких узлах. Возвращает биты: 1 — левая
   половина готова, 2 — правая. */
#ifndef CPA_FEW
#define CPA_FEW 1
#endif
static int cpa_try_presorted(uint64_t *Q, int64_t x, int64_t z, int64_t y, int64_t sw) {
  if (sw > CPA_FEW) return 0;
  int r = 0;
  if (z - 1 > x) { if (cpa_partial_insertion_lim(Q, x, z - 1, sw ? z - x : 8)) r |= 1; } else r |= 1;
  if (y > z) { if (cpa_partial_insertion_lim(Q, z, y, sw ? y - z + 1 : 8)) r |= 2; } else r |= 2;
  return r;
}

/* Блочное разбиение без ветвлений (схема BlockQuicksort в варианте pdqsort):
   * сначала с обоих концов ищутся первые элементы не на своей стороне; если
     указатели встретились, участок уже разбит, и об этом известно без перестановок;
   * затем блоки по CPA_BLOCK: смещения элементов не на своей стороне пишутся без
     ветвлений, пары меняются циклом (2·num + 2 записи) или обменами, если блоки
     равны (сохраняет зеркальность для убывающих данных);
   * конец обрабатывается блоками переменного размера — поэлементного хвоста нет.
   Возвращает z: [x, z) < key, [z, y] >= key; *nsw — число переставленных пар
   (0 — участок уже был разбит). */
static int64_t cpa_partition(uint64_t *Q, int64_t x, int64_t y, uint64_t key, int64_t *nsw) {
  uint64_t *first = Q + x, *last = Q + y + 1;               /* полуинтервал [first, last) */
  uint64_t *f0 = first, *l0 = last;
  while (first < last && *first < key) first++;
  while (first < last && !(*(last - 1) < key)) last--;
  CPA_C((first - f0) + (l0 - last) + 2);
  (void)f0; (void)l0;
  if (first == last) { *nsw = 0; return first - Q; }        /* уже разбит */
  --last;
  { uint64_t t = *first; *first = *last; *last = t; }       /* *last теперь >= key */
  ++first;
  int64_t swp = 1;                                           /* число переставленных пар */

  unsigned char offL[CPA_BLOCK], offR[CPA_BLOCK];
  uint64_t *baseL = first, *baseR = last;
  size_t nL = 0, nR = 0, sL = 0, sR = 0;
  while (first < last) {
    size_t unk = (size_t)(last - first);
    size_t lsplit = nL == 0 ? (nR == 0 ? unk / 2 : unk) : 0;
    size_t rsplit = nR == 0 ? (unk - lsplit) : 0;
    if (lsplit >= CPA_BLOCK) {
      CPA_C(CPA_BLOCK);
      for (size_t i = 0; i < CPA_BLOCK;) {
        offL[nL] = (unsigned char)i++; nL += !(*first < key); ++first;
        offL[nL] = (unsigned char)i++; nL += !(*first < key); ++first;
        offL[nL] = (unsigned char)i++; nL += !(*first < key); ++first;
        offL[nL] = (unsigned char)i++; nL += !(*first < key); ++first;
        offL[nL] = (unsigned char)i++; nL += !(*first < key); ++first;
        offL[nL] = (unsigned char)i++; nL += !(*first < key); ++first;
        offL[nL] = (unsigned char)i++; nL += !(*first < key); ++first;
        offL[nL] = (unsigned char)i++; nL += !(*first < key); ++first;
      }
    } else {
      CPA_C(lsplit);
      for (size_t i = 0; i < lsplit;) { offL[nL] = (unsigned char)i++; nL += !(*first < key); ++first; }
    }
    if (rsplit >= CPA_BLOCK) {
      CPA_C(CPA_BLOCK);
      for (size_t i = 0; i < CPA_BLOCK;) {
        offR[nR] = (unsigned char)++i; nR += (*--last < key);
        offR[nR] = (unsigned char)++i; nR += (*--last < key);
        offR[nR] = (unsigned char)++i; nR += (*--last < key);
        offR[nR] = (unsigned char)++i; nR += (*--last < key);
        offR[nR] = (unsigned char)++i; nR += (*--last < key);
        offR[nR] = (unsigned char)++i; nR += (*--last < key);
        offR[nR] = (unsigned char)++i; nR += (*--last < key);
        offR[nR] = (unsigned char)++i; nR += (*--last < key);
      }
    } else {
      CPA_C(rsplit);
      for (size_t i = 0; i < rsplit;) { offR[nR] = (unsigned char)++i; nR += (*--last < key); }
    }
    size_t num = nL < nR ? nL : nR;
    if (num) {
      const unsigned char *ol = offL + sL, *orr = offR + sR;
      if (nL == nR) {                                         /* обмены: зеркальность сохраняется */
        for (size_t i = 0; i < num; i++) {
          uint64_t *a = baseL + ol[i], *b = baseR - orr[i];
          uint64_t t = *a; *a = *b; *b = t;
        }
      } else {                                                /* циклом: 2·num + 2 записи */
        uint64_t *l = baseL + ol[0], *r = baseR - orr[0];
        uint64_t tmp = *l; *l = *r;
        for (size_t i = 1; i < num; i++) {
          l = baseL + ol[i]; *r = *l;
          r = baseR - orr[i]; *l = *r;
        }
        *r = tmp;
      }
    }
    swp += (int64_t)num;
    nL -= num; nR -= num; sL += num; sR += num;
    if (nL == 0) { sL = 0; baseL = first; }
    if (nR == 0) { sR = 0; baseR = last; }
  }
  /* оставшиеся смещения одной стороны — обменами к границе
     (обмен элемента с самим собой перестановкой не считается) */
  if (nL) {
    const unsigned char *ol = offL + sL;
    while (nL--) { uint64_t *a = baseL + ol[nL]; --last; swp += a != last; uint64_t t = *a; *a = *last; *last = t; }
    first = last;
  }
  if (nR) {
    const unsigned char *orr = offR + sR;
    while (nR--) { uint64_t *b = baseR - orr[nR]; swp += b != first; uint64_t t = *b; *b = *first; *first = t; ++first; }
  }
  *nsw = swp;
  return first - Q;
}


static void cpa_minmax(const uint64_t *Q, int64_t x, int64_t y, uint64_t *mn, uint64_t *mx) {
  uint64_t a = Q[x], b = Q[x];
  CPA_C(2 * (y - x));
  for (int64_t i = x + 1; i <= y; i++) { uint64_t v = Q[i]; a = v < a ? v : a; b = v > b ? v : b; }
  *mn = a; *mx = b;
}

/* первый элемент последовательности, построенной на лету для [lo, hi] при c = 2 */
static inline uint64_t cpa_bisect(uint64_t lo, uint64_t hi) { uint64_t w = hi - lo; return lo + (w - w / 2); }

/* ---------- cs_pdq_sort ---------- */
static inline uint64_t cpa_med3(uint64_t a, uint64_t b, uint64_t c) {
  uint64_t mn = a < b ? a : b, mx = a < b ? b : a;
  uint64_t m = mx < c ? mx : c;
  return mn > m ? mn : m;
}

static inline void cpa_swap(uint64_t *Q, int64_t a, int64_t b) { uint64_t t = Q[a]; Q[a] = Q[b]; Q[b] = t; }

/* После перекошенного разбиения переставляем несколько элементов, чтобы
   сломать закономерность, на которой споткнулась медиана (как в pdqsort). */
static void cpa_break_pattern(uint64_t *Q, int64_t x, int64_t y) {
  int64_t n = y - x + 1;
  if (n < CPA_INS) return;
  int64_t q = n / 4;
  cpa_swap(Q, x, x + q);
  cpa_swap(Q, y, y - q);
  if (n > 128) {
    cpa_swap(Q, x + 1, x + q + 1); cpa_swap(Q, x + 2, x + q + 2);
    cpa_swap(Q, y - 1, y - q - 1); cpa_swap(Q, y - 2, y - q - 2);
  }
}


/* Медиана s равномерно расставленных элементов (s <= 127, только для крупных узлов). */
static uint64_t cpa_sample_median(const uint64_t *Q, int64_t x, int64_t n, int s) {
  uint64_t buf[128];
  for (int i = 0; i < s; i++) buf[i] = Q[x + (int64_t)((double)i * (n - 1) / (s - 1))];
  for (int i = 1; i < s; i++) {
    uint64_t v = buf[i]; int j = i - 1;
    while (j >= 0 && (CPA_C(1), buf[j] > v)) { buf[j + 1] = buf[j]; j--; }
    buf[j + 1] = v;
  }
  return buf[s / 2];
}

/* ---------- таблица густоты ----------
   Таблица T[0..s) — отсортированные значения (выборка или квантили), описывающие густоту ключей.
   В режиме значений порог узла [lo, hi] — медиана массы по точкам таблицы внутри [lo, hi]:
   t = T[(ta + tb) / 2], где [ta, tb) — точки таблицы в [lo, hi]; меньше двух точек — деление
   пополам (c = 2). Индексы [ta, tb) передаются частям вместе с границами, так что каждый узел
   получает свою часть таблицы без поиска с начала. */
#ifndef CSS_TAB_FLAT
#define CSS_TAB_FLAT 1.6       /* выборка корня почти равномерна: KS·sqrt(s) меньше — не брать */
#endif
#ifndef CSS_KS_C
#define CSS_KS_C 2.25          /* таблица прошлого массива согласна с выборкой: KS·sqrt(s) не больше */
#endif
/* таблица прошлого массива (css_sort_tab); NULL — нет */
static const uint64_t *css_prev; static int64_t css_prev_n;
static inline int64_t cpa_tab_lb(const uint64_t *T, int64_t a, int64_t b, uint64_t v) {   /* первый T[i] >= v */
  while (a < b) { int64_t m = (a + b) >> 1; CPA_C(1); if (T[m] < v) a = m + 1; else b = m; }
  return a;
}
/* Выборка корня T[0..s) как таблица для [lo, hi]: брать, если она не почти равномерна
   (расстояние Колмогорова до равномерной на [lo, hi] больше CSS_TAB_FLAT / sqrt(s)). */
static int cpa_tab_useful(const uint64_t *T, int s, uint64_t lo, uint64_t hi) {
  if (s < 31 || hi <= lo) return 0;
  double W = (double)(hi - lo), D = 0;
  for (int i = 0; i < s; i++) {
    double u = (double)(T[i] - lo) / W, e = (i + 0.5) / s, d = fabs(u - e);
    if (d > D) D = d;
  }
  return D * sqrt((double)s) > CSS_TAB_FLAT;
}
/* Таблица прошлого массива P[0..S) против отсортированной выборки B[0..s) текущего сегмента:
   расстояние Колмогорова между функциями распределения (для серии равных — середина ступени).
   1 — таблица согласна с выборкой (D <= CSS_KS_C / sqrt(s)). */
static int css_tab_agrees(const uint64_t *P, int64_t S, const uint64_t *B, int s) {
  double D = 0;
  for (int i = 0; i < s;) {
    int j = i; while (j < s && B[j] == B[i]) j++;            /* серия [i, j) равных */
    int64_t a = cpa_tab_lb(P, 0, S, B[i]), b = B[i] == UINT64_MAX ? S : cpa_tab_lb(P, a, S, B[i] + 1);
    double F = (double)(a + b) / (2.0 * (double)S), e = (double)(i + j) / (2.0 * s);
    if (fabs(F - e) > D) D = fabs(F - e);
    i = j;
  }
  return D * sqrt((double)s) <= CSS_KS_C;
}

/* Таблица прошлого массива P[ta..tb) на [lo, hi]: не почти ли она равномерна. Точность таблицы
   высока, но выигрыш от неё ощутим, только если отклонение от равномерной заметно на масштабе
   выборки сегмента (s элементов): KS·sqrt(s) > CSS_TAB_FLAT; иначе деление пополам не хуже и
   дешевле (нет двоичного поиска в узлах). Проверяется не больше 256 точек таблицы. */
static int css_tab_far(const uint64_t *P, int64_t ta, int64_t tb, uint64_t lo, uint64_t hi, int s) {
  int64_t m = tb - ta;
  if (m < 2 || hi <= lo) return 0;
  int64_t step = m > 256 ? m / 256 : 1;
  double W = (double)(hi - lo), D = 0;
  for (int64_t i = 0; i < m; i += step) {
    double u = (double)(P[ta + i] - lo) / W, e = (i + 0.5) / (double)m, d = fabs(u - e);
    if (d > D) D = d;
  }
  return D * sqrt((double)s) > CSS_TAB_FLAT;
}

/* ---------- ядро: узел [x, y] с границами значений [lo, hi] ----------
   bad_allowed — запас перекосов в режиме выборки (вначале log2 n);
   vmode = 1 — режим значений (порог по таблице или делением пополам), 0 — режим выборки;
   T, [ta, tb) — таблица густоты и точки в [lo, hi] (T = NULL — без таблицы).
   Рекурсия — только в меньшую часть, большая обрабатывается в цикле. */
#ifndef CSS_TAB_MIN
#define CSS_TAB_MIN 1024       /* узлы короче таблицей не пользуются (делят пополам) */
#endif
#ifdef CSS_STATS               /* статистика проходов для анализа худшего случая */
int64_t css_st_lg0 = -1, css_st_badmax, css_st_badmin = 1 << 30, css_st_bisect, css_st_tryfail, css_st_good_v, css_st_good_s, css_st_eq, css_st_bad, css_st_deg;
#define CSS_ST(e) (e)
#else
#define CSS_ST(e) ((void)0)
#endif
#ifndef CSS_BUDGET_MAX
#define CSS_BUDGET_MAX 2       /* запас перекосов = min(floor(log2 n), CSS_BUDGET_MAX) */
#endif
#define CSS_BUDGET(lg) ((lg) < CSS_BUDGET_MAX ? (lg) : CSS_BUDGET_MAX)
#ifndef CSS_TRYK
#define CSS_TRYK 4             /* неудачных досортировок вставками на пути элемента не больше CSS_TRYK */
#endif
static void cs_pdq_rec_x(uint64_t *Q, int64_t x, int64_t y, uint64_t lo, uint64_t hi,
                         int bad_allowed, int vmode, const uint64_t *T, int64_t ta, int64_t tb, int tr);
static void cs_pdq_rec_t(uint64_t *Q, int64_t x, int64_t y, uint64_t lo, uint64_t hi,
                         int bad_allowed, int vmode, const uint64_t *T, int64_t ta, int64_t tb) {
  cs_pdq_rec_x(Q, x, y, lo, hi, bad_allowed, vmode, T, ta, tb, CSS_TRYK);
}
/* tr — сколько ещё неудачных досортировок вставками допускается на пути (0 — не пробовать) */
static void cs_pdq_rec_x(uint64_t *Q, int64_t x, int64_t y, uint64_t lo, uint64_t hi,
                         int bad_allowed, int vmode, const uint64_t *T, int64_t ta, int64_t tb, int tr) {
  for (;;) {
    int64_t n = y - x + 1;
    if (n < CPA_INS) { if (x < y) cpa_insertion(Q, x, y, 1); return; }
    if (lo >= hi) return;                                     /* все значения равны */
    if (n < CSS_TAB_MIN) T = NULL;                            /* малый узел: поиск по таблице дороже пользы */
    int bisect = !vmode && bad_allowed <= 0;                  /* запас перекосов исчерпан */


    /* порог: первый элемент порождающей последовательности узла */
    uint64_t t; int eq_run = 0;
    if (vmode && T && tb - ta >= 2) { t = T[(ta + tb) >> 1]; if (t <= lo) t = lo + 1; }   /* медиана массы по таблице */
    else if (vmode || bisect) t = cpa_bisect(lo, hi);          /* c = 2 */
    else {
      uint64_t p;                                              /* c по медиане выборки */
      if (n >= 4096) p = cpa_sample_median(Q, x, n, n >= 65536 ? 127 : 31);
      else {
        int64_t m = x + n / 2;
        CPA_C(n > 128 ? 12 : 3);
        p = n > 128
          ? cpa_med3(cpa_med3(Q[x], Q[m], Q[y]), cpa_med3(Q[x + 1], Q[m - 1], Q[y - 1]), cpa_med3(Q[x + 2], Q[m + 1], Q[y - 2]))
          : cpa_med3(Q[x], Q[m], Q[y]);
      }
      if (p <= lo) { t = lo + 1; eq_run = 1; }                  /* b = 1: серия, равная lo */
      else t = p;
    }

    int64_t sw, z = cpa_partition(Q, x, y, t, &sw);
    int64_t nl = z - x, nr = y - z + 1;

    if (eq_run) {                                              /* [x, z) — все равны lo, готово */
      /* серия, убравшая меньше n/8 элементов, считается перекосом: иначе цепочка
         значений lo, lo+1, lo+2, ..., поставленных в позиции выборки, даёт O(n^2) */
      if (8 * nl < n) { bad_allowed--; CSS_ST(css_st_bad += n); } else CSS_ST(css_st_eq += n);
      CSS_ST(css_st_lg0 >= 0 && css_st_lg0 - bad_allowed > css_st_badmax ? (css_st_badmax = css_st_lg0 - bad_allowed) : 0);
      x = z; lo = t; continue;
    }
    CSS_ST(bisect ? (css_st_bisect += n) : 0);
    if (nl == 0 || nr == 0) {                                  /* вырожденный проход */
      CSS_ST(css_st_deg += n);
      if (vmode || bisect) {
        cpa_minmax(Q, x, y, &lo, &hi);                         /* новые точные границы */
        vmode = 0;            /* пустая сторона — значения неравномерны (пустоты, кластеры) */
      }
      else {                  /* по медиане пусто только слева: p — минимум, дальше серия равных; */
        lo = t;               /* проход потрачен впустую и расходует запас перекосов */
        bad_allowed--;
        CSS_ST(css_st_lg0 >= 0 && css_st_lg0 - bad_allowed > css_st_badmax ? (css_st_badmax = css_st_lg0 - bad_allowed) : 0);
      }
      continue;
    }
    int64_t tm = vmode && T && tb > ta ? cpa_tab_lb(T, ta, tb, t) : ta;   /* точки таблицы < t */
    int64_t q = nl < nr ? nl : nr;
    if (vmode && 10 * q < n) vmode = 0;     /* сильный перекос — поддерево переходит на выборку */
    int bad = 8 * q < n;
    int done = 0, trL = tr, trR = tr;
    CSS_ST(bisect ? 0 : (!vmode && bad) ? (css_st_bad += n) : vmode ? (css_st_good_v += n) : (css_st_good_s += n));
    if (!vmode && bad && !bisect) {
      bad_allowed--;
      CSS_ST(css_st_lg0 >= 0 && css_st_lg0 - bad_allowed > css_st_badmax ? (css_st_badmax = css_st_lg0 - bad_allowed) : 0);
      cpa_break_pattern(Q, x, z - 1);
      cpa_break_pattern(Q, z, y);
    } else if (!bisect && tr > 0) {
      done = cpa_try_presorted(Q, x, z, y, sw);
      if (sw <= CPA_FEW) { if (!(done & 1)) { trL--; CSS_ST(css_st_tryfail += z - x); } if (!(done & 2)) { trR--; CSS_ST(css_st_tryfail += y - z + 1); } }   /* неудача расходует запас части */
    }
    if (done == 3) return;
    if (done == 1) { x = z; lo = t; ta = tm; tr = trR; continue; }               /* левая готова */
    if (done == 2) { y = z - 1; hi = t - 1; tb = tm; tr = trL; continue; }       /* правая готова */
    if (nl < nr) { cs_pdq_rec_x(Q, x, z - 1, lo, t - 1, bad_allowed, vmode, T, ta, tm, trL); x = z; lo = t; ta = tm; tr = trR; }
    else { cs_pdq_rec_x(Q, z, y, t, hi, bad_allowed, vmode, T, tm, tb, trR); y = z - 1; hi = t - 1; tb = tm; tr = trL; }
  }
}
static void cs_pdq_rec(uint64_t *Q, int64_t x, int64_t y, uint64_t lo, uint64_t hi, int bad_allowed, int vmode) {
  cs_pdq_rec_t(Q, x, y, lo, hi, bad_allowed, vmode, NULL, 0, 0);
}


/* Мало различных значений: в выборке из s элементов различных не больше s/8.
   Тогда режим значений тратит проходы на пустые промежутки между
   значениями, и сортировка сразу начинается в режиме выборки с сериями равных. */

/* то же, но отсортированная выборка остаётся в buf (s элементов; s = 0 при n < 4096) */
static int cpa_few_distinct_buf(const uint64_t *Q, int64_t n, uint64_t *buf, int *ps) {
  *ps = 0;
  if (n < 4096) return 0;
  int s = n >= 65536 ? 127 : 31;
  *ps = s;
  int64_t step = n / s;                           /* псевдослучайный сдвиг внутри шага: */
  for (int i = 0; i < s; i++) {                   /* периодический вход не совпадёт с шагом */
    uint64_t h = (uint64_t)(i + 1) * 0x9e3779b97f4a7c15ULL;
    buf[i] = Q[(int64_t)i * step + (int64_t)((h >> 33) % (uint64_t)step)];
  }
  for (int i = 1; i < s; i++) {
    uint64_t v = buf[i]; int j = i - 1;
    while (j >= 0 && (CPA_C(1), buf[j] > v)) { buf[j + 1] = buf[j]; j--; }
    buf[j + 1] = v;
  }
  int d = 1;
  for (int i = 1; i < s; i++) d += buf[i] != buf[i - 1];
  return d <= s / 8;
}

/* корень ядра на [x, x + n) для малых сегментов: «мало различных», таблица — выборка корня, ядро */
static void cpa_root_rec(uint64_t *Q, int64_t x, int64_t n, uint64_t lo, uint64_t hi, int lg) {
  uint64_t buf[128]; int s;
  int few = cpa_few_distinct_buf(Q + x, n, buf, &s);
  CSS_ST(css_st_lg0 = CSS_BUDGET(lg));
  if (!few && cpa_tab_useful(buf, s, lo, hi)) {
    int64_t ta = cpa_tab_lb(buf, 0, s, lo), tb = hi == UINT64_MAX ? s : cpa_tab_lb(buf, 0, s, hi + 1);
    cs_pdq_rec_t(Q, x, x + n - 1, lo, hi, CSS_BUDGET(lg), 1, buf, ta, tb);
  } else cs_pdq_rec(Q, x, x + n - 1, lo, hi, CSS_BUDGET(lg), !few);
}

/* ============================================================================
   3. Слой упорядоченных участков
   ============================================================================ */

/*==== cs_pdq + упорядоченные участки (runs) ====

  Надстройка над ядром cs_pdq для частично упорядоченных входов.
  Ядро не меняется; на входах без длинных участков путь тот же, что у cs_pdq.

  Корень:
  1. Первый монотонный участок ищется проходом, проверяющим по 16 пар за раз.
     Весь массив неубывающий — готово; невозрастающий — разворот.
  2. Стоит ли искать участки: первый участок не короче n/8 или в выборке из
     CPR_NWIN окон по CPR_WIN пар монотонных окон не меньше четверти (окно бросается,
     как только в нём есть и подъём, и спуск). Затем по выборке из 15–127 элементов оценивается энтропия
     значений H: ядро тратит на элемент около CQ0 + CQ·H, это база для сравнения.
     При H < CPR_HMIN (много повторов) ищутся только чистые участки, без выбросов:
     «участок», который держится на множестве выбросов среди повторов, дороже ядра;
     кроме того, по 8 окнам по 32 пары оценивается доля спусков, т.е. число участков k,
     и если слияние k участков (CM·n·log2 k) заведомо дороже ядра, разметка не начинается.
     Позиции выборок — умножением со сдвигом, без деления.
  3. Разметка — точный проход по участкам:
     * невозрастающий участок берётся как есть (длинный — разворачивается);
     * неубывающий продолжается через одиночные выбросы: если Q[r] рвёт порядок,
       но следующий элемент его продолжает («провал»), Q[r] — выброс; если Q[r]
       продолжает порядок без последнего взятого («пик»), выброс — последний взятый.
       Выбросы копятся в промежутке между концом сжатого участка и точкой чтения
       (каждый взятый элемент меняется местами с первым выбросом), так что
       дополнительной памяти не нужно. Выбросов не больше 1/CPR_XFRAC длины участка;
     * участок длиной >= R (R = max(CPR_RFLOOR, n / (CPR_SEGMAX/2 - 2))) — отсортированный
       сегмент, его выбросы и короткие участки — неупорядоченные сегменты.
  4. Короткий отсортированный сегмент между неупорядоченными поглощается ими,
     если он меньше их суммы в CPR_K раз.
  5. Модель стоимости: «досортировать неупорядоченные сегменты ядром, сжать участки
     с выбросами, развернуть убывающие и слить всё по дереву powersort» против
     «отсортировать весь массив ядром» (CQ0 + CQ·H на элемент); участки берутся, только
     если план дешевле с запасом CPR_MARGIN.
  6. Слияние соседних сегментов в порядке powersort (стек O(log n)).

  Слияние [a, m) и [m, b):
  * Q[m-1] <= Q[m] — ничего; иначе обрезка двоичным поиском (элементы левой
    <= min правой и элементы правой >= max левой уже на месте);
  * меньшая часть в CPR_SKEW раз меньше — галоп с memmove блоков;
  * весь участок помещается в буфер — копия и слияние обратно четырьмя
    независимыми цепочками без ветвлений (разбиение по ранжированию пополам,
    каждая половина — с обоих концов): цепочки не ждут друг друга;
  * в буфер помещается только меньшая часть — одна цепочка без ветвлений;
  * меньшая часть больше буфера (только на месте): по оценке стоимости либо
    кусками по CPR_CAP с поворотом остатка, либо разбиение по медиане большей
    части с поворотом (обмен блоков) и рекурсией в меньшую подзадачу.
  Неупорядоченные сегменты (и весь массив, если участки не взяты) сортирует корень ядра
  cpd_sort. Буфер слияния — стековый, CPR_CAP элементов (heap = 0), или n
  элементов в куче (heap = 1, как у fluxsort; при неудаче malloc — стековый). */


#ifndef CPR_CAP
#define CPR_CAP 4096           /* стековый буфер слияния, элементов (32 КБ стека) */
#endif
#ifndef CPR_SEGMAX
#define CPR_SEGMAX 1024        /* таблица сегментов */
#endif
#ifndef CPR_RFLOOR
#define CPR_RFLOOR 64          /* минимальная длина отсортированного сегмента */
#endif
#ifndef CPR_K
#define CPR_K 16               /* поглощение коротких сегментов */
#endif
#ifndef CPR_SKEW
#define CPR_SKEW 16            /* перекос, с которого слияние — галопом */
#endif
#ifndef CPR_LEAFSPLIT
#define CPR_LEAFSPLIT 8        /* лист на месте: разбивать, пока большая часть <= 8 меньших */
#endif
#ifndef CPR_XFRAC
#define CPR_XFRAC 8            /* выбросов не больше 1/CPR_XFRAC длины участка */
#endif
#ifndef CPR_XMIN
#define CPR_XMIN 32            /* выбросы — только у участков не короче */
#endif
#ifndef CPR_WIN
#define CPR_WIN 64
#endif
#ifndef CPR_NWIN
#define CPR_NWIN 32
#endif
#ifndef CPR_MIN_N
#define CPR_MIN_N 256
#endif
/* модель стоимости, нс на элемент (откалибрована на этой машине, случайные данные):
   ядро         CQ0 + CQ·log2(u);
   слияние      CM при T <= 16384, далее + CMS·log2(T/16384) (выход из кэша);
   на месте, если меньшая часть больше буфера, ещё CR0 + CR·(перемещения поворотами
   на элемент): min(кусками, разбиением) / T. */
#ifndef CPR_HMIN
#define CPR_HMIN 3.0           /* при энтропии значений ниже — только чистые участки, без выбросов */
#endif
#ifndef CPR_FIXED
#define CPR_FIXED 100.0        /* постоянные накладные расходы режима участков, нс */
#endif
#ifndef CPR_GATE_N
#define CPR_GATE_N 1024        /* оценка энтропии и доли спусков — с этого n */
#endif
#ifndef CPR_SAMPLE_N
#define CPR_SAMPLE_N 512       /* выборочная проверка — с этого n */
#endif
#ifndef CPR_CX
#define CPR_CX 1.4             /* сжатие участка с выбросами, нс на элемент */
#endif
#ifndef CPR_CV
#define CPR_CV 0.4             /* разворот, нс на элемент */
#endif
#ifndef CPR_MARGIN
#define CPR_MARGIN 0.9
#endif
#ifndef CPR_CQ0
#define CPR_CQ0 1.5
#endif
#ifndef CPR_CQ
#define CPR_CQ 1.15
#endif
#ifndef CPR_CM
#define CPR_CM 1.1
#endif
#ifndef CPR_CMS
#define CPR_CMS 0.25
#endif
#ifndef CPR_CR0
#define CPR_CR0 1.3
#endif
#ifndef CPR_CR
#define CPR_CR 0.27
#endif

typedef struct {
  uint64_t stack[CPR_CAP];
  uint64_t *buf; int64_t cap;            /* рабочий буфер: stack или куча */
} cpr_ctx;

/* ---------- поиск ---------- */
static inline int64_t cpr_upper(const uint64_t *Q, int64_t lo, int64_t hi, uint64_t v) {  /* первый > v */
  if (lo >= hi) return lo;
  const uint64_t *b = Q + lo; int64_t len = hi - lo;
  while (len > 1) { int64_t h = len / 2; b = (b[h] <= v) ? b + h : b; len -= h; }
  return (b - Q) + (*b <= v);
}
static inline int64_t cpr_lower(const uint64_t *Q, int64_t lo, int64_t hi, uint64_t v) {  /* первый >= v */
  if (lo >= hi) return lo;
  const uint64_t *b = Q + lo; int64_t len = hi - lo;
  while (len > 1) { int64_t h = len / 2; b = (b[h] < v) ? b + h : b; len -= h; }
  return (b - Q) + (*b < v);
}
/* галоп от правого края: первый индекс в [lo, hi) с Q[i] > v */
static inline int64_t cpr_gallop_upper_back(const uint64_t *Q, int64_t lo, int64_t hi, uint64_t v) {
  int64_t j = hi, step = 1;
  while (j > lo) {
    int64_t t = j - step; if (t < lo) t = lo;
    if (Q[t] > v) { j = t; step <<= 1; }
    else return cpr_upper(Q, t + 1, j, v);
  }
  return lo;
}
/* галоп от левого края: первый индекс в [lo, hi) с Q[i] >= v */
static inline int64_t cpr_gallop_lower_fwd(const uint64_t *Q, int64_t lo, int64_t hi, uint64_t v) {
  int64_t j = lo, step = 1;
  while (j < hi) {
    int64_t t = j + step - 1; if (t >= hi) t = hi - 1;
    if (Q[t] < v) { j = t + 1; step <<= 1; }
    else return cpr_lower(Q, j, t, v);
  }
  return hi;
}

/* ---------- монотонные участки ---------- */
static int64_t cpr_asc_end(const uint64_t *Q, int64_t i, int64_t n) {    /* [i, j) неубывающий, максимальный */
  int64_t j = i + 1;
  while (j + 16 <= n) {
    const uint64_t *p = Q + j - 1;
    int bad = (p[0] > p[1]) | (p[1] > p[2]) | (p[2] > p[3]) | (p[3] > p[4]) |
              (p[4] > p[5]) | (p[5] > p[6]) | (p[6] > p[7]) | (p[7] > p[8]) |
              (p[8] > p[9]) | (p[9] > p[10]) | (p[10] > p[11]) | (p[11] > p[12]) |
              (p[12] > p[13]) | (p[13] > p[14]) | (p[14] > p[15]) | (p[15] > p[16]);
    if (bad) break;
    j += 16;
  }
  while (j < n && Q[j - 1] <= Q[j]) j++;
  CPA_C(j - i);
  return j;
}
static int64_t cpr_desc_end(const uint64_t *Q, int64_t i, int64_t n) {   /* [i, j) невозрастающий */
  int64_t j = i + 1;
  while (j + 16 <= n) {
    const uint64_t *p = Q + j - 1;
    int bad = (p[0] < p[1]) | (p[1] < p[2]) | (p[2] < p[3]) | (p[3] < p[4]) |
              (p[4] < p[5]) | (p[5] < p[6]) | (p[6] < p[7]) | (p[7] < p[8]) |
              (p[8] < p[9]) | (p[9] < p[10]) | (p[10] < p[11]) | (p[11] < p[12]) |
              (p[12] < p[13]) | (p[13] < p[14]) | (p[14] < p[15]) | (p[15] < p[16]);
    if (bad) break;
    j += 16;
  }
  while (j < n && Q[j - 1] >= Q[j]) j++;
  CPA_C(j - i);
  return j;
}
static inline int64_t cpr_run_end(const uint64_t *Q, int64_t i, int64_t n, int *desc) {
  if (i + 1 >= n) { *desc = 0; return n; }
  if (Q[i] <= Q[i + 1]) { *desc = 0; return cpr_asc_end(Q, i, n); }
  *desc = 1; return cpr_desc_end(Q, i, n);
}
static void cpr_reverse(uint64_t *Q, int64_t a, int64_t b) {
  for (int64_t l = a, r = b - 1; l < r; l++, r--) { uint64_t t = Q[l]; Q[l] = Q[r]; Q[r] = t; }
}

/* Неубывающий участок от s через выбросы; [s, r0) уже известен как неубывающий
   (r0 > s). Возвращает r: [s, *wout) — сжатый неубывающий участок, [*wout, r) —
   выбросы. Разрыв Q[r] < last чинится локально: убрать p последних взятых и
   пропустить d следующих, p + d <= CPR_XMAX, так чтобы Q[r+d] продолжал участок
   (Q[r+d] >= p-го с конца взятого). Из подходящих — с меньшим p + d, при равенстве —
   с меньшим Q[r+d] (меньший «последний» оставляет больше возможностей, как в жадном
   поиске возрастающей подпоследовательности). Решения зависят только от последних
   четырёх взятых значений (в регистрах) и от ещё не прочитанных Q[r..r+3], поэтому
   проход без записи (dry = 1) даёт ту же разметку, что и проход с записью.
   При записи выбросы живут в промежутке [w, r): взятый элемент меняется местами
   с первым выбросом; убранные взятые и пропущенные просто остаются в промежутке. */
#ifndef CPR_XMAX
#define CPR_XMAX 3
#endif
static inline int64_t cpr_asc_run_x(uint64_t *Q, int64_t s, int64_t r0, int64_t n, int64_t *wout, const int dry) {
  int64_t r = r0, w = r0;
  if (r >= n || r0 - s < CPR_XMIN) { *wout = w; return r; }
  uint64_t L0 = Q[r0 - 1], L1 = Q[r0 - 2], L2 = Q[r0 - 3], L3 = Q[r0 - 4];
  int kn = 4;                                        /* сколько из L0..L3 известно */
  while (r < n) {                                    /* Q[r] < L0: разрыв */
    int64_t len = w - s, no = r - w;
    if (len < CPR_XMIN) break;
    int bp = -1, bd = 0; uint64_t bv = 0;
    for (int t = 1; t <= CPR_XMAX && bp < 0; t++) {
      if ((no + t) * CPR_XFRAC > len - t) break;
      for (int p = t; p >= 0; p--) {
        int d = t - p;
        if (p >= kn || r + d >= n) continue;
        uint64_t Lp = p == 0 ? L0 : p == 1 ? L1 : p == 2 ? L2 : L3;
        uint64_t q = Q[r + d];
        CPA_C(1);
        if (q >= Lp && (bp < 0 || q < bv)) { bp = p; bd = d; bv = q; }
      }
    }
    if (bp < 0) break;
    w -= bp; r += bd; kn -= bp;
    if (bp == 1) { L0 = L1; L1 = L2; L2 = L3; }
    else if (bp == 2) { L0 = L2; L1 = L3; }
    else if (bp == 3) { L0 = L3; }
    /* продолжение участка: Q[r] >= L0 по выбору */
    while (r < n) {
      uint64_t x = Q[r];
      CPA_C(1);
      if (x < L0) break;
      if (!dry) { Q[r] = Q[w]; Q[w] = x; }
      w++; r++; L3 = L2; L2 = L1; L1 = L0; L0 = x; kn += kn < 4;
    }
  }
  *wout = w;
  return r;
}

static int cpr_sample_monotone(const uint64_t *Q, int64_t n) {
  int W = n < 8192 ? 16 : CPR_WIN, nw = n < 8192 ? 8 : CPR_NWIN;
  if ((int64_t)nw * 2 * (W + 1) > n) nw = (int)(n / (2 * (W + 1)));
  if (nw < 4) return 0;
  int64_t step = n / nw, slack = step - (W + 1);
  int good = 0;
  for (int w = 0; w < nw; w++) {
    uint64_t h = (uint64_t)(w + 1) * 0x9e3779b97f4a7c15ULL;
    const uint64_t *p = Q + (int64_t)w * step + (int64_t)(((unsigned __int128)(h >> 32) * (uint64_t)slack) >> 32);
    int d = 0, u = 0;                                /* число спусков / подъёмов */
    for (int i = 0; i < W; i += 8) {                 /* по 8 пар, выход, когда есть оба */
      for (int q = i; q < i + 8; q++) { d += p[q] > p[q + 1]; u += p[q] < p[q + 1]; }
      CPA_C(16);
      if (d && u) break;
    }
    good += (d == 0 && u * 4 >= W) || (u == 0 && d * 4 >= W);
  }
  return good * 4 >= nw;
}

/* Оценка энтропии распределения значений, бит на элемент, по выборке из s элементов
   (псевдослучайные позиции внутри равных шагов): группа из c >= 2 равных даёт
   (c/s)·log2(s/c), одиночки считаются частью «сплошной» массы с различными значениями:
   f1·log2(n). Ядро cs_pdq тратит на элемент примерно CQ0 + CQ·H (серии равных
   отделяются за проход), поэтому H — оценка его стоимости на этих данных. */
static double cpr_entropy(const uint64_t *Q, int64_t n) {
  int s = n >= 1048576 ? 127 : n >= 65536 ? 63 : n >= 4096 ? 31 : 15;
  uint64_t buf[128];
  int64_t step = n / s;
  for (int i = 0; i < s; i++) {
    uint64_t h = (uint64_t)(i + 1) * 0x9e3779b97f4a7c15ULL;
    buf[i] = Q[(int64_t)i * step + (int64_t)(((unsigned __int128)(h >> 32) * (uint64_t)step) >> 32)];
  }
  for (int i = 1; i < s; i++) {
    uint64_t v = buf[i]; int j = i - 1;
    while (j >= 0 && (CPA_C(1), buf[j] > v)) { buf[j + 1] = buf[j]; j--; }
    buf[j + 1] = v;
  }
  double H = 0; int single = 0;
  for (int i = 0; i < s;) {
    int j = i + 1;
    while (j < s && buf[j] == buf[i]) j++;
    if (j - i == 1) single++;
    else H += (double)(j - i) / s * log2((double)s / (j - i));
    i = j;
  }
  return H + (double)single / s * log2((double)n);
}

/* Доля спусков (Q[i] > Q[i+1]) в выборке: 8 окон по 32 пары (хватает, чтобы отличить
   «несколько участков» от «тысяч»). */
static double cpr_descent_rate(const uint64_t *Q, int64_t n) {
  int nw = 8, W = 32;
  if ((int64_t)nw * (W + 1) > n) nw = (int)(n / (W + 1));
  if (nw < 1) return 0;
  int64_t step = n / nw, slack = step - (W + 1);
  if (slack < 1) slack = 1;
  int d = 0;
  for (int w = 0; w < nw; w++) {
    uint64_t h = (uint64_t)(w + 7) * 0x9e3779b97f4a7c15ULL;
    const uint64_t *p = Q + (int64_t)w * step + (int64_t)(((unsigned __int128)(h >> 32) * (uint64_t)slack) >> 32);
    for (int i = 0; i < W; i++) d += p[i] > p[i + 1];
    CPA_C(W);
  }
  return (double)d / (nw * W);
}

/* ---------- повороты ---------- */
static void cpr_swap_ranges(uint64_t *a, uint64_t *b, int64_t k) {
  for (int64_t i = 0; i < k; i++) { uint64_t t = a[i]; a[i] = b[i]; b[i] = t; }
}
/* [a, m) [m, b) -> [m, b) [a, m); малые повороты через буфер, большие — обмен блоков */
static void cpr_rotate(uint64_t *Q, int64_t a, int64_t m, int64_t b, uint64_t *buf, int64_t cap) {
  int64_t la = m - a, lb = b - m;
  while (la && lb) {
    if (la <= lb) {
      if (la <= cap) { memcpy(buf, Q + a, la * 8); memmove(Q + a, Q + m, lb * 8); memcpy(Q + a + lb, buf, la * 8); return; }
      cpr_swap_ranges(Q + a, Q + b - la, la);          /* [X Y1 Y2] -> [Y2 Y1 X] */
      b -= la; lb -= la;
    } else {
      if (lb <= cap) { memcpy(buf, Q + m, lb * 8); memmove(Q + a + lb, Q + a, la * 8); memcpy(Q + a, buf, lb * 8); return; }
      cpr_swap_ranges(Q + a, Q + m, lb);               /* [X1 X2 Y] -> [Y X2 X1] */
      a += lb; la -= lb;
    }
  }
}

/* ---------- ядра слияния ----------
   Во всех ядрах после обрезки: max левой > max правой, min левой > min правой. */

/* одна цепочка, левая [a, m) в буфере: правая кончается раньше */
static void cpr_merge_lo(uint64_t *Q, int64_t a, int64_t m, int64_t b, uint64_t *buf) {
  int64_t la = m - a;
  memcpy(buf, Q + a, la * 8);
  int64_t o = a, l = 0, r = m;
  CPA_C(la + (b - m));
  while (r < b) { uint64_t x = buf[l], y = Q[r]; int t = y < x; Q[o++] = t ? y : x; r += t; l += !t; }
  memcpy(Q + o, buf + l, (la - l) * 8);
}
/* одна цепочка, правая [m, b) в буфере: левая кончается раньше */
static void cpr_merge_hi(uint64_t *Q, int64_t a, int64_t m, int64_t b, uint64_t *buf) {
  int64_t lb = b - m;
  memcpy(buf, Q + m, lb * 8);
  int64_t o = b - 1, l = m - 1, r = lb - 1;
  CPA_C((m - a) + lb);
  while (l >= a) { uint64_t x = Q[l], y = buf[r]; int t = x > y; Q[o--] = t ? x : y; l -= t; r -= !t; }
  memcpy(Q + a, buf, (r + 1) * 8);
}
/* галоп: левая [a, m) мала, в буфере */
static void cpr_gallop_lo(uint64_t *Q, int64_t a, int64_t m, int64_t b, uint64_t *buf) {
  int64_t la = m - a, o = a, r = m;
  memcpy(buf, Q + a, la * 8);
  for (int64_t k = 0; k < la; k++) {
    uint64_t v = buf[k];
    int64_t q = cpr_gallop_lower_fwd(Q, r, b, v);    /* правые < v идут перед v */
    memmove(Q + o, Q + r, (q - r) * 8); o += q - r; r = q;
    Q[o++] = v;
  }
}
/* галоп: правая [m, b) мала, в буфере */
static void cpr_gallop_hi(uint64_t *Q, int64_t a, int64_t m, int64_t b, uint64_t *buf) {
  int64_t lb = b - m, lend = m;
  memcpy(buf, Q + m, lb * 8);
  for (int64_t k = lb - 1; k >= 0; k--) {
    uint64_t v = buf[k];
    int64_t p = cpr_gallop_upper_back(Q, a, lend, v);  /* левые > v идут после v */
    memmove(Q + p + k + 1, Q + p, (lend - p) * 8);
    Q[p + k] = v; lend = p;
  }
}

/* сколько элементов A среди первых K выхода слияния A и B */
static int64_t cpr_corank(const uint64_t *A, int64_t la, const uint64_t *B, int64_t lb, int64_t K) {
  int64_t lo = K > lb ? K - lb : 0, hi = K < la ? K : la;
  while (lo < hi) {
    int64_t i = (lo + hi) / 2, j = K - i;
    if (B[j - 1] > A[i]) lo = i + 1; else hi = i;   /* j >= 1 и i < la, т.к. i < hi <= min(K, la) */
  }
  return lo;
}

/* Слияние A[0..la) и B[0..lb) в D (не пересекается с A, B) четырьмя
   независимыми цепочками: разбиение по ранжированию пополам, каждая половина
   сливается одновременно спереди и сзади. Счёт шагов без проверки границ
   берётся как минимум запасов всех цепочек; хвосты — по одной цепочке. */
#define CPR_FIN_F(A_, a_, B_, b_, D_, l_, r_, o_, c_) \
  while (c_ > 0) { if (l_ == a_) { memcpy(D_ + o_, B_ + r_, c_ * 8); c_ = 0; } \
    else if (r_ == b_) { memcpy(D_ + o_, A_ + l_, c_ * 8); c_ = 0; } \
    else { uint64_t x = A_[l_], y = B_[r_]; int t = y < x; D_[o_++] = t ? y : x; r_ += t; l_ += !t; c_--; } }
#define CPR_FIN_G(A_, B_, D_, le_, re_, ob_, c_) \
  while (c_ > 0) { if (le_ < 0) { memcpy(D_ + ob_ - c_ + 1, B_ + re_ - c_ + 1, c_ * 8); c_ = 0; } \
    else if (re_ < 0) { memcpy(D_ + ob_ - c_ + 1, A_ + le_ - c_ + 1, c_ * 8); c_ = 0; } \
    else { uint64_t x = A_[le_], y = B_[re_]; int u = x > y; D_[ob_--] = u ? x : y; le_ -= u; re_ -= !u; c_--; } }
static void cpr_merge4(const uint64_t *A, int64_t la, const uint64_t *B, int64_t lb, uint64_t *D) {
  int64_t T = la + lb, K = T / 2;
  CPA_C(T);
  int64_t i = cpr_corank(A, la, B, lb, K), j = K - i;
  const uint64_t *A1 = A, *B1 = B, *A2 = A + i, *B2 = B + j;
  int64_t a1 = i, b1 = j, a2 = la - i, b2 = lb - j;
  uint64_t *D1 = D, *D2 = D + K;
  int64_t T1 = K, T2 = T - K;
  int64_t cf1 = T1 / 2, cg1 = T1 - T1 / 2, cf2 = T2 / 2, cg2 = T2 - T2 / 2;
  int64_t l1 = 0, r1 = 0, o1 = 0, le1 = a1 - 1, re1 = b1 - 1, ob1 = T1 - 1;
  int64_t l2 = 0, r2 = 0, o2 = 0, le2 = a2 - 1, re2 = b2 - 1, ob2 = T2 - 1;
  for (;;) {
    int64_t s = cf1;
    if (a1 - l1 < s) s = a1 - l1;
    if (b1 - r1 < s) s = b1 - r1;
    if (cg1 < s) s = cg1;
    if (le1 + 1 < s) s = le1 + 1;
    if (re1 + 1 < s) s = re1 + 1;
    if (cf2 < s) s = cf2;
    if (a2 - l2 < s) s = a2 - l2;
    if (b2 - r2 < s) s = b2 - r2;
    if (cg2 < s) s = cg2;
    if (le2 + 1 < s) s = le2 + 1;
    if (re2 + 1 < s) s = re2 + 1;
    if (s <= 0) break;
    cf1 -= s; cg1 -= s; cf2 -= s; cg2 -= s;
    do {
      { uint64_t x = A1[l1], y = B1[r1]; int t = y < x; D1[o1++] = t ? y : x; r1 += t; l1 += !t; }
      { uint64_t x = A1[le1], y = B1[re1]; int u = x > y; D1[ob1--] = u ? x : y; le1 -= u; re1 -= !u; }
      { uint64_t x = A2[l2], y = B2[r2]; int t = y < x; D2[o2++] = t ? y : x; r2 += t; l2 += !t; }
      { uint64_t x = A2[le2], y = B2[re2]; int u = x > y; D2[ob2--] = u ? x : y; le2 -= u; re2 -= !u; }
    } while (--s);
  }
  CPR_FIN_F(A1, a1, B1, b1, D1, l1, r1, o1, cf1);
  CPR_FIN_G(A1, B1, D1, le1, re1, ob1, cg1);
  CPR_FIN_F(A2, a2, B2, b2, D2, l2, r2, o2, cf2);
  CPR_FIN_G(A2, B2, D2, le2, re2, ob2, cg2);
}

/* слияние кусками для варианта на месте: правая [m, b) мала, но больше буфера.
   Берётся старший кусок правой (до cap), левые элементы больше его минимума
   поворотом переносятся за остаток правой, кусок сливается с ними из буфера. */
static void cpr_merge_chunks_hi(cpr_ctx *c, uint64_t *Q, int64_t a, int64_t m, int64_t b) {
  while (m < b && a < m) {
    int64_t lb = b - m, k = lb < c->cap ? lb : c->cap;
    int64_t cs = b - k;                                /* кусок [cs, b) */
    int64_t p = cpr_upper(Q, a, m, Q[cs]);             /* левые > min куска: [p, m) */
    cpr_rotate(Q, p, m, cs, c->buf, c->cap);           /* [a,p) [m,cs) [p,m) [cs,b) */
    int64_t np = p + (cs - m);                         /* левые > min куска теперь в [np, cs) */
    if (np < cs && Q[cs - 1] > Q[cs]) {
      int64_t la2 = cs - np;
      if (k * CPR_SKEW < la2) cpr_gallop_hi(Q, np, cs, b, c->buf);
      else cpr_merge_hi(Q, np, cs, b, c->buf);
    }
    m = p; b = np;                                     /* дальше: [a, p) и [p, np) */
  }
}
/* зеркально: левая [a, m) мала, но больше буфера */
static void cpr_merge_chunks_lo(cpr_ctx *c, uint64_t *Q, int64_t a, int64_t m, int64_t b) {
  while (a < m && m < b) {
    int64_t la = m - a, k = la < c->cap ? la : c->cap;
    int64_t ce = a + k;                                /* кусок [a, ce) */
    int64_t q = cpr_lower(Q, m, b, Q[ce - 1]);         /* правые < max куска: [m, q) */
    cpr_rotate(Q, ce, m, q, c->buf, c->cap);           /* [a,ce) [m,q) [ce,m) [q,b) */
    int64_t nq = ce + (q - m);                         /* правые < max куска теперь в [ce, nq) */
    if (ce < nq && Q[ce - 1] > Q[ce]) {
      int64_t lr = nq - ce;
      if (k * CPR_SKEW < lr) cpr_gallop_lo(Q, a, ce, nq, c->buf);
      else cpr_merge_lo(Q, a, ce, nq, c->buf);
    }
    a = nq; m = q;                                     /* дальше: [nq, q) и [q, b) */
  }
}

static void cpr_merge(cpr_ctx *c, uint64_t *Q, int64_t a, int64_t m, int64_t b) {
  for (;;) {
    if (a >= m || m >= b) return;
    CPA_C(1);
    if (Q[m - 1] <= Q[m]) return;
    a = cpr_upper(Q, a, m, Q[m]);                      /* левые <= min правой — на месте */
    b = cpr_lower(Q, m, b, Q[m - 1]);                  /* правые >= max левой — на месте */
    int64_t la = m - a, lb = b - m, T = la + lb;
    int64_t small = la < lb ? la : lb, big = T - small;
    CPA_C(1);
    if (Q[a] >= Q[b - 1]) {                            /* диапазоны не пересекаются: поворот */

      /* через стековый буфер: обмен блоков не трогает страницы кучи */
      cpr_rotate(Q, a, m, b, c->stack, CPR_CAP);
      return;
    }
    if (small <= c->cap) {
      if (small * CPR_SKEW < big) { if (lb <= la) cpr_gallop_hi(Q, a, m, b, c->buf); else cpr_gallop_lo(Q, a, m, b, c->buf); }
      else if (T <= c->cap) { memcpy(c->buf, Q + a, T * 8); cpr_merge4(c->buf, la, c->buf + la, lb, Q + a); }
      else if (CPR_LEAFSPLIT && big <= CPR_LEAFSPLIT * small) goto split;   /* ещё разбиение — и лист сольётся 4 цепочками */
      else { if (lb <= la) cpr_merge_hi(Q, a, m, b, c->buf); else cpr_merge_lo(Q, a, m, b, c->buf); }
      return;
    }
    /* меньшая часть больше буфера: кусками или разбиением — что дешевле по перемещениям */
    double cap = (double)c->cap, sm = (double)small;
    double chunk_moves = (double)big + sm * sm / (2 * cap);
    double split_moves = (double)T / 2 * log2(sm / cap) + (double)T;
    if (chunk_moves <= split_moves) {

      if (lb <= la) cpr_merge_chunks_hi(c, Q, a, m, b); else cpr_merge_chunks_lo(c, Q, a, m, b);
      return;
    }
  split:;

    int64_t i, j;
    if (la >= lb) { i = a + la / 2; j = cpr_lower(Q, m, b, Q[i]); }
    else { j = m + lb / 2; i = cpr_upper(Q, a, m, Q[j]); }
    cpr_rotate(Q, i, m, j, c->buf, c->cap);
    int64_t mid = i + (j - m);                          /* [a,i)[R1][L2][j,b) */
    if (mid - a < b - mid) { cpr_merge(c, Q, a, i, mid); a = mid; m = j; }
    else { cpr_merge(c, Q, mid, j, b); b = mid; m = i; }
  }
}

/* ============================================================================
   4. Корень ядра для неупорядоченного сегмента
   ============================================================================ */
/* корень ядра для малого сегмента (n < CPD_MIN_N): проход за min/max, затем ядро */
__attribute__((noinline)) static void cpr_core_old(uint64_t *Q, int64_t s, int64_t e) {
  int64_t len = e - s;
  if (len < 2) return;
  uint64_t lo, hi;
  cpa_minmax(Q, s, e - 1, &lo, &hi);
  if (lo == hi) return;
  int lg = 0; while (((int64_t)1 << (lg + 1)) <= len && lg < 62) lg++;
  cpa_root_rec(Q, s, len, lo, hi, lg);
}

/* ---------- корень ядра: частые значения и первое разбиение с min/max ----------
   cpd_sort(Q, s, e, depth) сортирует [s, e) ядром cs_pdq, но корень устроен иначе.
   * Выборка S элементов (63 / 127 / 255 по n) в псевдослучайных позициях, сортируется.
   * Частые значения — встретившиеся в выборке не меньше CPD_HCNT раз (до CPD_HMAX штук,
     самые частые). Если их доля F в выборке такова, что F·(CQ0 + CQ·H) > CPD_CE (H — оценка
     энтропии значений по той же выборке, т.е. оценка стоимости ядра на элемент), включается
     подсчёт. Ключи — голые uint64, равные элементы неразличимы, поэтому частые значения
     можно сосчитать, а не сортировать. Один проход: для каждого элемента — есть ли он среди
     частых (до 3 частых — сравнения в регистрах; больше — совершенная хэш-таблица: умножение,
     сдвиг, одна загрузка и сравнение, счётчики в 8 копиях, чтобы соседние одинаковые не
     ждали друг друга) и запись прочих подряд в начало. Прочие сортируются (рекурсия,
     подсчёт — не глубже CPD_DEPTH уровней), затем проход с конца расставляет серии частых
     значений между прочими (прочие сдвигаются блоками, memmove).
   * Иначе — первое разбиение по медиане выборки с попутным подсчётом min и max: отдельный
     проход за границами диапазона не нужен; части получают границы [min, t-1] и [t, max].
   Режим ядра (значений или выборки) выбирается по той же выборке: различных
   значений не больше S/8 — режим выборки. В режиме значений частям передаётся таблица
   густоты: таблица прошлого массива, если она согласна с выборкой (css_tab_agrees), иначе
   сама выборка, если она не почти равномерна (cpa_tab_useful), иначе таблицы нет. */
#ifndef CPD_MIN_N
#define CPD_MIN_N 2048         /* меньше — прежний корень */
#endif
#ifndef CPD_HMAX
#define CPD_HMAX 32            /* частых значений не больше */
#endif
#ifndef CPD_CE
#define CPD_CE 1.8             /* стоимость подсчёта и расстановки, нс на элемент (откалибровано) */
#endif
#ifndef CPD_DEPTH
#define CPD_DEPTH 2
#endif

/* разбиение как cpa_partition, но с min/max всех элементов; без счёта перестановок */
static int64_t cpd_partition_mm(uint64_t *Q, int64_t x, int64_t y, uint64_t key, uint64_t *pmn, uint64_t *pmx) {
  uint64_t mn = UINT64_MAX, mx = 0;
#define CPD_MM(v) do { uint64_t v_ = (v); mn = v_ < mn ? v_ : mn; mx = v_ > mx ? v_ : mx; } while (0)
  uint64_t *first = Q + x, *last = Q + y + 1, *f0 = first, *l0 = last;
  while (first < last) { uint64_t v = *first; CPD_MM(v); if (!(v < key)) break; first++; }
  while (first < last) { uint64_t v = *(last - 1); CPD_MM(v); if (v < key) break; last--; }
  CPA_C((first - f0) + (l0 - last) + 2);
  (void)f0; (void)l0;
  if (first == last) { *pmn = mn; *pmx = mx; return first - Q; }
  --last;
  { uint64_t t = *first; *first = *last; *last = t; }
  ++first;
  unsigned char offL[CPA_BLOCK], offR[CPA_BLOCK];
  uint64_t *baseL = first, *baseR = last;
  size_t nL = 0, nR = 0, sL = 0, sR = 0;
  while (first < last) {
    size_t unk = (size_t)(last - first);
    size_t lsplit = nL == 0 ? (nR == 0 ? unk / 2 : unk) : 0;
    size_t rsplit = nR == 0 ? (unk - lsplit) : 0;
    if (lsplit >= CPA_BLOCK) {
      CPA_C(CPA_BLOCK);
      for (size_t i = 0; i < CPA_BLOCK;) {
        for (int u = 0; u < 8; u++) { uint64_t v = *first; CPD_MM(v); offL[nL] = (unsigned char)i++; nL += !(v < key); ++first; }
      }
    } else {
      CPA_C(lsplit);
      for (size_t i = 0; i < lsplit;) { uint64_t v = *first; CPD_MM(v); offL[nL] = (unsigned char)i++; nL += !(v < key); ++first; }
    }
    if (rsplit >= CPA_BLOCK) {
      CPA_C(CPA_BLOCK);
      for (size_t i = 0; i < CPA_BLOCK;) {
        for (int u = 0; u < 8; u++) { uint64_t v = *--last; CPD_MM(v); offR[nR] = (unsigned char)++i; nR += v < key; }
      }
    } else {
      CPA_C(rsplit);
      for (size_t i = 0; i < rsplit;) { uint64_t v = *--last; CPD_MM(v); offR[nR] = (unsigned char)++i; nR += v < key; }
    }
    size_t num = nL < nR ? nL : nR;
    if (num) {
      const unsigned char *ol = offL + sL, *orr = offR + sR;
      if (nL == nR) {
        for (size_t i = 0; i < num; i++) { uint64_t *a = baseL + ol[i], *b = baseR - orr[i]; uint64_t t = *a; *a = *b; *b = t; }
      } else {
        uint64_t *l = baseL + ol[0], *r = baseR - orr[0];
        uint64_t tmp = *l; *l = *r;
        for (size_t i = 1; i < num; i++) { l = baseL + ol[i]; *r = *l; r = baseR - orr[i]; *l = *r; }
        *r = tmp;
      }
    }
    nL -= num; nR -= num; sL += num; sR += num;
    if (nL == 0) { sL = 0; baseL = first; }
    if (nR == 0) { sR = 0; baseR = last; }
  }
  if (nL) {
    const unsigned char *ol = offL + sL;
    while (nL--) { uint64_t *a = baseL + ol[nL]; --last; uint64_t t = *a; *a = *last; *last = t; }
    first = last;
  }
  if (nR) {
    const unsigned char *orr = offR + sR;
    while (nR--) { uint64_t *b = baseR - orr[nR]; uint64_t t = *b; *b = *first; *first = t; ++first; }
  }
#undef CPD_MM
  *pmn = mn; *pmx = mx;
  return first - Q;
}

/* совершенная хэш-таблица для h частых значений: умножение и сдвиг без коллизий */
typedef struct { uint64_t key[256]; uint64_t mul; int sh, size; } cpd_tab;
static int cpd_tab_build(cpd_tab *T, const uint64_t *V, int h, int *slot_of) {
  int b = 3; while ((1 << b) < 8 * h && b < 8) b++;    /* заполнение не больше 1/8 */
  T->size = 1 << b; T->sh = 64 - b;
  uint64_t m = 0x9e3779b97f4a7c15ULL;
  for (int tr = 0; tr < 64; tr++) {
    m = m * 0xbf58476d1ce4e5b9ULL + 0x94d049bb133111ebULL;
    uint64_t mul = m | 1;
    unsigned char used[256]; memset(used, 0, sizeof used);
    int ok = 1;
    for (int j = 0; j < h && ok; j++) {
      int sl = (int)((V[j] * mul) >> T->sh);
      if (used[sl]) ok = 0; else { used[sl] = 1; slot_of[j] = sl; }
    }
    if (!ok) continue;
    T->mul = mul;
    /* пустая ячейка хранит V[0]: оно попадает в свою ячейку, так что ложного совпадения нет */
    for (int sl = 0; sl < T->size; sl++) T->key[sl] = V[0];
    for (int j = 0; j < h; j++) T->key[slot_of[j]] = V[j];
    return 1;
  }
  return 0;
}

/* проход подсчёта: частые считаются, прочие пишутся подряд с s; возвращает конец прочих */
static int64_t cpd_count1(uint64_t *Q, int64_t s, int64_t e, uint64_t v, uint64_t *cnt) {
  int64_t w = s; uint64_t c0 = 0, c1 = 0;
  int64_t i = s;
  CPA_C(e - s);
  for (; i + 2 <= e; i += 2) {
    uint64_t x0 = Q[i], x1 = Q[i + 1];
    int q0 = x0 == v, q1 = x1 == v;
    Q[w] = x0; w += !q0; Q[w] = x1; w += !q1; c0 += q0; c1 += q1;
  }
  for (; i < e; i++) { uint64_t x = Q[i]; int q = x == v; Q[w] = x; w += !q; c0 += q; }
  *cnt = c0 + c1;
  return w;
}
/* 2..4 частых: сравнения в регистрах, без таблицы */
static int64_t cpd_count_few(uint64_t *Q, int64_t s, int64_t e, const uint64_t *V, int h, uint64_t *cnt) {
  uint64_t v0 = V[0], v1 = V[1], v2 = h > 2 ? V[2] : V[0], v3 = h > 3 ? V[3] : V[0];
  uint64_t c0 = 0, c1 = 0, c2 = 0, c3 = 0; int64_t w = s;
  CPA_C((e - s) * h);
  if (h == 2) {
    for (int64_t i = s; i < e; i++) { uint64_t x = Q[i]; int q0 = x == v0, q1 = x == v1; c0 += q0; c1 += q1; Q[w] = x; w += !(q0 | q1); }
  } else if (h == 3) {
    for (int64_t i = s; i < e; i++) { uint64_t x = Q[i]; int q0 = x == v0, q1 = x == v1, q2 = x == v2; c0 += q0; c1 += q1; c2 += q2; Q[w] = x; w += !(q0 | q1 | q2); }
  } else {
    for (int64_t i = s; i < e; i++) { uint64_t x = Q[i]; int q0 = x == v0, q1 = x == v1, q2 = x == v2, q3 = x == v3; c0 += q0; c1 += q1; c2 += q2; c3 += q3; Q[w] = x; w += !(q0 | q1 | q2 | q3); }
  }
  cnt[0] = c0; cnt[1] = c1; if (h > 2) cnt[2] = c2; if (h > 3) cnt[3] = c3;
  return w;
}
static int64_t cpd_count_tab(uint64_t *Q, int64_t s, int64_t e, const cpd_tab *T, uint64_t (*cnt)[256]) {
  int64_t w = s, i = s;
  const uint64_t mul = T->mul; const int sh = T->sh;
  CPA_C(e - s);
  for (; i + 8 <= e; i += 8) {
    for (int k = 0; k < 8; k++) {
      uint64_t x = Q[i + k]; unsigned sl = (unsigned)((x * mul) >> sh);
      int q = T->key[sl] == x;
      cnt[k][sl] += (uint64_t)q; Q[w] = x; w += !q;
    }
  }
  for (; i < e; i++) {
    uint64_t x = Q[i]; unsigned sl = (unsigned)((x * mul) >> sh);
    int q = T->key[sl] == x;
    cnt[0][sl] += (uint64_t)q; Q[w] = x; w += !q;
  }
  return w;
}
/* расстановка с конца: прочие [s, w) отсортированы, частые V[0..h) по возрастанию с c[j] */
static void cpd_expand(uint64_t *Q, int64_t s, int64_t w, int64_t e, const uint64_t *V, const uint64_t *c, int h) {
  int64_t out = e, lend = w;
  for (int j = h - 1; j >= 0; j--) {
    int64_t p = cpr_upper(Q, s, lend, V[j]);            /* прочие > V[j]: [p, lend) */
    int64_t m = lend - p;
    if (m && out - m != p) memmove(Q + out - m, Q + p, (size_t)m * 8);
    out -= m; lend = p;
    int64_t c0 = (int64_t)c[j], a = out - c0;          /* серия V[j] в [a, out): заливка удвоением */
    if (c0 > 0) {
      int64_t f = c0 < 8 ? c0 : 8;
      for (int64_t i = 0; i < f; i++) Q[a + i] = V[j];
      while (f < c0) { int64_t k = f < c0 - f ? f : c0 - f; memcpy(Q + a + f, Q + a, (size_t)k * 8); f += k; }
    }
    out = a;
  }
}

__attribute__((noinline)) static void cpd_sort(uint64_t *Q, int64_t s, int64_t e, int depth) {
  int64_t n = e - s;
  if (n < CPD_MIN_N) { cpr_core_old(Q, s, e); return; }
  int S = n >= (1 << 18) ? 255 : n >= (1 << 14) ? 127 : 63;
  int hcnt = S >= 255 ? 4 : 3;
  uint64_t buf[256];
  int64_t step = n / S;
  for (int i = 0; i < S; i++) {
    uint64_t hh = (uint64_t)(i + 1) * 0x9e3779b97f4a7c15ULL;
    buf[i] = Q[s + (int64_t)i * step + (int64_t)(((unsigned __int128)(hh >> 32) * (uint64_t)step) >> 32)];
  }
  { uint64_t lo, hi; cpa_minmax(buf, 0, S - 1, &lo, &hi); CSS_ST(css_st_lg0 = -1); if (lo < hi) cs_pdq_rec(buf, 0, S - 1, lo, hi, 8, 0); }
  /* группы равных в выборке: частые, число различных, оценка энтропии */
  uint64_t cv[128]; int cc[128]; int nc = 0, distinct = 0, single = 0; double Hs = 0;
  for (int i = 0; i < S;) {
    int j = i + 1; while (j < S && buf[j] == buf[i]) j++;
    int c = j - i; distinct++;
    if (c == 1) single++; else Hs += (double)c / S * log2((double)S / c);
    if (c >= hcnt && nc < 128) { cv[nc] = buf[i]; cc[nc] = c; nc++; }
    i = j;
  }
  double lgn = log2((double)n);
  Hs += (double)single / S * lgn;
  if (Hs > lgn) Hs = lgn;
  int few = distinct <= S / 8;
  if (nc > CPD_HMAX) {                                  /* оставить CPD_HMAX самых частых */
    for (int a = 0; a < CPD_HMAX; a++) {
      int b = a; for (int q = a + 1; q < nc; q++) if (cc[q] > cc[b]) b = q;
      uint64_t tv = cv[a]; cv[a] = cv[b]; cv[b] = tv; int tc = cc[a]; cc[a] = cc[b]; cc[b] = tc;
    }
    nc = CPD_HMAX;
    for (int a = 1; a < nc; a++) {                      /* обратно по возрастанию значений */
      uint64_t tv = cv[a]; int tc = cc[a], b = a - 1;
      while (b >= 0 && cv[b] > tv) { cv[b + 1] = cv[b]; cc[b + 1] = cc[b]; b--; }
      cv[b + 1] = tv; cc[b + 1] = tc;
    }
  }
  int ht = 0; for (int a = 0; a < nc; a++) ht += cc[a];
  double F = (double)ht / S;
  if (nc > 0 && depth < CPD_DEPTH && F * (CPR_CQ0 + CPR_CQ * Hs) > CPD_CE) {
    uint64_t cnt[CPD_HMAX]; int64_t w;

    if (nc == 1) w = cpd_count1(Q, s, e, cv[0], &cnt[0]);
    else if (nc <= 3) w = cpd_count_few(Q, s, e, cv, nc, cnt);
    else {
      cpd_tab T; int slot_of[CPD_HMAX];
      if (!cpd_tab_build(&T, cv, nc, slot_of)) goto split;
      uint64_t cn[8][256];                              /* 8 копий счётчиков: соседние одинаковые не ждут друг друга */
      for (int k = 0; k < 8; k++) memset(cn[k], 0, (size_t)T.size * sizeof(uint64_t));
      w = cpd_count_tab(Q, s, e, &T, cn);
      for (int j = 0; j < nc; j++) { int sl = slot_of[j]; cnt[j] = 0; for (int k = 0; k < 8; k++) cnt[j] += cn[k][sl]; }
    }
    if (w - s > 1) cpd_sort(Q, s, w, depth + 1);
    cpd_expand(Q, s, w, e, cv, cnt, nc);
    return;
  }
split:;
  /* первое разбиение по медиане выборки, попутно min и max */
  uint64_t p = buf[S / 2], t = p;
  if (p == buf[0]) { if (p == UINT64_MAX) { cpr_core_old(Q, s, e); return; } t = p + 1; }
  uint64_t mn, mx;

  int64_t z = cpd_partition_mm(Q, s, e - 1, t, &mn, &mx);
  if (mn == mx) return;
  int lg = 0; while (((int64_t)1 << (lg + 1)) <= n && lg < 62) lg++;
  int vmode = !few;
  /* таблица густоты: прошлого массива, если она согласна с выборкой сегмента и не почти
     равномерна на [mn, mx]; иначе выборка корня, если она не почти равномерна; иначе деление пополам */
  const uint64_t *TT = NULL; int64_t TS = 0;
  if (vmode && css_prev && css_tab_agrees(css_prev, css_prev_n, buf, S)) {
    int64_t a = cpa_tab_lb(css_prev, 0, css_prev_n, mn), b = mx == UINT64_MAX ? css_prev_n : cpa_tab_lb(css_prev, a, css_prev_n, mx + 1);
    if (css_tab_far(css_prev, a, b, mn, mx, S)) { TT = css_prev; TS = css_prev_n; }
  }
  if (!TT && vmode && cpa_tab_useful(buf, S, mn, mx)) { TT = buf; TS = S; }
  int64_t ta = TT ? cpa_tab_lb(TT, 0, TS, mn) : 0, tb = !TT ? 0 : mx == UINT64_MAX ? TS : cpa_tab_lb(TT, 0, TS, mx + 1);
  int64_t tm = TT ? cpa_tab_lb(TT, ta, tb, t) : 0;
  CSS_ST(css_st_lg0 = CSS_BUDGET(lg));
  if (z == s || z == e) { cs_pdq_rec_t(Q, s, e - 1, mn, mx, CSS_BUDGET(lg), vmode, TT, ta, tb); return; }
  if (z - s >= 2) cs_pdq_rec_t(Q, s, z - 1, mn, t - 1, CSS_BUDGET(lg), vmode, TT, ta, tm);
  if (e - z >= 2) cs_pdq_rec_t(Q, z, e - 1, t, mx, CSS_BUDGET(lg), vmode, TT, tm, tb);
}

/* ============================================================================
   5. Порядок слияний (powersort) и режим участков
   ============================================================================ */
static int cpr_power(int64_t s1, int64_t n1, int64_t n2, int64_t n) {
  int r = 0;
  int64_t a = 2 * s1 + n1, b = a + n1 + n2;
  for (;;) {
    ++r;
    if (a >= n) { a -= n; b -= n; }
    else if (b >= n) break;
    a <<= 1; b <<= 1;
  }
  return r;
}

static double cpr_qs_cost(double u) { return u < 2 ? 0 : u * (CPR_CQ0 + CPR_CQ * log2(u)); }
static double cpr_merge_cost(double la, double lb, double cap) {
  double s = la < lb ? la : lb, T = la + lb;
  double c = T * (CPR_CM + (T > 16384 ? CPR_CMS * log2(T / 16384) : 0));
  if (s > cap) {
    double ch = (T - s) + s * s / (2 * cap), sp = T / 2 * log2(s / cap) + T;
    c += T * CPR_CR0 + CPR_CR * (ch < sp ? ch : sp);
  }
  return c;
}

/* Слияние сегментов с границами bd[0] = 0 < ... < bd[k] = n по powersort.
   sim != 0 — только оценка стоимости по длинам. */
static double cpr_powersort(cpr_ctx *c, uint64_t *Q, int64_t n, const int64_t *bd, int k, int sim) {
  int64_t ss[72], se[72]; int sp[72]; int top;
  double cost = 0;
  ss[0] = bd[0]; se[0] = bd[1]; top = 1;
  for (int t = 1; t <= k; t++) {
    int last = t == k, p = 0;
    if (!last) p = cpr_power(ss[top - 1], se[top - 1] - ss[top - 1], bd[t + 1] - bd[t], n);
    while (top > 1 && (last || sp[top - 2] > p)) {
      int64_t a = ss[top - 2], m = ss[top - 1], b = se[top - 1];
      if (sim) cost += cpr_merge_cost((double)(m - a), (double)(b - m), (double)c->cap);
      else cpr_merge(c, Q, a, m, b);
      se[top - 2] = b; top--;
    }
    if (last) break;
    sp[top - 1] = p;
    ss[top] = bd[t]; se[top] = bd[t + 1]; top++;
  }
  return cost;
}

/* Режим участков. 1 — массив отсортирован; 0 — участки невыгодны (массив
   остаётся перестановкой исходного, ядро досортирует его целиком). */
/* H — оценка энтропии значений; outl = 0 — участки без выбросов (при H < CPR_HMIN). */
static int cpr_run_mode(uint64_t *Q, int64_t n, int heap, int64_t r0, int desc0, double H, int outl) {
  int64_t R = n / (CPR_SEGMAX / 2 - 2) + 1;
  if (R < CPR_RFLOOR) R = CPR_RFLOOR;
  int64_t skip = R / 4 > 16 ? R / 4 : 16;
  int64_t st[CPR_SEGMAX + 1]; unsigned char so[CPR_SEGMAX + 1];
  /* so: 0 — неупорядоченный, 1 — неубывающий, 2 — неубывающий с выбросами (нужно сжатие),
     3 — невозрастающий (нужен разворот). Проход A только читает массив. */
  int k = 0, nsorted = 0;
#define CPR_ADD_UNSORTED(pos) do { if (k == 0 || so[k - 1]) { st[k] = (pos); so[k] = 0; k++; } } while (0)
  for (int64_t i = 0; i < n;) {
    int desc; int64_t j, w;
    if (i == 0) { desc = desc0; j = r0; }
    else j = cpr_run_end(Q, i, n, &desc);
    if (desc || !outl) w = j;
    else j = cpr_asc_run_x(Q, i, j, n, &w, 1);
    if (w - i >= R) {
      st[k] = i; so[k] = desc ? 3 : (j > w ? 2 : 1); k++; nsorted++;
      if (j > w) CPR_ADD_UNSORTED(w);                 /* выбросы */
    } else {
      CPR_ADD_UNSORTED(i);
      /* явно неупорядоченная область (сегмент уже не короче R): дальше не каждый
         короткий участок, а скачок на R/4 — длинный участок всё равно будет найден */
      if (j - st[k - 1] >= R) { int64_t jj = i + skip; if (jj > n) jj = n; if (jj > j) j = jj; }
    }
    i = j;
  }
#undef CPR_ADD_UNSORTED
  st[k] = n;
  if (!nsorted) return 0;
  /* поглощение коротких отсортированных сегментов */
  for (int t = 0; t < k; t++) {
    if (!so[t]) continue;
    int64_t s = st[t + 1] - st[t], u = 0;
    if (t > 0 && !so[t - 1]) u += st[t] - st[t - 1];
    if (t + 1 < k && !so[t + 1]) u += st[t + 2] - st[t + 1];
    if (s * CPR_K < u) so[t] = 0;
  }
  int k2 = 0;
  for (int t = 0; t < k; t++) {
    if (k2 > 0 && !so[t] && !so[k2 - 1]) continue;     /* соседние неупорядоченные — один сегмент */
    st[k2] = st[t]; so[k2] = so[t]; k2++;
  }
  /* поглощённый сегмент с выбросами (so был 2) сливается с ними в один неупорядоченный — сжатие не нужно */
  st[k2] = n; k = k2;
  nsorted = 0; for (int t = 0; t < k; t++) nsorted += so[t] != 0;
  if (!nsorted) return 0;
  /* модель стоимости */
  cpr_ctx cx, *c = &cx;
  c->buf = c->stack; c->cap = CPR_CAP;
  uint64_t *hb = NULL;
  if (heap && k > 1) {
    hb = (uint64_t *)malloc((size_t)n * sizeof(uint64_t));
    if (hb) { c->buf = hb; c->cap = n; }
  }
  double plan = CPR_FIXED;
  for (int t = 0; t < k; t++) {
    double len = (double)(st[t + 1] - st[t]);
    if (!so[t]) plan += cpr_qs_cost(len);
    else if (so[t] == 2) plan += CPR_CX * len;   /* проход B: сжатие */
    else if (so[t] == 3) plan += CPR_CV * len;   /* разворот */
  }
  if (k > 1) plan += cpr_powersort(c, Q, n, st, k, 1);
  /* ядро на всём массиве: CQ0 + CQ·H на элемент, H — оценка энтропии значений */
  double lg = log2((double)n), base = (double)n * (CPR_CQ0 + CPR_CQ * (H < lg ? H : lg));
  if (plan >= CPR_MARGIN * base) { free(hb); return 0; }   /* модель неточна: нужен запас */
  /* проход B: сжатие принятых участков с выбросами, развороты */
  for (int t = 0; t < k; t++) {
    if (so[t] == 3) cpr_reverse(Q, st[t], st[t + 1]);
    else if (so[t] == 2) {
      int64_t w, e0 = cpr_asc_end(Q, st[t], n);
      cpr_asc_run_x(Q, st[t], e0, n, &w, 0);
      (void)w;                                         /* w == st[t + 1] по построению */
    }
  }
  for (int t = 0; t < k; t++) if (!so[t]) cpd_sort(Q, st[t], st[t + 1], 0);
  if (k > 1) cpr_powersort(c, Q, n, st, k, 0);
  free(hb);

  return 1;
}
static void cpr_sort(uint64_t *Q, int64_t n, int heap) {
  if (n < 2) return;
  int desc; int64_t r0 = cpr_run_end(Q, 0, n, &desc);
  if (r0 == n) { if (desc) cpr_reverse(Q, 0, n); return; }      /* уже упорядочен */
  int likely = r0 * 8 >= n || (n >= CPR_SAMPLE_N && cpr_sample_monotone(Q, n));
  if (n >= CPR_MIN_N && likely) {
    /* энтропия — только если участки вероятны; при n < CPR_GATE_N считается H = log2 n */
    double H = n >= CPR_GATE_N ? cpr_entropy(Q, n) : log2((double)n);
    int go = 1;
    if (H < CPR_HMIN) {
      /* много повторов: если слияние участков заведомо дороже ядра, не размечать */
      double lg = log2((double)n), base = (double)n * (CPR_CQ0 + CPR_CQ * (H < lg ? H : lg));
      double kest = (double)n * cpr_descent_rate(Q, n);
      double est = CPR_FIXED + (kest >= 2 ? CPR_CM * (double)n * log2(kest) : 0);
      go = est < CPR_MARGIN * base;
    }
    if (go && cpr_run_mode(Q, n, heap, r0, desc, H, H >= CPR_HMIN)) return;
  }
  if (n >= CPD_MIN_N) { cpd_sort(Q, 0, n, 0); return; }
  /* малые n: начало [0, r0) монотонно, его границы известны — min/max только по остатку */
  uint64_t lo = desc ? Q[r0 - 1] : Q[0], hi = desc ? Q[0] : Q[r0 - 1];
  CPA_C(2 * (n - r0));
  for (int64_t i = r0; i < n; i++) { uint64_t v = Q[i]; lo = v < lo ? v : lo; hi = v > hi ? v : hi; }
  int lg = 0; while (((int64_t)1 << (lg + 1)) <= n && lg < 62) lg++;
  cpa_root_rec(Q, 0, n, lo, hi, lg);
}

/* ============================================================================
   6. Функции
   ============================================================================ */

void css_sort_ex(uint64_t *Q, uint64_t n, const uint64_t *T, uint64_t S, int heap) {
  css_prev = T && S >= 2 ? T : NULL; css_prev_n = css_prev ? (int64_t)S : 0;
  cpr_sort(Q, (int64_t)n, heap);
  css_prev = NULL; css_prev_n = 0;
}
void css_sort(uint64_t *Q, uint64_t n) { css_sort_ex(Q, n, NULL, 0, 0); }
void css_sort_tab(uint64_t *Q, uint64_t n, const uint64_t *T, uint64_t S) { css_sort_ex(Q, n, T, S, 0); }

/* S квантилей уровней (i + 1/2)/S отсортированного массива sorted[0..n), n >= 1 */
void css_table_from_sorted(const uint64_t *sorted, uint64_t n, uint64_t *T, uint64_t S) {
  /* T[i] = sorted[floor((2i + 1)·n / (2S))]; индекс накапливается без деления: частное q и остаток r
     числителя (2i + 1)·n по модулю 2S растут на 2n за шаг */
  uint64_t D = 2 * S, q = n / D, r = n % D, dq = (2 * n) / D, dr = (2 * n) % D;
  for (uint64_t i = 0; i < S; i++) {
    T[i] = sorted[q];
    q += dq; r += dr; if (r >= D) { r -= D; q++; }
  }
}

#ifdef CPA_COUNT
int64_t css_cmp_count(void) { return cpa_cmp_count; }
void css_cmp_reset(void) { cpa_cmp_count = 0; }
#endif

/* ============================================================================
   7. Самопроверка и замер времени (отключается -DCSS_NO_MAIN)
   ============================================================================ */
#ifndef CSS_NO_MAIN
#include <stdio.h>
#include <time.h>

static uint64_t t_state = 1;
static uint64_t t_rnd(void) {                       /* splitmix64 */
  uint64_t z = (t_state += 0x9e3779b97f4a7c15ULL);
  z = (z ^ (z >> 30)) * 0xbf58476d1ce4e5b9ULL; z = (z ^ (z >> 27)) * 0x94d049bb133111ebULL;
  return z ^ (z >> 31);
}
static int t_cmp(const void *a, const void *b) {
  uint64_t x = *(const uint64_t *)a, y = *(const uint64_t *)b; return x < y ? -1 : x > y;
}
#define T_NP 14
static const char *t_name[T_NP] = {"random", "sorted", "reversed", "nearly_sorted", "pipeorgan",
  "sawtooth", "few_values", "binary", "zipf", "lognormal", "clusters", "powers2", "sorted95", "m3killer"};
static void t_gen(int p, uint64_t *A, int64_t n) {
  if (n <= 0) return;
  for (int64_t i = 0; i < n; i++) A[i] = t_rnd();
  switch (p) {
  case 1: for (int64_t i = 0; i < n; i++) A[i] = (uint64_t)i * 3; break;
  case 2: for (int64_t i = 0; i < n; i++) A[i] = (uint64_t)(n - i) * 3; break;
  case 3: for (int64_t i = 0; i < n; i++) A[i] = (uint64_t)i;
    for (int64_t k = 0; k < n / 100 + 1; k++) { int64_t a = t_rnd() % n, b = t_rnd() % n; uint64_t t = A[a]; A[a] = A[b]; A[b] = t; } break;
  case 4: for (int64_t i = 0; i < n; i++) A[i] = (uint64_t)(i < n / 2 ? i : n - i); break;
  case 5: for (int64_t i = 0; i < n; i++) A[i] = (uint64_t)(i % 1000); break;
  case 6: for (int64_t i = 0; i < n; i++) A[i] %= 16; break;
  case 7: for (int64_t i = 0; i < n; i++) A[i] &= 1; break;
  case 8: for (int64_t i = 0; i < n; i++) { double u = (double)(A[i] >> 11) / 9007199254740992.0; A[i] = (uint64_t)(1.0 / (u + 1e-9)); } break;
  case 9: for (int64_t i = 0; i < n; i++) { double u1 = ((A[i] >> 11) + 1.0) / 9007199254740993.0, u2 = (double)(t_rnd() >> 11) / 9007199254740992.0;
            A[i] = (uint64_t)exp(20 + 2 * sqrt(-2 * log(u1)) * cos(6.283185307179586 * u2)); } break;
  case 10: for (int64_t i = 0; i < n; i++) A[i] = (A[i] % 8) * 1000000000000ULL + (t_rnd() % 1000000); break;
  case 11: for (int64_t i = 0; i < n; i++) A[i] = 1ULL << (A[i] % 64); break;
  case 12: for (int64_t i = 0; i < n; i++) A[i] = (uint64_t)i * 7;
           for (int64_t i = n - n / 20; i < n; i++) A[i] = t_rnd() % (7 * (uint64_t)n);
           break;
  case 13: { int64_t h = n / 2;                       /* «убийца медианы трёх» (Musser) */
           for (int64_t i = 0; i < n; i++) A[i] = (uint64_t)i;
           for (int64_t i = 1; i <= h; i++) { if (i % 2) { A[i - 1] = i; A[i + h - 1] = i + h; } else { A[i - 1] = h + i; A[i + h - 1] = 2 * i; } } } break;
  }
}

int main(int argc, char **argv) {
  static const int64_t sizes[] = {0, 1, 2, 3, 5, 16, 23, 24, 25, 100, 255, 256, 1000, 2047, 2048, 4095, 4096, 10000, 65536, 100000, 300000};
  int nz = (int)(sizeof sizes / sizeof sizes[0]), runs = 0, errs = 0;
  int64_t nmax = 300000;
  uint64_t *A = malloc(nmax * 8), *B = malloc(nmax * 8), *P = malloc(nmax * 8), T[4096];
  for (int p = 0; p < T_NP; p++) for (int z = 0; z < nz; z++) for (int mode = 0; mode < 4; mode++) {
    int64_t n = sizes[z];
    /* mode: 0 — без таблицы, 1 — таблица прошлого массива того же вида,
       2 — таблица чужого вида (проверка Колмогорова должна её отвергнуть), 3 — буфер в куче */
    const uint64_t *tab = NULL; uint64_t S = 0;
    if (mode == 1 || mode == 2) {
      int q = mode == 1 ? p : (p + 7) % T_NP; int64_t m = n > 16 ? n : 16;
      t_gen(q, P, m); qsort(P, m, 8, t_cmp);
      S = m < 4096 ? (uint64_t)m : 4096; css_table_from_sorted(P, m, T, S); tab = T;
    }
    t_gen(p, A, n); memcpy(B, A, n * 8);
    css_sort_ex(A, n, tab, S, mode == 3);
    qsort(B, n, 8, t_cmp);
    runs++;
    if (memcmp(A, B, n * 8)) { errs++; printf("ОШИБКА: %s n=%lld mode=%d\n", t_name[p], (long long)n, mode); }
  }
  printf("самопроверка: %d запусков, ошибок %d\n", runs, errs);
  if (argc > 1) {
    int64_t n = atoll(argv[1]);
    uint64_t *X = malloc(n * 8), *Y = malloc(n * 8), *Z = malloc(n * 8);
    printf("%-14s %10s %10s %10s   нс на элемент, n = %lld\n", "вход", "qsort", "css", "css+табл", (long long)n);
    for (int p = 0; p < T_NP; p++) {
      t_gen(p, Z, n); memcpy(Y, Z, n * 8); qsort(Y, n, 8, t_cmp);
      css_table_from_sorted(Y, n, T, 4096);              /* таблица «прошлого» массива того же вида */
      t_gen(p, Z, n);
      double tm[3];
      for (int v = 0; v < 3; v++) {
        memcpy(X, Z, n * 8);
        struct timespec a, b; clock_gettime(CLOCK_MONOTONIC, &a);
        if (v == 0) qsort(X, n, 8, t_cmp); else css_sort_tab(X, n, v == 2 ? T : NULL, 4096);
        clock_gettime(CLOCK_MONOTONIC, &b);
        tm[v] = ((b.tv_sec - a.tv_sec) * 1e9 + (b.tv_nsec - a.tv_nsec)) / n;
      }
      printf("%-14s %10.1f %10.1f %10.1f\n", t_name[p], tm[0], tm[1], tm[2]);
    }
    free(X); free(Y); free(Z);
  }
  free(A); free(B); free(P);
  return errs != 0;
}
#endif
