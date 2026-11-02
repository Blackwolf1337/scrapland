#!/usr/bin/env python3
# the objects still named after an rva: strings by their text, scalar constants inlined as literals (exact values),
# tables by the function that uses them and their kind. The test maps keep their keys.
import re, os, sys, glob, struct, collections
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import datalib as d, users as u
objs = d.objects()
RVA = re.compile(r'(^(data|str|group)_[0-9a-f]{6}$)|_[0-9a-f]{6}$')
rva = [n for n in objs if RVA.search(n)]
allnames = set()
files = [f for f in glob.glob(d.ROOT + 'src/**/*.[ch]', recursive=True) + glob.glob(d.ROOT + 'include/scrap/*.h') +
         glob.glob(d.ROOT + 'src/**/*.inc', recursive=True) if '/core/test' not in f]
text = {f: open(f).read() for f in files}
for s in text.values():
    allnames |= set(re.findall(r'[A-Za-z_]\w*', s))


def uniq(base):
    n, k = base, 2
    while n in allnames:
        n = '%s%d' % (base, k)
        k += 1
    allnames.add(n)
    return n


def camel(s):
    w = re.findall(r'[A-Za-z0-9]+', s)
    return ''.join(x[:1].upper() + x[1:] for x in w)[:40] or 'Empty'


def kind(o, name):
    h = o['head']
    dims = re.findall(r'\[([^\]]*)\]', h)
    base = h.split(name)[0].replace('const', '').strip()
    if base == 'vec3_t':
        return 'Pos'
    if base == 'color4_t':
        return 'Color'
    if base == 'uint32_t' and dims and dims[0].strip().isdigit() and int(dims[0]) % 9 == 0:
        return 'Ramp' if int(dims[0]) == 9 else 'Ramps'
    if base in ('float', 'double'):
        return 'Table' if dims else 'Value'
    if base in ('int', 'uint32_t'):
        return 'Table' if dims else 'Count'
    return 'Data'


ren, inline = {}, {}
for n in sorted(rva):
    o = objs[n]
    h = o['head']
    init = o['text'].split('=', 1)[1].strip().rstrip(';').strip() if '=' in o['text'] else None
    if n.startswith('str_'):
        m = re.match(r'"((?:\\.|[^"\\])*)"$', init or '')
        ren[n] = uniq('str_' + camel(m.group(1) if m else n[4:]))
    elif init and not h.rstrip().endswith(']') and re.match(r'const (double|float|int|uint32_t) ', h):
        t = h.split()[1]
        if t == 'double':
            v = float(init)
            f32 = struct.unpack('<f', struct.pack('<f', v))[0]
            if f32 == v:
                for p in range(1, 10):
                    lit = '%.*g' % (p, v)
                    if struct.unpack('<f', struct.pack('<f', float(lit)))[0] == v:
                        break
                if float(lit) == v:
                    inline[n] = lit if ('.' in lit or 'e' in lit) else lit + '.0'
                else:
                    inline[n] = '(double)%sf' % (lit if ('.' in lit or 'e' in lit) else lit + '.0')
            else:
                inline[n] = repr(v)
        else:
            inline[n] = init
    else:
        base = re.sub(r'_[0-9a-f]{6}$', '', n)
        if base in ('data', 'str', 'group'):
            base = 'Data'
        ren[n] = uniq(base + kind(o, n))
for f, s in text.items():
    t = s
    for n, v in inline.items():
        if not re.search(r'\b%s\b' % n, t):
            continue
        t = re.sub(r'^(const|extern const) [^\n]*\b%s\b[^\n]*;\n(\n)?' % n, '', t, flags=re.M)
        # StaticInit.inc takes the address of the value
        ty = objs[n]['head'].split()[1]
        t = re.sub(r'SI_REF\(%s\)' % n, '&(const %s){ %s }' % (ty, v), t)
        t = re.sub(r'\b%s\b' % n, v, t)
    for n, v in ren.items():
        if re.search(r'\b%s\b' % n, t):
            t = re.sub(r'\b%s\b' % n, v, t)
    if t != s:
        open(f, 'w').write(t)
for p in ('src/core/testaddrs.inc', 'src/core/testsyms.inc'):
    s = open(d.ROOT + p).read()
    for n in inline:
        s = re.sub(r'^SD_TEST_ADDR\(%s, [^\n]*\n' % n, '', s, flags=re.M)
    s = re.sub(r'(SD_TEST_(?:ADDR|OBJ|DATA)\((?:"[^"]*", )?)(\w+)', lambda m: m.group(1) + ren.get(m.group(2), m.group(2)), s)
    open(d.ROOT + p, 'w').write(s)
print(len(ren), 'renamed,', len(inline), 'inlined')
for n in list(inline)[:8]:
    print(n, '->', inline[n])
for n in list(ren)[:200:10]:
    print(n, '->', ren[n])
