#!/usr/bin/env python3
# '#define X GAME_GLOBAL(T, X)' and '#define X (GAME_ARRAY(T, X))' with the name of the object: an extern declaration.
# A GAME_ARRAY view of a single object was its address: the uses become &X.
import re, glob, sys, os
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import datalib as d
objs = d.objects()
files = [f for f in glob.glob(d.ROOT + 'src/**/*.[ch]', recursive=True) + glob.glob(d.ROOT + 'include/scrap/*.h') +
         glob.glob(d.ROOT + 'src/**/*.inc', recursive=True)
         if '/core/test' not in f and f not in d.DATA and not f.endswith('/game.h')]
single = set()
for f in files:
    s = open(f).read()
    def rep(m):
        mac, kind, T, name, tail = m.group(1), m.group(2), m.group(3).strip(), m.group(4), m.group(5)
        if mac != name or name not in objs:
            return m.group(0)
        if kind == 'ARRAY' and not objs[name]['head'].rstrip().endswith(']'):
            single.add(name)
        return d.extern_of(objs[name]) + (('\t' + tail.strip()) if tail.strip() else '')
    t = re.sub(r'^#define (\w+) \(?GAME_(GLOBAL|ARRAY)\(([^,]+), (\w+)\)\)?([^\n]*)$', rep, s, flags=re.M)
    t = re.sub(r'GAME_GVAR(?:FUNC)?\((\w+)\)->', r'\1.', t)
    t = re.sub(r'GAME_GVAR(?:FUNC)?\((\w+)\)', r'(&\1)', t)
    if t != s:
        open(f, 'w').write(t)
# the uses of the single objects that were addresses
for f in files:
    s = open(f).read()
    t = s
    for name in single:
        if not re.search(r'\b%s\b' % name, t):
            continue
        lines = t.split('\n')
        for i, l in enumerate(lines):
            if re.match(r'\s*extern\b', l) or re.match(r'\s*#\s*(undef|define %s\b)' % name, l):
                continue
            l2 = re.sub(r'(?<![\w.>&])%s\b' % name, '(&%s)' % name, l)
            l2 = l2.replace('(&%s)->' % name, '%s.' % name).replace('*(&%s)' % name, name)
            l2 = l2.replace('&(&%s)' % name, '&%s' % name)
            lines[i] = l2
        t = '\n'.join(lines)
    if t != s:
        open(f, 'w').write(t)
print('single objects:', len(single))
