"""css.py — CSS: сортировка сравнениями построением порождающей последовательности на лету.

SPDX-License-Identifier: MIT
Copyright (c) 2026 Ю. Н. Артамонов (Yu. N. Artamonov) А.Д. Степанов (A.D.Stepanov)

Читаемый перенос алгоритма css.c на Python. Структура и решения те же:
  * слой упорядоченных участков: монотонные участки с выбросами, модель стоимости,
    развороты, слияние в порядке powersort;
  * корень ядра: подсчёт частых значений по выборке или первое разбиение по медиане выборки
    с попутными min/max; выбор таблицы густоты (прошлого массива или выборки корня);
  * ядро: в каждом узле [lo, hi] строится первый элемент порождающей последовательности —
    порог t (c = 2, медиана массы по таблице густоты или медиана выборки), серии равных,
    запас перекосов min(log2 n, 2) и запасное деление пополам, проверка упорядоченности.
Отличия от C, не меняющие логики: разбиение — сборкой двух списков (в C — блочное без
ветвлений, на месте), листья — вставками (в C — сети сортировки), слияние — простое
двухпутевое (в C — галоп, четыре цепочки, повороты). Поэтому числа сравнений близки, но не
равны числам C. Параметры (размер листа, пороги, размеры выборок) собраны в Params: для
наглядных примеров на малых массивах их можно уменьшить.

Использование:
    from css import css_sort, css_table_from_sorted
    css_sort(a)                                 # сортирует список a на месте
    T = css_table_from_sorted(sorted(prev), 4096)
    css_sort(b, table=T)                        # с таблицей густоты прошлого массива
    css_sort(a, trace=print)                    # с печатью решений по узлам
Запуск файла — самопроверка на разных входах и пример с печатью узлов.
"""

import math
import random
from dataclasses import dataclass

GOLD = 0x9E3779B97F4A7C15
M64 = (1 << 64) - 1


@dataclass
class Params:
    ins: int = 24            # лист: участок короче — вставками
    tab_min: int = 1024      # узлы короче таблицей не пользуются
    try_k: int = 4           # неудачных досортировок вставками на пути элемента не больше
    budget_max: int = 2      # запас перекосов = min(floor(log2 n), budget_max)
    few: int = 1             # проверка упорядоченности: переставлено не больше пар
    sample_big: int = 4096   # с этой длины порог по выборке — медиана 31 (127 с 65536) элементов
    tab_flat: float = 1.6    # выборка корня почти равномерна: KS·sqrt(s) меньше — не брать
    ks_c: float = 2.25       # таблица прошлого массива согласна с выборкой: KS·sqrt(s) не больше
    cpd_min_n: int = 2048    # корень с частыми значениями — с этого n
    cpd_hmax: int = 32       # частых значений не больше
    cpd_ce: float = 1.8      # стоимость подсчёта частых, нс на элемент
    cpd_depth: int = 2       # глубина рекурсии подсчёта
    root_few_n: int = 4096   # «мало различных» по выборке корня — с этого n (малые сегменты)
    # слой участков
    cap: int = 4096
    segmax: int = 1024
    rfloor: int = 64
    k_absorb: int = 16
    xfrac: int = 8
    xmin: int = 32
    xmax: int = 3
    win: int = 64
    nwin: int = 32
    min_n: int = 256
    hmin: float = 3.0
    fixed: float = 100.0
    gate_n: int = 1024
    sample_n: int = 512
    cx: float = 1.4
    cv: float = 0.4
    margin: float = 0.9
    cq0: float = 1.5
    cq: float = 1.15
    cm: float = 1.1
    cms: float = 0.25
    cr0: float = 1.3
    cr: float = 0.27


class Sorter:
    def __init__(self, Q, params=None, table=None, trace=None):
        self.Q = Q
        self.p = params or Params()
        self.prev = table if table and len(table) >= 2 else None
        self.trace = trace
        self.cmp = 0                         # число сравнений ключей (приближённо как в C)

    def log(self, depth, msg):
        if self.trace:
            self.trace("  " * depth + msg)

    # ------------------------------------------------------------------ таблицы густоты
    def tab_lb(self, T, a, b, v):
        """первый i из [a, b) с T[i] >= v"""
        while a < b:
            m = (a + b) >> 1
            self.cmp += 1
            if T[m] < v:
                a = m + 1
            else:
                b = m
        return a

    def tab_useful(self, T, lo, hi):
        """выборка корня T как таблица на [lo, hi]: не почти ли она равномерна"""
        s = len(T)
        if s < 31 or hi <= lo:
            return False
        W = hi - lo
        D = max(abs((T[i] - lo) / W - (i + 0.5) / s) for i in range(s))
        return D * math.sqrt(s) > self.p.tab_flat

    def tab_far(self, P, ta, tb, lo, hi, s):
        """таблица прошлого массива P[ta..tb) на [lo, hi]: отклонение от равномерной заметно
        на масштабе выборки сегмента (s элементов); не больше 256 точек"""
        m = tb - ta
        if m < 2 or hi <= lo:
            return False
        step, W = max(1, m // 256), hi - lo
        D = max(abs((P[ta + i] - lo) / W - (i + 0.5) / m) for i in range(0, m, step))
        return D * math.sqrt(s) > self.p.tab_flat

    def tab_agrees(self, P, B):
        """таблица прошлого массива P против отсортированной выборки B: расстояние Колмогорова"""
        S, s, D, i = len(P), len(B), 0.0, 0
        while i < s:
            j = i
            while j < s and B[j] == B[i]:
                j += 1
            a = self.tab_lb(P, 0, S, B[i])
            b = self.tab_lb(P, a, S, B[i] + 1)
            F, e = (a + b) / (2.0 * S), (i + j) / (2.0 * s)
            D = max(D, abs(F - e))
            i = j
        return D * math.sqrt(s) <= self.p.ks_c

    # ------------------------------------------------------------------ ядро
    def insertion(self, x, y):
        Q = self.Q
        for i in range(x + 1, y + 1):
            v, j = Q[i], i - 1
            while j >= x:
                self.cmp += 1
                if Q[j] <= v:
                    break
                Q[j + 1] = Q[j]
                j -= 1
            Q[j + 1] = v

    def partial_insertion(self, x, y, limit):
        """вставки с лимитом перемещений: True — участок досортирован"""
        Q, moved = self.Q, 0
        for i in range(x + 1, y + 1):
            self.cmp += 1
            if Q[i] < Q[i - 1]:
                v, j = Q[i], i
                while True:
                    Q[j] = Q[j - 1]
                    j -= 1
                    if j <= x:
                        break
                    self.cmp += 1
                    if not Q[j - 1] > v:
                        break
                Q[j] = v
                moved += i - j
                if moved > limit:
                    return False
        return True

    def try_presorted(self, x, z, y, sw):
        if sw > self.p.few:
            return 0
        r = 0
        if z - 1 > x:
            if self.partial_insertion(x, z - 1, z - x if sw else 8):
                r |= 1
        else:
            r |= 1
        if y > z:
            if self.partial_insertion(z, y, y - z + 1 if sw else 8):
                r |= 2
        else:
            r |= 2
        return r

    def partition(self, x, y, t):
        """[x, z) < t, [z, y] >= t; sw — сколько элементов слева стояли не на своей стороне"""
        seg = self.Q[x:y + 1]
        self.cmp += len(seg)
        left = [v for v in seg if v < t]
        right = [v for v in seg if v >= t]
        nl = len(left)
        sw = sum(1 for v in seg[:nl] if v >= t)
        self.Q[x:y + 1] = left + right
        return x + nl, sw

    def minmax(self, x, y):
        seg = self.Q[x:y + 1]
        self.cmp += 2 * (len(seg) - 1)
        return min(seg), max(seg)

    def sample_median(self, x, n, s):
        buf = sorted(self.Q[x + (i * (n - 1)) // (s - 1)] for i in range(s))
        self.cmp += s * int(math.log2(s))
        return buf[s // 2]

    @staticmethod
    def med3(a, b, c):
        return sorted((a, b, c))[1]

    def break_pattern(self, x, y):
        Q, n = self.Q, y - x + 1
        if n < self.p.ins:
            return
        q = n // 4
        pairs = [(x, x + q), (y, y - q)]
        if n > 128:
            pairs += [(x + 1, x + q + 1), (x + 2, x + q + 2), (y - 1, y - q - 1), (y - 2, y - q - 2)]
        for a, b in pairs:
            Q[a], Q[b] = Q[b], Q[a]

    def core(self, x, y, lo, hi, bad_allowed, vmode, T=None, ta=0, tb=0, depth=0, tr=None):
        """узел [x, y] с границами значений [lo, hi]; рекурсия в меньшую часть, большая — в цикле"""
        Q, p = self.Q, self.p
        if tr is None:
            tr = p.try_k
        while True:
            n = y - x + 1
            if n < p.ins:
                if x < y:
                    self.insertion(x, y)
                    self.log(depth, f"лист [{x},{y}]: вставки")
                return
            if lo >= hi:
                self.log(depth, f"[{x},{y}]: все значения равны {lo}")
                return
            if n < p.tab_min:
                T = None                              # малый узел: таблица не нужна
            bisect = not vmode and bad_allowed <= 0
            eq_run = False
            if vmode and T is not None and tb - ta >= 2:
                t = max(T[(ta + tb) >> 1], lo + 1)
                how = f"медиана массы по таблице, точки [{ta},{tb})"
            elif vmode or bisect:
                w = hi - lo
                t = lo + (w - w // 2)                 # первый элемент последовательности при c = 2
                how = "деление пополам (c = 2)" + (", запасное" if bisect else "")
            else:
                if n >= p.sample_big:
                    pv = self.sample_median(x, n, 127 if n >= 65536 else 31)
                else:
                    m = x + n // 2
                    if n > 128:
                        self.cmp += 12
                        pv = self.med3(self.med3(Q[x], Q[m], Q[y]), self.med3(Q[x + 1], Q[m - 1], Q[y - 1]),
                                       self.med3(Q[x + 2], Q[m + 1], Q[y - 2]))
                    else:
                        self.cmp += 3
                        pv = self.med3(Q[x], Q[m], Q[y])
                if pv <= lo:
                    t, eq_run = lo + 1, True
                    how = f"серия равных {lo} (b = 1)"
                else:
                    t = pv
                    how = "медиана выборки"
            z, sw = self.partition(x, y, t)
            nl, nr = z - x, y - z + 1
            self.log(depth, f"узел [{x},{y}] значения [{lo},{hi}]: порог t = {t} — {how}; "
                            f"слева {nl}, справа {nr}")
            if eq_run:
                if 8 * nl < n:
                    bad_allowed -= 1
                x, lo = z, t
                continue
            if nl == 0 or nr == 0:                     # вырожденный проход
                if vmode or bisect:
                    lo, hi = self.minmax(x, y)
                    vmode = False
                    self.log(depth, f"  пустая сторона: новые границы [{lo},{hi}], режим выборки")
                else:
                    lo = t                            # p — минимум; проход потрачен, расходует запас
                    bad_allowed -= 1
                continue
            tm = self.tab_lb(T, ta, tb, t) if vmode and T is not None and tb > ta else ta
            q = min(nl, nr)
            if vmode and 10 * q < n:
                vmode = False
                self.log(depth, "  перекос: поддерево переходит в режим выборки")
            bad = 8 * q < n
            done, trL, trR = 0, tr, tr
            if not vmode and bad and not bisect:
                bad_allowed -= 1
                self.break_pattern(x, z - 1)
                self.break_pattern(z, y)
            elif not bisect and tr > 0:
                done = self.try_presorted(x, z, y, sw)
                if sw <= p.few:                       # неудача расходует запас досортировок части
                    trL -= 0 if done & 1 else 1
                    trR -= 0 if done & 2 else 1
            if done == 3:
                self.log(depth, "  обе части уже упорядочены")
                return
            if done == 1:
                x, lo, ta, tr = z, t, tm, trR
                continue
            if done == 2:
                y, hi, tb, tr = z - 1, t - 1, tm, trL
                continue
            if nl < nr:
                self.core(x, z - 1, lo, t - 1, bad_allowed, vmode, T, ta, tm, depth + 1, trL)
                x, lo, ta, tr = z, t, tm, trR
            else:
                self.core(z, y, t, hi, bad_allowed, vmode, T, tm, tb, depth + 1, trR)
                y, hi, tb, tr = z - 1, t - 1, tm, trL

    # ------------------------------------------------------------------ корень ядра
    def sample(self, s, e, S):
        """S элементов в псевдослучайных позициях внутри равных шагов"""
        n, step = e - s, (e - s) // S
        return [self.Q[s + i * step + ((((i + 1) * GOLD & M64) >> 32) * step >> 32)] for i in range(S)]

    def root_small(self, s, e, depth=0):
        """малый сегмент: min/max, «мало различных» по выборке, таблица — выборка корня"""
        n = e - s
        if n < 2:
            return
        lo, hi = self.minmax(s, e - 1)
        if lo == hi:
            return
        lg = min(n.bit_length() - 1, self.p.budget_max)   # запас перекосов
        few, T = False, None
        if n >= self.p.root_few_n:
            S = 127 if n >= 65536 else 31
            buf = sorted(self.Q[s + i * (n // S) + ((((i + 1) * GOLD & M64) >> 33) % (n // S))] for i in range(S))
            few = len(set(buf)) <= S // 8
            if not few and self.tab_useful(buf, lo, hi):
                T = buf
        if T is not None:
            ta, tb = self.tab_lb(T, 0, len(T), lo), self.tab_lb(T, 0, len(T), hi + 1)
            self.core(s, e - 1, lo, hi, lg, True, T, ta, tb, depth)
        else:
            self.core(s, e - 1, lo, hi, lg, not few, None, 0, 0, depth)

    def cpd_sort(self, s, e, depth=0):
        """корень ядра: частые значения считаются, иначе первое разбиение по медиане выборки"""
        p, Q, n = self.p, self.Q, e - s
        if n < p.cpd_min_n:
            self.root_small(s, e, depth)
            return
        S = 255 if n >= (1 << 18) else 127 if n >= (1 << 14) else 63
        if S > n:
            S = n
        hcnt = 4 if S >= 255 else 3
        buf = sorted(self.sample(s, e, S))
        self.cmp += S * max(1, int(math.log2(S)))
        groups, i = [], 0                              # группы равных в выборке
        while i < S:
            j = i + 1
            while j < S and buf[j] == buf[i]:
                j += 1
            groups.append((buf[i], j - i))
            i = j
        lgn = math.log2(n)
        Hs = sum(c / S * math.log2(S / c) for _, c in groups if c > 1)
        Hs += sum(1 for _, c in groups if c == 1) / S * lgn
        Hs = min(Hs, lgn)
        few = len(groups) <= S // 8
        freq = [(v, c) for v, c in groups if c >= hcnt]
        if len(freq) > p.cpd_hmax:
            freq = sorted(sorted(freq, key=lambda vc: -vc[1])[:p.cpd_hmax])
        F = sum(c for _, c in freq) / S
        if freq and depth < p.cpd_depth and F * (p.cq0 + p.cq * Hs) > p.cpd_ce:
            # подсчёт частых значений: один проход, прочие — подряд в начало
            fv = {v: 0 for v, _ in freq}
            rest = []
            for v in Q[s:e]:
                if v in fv:
                    fv[v] += 1
                else:
                    rest.append(v)
            self.cmp += n * min(len(fv), 4) if len(fv) <= 3 else n
            self.log(depth, f"корень [{s},{e}): частые значения {sorted(fv.items())}, прочих {len(rest)}")
            w = s + len(rest)
            Q[s:w] = rest
            if w - s > 1:
                self.cpd_sort(s, w, depth + 1)
            # расстановка: серии частых значений между отсортированными прочими
            others, out, k = Q[s:w], [], 0
            for v in sorted(fv):
                while k < len(others) and others[k] <= v:
                    out.append(others[k]); k += 1
                out.extend([v] * fv[v])
            out.extend(others[k:])
            Q[s:e] = out
            return
        # первое разбиение по медиане выборки, попутно min и max
        pv = buf[S // 2]
        t = pv
        if pv == buf[0]:
            t = pv + 1
        seg = Q[s:e]
        self.cmp += n
        mn, mx = min(seg), max(seg)
        z, _ = self.partition(s, e - 1, t)
        if mn == mx:
            return
        lg = min(n.bit_length() - 1, self.p.budget_max)   # запас перекосов
        vmode = not few
        TT, src = None, "нет (деление пополам)"
        if vmode and self.prev is not None and self.tab_agrees(self.prev, buf):
            a = self.tab_lb(self.prev, 0, len(self.prev), mn)
            b = self.tab_lb(self.prev, a, len(self.prev), mx + 1)
            if self.tab_far(self.prev, a, b, mn, mx, S):
                TT, src = self.prev, f"прошлого массива ({len(self.prev)} квантилей)"
            else:
                src = "прошлого массива почти равномерна — деление пополам"
        if TT is None and vmode and self.tab_useful(buf, mn, mx):
            TT, src = buf, f"выборка корня ({S} точек)"
        self.log(depth, f"корень [{s},{e}): выборка {S}, медиана {t}, min {mn}, max {mx}; "
                        f"режим {'значений' if vmode else 'выборки'}; таблица: {src}")
        ta = tb = tm = 0
        if TT is not None:
            ta = self.tab_lb(TT, 0, len(TT), mn)
            tb = self.tab_lb(TT, 0, len(TT), mx + 1)
            tm = self.tab_lb(TT, ta, tb, t)
        if z == s or z == e:
            self.core(s, e - 1, mn, mx, lg, vmode, TT, ta, tb, depth)
            return
        if z - s >= 2:
            self.core(s, z - 1, mn, t - 1, lg, vmode, TT, ta, tm, depth + 1)
        if e - z >= 2:
            self.core(z, e - 1, t, mx, lg, vmode, TT, tm, tb, depth + 1)

    # ------------------------------------------------------------------ слой участков
    def run_end(self, i, n):
        Q = self.Q
        if i + 1 >= n:
            return n, False
        desc = Q[i] > Q[i + 1]
        j = i + 1
        while j < n and (Q[j - 1] >= Q[j] if desc else Q[j - 1] <= Q[j]):
            j += 1
        self.cmp += j - i
        return j, desc

    def asc_end(self, i, n):
        j, _ = self.run_end_asc(i, n)
        return j

    def run_end_asc(self, i, n):
        Q, j = self.Q, i + 1
        while j < n and Q[j - 1] <= Q[j]:
            j += 1
        self.cmp += j - i
        return j, False

    def asc_run_x(self, s, r0, n, dry):
        """неубывающий участок от s через выбросы: (r, w) — [s, w) сжатый участок, [w, r) — выбросы"""
        Q, p = self.Q, self.p
        r = w = r0
        if r >= n or r0 - s < p.xmin:
            return r, w
        L = [Q[r0 - 1], Q[r0 - 2], Q[r0 - 3], Q[r0 - 4]]   # последние четыре взятых, L[0] — последний
        kn = 4
        while r < n:
            ln, no = w - s, r - w
            if ln < p.xmin:
                break
            bp, bd, bv = -1, 0, 0
            for t in range(1, p.xmax + 1):
                if bp >= 0 or (no + t) * p.xfrac > ln - t:
                    break
                for pp in range(t, -1, -1):
                    d = t - pp
                    if pp >= kn or r + d >= n:
                        continue
                    q = Q[r + d]
                    self.cmp += 1
                    if q >= L[pp] and (bp < 0 or q < bv):
                        bp, bd, bv = pp, d, q
            if bp < 0:
                break
            w -= bp; r += bd; kn -= bp
            L = L[bp:] + [0] * bp
            while r < n:
                xv = Q[r]
                self.cmp += 1
                if xv < L[0]:
                    break
                if not dry:
                    Q[r] = Q[w]; Q[w] = xv
                w += 1; r += 1
                L = [xv] + L[:3]
                kn += kn < 4
        return r, w

    def sample_monotone(self, n):
        Q, p = self.Q, self.p
        W, nw = (16, 8) if n < 8192 else (p.win, p.nwin)
        if nw * 2 * (W + 1) > n:
            nw = n // (2 * (W + 1))
        if nw < 4:
            return False
        step, slack = n // nw, n // nw - (W + 1)
        good = 0
        for k in range(nw):
            b = k * step + ((((k + 1) * GOLD & M64) >> 32) * slack >> 32)
            d = u = 0
            for i in range(0, W, 8):
                for q in range(i, i + 8):
                    d += Q[b + q] > Q[b + q + 1]
                    u += Q[b + q] < Q[b + q + 1]
                self.cmp += 16
                if d and u:
                    break
            good += (d == 0 and u * 4 >= W) or (u == 0 and d * 4 >= W)
        return good * 4 >= nw

    def entropy(self, n):
        s = 127 if n >= 1048576 else 63 if n >= 65536 else 31 if n >= 4096 else 15
        buf = sorted(self.sample(0, n, s))
        self.cmp += s * int(math.log2(s))
        H, single, i = 0.0, 0, 0
        while i < s:
            j = i + 1
            while j < s and buf[j] == buf[i]:
                j += 1
            if j - i == 1:
                single += 1
            else:
                H += (j - i) / s * math.log2(s / (j - i))
            i = j
        return H + single / s * math.log2(n)

    def descent_rate(self, n):
        Q, nw, W = self.Q, 8, 32
        if nw * (W + 1) > n:
            nw = n // (W + 1)
        if nw < 1:
            return 0.0
        step, slack = n // nw, max(1, n // nw - (W + 1))
        d = 0
        for k in range(nw):
            b = k * step + ((((k + 7) * GOLD & M64) >> 32) * slack >> 32)
            d += sum(Q[b + i] > Q[b + i + 1] for i in range(W))
            self.cmp += W
        return d / (nw * W)

    def merge(self, a, m, b):
        """слияние [a, m) и [m, b); в C — обрезка, галоп, четыре цепочки, повороты"""
        Q = self.Q
        if m <= a or b <= m or Q[m - 1] <= Q[m]:
            self.cmp += 1
            return
        A, B, out, i, j = Q[a:m], Q[m:b], [], 0, 0
        while i < len(A) and j < len(B):
            self.cmp += 1
            if B[j] < A[i]:
                out.append(B[j]); j += 1
            else:
                out.append(A[i]); i += 1
        out += A[i:] + B[j:]
        Q[a:b] = out

    @staticmethod
    def power(s1, n1, n2, n):
        r, a = 0, 2 * s1 + n1
        b = a + n1 + n2
        while True:
            r += 1
            if a >= n:
                a -= n; b -= n
            elif b >= n:
                break
            a <<= 1; b <<= 1
        return r

    def qs_cost(self, u):
        return 0 if u < 2 else u * (self.p.cq0 + self.p.cq * math.log2(u))

    def merge_cost(self, la, lb):
        p = self.p
        s, T = min(la, lb), la + lb
        c = T * (p.cm + (p.cms * math.log2(T / 16384) if T > 16384 else 0))
        if s > p.cap:
            ch = (T - s) + s * s / (2 * p.cap)
            sp = T / 2 * math.log2(s / p.cap) + T
            c += T * p.cr0 + p.cr * min(ch, sp)
        return c

    def powersort(self, n, bd, sim):
        """слияние сегментов bd[0] = 0 < ... < bd[k] = n в порядке powersort; sim — только стоимость"""
        k, cost = len(bd) - 1, 0.0
        ss, se, sp = [bd[0]], [bd[1]], []
        for t in range(1, k + 1):
            last = t == k
            pw = 0 if last else self.power(ss[-1], se[-1] - ss[-1], bd[t + 1] - bd[t], n)
            while len(ss) > 1 and (last or sp[-1] > pw):
                a, m, b = ss[-2], ss[-1], se[-1]
                if sim:
                    cost += self.merge_cost(m - a, b - m)
                else:
                    self.merge(a, m, b)
                se[-2] = b
                ss.pop(); se.pop(); sp.pop()
            if last:
                break
            sp.append(pw)
            ss.append(bd[t]); se.append(bd[t + 1])
        return cost

    def run_mode(self, n, r0, desc0, H, outl):
        """разметка участков, модель стоимости, сжатие, ядро для неупорядоченных, слияние"""
        p, Q = self.p, self.Q
        R = max(p.rfloor, n // (p.segmax // 2 - 2) + 1)
        skip = max(R // 4, 16)
        st, so = [], []                               # so: 0 неупоряд., 1 неубыв., 2 с выбросами, 3 невозраст.

        def add_unsorted(pos):
            if not so or so[-1]:
                st.append(pos); so.append(0)
        i = 0
        while i < n:
            if i == 0:
                j, desc = r0, desc0
            else:
                j, desc = self.run_end(i, n)
            if desc or not outl:
                w = j
            else:
                j, w = self.asc_run_x(i, j, n, True)
            if w - i >= R:
                st.append(i); so.append(3 if desc else (2 if j > w else 1))
                if j > w:
                    add_unsorted(w)
            else:
                add_unsorted(i)
                if j - st[-1] >= R:
                    j = max(j, min(i + skip, n))
            i = j
        st.append(n)
        if not any(so):
            return False
        k = len(so)
        for t in range(k):                            # поглощение коротких отсортированных
            if not so[t]:
                continue
            s_len, u = st[t + 1] - st[t], 0
            if t > 0 and not so[t - 1]:
                u += st[t] - st[t - 1]
            if t + 1 < k and not so[t + 1]:
                u += st[t + 2] - st[t + 1]
            if s_len * p.k_absorb < u:
                so[t] = 0
        st2, so2 = [], []
        for t in range(k):
            if so2 and not so[t] and not so2[-1]:
                continue
            st2.append(st[t]); so2.append(so[t])
        st, so = st2 + [n], so2
        k = len(so)
        if not any(so):
            return False
        plan = p.fixed
        for t in range(k):
            ln = st[t + 1] - st[t]
            plan += self.qs_cost(ln) if not so[t] else p.cx * ln if so[t] == 2 else p.cv * ln if so[t] == 3 else 0
        if k > 1:
            plan += self.powersort(n, st, True)
        lg = math.log2(n)
        base = n * (p.cq0 + p.cq * min(H, lg))
        names = {0: "неупорядоченный", 1: "неубывающий", 2: "неубывающий с выбросами", 3: "невозрастающий"}
        self.log(0, "сегменты: " + ", ".join(f"[{st[t]},{st[t + 1]}) {names[so[t]]}" for t in range(k)))
        self.log(0, f"модель стоимости: план {plan:.0f} против ядра {base:.0f}")
        if plan >= p.margin * base:
            return False
        for t in range(k):
            if so[t] == 3:
                Q[st[t]:st[t + 1]] = Q[st[t]:st[t + 1]][::-1]
            elif so[t] == 2:
                e0 = self.asc_end(st[t], n)
                self.asc_run_x(st[t], e0, n, False)
        for t in range(k):
            if not so[t]:
                self.cpd_sort(st[t], st[t + 1], 1)
        if k > 1:
            self.powersort(n, st, False)
        return True

    # ------------------------------------------------------------------ вход
    def sort(self):
        p, Q, n = self.p, self.Q, len(self.Q)
        if n < 2:
            return
        r0, desc = self.run_end(0, n)
        if r0 == n:
            if desc:
                Q.reverse()
            self.log(0, "массив уже упорядочен")
            return
        likely = r0 * 8 >= n or (n >= p.sample_n and self.sample_monotone(n))
        if n >= p.min_n and likely:
            H = self.entropy(n) if n >= p.gate_n else math.log2(n)
            go = True
            if H < p.hmin:
                lg = math.log2(n)
                base = n * (p.cq0 + p.cq * min(H, lg))
                kest = n * self.descent_rate(n)
                est = p.fixed + (p.cm * n * math.log2(kest) if kest >= 2 else 0)
                go = est < p.margin * base
            if go and self.run_mode(n, r0, desc, H, H >= p.hmin):
                return
        if n >= p.cpd_min_n:
            self.cpd_sort(0, n, 0)
        else:
            self.root_small(0, n, 0)


def css_sort(Q, table=None, params=None, trace=None):
    """Сортирует список натуральных чисел Q на месте; возвращает число сравнений ключей."""
    s = Sorter(Q, params, table, trace)
    s.sort()
    return s.cmp


def css_table_from_sorted(sorted_vals, S=4096):
    """S квантилей уровней (i + 1/2)/S отсортированного массива — таблица густоты"""
    n = len(sorted_vals)
    S = min(S, n)
    return [sorted_vals[((2 * i + 1) * n) // (2 * S)] for i in range(S)]


# ---------------------------------------------------------------------- самопроверка
def _inputs(n, rnd):
    yield "random", [rnd.getrandbits(64) for _ in range(n)]
    yield "sorted", list(range(n))
    yield "reversed", list(range(n, 0, -1))
    a = list(range(n))
    for _ in range(n // 100 + 1 if n else 0):
        i, j = rnd.randrange(n), rnd.randrange(n)
        a[i], a[j] = a[j], a[i]
    yield "nearly_sorted", a
    yield "pipeorgan", [i if i < n // 2 else n - i for i in range(n)]
    yield "sawtooth", [i % 1000 for i in range(n)]
    yield "few_values", [rnd.randrange(16) for _ in range(n)]
    yield "binary", [rnd.randrange(2) for _ in range(n)]
    yield "lognormal", [int(math.exp(20 + 2 * rnd.gauss(0, 1))) for _ in range(n)]
    yield "clusters", [rnd.randrange(8) * 10 ** 12 + rnd.randrange(10 ** 6) for _ in range(n)]
    yield "powers2", [1 << rnd.randrange(64) for _ in range(n)]
    b = [7 * i for i in range(n)]
    for i in range(n - n // 20, n):
        b[i] = rnd.randrange(7 * n)
    yield "sorted95", b


def _selftest():
    rnd = random.Random(1)
    runs = errs = 0
    for n in (0, 1, 2, 3, 10, 23, 24, 25, 100, 300, 1000, 2047, 2048, 5000, 20000, 70000, 200000):
        for name, a in _inputs(n, rnd):
            prev = sorted(dict(_inputs(max(n, 16), random.Random(2)))[name])
            for table in (None, css_table_from_sorted(prev)):
                b = list(a)
                css_sort(b, table)
                runs += 1
                if b != sorted(a):
                    errs += 1
                    print("ОШИБКА", name, n, table is not None)
    print(f"самопроверка: {runs} запусков, ошибок {errs}")
    return errs == 0


def _example():
    """Пример статьи (табл. II): 16 чисел, лист — до 3 элементов, без таблицы и с таблицей прошлого массива"""
    a = [3, 41, 0, 7, 12, 1, 63, 5, 2, 19, 9, 0, 28, 4, 6, 1]
    prev = [0, 0, 1, 1, 2, 3, 4, 5, 6, 8, 10, 13, 17, 24, 35, 60]
    T = css_table_from_sorted(prev, 8)
    p = Params(ins=4, tab_min=0)
    print("\nПример: вход", a, "\nтаблица прошлого массива T =", T)
    for tab in (None, T):
        b = list(a)
        s = Sorter(b, p, None, print)
        lo, hi = min(b), max(b)
        print("\n--- без таблицы (c = 2)" if tab is None else "\n--- с таблицей T")
        if tab is None:
            s.core(0, len(b) - 1, lo, hi, 4, True)
        else:
            s.core(0, len(b) - 1, lo, hi, 4, True, tab, s.tab_lb(tab, 0, len(tab), lo), s.tab_lb(tab, 0, len(tab), hi + 1))
        print("выход:", b, "верно:", b == sorted(a))


if __name__ == "__main__":
    ok = _selftest()
    _example()
    raise SystemExit(0 if ok else 1)
