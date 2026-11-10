# Соперники / Third-party sorts

Код в этом каталоге написан не авторами CSS и распространяется под лицензиями его авторов,
файлы которых лежат в каждом подкаталоге. Лицензия MIT репозитория на него не распространяется.

| Каталог | Что это | Источник и версия | Лицензия | Изменения |
|---|---|---|---|---|
| pdqsort | O. R. L. Peters, pattern-defeating quicksort | github.com/orlp/pdqsort, b1ef26a (2021-03-14) | zlib | нет |
| crumsort | Scandum, crumsort | github.com/scandum/crumsort, d21d7c3 (2024-07-27) | Unlicense | взят только src/ |
| fluxsort | Scandum, fluxsort | github.com/scandum/fluxsort, 93aac37 (2024-07-27) | Unlicense | взят только src/ |
| ipnsort | L. Bergdoll, O. Peters, ipnsort (= slice::sort_unstable в Rust ≥ 1.81) | github.com/Voultapher/sort-research-rs/tree/main/ipnsort, 8d3eae4 (2026-03-02) | Apache 2.0 | из Cargo.toml убраны тесты |
| ips4o | M. Axtmann и др., In-place Parallel Super Scalar Samplesort (используется последовательный режим) | github.com/ips4o/ips4o, 08a5b92 (2025-01-15) | BSD 2-Clause | взят только include/ |

Обёртки для C (`../pdq_w.cpp`, `../crum_w.c`, `../flux_w.c`, `../rivals_std.cpp`, `../ipn/`) и заглушка
`../stub/tbb/concurrent_queue.h`, без которой ips4o не собирается в последовательном режиме без TBB, —
часть репозитория под MIT.
