#!/usr/bin/env python3
# every object of the data files into its owner file (owners.py): before the first top-level construct that uses it
# (else at the end), with the SD_SYM labels its initializer uses; the extern of it in that file goes
import re, os, sys, collections
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import datalib as d, owners as ow

objs, own, why, us = ow.owners()
names = set(objs)
TOK = re.compile(r'[A-Za-z_]\w*')
symlines = {}
for f in d.DATA:
    for m in re.finditer(r'^SD_SYM\((\w+)\);\n', open(f).read(), re.M):
        symlines[m.group(1)] = m.group(0)
by_owner = collections.defaultdict(list)
for n, f in own.items():
    by_owner[f].append(n)
# out of the data files
datatext = {f: open(f).read() for f in d.DATA}
for n, f in own.items():
    o = objs[n]
    s = datatext[o['file']]
    t = o['text'] + '\n'
    assert s.count(t) >= 1, n
    datatext[o['file']] = s.replace(t, '', 1)
for f, s in datatext.items():
    s = re.sub(r'^#undef (\w+)\n', lambda m: '' if m.group(1) in own else m.group(0), s, flags=re.M)
    open(f, 'w').write(s)
# into the owners
for f, ns in by_owner.items():
    s = open(f).read()
    for n in sorted(ns, key=lambda n: objs[n]['start']):
        o = objs[n]
        s = re.sub(r'^extern [^;\n]*\b%s\b[^;\n]*;[^\n]*\n' % n, '', s, flags=re.M)
        labels = [x[3:] for x in TOK.findall(o['text']) if x.startswith('sd_')]
        block = ''.join(symlines[x] for x in dict.fromkeys(labels)
                        if x in symlines and not re.search(r'^SD_SYM\(%s\);' % x, s, re.M))
        block += o['text'] + '\n'
        c = d.strip_comments(s)
        c = re.sub(r'"(\\.|[^"\\\n])*"', lambda m: ' ' * len(m.group(0)), c)
        m = re.search(r'\b(sd_)?%s\b' % n, c)
        if m:
            decls = d.top_decls(s)
            at = next((a for a, b, t in decls if a <= m.start() < b), None)
            if at is None:
                at = c.rfind('\n', 0, m.start()) + 1
                while at > 0 and c[at - 2] == '\\':
                    at = c.rfind('\n', 0, at - 1) + 1
            # before the comment that belongs to that construct
            cm = re.search(r'(/\*(?:(?!\*/).)*\*/\n)$', s[:at], re.S)
            if cm and s[:at].endswith('*/\n'):
                at = cm.start()
            s = s[:at] + block + '\n' + s[at:]
        else:
            s = s.rstrip('\n') + '\n\n' + block
    open(f, 'w').write(s)
print(len(own), 'objects into', len(by_owner), 'files')
