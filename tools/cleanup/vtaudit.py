#!/usr/bin/env python3
# checks of the conversion: prototypes inside function bodies (a statement the old prototype regex replaced)
import re, subprocess, sys
files = subprocess.run(['git', 'diff', '--name-only', 'HEAD'], capture_output=True, text=True).stdout.split()
bad = 0
for f in files:
    if not f.endswith('.c'):
        continue
    depth = 0
    for i, l in enumerate(open(f).read().split('\n')):
        code = re.sub(r'"(\\.|[^"\\])*"', '""', l)
        if depth > 0 and re.match(r'[A-Za-z_][\w \*]*\b\w+\(.*\);$', l):
            print('%s:%d: %s' % (f, i + 1, l[:120]))
            bad += 1
        depth += code.count('{') - code.count('}')
sys.exit(1 if bad else 0)
