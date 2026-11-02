#!/usr/bin/env python3
# GAME_GLOBAL(T, X) and GAME_ARRAY(T, X) to plain C: X when the type of the object fits, else a cast (reported)
import re, glob, sys, os
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import datalib as d
objs = d.objects()


def norm(t):
    t = re.sub(r'\bconst\b|\bstruct\b', '', t)
    t = re.sub(r'^\s*typeof\((.*)\)\s*$', r'\1', t)
    return re.sub(r'\s+', '', t)


def parts(name):
    h = objs[name]['head']
    m = re.match(r'^(.*?)\b%s\b\s*((?:\[[^\]]*\]\s*)*)$' % name, h.strip(), re.S)
    base = re.sub(r'^\s*static\s+', '', m.group(1)).strip()
    dims = re.findall(r'\[[^\]]*\]', m.group(2))
    return base, dims


casts = []
files = [f for f in glob.glob(d.ROOT + 'src/**/*.[ch]', recursive=True) + glob.glob(d.ROOT + 'include/scrap/*.h') +
         glob.glob(d.ROOT + 'src/**/*.inc', recursive=True)
         if '/core/test' not in f and f not in d.DATA]
ARG = r'((?:[^(),]|\([^()]*\))+)'
for f in files:
    s = open(f).read()
    if f.endswith('/game.h'):
        continue

    def rep(m):
        kind, T, X = m.group(1), m.group(2).strip(), m.group(3)
        if X not in objs:
            casts.append((d.rel(f), m.group(0), 'no object'))
            return m.group(0)
        base, dims = parts(X)
        if kind == 'GLOBAL':
            if not dims and norm(base) == norm(T):
                return X
            casts.append((d.rel(f), m.group(0), 'cast'))
            return '(*(%s *)&%s)' % (T, X)
        # ARRAY: the address of the first element as T *
        if dims:
            elem = base + ''.join(dims[1:])
            if norm(elem) == norm(T) or norm(base + ' ' + ''.join(dims[1:])) == norm(T):
                return X
            casts.append((d.rel(f), m.group(0), 'cast'))
            return '((%s *)%s)' % (T, X)
        if norm(base) == norm(T):
            return '(&%s)' % X
        casts.append((d.rel(f), m.group(0), 'cast'))
        return '((%s *)&%s)' % (T, X)
    t = re.sub(r'\bGAME_(GLOBAL|ARRAY)\(' + ARG + r', (\w+)\)', rep, s)
    if t != s:
        open(f, 'w').write(t)
for c in casts:
    print('%s: %s (%s)' % c)
