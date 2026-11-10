# SPDX-License-Identifier: MIT
"""Сводка совместного прогона bench_ipn (CSS, pdqsort, crumsort, fluxsort, ipnsort):
среднее геометрическое по группам, выигрыш CSS и число входов, где CSS быстрее.
  python3 tables.py ../results/i9-13900HK
Читает ipn_1e5.csv, ipn_1e6.csv, ipn_1e7.csv и ipn_hash.csv из указанного каталога."""
import csv, math, os, sys
D = sys.argv[1] if len(sys.argv) > 1 else '../results/local'
gm = lambda xs: math.exp(sum(math.log(x) for x in xs) / len(xs))
G = {0: 'случайные', 1: 'порядок', 2: 'повторы', 3: 'густота', None: 'все 30'}
H = ['css', 'css+tab', 'pdqsort', 'crumsort', 'fluxsort', 'ipnsort']
for n in ('1e5', '1e6', '1e7'):
    f = os.path.join(D, f'ipn_{n}.csv')
    if not os.path.exists(f):
        continue
    d = [(x[0], int(x[1]), [float(v) for v in x[2:]]) for x in list(csv.reader(open(f)))[2:]]
    print(f'\nn = {n}: среднее геометрическое, нс на элемент')
    print('%-12s' % 'группа' + ''.join('%10s' % h for h in H))
    for g in (0, 1, 2, 3, None):
        s = [v for _, gg, v in d if g is None or gg == g]
        print('%-12s' % G[g] + ''.join('%10.2f' % gm([v[i] for v in s]) for i in range(6)))
    c = gm([v[1] for _, _, v in d])
    for i in range(2, 6):
        r = gm([v[i] for _, _, v in d])
        print(f'  CSS с таблицей против {H[i]}: на {100 * (1 - c / r):.0f}% меньше времени, быстрее на {sum(v[1] < v[i] for _, _, v in d)} входах из 30')
    best = sum(v[1] < min(v[2:6]) for _, _, v in d)
    print(f'  быстрее лучшего из четырёх соперников на {best} входах из 30')
f = os.path.join(D, 'ipn_hash.csv')
if os.path.exists(f):
    print('\nхэши, нс на элемент: n, ' + ', '.join(H))
    for x in csv.reader(open(f)):
        print('  ' + ', '.join(x[:1] + x[2:]))
