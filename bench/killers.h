/* SPDX-License-Identifier: MIT */
#ifndef KILLERS_H
#define KILLERS_H

#include <stdint.h>

/* Киллер-последовательности для quicksort/divide-and-conquer и cs-сортировок.
   Все генераторы используют общий внутренний ГПСЧ (по умолчанию seed = 42),
   результаты воспроизводимы. Вызывать killer_set_seed() для смены сида.
   Буфер A должен иметь ёмкость >= n uint64_t. */

void killer_set_seed(uint64_t seed);

/* «Органные трубы»: пила 1..mid, mid+1..n = n-i. Классический киллер
   med3-сэмплинга (2-пивотные/3-way quicksort -> O(n^2)). */
void killer_pipeorgan(uint64_t *A, int n);

/* m3-киллер: чётные позиции 1..k, нечётные k+1..n + локальные свапы. */
void killer_m3killer(uint64_t *A, int n);

/* Органные трубы с рампой до max_val (значения кратные step). */
void killer_pipeorganmx(uint64_t *A, int n, uint64_t max_val);

/* Неравномерные случайные: 90% в плато mid_val +/- delta, 10% по краям. */
void killer_unevenseq(uint64_t *A, int n, uint64_t max_val);

/* Те же неравномерные, но отсортированные по возрастанию (киллер для yar/qs3). */
void killer_unevensorted(uint64_t *A, int n, uint64_t max_val);

/* Случайные байты: равномерно в [0,255]. */
void killer_0to255(uint64_t *A, int n);

/* Сильные дубликаты: k равновероятных ключей (k = 4, 64, ...). */
void killer_rep(uint64_t *A, int n, uint64_t k);

/* 90% элементов один популярный ключ (max_val/2), 10% - разброс по диапазону. */
void killer_rep9010(uint64_t *A, int n, uint64_t max_val);

/* 2 ключа 50/50 (например 0 и 1e6). */
void killer_binary(uint64_t *A, int n, uint64_t vlo, uint64_t vhi);

/* 3 ключа равновероятно. */
void killer_ternary(uint64_t *A, int n, uint64_t v0, uint64_t v1, uint64_t v2);

/* 5 ключей равновероятно; values - массив из 5 элементов (напр. {10,100,500,1000,5000}). */
void killer_quintary(uint64_t *A, int n, const uint64_t *values);

/* КИЛЛЕР ДЛЯ CS2-2/CS4-2: единственный ключ на огромной величине 2^shift.
   C -> ~shift сравнений на элемент (значение делится порогами S все уровни). */
void killer_mag(uint64_t *A, int n, int shift);

#endif /* KILLERS_H */
