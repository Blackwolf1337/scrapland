#!/usr/bin/env python3
# The group_<rva> blobs of the data files (anonymous structs of byte flags) become one object per field; the test
# maps (testaddrs.inc, testsyms.inc) follow, the GAME_GLOBAL_IN/ARRAY_IN views become the names.
import re, glob, sys, os
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import datalib as d
objs = d.objects()
SIZES = {'byte': 1, 'char': 1, 'int': 4, 'float': 4, 'uint32_t': 4}
groups = {}
for name, o in objs.items():
    if not re.match(r'group_[0-9a-f]{6}$', name):
        continue
    inits = dict(re.findall(r'\.(\w+) = ([^,}]+)', o['text'].split('}', 1)[1])) if '=' in o['text'].split('}', 1)[1] else {}
    fields, off = [], 0
    for fm in re.finditer(r'\t(\w+) (\w+)(?:\[(\d+)\])?;', o['text']):
        t, fname, n = fm.group(1), fm.group(2), int(fm.group(3) or 1)
        size = SIZES[t] * n
        if not fname.startswith('pad_'):
            fields.append((off, t, fname, fm.group(3), size, inits.get(fname)))
        off += size
    groups[name] = fields
# the data files
for name, fields in groups.items():
    o = objs[name]
    s = open(o['file']).read()
    defs = ''.join('%s %s%s%s;\n' % (t, f, '[%s]' % n if n else '', ' = ' + v.strip() if v else '') for off, t, f, n, size, v in fields)
    s = s.replace(o['text'] + '\n', defs, 1)
    open(o['file'], 'w').write(s)
# the test maps
ta = d.ROOT + 'src/core/testaddrs.inc'
s = open(ta).read()
for name, fields in groups.items():
    m = re.search(r'^SD_TEST_ADDR\(%s, (0x[0-9a-f]+), \d+\)\n' % name, s, re.M)
    base = int(m.group(1), 16)
    s = s.replace(m.group(0), ''.join('SD_TEST_ADDR(%s, 0x%x, %d)\n' % (f, base + off, size) for off, t, f, n, size, *_ in fields))
open(ta, 'w').write(s)
ts = d.ROOT + 'src/core/testsyms.inc'
s = open(ts).read()


def field_at(name, off):
    for foff, t, f, n, size, v in groups[name]:
        if foff <= off < foff + size:
            return f, off - foff
    raise KeyError((name, off))


def rep_obj(m):
    f, rest = field_at(m.group(2), 0)
    return 'SD_TEST_OBJ(%s, %s, %s)' % (m.group(1), f, m.group(3))


def rep_data(m):
    f, rest = field_at(m.group(2), int(m.group(3), 0))
    if rest == 0:
        return 'SD_TEST_OBJ(%s, %s, %s)' % (m.group(1), f, m.group(4))
    return 'SD_TEST_DATA(%s, %s, %d, %s)' % (m.group(1), f, rest, m.group(4))
s = re.sub(r'SD_TEST_OBJ\(("\w+"), (group_[0-9a-f]{6}), (0x[0-9a-f]+)\)', rep_obj, s)
s = re.sub(r'SD_TEST_DATA\(("\w+"), (group_[0-9a-f]{6}), (\w+), (0x[0-9a-f]+)\)', rep_data, s)
open(ts, 'w').write(s)
# the views
files = [f for f in glob.glob(d.ROOT + 'src/**/*.[ch]', recursive=True) + glob.glob(d.ROOT + 'include/scrap/*.h')
         if '/core/test' not in f and f not in d.DATA]
for f in files:
    s = open(f).read()

    def rep_def(m):
        mac, T, g, off = m.group(1), m.group(2), m.group(3), int(m.group(4), 0)
        fld, rest = field_at(g, off)
        assert rest == 0
        tail = m.group(5).strip()
        t, n = next((t, n) for o2, t, f2, n, sz, v in groups[g] if f2 == fld)
        if mac == fld:
            return 'extern %s %s%s;%s' % (t, fld, '[%s]' % n if n else '', ('\t' + tail) if tail else '')
        return '#define %s %s%s' % (mac, fld, ('\t' + tail) if tail else '')
    t2 = re.sub(r'^#define (\w+) \(?GAME_GLOBAL_IN\((\w+), (group_[0-9a-f]{6}), (\w+)\)\)?([^\n]*)$', rep_def, s, flags=re.M)

    def rep_use(m):
        fld, rest = field_at(m.group(2), int(m.group(3), 0))
        assert rest == 0
        return fld
    t2 = re.sub(r'GAME_GLOBAL_IN\((\w+), (group_[0-9a-f]{6}), (\w+)\)', rep_use, t2)
    if t2 != s:
        open(f, 'w').write(t2)
print(len(groups), 'groups')
