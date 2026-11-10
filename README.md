# CSS — сортировка натуральных чисел построением порождающей последовательности на лету

**CSS** (complete-sequence sort) — гибридная сортировка 64-битных беззнаковых ключей сравнениями. В каждом узле
быстрой сортировки с границами значений [lo, hi] порог t выбирается из чисел (делением диапазона пополам или по
таблице густоты значений), а не из элементов массива; при перекосе узел переходит к медиане выборки, а при
исчерпании счётчика перекосов — к делению пополам, которое ограничивает глубину шириной диапазона значений
и делает запасную пирамидальную сортировку ненужной. Сверху работает слой упорядоченных участков со слиянием
в порядке powersort.

Статья: Ю. Н. Артамонов, «Гибридный алгоритм сортировки натуральных чисел на основе порождающей
последовательности, построенной на лету», IEEE REEPE-2027 (в печати).

*English summary.* CSS is a hybrid comparison sort for `uint64_t` keys. A quicksort node with value bounds
[lo, hi] picks its pivot as a number (bisection of the value range or the mass median of a density table)
rather than as an array element; skewed nodes switch to a sample median, and after a small skew budget is
spent the node bisects the value range, which bounds the depth by the key width and removes the need for a
heapsort fallback. A run layer merges presorted runs in powersort order. The `bench/` directory compares CSS
with pdqsort, crumsort, fluxsort, ipnsort (Rust `sort_unstable`), `std::sort` and sequential IPS4o on 30 inputs.

## Состав

```
css.c            CSS на C — единственный файл алгоритма (MIT)
css.py           читаемый перенос на Python с печатью решений по узлам (MIT)
bench/           стенд: замеры CSS против соперников, скрипт run.sh, сводка и рисунок (MIT)
bench/rivals/    исходники соперников под их собственными лицензиями (см. bench/rivals/README.md)
bench/ablation/  опыт «деление пополам против медианы выборки» (патч к css.c и программы замера)
results/         замеры, по которым написана статья (Intel Core i9-13900HK)
```

## Использование

```c
#define CSS_NO_MAIN
#include "css.c"

void css_sort(uint64_t *Q, uint64_t n);                                     // без таблицы прошлого массива
void css_sort_tab(uint64_t *Q, uint64_t n, const uint64_t *T, uint64_t S);  // с таблицей густоты T[0..S)
void css_sort_ex(uint64_t *Q, uint64_t n, const uint64_t *T, uint64_t S, int heap);  // heap = 1: буфер слияния n ключей
void css_table_from_sorted(const uint64_t *sorted, uint64_t n, uint64_t *T, uint64_t S); // S квантилей
```

Сортировка по возрастанию, неустойчивая, однопоточная; память — O(log n) стека и стековый буфер слияния 32 КБ.
Таблицу густоты строят из ранее отсортированного массива того же источника; таблицу, не согласную с выборкой
текущего массива по критерию Колмогорова–Смирнова, CSS отвергает. Корректность от таблицы не зависит.
`-DCSS_COUNT` включает счётчик сравнений (`css_cmp_count`, `css_cmp_reset`), `-DCSS_STATS` — статистику проходов.

Самопроверка без стенда:

```sh
gcc -O2 css.c -lm -o css && ./css          # 1176 запусков на разных входах; ./css 1000000 — ещё и время
python3 css.py                             # самопроверка и пример с печатью узлов
```

Число сравнений: в среднем на равномерных ключах n·log₂ n + O(n); в худшем случае не больше
n·(2.14·log₂ n + 2.29·w + 63), где w = ⌈log₂(max − min + 1)⌉ ≤ 64.

## Стенд

### Что нужно

* Linux x86-64, не меньше двух ядер (программы замеров закрепляют себя за одним логическим процессором);
* gcc и g++ (C11 и C++17, нужен `unsigned __int128`), binutils (`objcopy`), make, `patch`;
* Rust ≥ 1.81 с cargo — для ipnsort. Его исходник использует нестабильные возможности языка, поэтому Makefile
  собирает его обычным компилятором с переменной `RUSTC_BOOTSTRAP=1`. Сеть не нужна: у крейта нет зависимостей;
* python3; для рисунка — matplotlib.

### Сборка и проверка

```sh
cd bench
make                     # около минуты; программы — в bin/
bin/selftest             # самопроверка CSS
bin/ipn_check            # ipnsort из rivals/ против slice::sort_unstable текущего Rust: должно быть «совпали 30/30»
```

### Замеры

`./run.sh [каталог]` выполняет все замеры по порядку (около 40 минут) и пишет их в `../results/local/`
или в указанный каталог:

| Шаг | Программа | Файлы |
|---|---|---|
| самопроверка, пример с печатью узлов | `selftest`, `css.py` | example.txt |
| сверка ipnsort с Rust std | `ipn_check` | ipn_check.txt |
| время CSS, CSS с таблицей, pdqsort, crumsort, fluxsort, ipnsort в одном прогоне; n = 10⁵, 10⁶, 10⁷ | `bench_ipn time n R` | ipn_1e5.csv, ipn_1e6.csv, ipn_1e7.csv |
| то же на равномерных 64-битных ключах, n до 10⁸ | `bench_ipn hash n R` | ipn_hash.csv |
| сравнения на ключ | `bench_cmp cmp n` (CSS, pdqsort, crumsort, fluxsort), `bench_ipn cmp n` (ipnsort) | cmp_1e5.csv, cmp_1e6.csv, ipn_cmp_1e5.csv, ipn_cmp_1e6.csv |
| работа проходов по видам | `stats` | stats_1e6.txt |
| своя и чужая таблица густоты | `bench_mism n R` | mism_1e6.csv |
| std::sort и последовательный IPS4o | `bench_std time n R` | all_1e6.csv, all_1e7.csv |
| деление пополам против медианы выборки | `bench_med cmp/time n`, `bench_med_hash n R` | med_cmp*.csv, med_time*.csv, med_hash.txt |
| вход-противник (нижняя граница худшего случая) | `adv4b n`, `adv4 n` | adv.txt |
| сводка по группам входов и рисунок | `tables.py`, `chart_groups.py` | ipn_summary.txt, groups_en.png, groups_ru.png |

Формат CSV: первая строка — режим и n, вторая — заголовок (вход, группа, алгоритмы), дальше по строке на вход.
Время — нс на ключ, медиана R повторов; алгоритмы в каждом повторе идут в случайном порядке. Таблица
«прошлого массива» для CSS с таблицей — 4096 квантилей массива того же вида с другим зерном.

Входы (`bench/inputs.h`, `bench/killers.c`), 30 видов по четырём группам: 0 — случайные (random, random_n);
1 — упорядоченность и «убийцы» (sorted, reversed, nearly_sorted, pipeorgan, m3killer, pipeorganmx, sawtooth,
push_front, push_middle, unevensorted, sorted95); 2 — повторы (0to255, rep4, rep64, rep9010, binary, ternary,
quintary, dist20, zipf); 3 — неравномерная густота (unevenseq, outlier, powers2, logbits, pareto, lognormal,
normal, clusters8).

В `bench_med` столбцы: c2 — CSS (деление пополам), c1.9 — медиана выборки вместо деления пополам в режиме
значений, c2.1 — только медиана выборки, без режима значений; «+tab» — с таблицей прошлого массива. Варианты
собираются из `css.c` патчем `ablation/css_med.patch`, сам `css.c` не меняется.

### Выбор ядра

Каждая программа замера закрепляет себя за логическим процессором из переменной `CSS_CPU` (по умолчанию 1)
и печатает в stderr его номер и частоту после короткого разогрева: `CSS_CPU=6 ./run.sh`.

* На гибридных процессорах Intel (P- и E-ядра) нужен поток производительного ядра: список — `cat /sys/devices/cpu_core/cpus`,
  частоты — `lscpu -e=CPU,CORE,MAXMHZ`;
* второй поток того же ядра должен быть свободен (процессоры 0 и 1 часто делят одно ядро, а на 0 работает система);
* замеры запускаются по одному;
* ноутбук — от сети, в режиме производительности; во время длинного прогона стоит проверить, что частота не падает.

Абсолютные времена разных прогонов сравнимы, только если условия одинаковы; отношения внутри одного прогона
устойчивее. Числа сравнений CSS, pdqsort и ipnsort от прогона не зависят, у crumsort и fluxsort немного меняются.

## Результаты

`results/i9-13900HK/` — замеры статьи: Intel Core i9-13900HK (AVX2, процесс на производительном ядре 6, 5.4 ГГц),
gcc 13.3, Rust 1.99, ключи сборки `-O2 -march=native`. На другой машине абсолютные времена будут другими,
отношения — близкими.

## Лицензия

`css.c`, `css.py`, стенд и скрипты — MIT (файл `LICENSE`). Код в `bench/rivals/` распространяется под лицензиями
его авторов: pdqsort — zlib, crumsort и fluxsort — Unlicense, ipnsort — Apache 2.0, ips4o — BSD 2-Clause
(подробности и версии — `bench/rivals/README.md`). Как цитировать — `CITATION.cff`.
