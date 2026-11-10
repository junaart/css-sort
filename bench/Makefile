# SPDX-License-Identifier: MIT
# Стенд CSS: сборка программ замеров.
#   make         — всё (нужны gcc, g++, cargo; сеть не нужна)
#   make clean
# CSS берётся из ../css.c, соперники — из rivals/. Программы кладутся в bin/.

CC  ?= gcc
CXX ?= g++
OPT  = -O2 -march=native
CSS  = ../css.c
R    = rivals
LIBIPN = ipn/target/release/libipn.a

PROGS = selftest bench_ipn bench_cmp bench_mism bench_std stats adv4 adv4b ipn_check bench_med bench_med_hash
.PHONY: all clean
all: $(addprefix bin/,$(PROGS))

obj bin:
	mkdir -p $@

# ---------- соперники ----------
obj/crum_w.o: crum_w.c | obj
	$(CC) $(OPT) -I$(R)/crumsort -c $< -o $@
	objcopy --keep-global-symbol=crum_u64 --keep-global-symbol=crum_u64_cnt $@   # у crumsort и fluxsort общие внутренние имена
obj/flux_w.o: flux_w.c | obj
	$(CC) $(OPT) -I$(R)/fluxsort -c $< -o $@
	objcopy --keep-global-symbol=flux_u64 --keep-global-symbol=flux_u64_cnt $@
obj/pdq_w.o: pdq_w.cpp | obj
	$(CXX) $(OPT) -I$(R)/pdqsort -c $< -o $@
obj/rivals_std.o: rivals_std.cpp | obj
	$(CXX) $(OPT) -std=c++17 -Istub -I$(R) -c $< -o $@
$(LIBIPN): ipn/src/lib.rs ipn/Cargo.toml $(wildcard $(R)/ipnsort/src/*.rs)
	cd ipn && RUSTC_BOOTSTRAP=1 cargo build --release --offline

# ---------- входы и CSS ----------
obj/killers.o: killers.c killers.h | obj
	$(CC) $(OPT) -c $< -o $@
obj/%.o: %.c inputs.h | obj
	$(CC) $(OPT) -c $< -o $@
obj/css_t.o: css_t.c $(CSS) | obj
	$(CC) $(OPT) -c $< -o $@
obj/css_c.o: css_c.c $(CSS) | obj
	$(CC) $(OPT) -c $< -o $@

bin/selftest: $(CSS) | bin
	$(CC) $(OPT) $< -lm -o $@
bin/bench_ipn: obj/bench_ipn.o obj/css_t.o obj/pdq_w.o obj/crum_w.o obj/flux_w.o obj/killers.o $(LIBIPN) | bin
	$(CXX) $^ -lm -lpthread -ldl -o $@
bin/bench_cmp: obj/bench_cmp.o obj/css_t.o obj/css_c.o obj/killers.o obj/crum_w.o obj/flux_w.o obj/pdq_w.o | bin
	$(CXX) $^ -lm -o $@
bin/bench_mism: obj/bench_mism.o obj/css_t.o obj/css_c.o obj/killers.o | bin
	$(CXX) $^ -lm -o $@
bin/bench_std: obj/bench_std.o obj/css_t.o obj/killers.o obj/crum_w.o obj/flux_w.o obj/pdq_w.o obj/rivals_std.o | bin
	$(CXX) $^ -lm -lpthread -o $@
bin/stats: stats.c $(CSS) obj/killers.o | bin
	$(CC) $(OPT) stats.c obj/killers.o -lm -o $@
bin/adv4: adv4.c $(CSS) | bin
	$(CC) $(OPT) $< -lm -o $@
bin/adv4b: adv4b.c $(CSS) | bin
	$(CC) $(OPT) $< -lm -o $@
bin/ipn_check: ipn_check.c obj/killers.o $(LIBIPN) | bin
	$(CC) $(OPT) $^ -lm -lpthread -ldl -o $@

# ---------- опыт: деление пополам против медианы (ablation/) ----------
obj/css_med.c: $(CSS) ablation/css_med.patch | obj
	patch -s -o $@ $(CSS) ablation/css_med.patch
MED = 19 20 21
MEDOBJ = $(foreach k,$(MED),obj/med_t$(k).o obj/med_c$(k).o)
obj/med_%.o: ablation/%.c obj/css_med.c $(CSS) | obj
	$(CC) $(OPT) -c $< -o $@
obj/bench_med.o: ablation/bench_med.c inputs.h | obj
	$(CC) $(OPT) -c $< -o $@
obj/bench_med_hash.o: ablation/bench_med_hash.c | obj
	$(CC) $(OPT) -c $< -o $@
bin/bench_med: obj/bench_med.o $(MEDOBJ) obj/killers.o | bin
	$(CC) $^ -lm -o $@
bin/bench_med_hash: obj/bench_med_hash.o $(MEDOBJ) obj/pdq_w.o obj/crum_w.o | bin
	$(CXX) $^ -lm -o $@

clean:
	rm -rf obj bin ipn/target ipn/Cargo.lock
