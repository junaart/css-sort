#!/bin/sh
# SPDX-License-Identifier: MIT
# Все замеры по порядку. Перед запуском: make.
#   ./run.sh [каталог результатов]      (по умолчанию ../results/local; существующие файлы перезаписываются)
# Время работы — около 40 минут. Процессор для замеров задаёт переменная CSS_CPU (по умолчанию 1).
set -e
cd "$(dirname "$0")"
B=bin; R=${1:-../results/local}; mkdir -p "$R"
echo "1. самопроверка C и Python, пример с печатью узлов"; $B/selftest; python3 ../css.py > "$R/example.txt"
echo "2. сверка ipnsort из rivals/ с Rust std";              $B/ipn_check > "$R/ipn_check.txt"
echo "3. время: CSS, pdqsort, crumsort, fluxsort, ipnsort в одном прогоне"
$B/bench_ipn time 100000 21 > "$R/ipn_1e5.csv"
$B/bench_ipn time 1000000 11 > "$R/ipn_1e6.csv"
$B/bench_ipn time 10000000 5 > "$R/ipn_1e7.csv"
for n in 1000000 10000000 100000000; do $B/bench_ipn hash $n 3 | tail -1 | sed "s/^hash/$n/"; done > "$R/ipn_hash.csv"
echo "4. сравнения на ключ";                                  $B/bench_cmp cmp 100000 > "$R/cmp_1e5.csv"; $B/bench_cmp cmp 1000000 > "$R/cmp_1e6.csv"
                                                              $B/bench_ipn cmp 100000 > "$R/ipn_cmp_1e5.csv"; $B/bench_ipn cmp 1000000 > "$R/ipn_cmp_1e6.csv"
echo "5. работа проходов по видам";                           $B/stats > "$R/stats_1e6.txt"
echo "6. своя и чужая таблица густоты";                       $B/bench_mism 1000000 7 > "$R/mism_1e6.csv"
echo "7. std::sort и последовательный IPS4o";                 $B/bench_std time 1000000 7 > "$R/all_1e6.csv"; $B/bench_std time 10000000 3 > "$R/all_1e7.csv"
echo "8. деление пополам против медианы выборки"
for n in 100000 1000000 10000000; do e=$(printf %s $n | wc -c); e=$((e - 1))
  $B/bench_med cmp $n > "$R/med_cmp$e.csv"; $B/bench_med time $n 7 > "$R/med_time$e.csv"; done
for n in 1000000 10000000 100000000 200000000; do $B/bench_med_hash $n 3; done > "$R/med_hash.txt"
echo "9. вход-противник (нижняя граница худшего случая)"
{ for n in 1048576 4194304 16777216; do echo "n = $n"; $B/adv4b $n; done; echo "n = 1048576, доли 0.1–0.5"; $B/adv4 1048576; } > "$R/adv.txt"
echo "10. сводка и рисунок"
python3 tables.py "$R" > "$R/ipn_summary.txt"
python3 chart_groups.py "$R/ipn_1e6.csv" "$R/groups_en.png" en && python3 chart_groups.py "$R/ipn_1e6.csv" "$R/groups_ru.png" ru || echo "(для рисунка нужен matplotlib)"
echo "готово: $R"
