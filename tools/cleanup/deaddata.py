#!/usr/bin/env python3
# the objects of the data files nothing uses (no C reference, not in StaticInit.inc, not named by a test): removed
import re, sys, os
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import datalib as d, users as u
objs = d.objects()
us = u.users(objs)
si = open(d.ROOT + 'src/game/Global/StaticInit.inc').read()
tsy = set(re.findall(r'SD_TEST_(?:OBJ|DATA)\("[^"]*", (\w+)', open(d.ROOT + 'src/core/testsyms.inc').read()))
live = set(n for n in objs if us.get(n) or re.search(r'\b%s\b' % n, si) or n in tsy)
# what the initializers of live objects point at (sd_<name> labels or the names) lives too
tok = re.compile(r'[A-Za-z_]\w*')
refs = {n: set(t[3:] if t.startswith('sd_') else t for t in tok.findall(o['text'].split('=', 1)[1])) & set(objs)
        for n, o in objs.items() if '=' in o['text']}
todo = list(live)
while todo:
    n = todo.pop()
    for r in refs.get(n, ()):
        if r not in live:
            live.add(r)
            todo.append(r)
dead = [n for n in objs if n not in live]
byfile = {}
for n in dead:
    byfile.setdefault(objs[n]['file'], []).append(n)
for f, names in byfile.items():
    s = open(f).read()
    for n in sorted(names, key=lambda n: -objs[n]['start']):
        o = objs[n]
        a, b = o['start'], o['end']
        if s[b:b + 1] == '\n':
            b += 1
        s = s[:a] + s[b:]
    for n in names:
        s = re.sub(r'^#undef %s\n' % n, '', s, flags=re.M)
        s = re.sub(r'^SD_SYM\(%s\);\n' % n, '', s, flags=re.M)
    open(f, 'w').write(s)
ta = d.ROOT + 'src/core/testaddrs.inc'
s = open(ta).read()
for n in dead:
    s = re.sub(r'^SD_TEST_ADDR\(%s, [^\n]*\n' % n, '', s, flags=re.M)
open(ta, 'w').write(s)
print(len(dead), 'removed')
