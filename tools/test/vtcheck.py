#!/usr/bin/env python3
# Checks the slots of the vtables of src/game/Global/Vtables.c (arrays of void (*)(void), so the compiler checks
# nothing) against the typed *Vtbl structs of the headers: the classes of the parameters (float/double, struct by
# value, integer/pointer) and of the return value of each C function must be the ones of its slot. On i386 every
# argument is a stack word, a wrong class works by chance; on x86_64 floats go in other registers than integers and
# the following arguments shift. A vtable is checked against the struct whose field names match its function names
# best (at least 3 and 60 %); the vtables without such a struct are compared within their family per slot. Stubs
# that ignore their arguments (Nop, Stub, Nothing) are left out. Exit code 1 when something differs.
#
# usage: tools/test/vtcheck.py
import os
# C function in that slot (src/game/Global/Vtables.c): float/double vs integer/pointer vs struct by value.
import re, glob, collections, sys
ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', '..') + '/'
def strip_comments(s):
    return re.sub(r'/\*.*?\*/', ' ', s, flags=re.S)

def classify(params):
    params = params.strip()
    if params in ('', 'void'):
        return []
    out = []
    depth = 0; cur = ''
    for ch in params:
        if ch == '(':
            depth += 1
        elif ch == ')':
            depth -= 1
        if ch == ',' and depth == 0:
            out.append(cur); cur = ''
        else:
            cur += ch
    out.append(cur)
    cls = []
    for p in out:
        p = p.strip()
        if p == '...':
            cls.append('V'); continue
        if '(*' in p or '*' in p or '[' in p:
            cls.append('I'); continue
        toks = re.sub(r'\b(const|volatile|struct|unsigned|signed|register)\b', ' ', p).split()
        t = toks[0] if toks else ''
        if t in ('float', 'double'):
            cls.append('F')
        elif t in ('vec3_t', 'quat_t', 'matrix_t', 'box_t', 'plane_t') or (t and t[0] in 'cC' and t[1:2].isupper()) :
            cls.append('S:' + t)
        else:
            cls.append('I')
    return cls

# typed vtable structs: offset -> (field, classes)
structs = {}
for f in glob.glob(ROOT + 'include/scrap/*.h') + glob.glob(ROOT + 'src/game/**/*.[ch]', recursive=True):
    s = open(f, errors='replace').read()
    for m in re.finditer(r'typedef struct (\w*Vtbl\w*) \{(.*?)\n\}', s, re.S):
        body = m.group(2)
        fields = {}
        for fm in re.finditer(r'\(\s*\*\s*(\w+)\s*\)\s*\(([^;]*?)\)\s*;\s*/\*\s*(0x[0-9a-f]+)', body, re.S):
            fields[int(fm.group(3), 16)] = (fm.group(1), classify(strip_comments(fm.group(2))))
        if fields:
            structs[m.group(1)] = fields

# function definitions
defs = {}
for f in glob.glob(ROOT + 'src/**/*.c', recursive=True):
    s = strip_comments(open(f, errors='replace').read())
    for m in re.finditer(r'^[A-Za-z_][\w \t\*]*?\b(\w+)\(([^;{)]*(?:\([^)]*\)[^;{)]*)*)\)\s*\{', s, re.M):
        name = m.group(1)
        if name in ('if', 'while', 'for', 'switch'):
            continue
        defs.setdefault(name, (f.replace(ROOT, ''), classify(m.group(2))))

vt = open(ROOT + 'src/game/Global/Vtables.c').read()
problems = []
SKIPPED = []
for m in re.finditer(r'void \(\*const (\w+)\[(\d+)\]\)\(void\) = \{(.*?)\n\};', vt, re.S):
    name, body = m.group(1), m.group(3)
    slots = []
    for sm in re.finditer(r'^\s*(\w+),\s*/\*\s*(0x[0-9a-f]+)', body, re.M):
        slots.append((int(sm.group(2), 16), sm.group(1)))
    # the struct whose field names match the function names best
    best, score = None, 0
    for sn, fields in structs.items():
        sc = sum(1 for off, fn in slots if off in fields and fn.endswith('_' + fields[off][0]))
        if sc > score:
            best, score = sn, sc
    common = sum(1 for off, fn in slots if best and off in structs[best])
    if not best or score < 3 or score < 0.6 * common:
        SKIPPED.append((name, len(slots), best, score, common))
        continue
    fields = structs[best]
    for off, fn in slots:
        if off not in fields or fn not in defs:
            continue
        fname, want = fields[off]
        have = defs[fn][1]
        # compare float/struct positions; integer vs pointer is the same register class
        w = [c if c != 'V' else 'V' for c in want]
        h = have
        def key(c):
            return 'F' if c == 'F' else ('S' if c.startswith('S:') else ('V' if c == 'V' else 'I'))
        wk = [key(c) for c in w]; hk = [key(c) for c in h]
        if 'V' in wk:
            n = wk.index('V'); wk = wk[:n]; hk = hk[:n]
        if wk != hk[:len(wk)] or (len(hk) > len(wk) and 'V' not in [key(c) for c in w]):
            problems.append((name, best, hex(off), fname, fn, ''.join(wk), ''.join(hk), defs[fn][0]))
STUB = re.compile(r'Nop|Stub|Nothing')
problems = [p for p in problems if not STUB.search(p[4])]
seen = set()
for p in problems:
    k = (p[4], p[3])
    if k in seen:
        continue
    seen.add(k)
    print('%-34s %-22s %-5s %-18s %-40s want %-8s have %-8s %s' % p)
FOUND = len(seen)


# family check: vtables grouped by the weakly matched struct and their slot count; per slot the float/struct
# positions of all implementations (stubs that ignore their arguments left out) must agree
fam = collections.defaultdict(list)
for m in re.finditer(r'void \(\*const (\w+)\[(\d+)\]\)\(void\) = \{(.*?)\n\};', vt, re.S):
    name, body = m.group(1), m.group(3)
    slots = [(int(sm.group(2), 16), sm.group(1)) for sm in re.finditer(r'^\s*(\w+),\s*/\*\s*(0x[0-9a-f]+)', body, re.M)]
    best = None; score = 0
    for sn, fields in structs.items():
        sc = sum(1 for off, fn in slots if off in fields and fn.endswith('_' + fields[off][0]))
        if sc > score:
            best, score = sn, sc
    fam[(best, len(slots))].append((name, slots))
def key(c):
    return 'F' if c == 'F' else ('S' if c.startswith('S:') else 'I')
# grouped with a family by a shared field name only
UNRELATED = {'CParticleActive_Update'}
print('---- family check')
for (best, n), members in sorted(fam.items(), key=lambda x: str(x[0])):
    if len(members) < 2:
        continue
    per = collections.defaultdict(list)
    for name, slots in members:
        for off, fn in slots:
            if fn in defs and not re.search(r'Nop|Stub|Nothing|Clear\d|_Zero|_Ret|Pure', fn) and fn not in UNRELATED:
                per[off].append((fn, ''.join(key(c) for c in defs[fn][1])))
    for off, impls in sorted(per.items()):
        sigs = collections.Counter(sg for fn, sg in impls)
        # only the positions of F and S matter: strip trailing I that differ in count
        norm = collections.defaultdict(list)
        for fn, sg in impls:
            norm[sg.rstrip('I')].append(fn)
        if len(norm) > 1 and not all(STUB.search(fn) for v in list(norm.values())[1:] for fn in v):
            FOUND += 1
            print('%s/%d slot %s:' % (best, n, hex(off)), '; '.join('%s=%s' % (k or '-', ','.join(sorted(set(v))[:4])) for k, v in norm.items()))

# return types: void against a value, float against integer
print('---- return check')
def rclass(t):
    t = re.sub(r'\b(static|inline|const|extern|struct)\b', ' ', t).strip()
    if t == 'void':
        return 'v'
    if '*' in t:
        return 'I'
    if t.split()[-1:] in (['float'], ['double']):
        return 'F'
    return 'I'
rets = {}
for f in glob.glob(ROOT + 'src/**/*.c', recursive=True):
    s = strip_comments(open(f, errors='replace').read())
    for m in re.finditer(r'^([A-Za-z_][\w \t\*]*?)\b(\w+)\([^;{]*?\)\s*\{', s, re.M):
        rets.setdefault(m.group(2), rclass(m.group(1)))
fret = {}
for f in glob.glob(ROOT + 'include/scrap/*.h') + glob.glob(ROOT + 'src/game/**/*.[ch]', recursive=True):
    s = open(f, errors='replace').read()
    for m in re.finditer(r'typedef struct (\w*Vtbl\w*) \{(.*?)\n\}', s, re.S):
        d = {}
        for fm in re.finditer(r'^\s*([\w \t\*]+?)\s*\(\s*\*\s*(\w+)\s*\)\s*\([^;]*?\)\s*;\s*/\*\s*(0x[0-9a-f]+)', m.group(2), re.S | re.M):
            d[int(fm.group(3), 16)] = rclass(fm.group(1))
        fret[m.group(1)] = d
for m in re.finditer(r'void \(\*const (\w+)\[(\d+)\]\)\(void\) = \{(.*?)\n\};', vt, re.S):
    name, body = m.group(1), m.group(3)
    slots = [(int(sm.group(2), 16), sm.group(1)) for sm in re.finditer(r'^\s*(\w+),\s*/\*\s*(0x[0-9a-f]+)', body, re.M)]
    best = None; score = 0
    for sn, fields in structs.items():
        sc = sum(1 for off, fn in slots if off in fields and fn.endswith('_' + fields[off][0]))
        if sc > score:
            best, score = sn, sc
    common = sum(1 for off, fn in slots if best and off in structs[best])
    if not best or score < 3 or score < 0.6 * common:
        continue
    for off, fn in slots:
        want = fret.get(best, {}).get(off)
        have = rets.get(fn)
        if want and have and want != have and want != 'v' and not STUB.search(fn):
            FOUND += 1
            print('%-34s %-20s %-5s %-36s want %s have %s' % (name, best, hex(off), fn, want, have))

print('%d differences' % FOUND)
sys.exit(1 if FOUND else 0)
