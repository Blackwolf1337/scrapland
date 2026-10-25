#!/usr/bin/env python3
"""Decompilation progress: functions replaced by C (src/hooks.inc) against the functions of Scrap.exe
(symbols/functions.tsv). The script API modules are listed one by one.

usage: progress.py
"""
import collections
import os
import re

ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..')
MODULES = ('Scrap', 'SAct', 'SAI', 'SFX', 'SInput', 'SLogic', 'SNet', 'SScorer', 'SSound', 'SVec', 'SWeap',
           'cPyEntity')


def main():
    hooks = set(re.findall(r'^SD_HOOK\((\w+),', open(os.path.join(ROOT, 'src', 'hooks.inc')).read(), re.M))
    total = collections.Counter()
    done = collections.Counter()
    nfunc = 0
    for line in open(os.path.join(ROOT, 'symbols', 'functions.tsv')).read().split('\n')[1:]:
        f = line.split('\t')
        if len(f) < 5:
            continue
        nfunc += 1
        m = re.match(r'(\w+?)_', f[4])
        if m and m.group(1) in MODULES:
            total[m.group(1)] += 1
            done[m.group(1)] += f[4] in hooks
    for mod in MODULES:
        print(f'{mod:10} {done[mod]:4} / {total[mod]:4}')
    print(f'{"script API":10} {sum(done.values()):4} / {sum(total.values()):4}')
    print(f'{"all":10} {len(hooks):4} / {nfunc} functions in the exe (CRT and libraries included)')


if __name__ == '__main__':
    main()
