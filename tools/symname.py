#!/usr/bin/env python3
"""Sets hand-made names in symbols/functions.tsv (source USER_DEFINED), for tools/ghidra/ImportSymbols.java.

usage: symname.py <rva> <name> [<calling convention> [<signature>]] ...   (groups of up to 4, separated by --)
  symname.py 0015e2e0 NormAng __cdecl 'float __cdecl(float ang)' -- 002efd30 PyErr_Clear __cdecl 'void __cdecl(void)'
The signature has the form of ExportSymbols.java: "rettype __cc(type name, ...)" without the function name.
An empty calling convention or signature keeps the old one.
"""
import os
import sys

TSV = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', 'symbols', 'functions.tsv')


def main():
    groups, cur = [], []
    for a in sys.argv[1:]:
        if a == '--':
            groups.append(cur)
            cur = []
        else:
            cur.append(a)
    if cur:
        groups.append(cur)
    if not groups or any(len(g) < 2 or len(g) > 4 for g in groups):
        sys.exit(__doc__)
    want = {}
    for g in groups:
        rva = g[0].lower().removeprefix('0x').zfill(8)
        if rva in want:
            # shared stubs (identical code folding) sit in many vtables: one function, one name
            sys.exit(f'{rva} given twice ({want[rva][0]}, {g[1]}): one function can only have one name')
        want[rva] = (g + ['', ''])[1:4]

    lines = open(TSV, encoding='utf-8').read().split('\n')
    for i, line in enumerate(lines):
        f = line.split('\t')
        if f[0] in want:
            name, cc, sig = want.pop(f[0])
            print(f'{f[0]} {name}' + (f'  (was {f[4]})' if f[2] != 'DEFAULT' and f[4] != name else ''))
            f[2], f[4] = 'USER_DEFINED', name
            f[3] = cc or f[3]
            f[5] = sig or f[5]
            lines[i] = '\t'.join(f)
    open(TSV, 'w', encoding='utf-8').write('\n'.join(lines))
    for rva in want:
        print(f'{rva}: no function there in functions.tsv', file=sys.stderr)
    sys.exit(1 if want else 0)


if __name__ == '__main__':
    main()
