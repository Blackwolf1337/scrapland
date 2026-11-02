#!/usr/bin/env python3
# "'X' undeclared" for an object of the data files: its extern declaration after the last #include of that file
import re, sys, os
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import datalib as d

objs = d.objects()
need = {}
for line in open(sys.argv[1], errors='replace'):
    m = re.match(r"^(.+?):(\d+):\d+: error: ‘(\w+)’ undeclared", line)
    if not m or m.group(3) not in objs:
        continue
    f = os.path.realpath(d.ROOT + m.group(1))
    # an error inside a header's macro shows at the header line: the declaration goes to that header
    need.setdefault(f, set()).add(m.group(3))
for f, names in need.items():
    s = open(f).read()
    names = [n for n in sorted(names) if not re.search(r'^extern [^;]*\b%s\b' % n, s, re.M)]
    if not names:
        continue
    # before the top-level construct (a #define line or a declaration) of the first use, so its types are known
    decls = d.top_decls(s)
    c = d.strip_comments(s)
    for n in names:
        m = re.search(r'\b%s\b' % n, c)
        if not m:
            continue
        at = m.start()
        top = next((a for a, b, t in decls if a <= at < b), None)
        if top is None:
            top = c.rfind('\n', 0, at) + 1
            while top > 0 and c[top - 2] == '\\':
                top = c.rfind('\n', 0, top - 1) + 1
        decl = d.extern_of(objs[n]) + '\n'
        s = s[:top] + decl + s[top:]
        c = c[:top] + ' ' * len(decl) + c[top:]
        decls = d.top_decls(s)
    open(f, 'w').write(s)
    print(d.rel(f), ' '.join(names))
