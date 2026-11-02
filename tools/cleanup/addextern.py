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
    incs = list(re.finditer(r'^#include [^\n]*\n', s, re.M))
    at = incs[-1].end() if incs else 0
    block = ''.join(d.extern_of(objs[n]) + '\n' for n in names)
    s = s[:at] + ('\n' if incs else '') + block + s[at:]
    open(f, 'w').write(s)
    print(d.rel(f), ' '.join(names))
