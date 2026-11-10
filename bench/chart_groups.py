# SPDX-License-Identifier: MIT
"""Рисунок по группам входов: время лучшего из pdqsort, crumsort, fluxsort, ipnsort / время CSS с таблицей,
n = 10^6, совместный прогон bench_ipn; внутри группы — по убыванию. Оттенки серого и штриховка — для чёрно-белой печати.
  python3 chart_groups.py ../results/i9-13900HK/ipn_1e6.csv groups.png en|ru [ширина высота шрифт dpi]"""
import csv, sys, matplotlib
matplotlib.use('Agg'); import matplotlib.pyplot as plt
from matplotlib.ticker import FixedLocator, FixedFormatter, NullLocator
from matplotlib.patches import Patch
src, out, lang = sys.argv[1], sys.argv[2], sys.argv[3] if len(sys.argv) > 3 else 'ru'
w, h, fs, dpi = (float(sys.argv[4]), float(sys.argv[5]), float(sys.argv[6]), int(sys.argv[7])) if len(sys.argv) > 7 else (3.45, 4.0, 7, 300)
NAMES = {'en': ['random', 'ordered and order killers', 'repeated values', 'nonuniform density'],
         'ru': ['случайные', 'упорядоченные и «убийцы» порядка', 'повторы', 'неравномерная густота']}[lang]
STYLE = [dict(color='#202020', hatch=''), dict(color='#7a7a7a', hatch=''), dict(color='#ffffff', hatch='////'), dict(color='#c8c8c8', hatch='')]
RIV = ['pdqsort', 'crumsort', 'fluxsort', 'ipnsort']
def best(x):
    v = [float(t) for t in x[4:8]]; i = v.index(min(v)); return v[i] / float(x[3]), RIV[i]
rows = [(x[0], int(x[1])) + best(x) for x in list(csv.reader(open(src)))[2:]]
plt.rcParams['font.family'] = 'DejaVu Serif'; plt.rcParams['hatch.linewidth'] = 0.6
fig, ax = plt.subplots(figsize=(w, h))
y = 0; ticks, labels, heads = [], [], []
for g in (0, 1, 2, 3):
    heads.append((y, NAMES[g])); y += 1
    for nm, _, q, who in sorted((r for r in rows if r[1] == g), key=lambda r: -r[2]):
        st = STYLE[g]
        ax.barh(y, q - 1, left=1, height=0.72, color=st['color'], hatch=st['hatch'], edgecolor='#202020', linewidth=0.4)
        if q > 1:   # CSS быстрее: число у конца полосы
            ax.text(q * 1.02, y, f'{q:.2f}', va='center', ha='left', fontsize=fs - 2)
        else:       # быстрее соперник: число и его имя справа от линии 1
            ax.text(1.02, y, f'{q:.2f} {who}', va='center', ha='left', fontsize=fs - 2)
        ticks.append(y); labels.append(nm); y += 1
    y += 0.3
qs = [r[2] for r in rows]; lo = min(0.4, min(qs) * 0.62)
ax.set_xscale('log'); ax.set_xlim(lo, 2.9); ax.axvline(1, color='black', lw=0.8, zorder=1)
for yy, t in heads:
    ax.text(lo * 1.03, yy, t, va='center', ha='left', fontsize=fs - 0.5, fontweight='bold', zorder=5,
            bbox=dict(facecolor='white', edgecolor='none', pad=0.6))
ax.set_yticks(ticks); ax.set_yticklabels(labels, fontsize=fs - 0.5); ax.set_ylim(y - 0.6, -0.7)
ax.tick_params(axis='y', length=0)
t = ([0.25] if lo < 0.25 else []) + [0.5, 0.75, 1, 1.5, 2]
ax.xaxis.set_major_locator(FixedLocator(t)); ax.xaxis.set_major_formatter(FixedFormatter([str(x) for x in t])); ax.xaxis.set_minor_locator(NullLocator())
ax.tick_params(axis='x', labelsize=fs)
ax.set_xlabel('Best rival time / CSS time (log scale)\nright of 1: CSS is faster; left: the faster rival is named' if lang == 'en'
              else 'Время лучшего соперника / время CSS (лог. шкала)\nправее 1 — CSS быстрее; левее — указан более быстрый соперник', fontsize=fs - 1.5)
for s in ('top', 'right', 'left'): ax.spines[s].set_visible(False)
fig.tight_layout(); fig.savefig(out, dpi=dpi)
