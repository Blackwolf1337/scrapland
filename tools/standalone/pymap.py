#!/usr/bin/env python3
# pymap.py [-c] [Scrap.exe]: name map of the Python 1.5.2 in Scrap.exe (code rva 0x2da770..0x313634, data below),
# in the names of third_party/python-1.5.2, for ScrapC.exe (docs/standalone.md, stage 2a). One line per symbol:
# rva, name, how it was found:
#   functions.tsv  named function of symbols/functions.tsv
#   strings        the vendored function that has the string literals the function uses (unique match)
#   inittab        init function of a built-in module, by the module table of the exe and src/python/config.c
#   tp_name        type object (ob_type &PyType_Type), by its name string and the definitions in the sources
#   ob_type        object of a type with one instance (_Py_NoneStruct, _Py_EllipsisObject)
#   xref           global found next to a string in the code that uses it (flags of Py_Initialize, ...)
# -c prints instead the table PY_SYMBOLS of tools/standalone/datagen.py: every rva of the Python that a pointer
# outside the Python (from .reloc) holds, with its real symbol; it stops with an error for a target without a name.
# It counts the pointers of the whole exe, datagen.py fixes those in the data it builds in.
import collections, os, re, struct, sys

args = [a for a in sys.argv[1:] if not a.startswith('-')]
EXE = args[0] if args else 'game/Bin/Scrap.exe'
SRC = 'third_party/python-1.5.2'
BASE = 0x400000
hdr = open('include/scrap/origpython.h').read()
PY_CODE = tuple(int(re.search(r'#define ORIG_PY_CODE_%s (0x[0-9a-f]+)' % k, hdr).group(1), 16) for k in ('START', 'END'))
PY_DATA = tuple((int(lo, 16), int(hi, 16)) for lo, hi in
	re.findall(r'\{ (0x[0-9a-f]+), (0x[0-9a-f]+) \}', hdr[hdr.index('ORIG_PY_DATA'):]))
INITTAB = 0x428d58

b = open(EXE, 'rb').read()
pe = struct.unpack_from('<I', b, 0x3c)[0]
opt = pe + 24
secs = []
for i in range(struct.unpack_from('<H', b, pe + 6)[0]):
	o = opt + struct.unpack_from('<H', b, pe + 20)[0] + i * 40
	vsz, va, rsz, rptr = struct.unpack_from('<IIII', b, o + 8)
	secs.append((b[o:o + 8].rstrip(b'\0').decode(), va, vsz, rptr, rsz))


def off(rva):
	for n, va, vsz, rp, rs in secs:
		if va <= rva < va + max(vsz, rs):
			return rp + rva - va if rva - va < rs else None
	return None


def u32(rva):
	o = off(rva)
	return None if o is None else struct.unpack_from('<I', b, o)[0]


def cstr(rva):
	o = off(rva)
	if o is None:
		return None
	e = b.index(b'\0', o)
	s = b[o:e].decode('latin1')
	return s if s and all(32 <= ord(c) < 127 or c in '\n\t' for c in s) else None


def in_code(r):
	return PY_CODE[0] <= r < PY_CODE[1]


def in_data(r):
	return any(lo <= r < hi for lo, hi in PY_DATA)


rrva, rsz = struct.unpack_from('<II', b, opt + 96 + 5 * 8)
relocs = []
p = off(rrva)
end = p + rsz
while p < end:
	page, blk = struct.unpack_from('<II', b, p)
	if not blk:
		break
	for i in range((blk - 8) // 2):
		e = struct.unpack_from('<H', b, p + 8 + 2 * i)[0]
		if e >> 12 == 3:
			relocs.append(page + (e & 0xfff))
	p += blk
relocs.sort()

funcs = {}
for l in open('symbols/functions.tsv'):
	f = l.rstrip('\n').split('\t')
	try:
		a = int(f[0], 16)
	except ValueError:
		continue
	if in_code(a):
		funcs[a] = (int(f[1]), f[4])
starts = sorted(funcs)

# the vendored sources: function bodies with their string literals, type objects with their tp_name
LIT = re.compile(r'"((?:[^"\\\n]|\\.)*)"')


def unescape(s):
	return re.sub(r'\\(.)', lambda m: {'n': '\n', 't': '\t'}.get(m.group(1), m.group(1)), s)


body_strs = {}
types = collections.defaultdict(set)
for d in ('Objects', 'Python', 'Parser', 'Modules', 'PC'):
	for fn in sorted(os.listdir(os.path.join(SRC, d))):
		if not fn.endswith('.c'):
			continue
		lines = open(os.path.join(SRC, d, fn), encoding='latin1').read().split('\n')
		for i, l in enumerate(lines):
			m = re.match(r'(?:static\s+)?PyTypeObject\s+(\w+)\s*=\s*\{', l)
			if m:
				for k in lines[i + 1:i + 6]:
					s = LIT.search(k)
					if s:
						types[s.group(1)].add(m.group(1))
						break
			m = re.match(r'([A-Za-z_]\w*)\s*\(', l)
			if not m or l.rstrip().endswith(';'):
				continue
			j = i + 1
			while j < len(lines) and not lines[j].startswith('{') and j - i < 12:
				j += 1
			if j >= len(lines) or not lines[j].startswith('{'):
				continue
			k = j + 1
			while k < len(lines) and lines[k] != '}':
				k += 1
			strs = set(unescape(s) for s in LIT.findall('\n'.join(lines[j:k])))
			body_strs.setdefault(m.group(1), set()).update(strs)
owners = collections.defaultdict(set)
for n, ss in body_strs.items():
	for s in ss:
		owners[s].add(n)

names = {}


def name(rva, n, how):
	if rva not in names:
		names[rva] = (n, how)


for a, (sz, n) in funcs.items():
	if not re.match(r'(FUN|thunk_FUN|LAB)_', n) and n in body_strs:
		name(a, n, 'functions.tsv')

# init functions of the built-in modules: the exe's table and the one of src/python/config.c
ours = dict(re.findall(r'\{ "(\w+)", (\w+) \}', open('src/python/config.c').read()))
for i in range(64):
	s, f = u32(INITTAB + 8 * i), u32(INITTAB + 8 * i + 4)
	if not s:
		break
	mod = cstr(s - BASE)
	if f and in_code(f - BASE) and mod in ours:
		name(f - BASE, ours[mod], 'inittab')

# functions by their strings
used = collections.defaultdict(set)
for at in relocs:
	if in_code(at):
		i = max(0, __import__('bisect').bisect_right(starts, at) - 1)
		a = starts[i]
		if a <= at < a + funcs[a][0]:
			s = cstr(u32(at) - BASE) if u32(at) else None
			if s:
				used[a].add(s)
best = {}
for a, ss in used.items():
	votes = collections.Counter()
	for s in ss:
		if len(owners.get(s, ())) == 1:
			votes[next(iter(owners[s]))] += 1
	if votes:
		(n, c), = votes.most_common(1)
		if list(votes.values()).count(c) == 1:
			best.setdefault(n, []).append((c, a))
for n, l in best.items():
	l.sort(reverse=True)
	if len(l) == 1 or l[0][0] > l[1][0]:
		name(l[0][1], n, 'strings')

# type objects: ob_refcnt 1, ob_type the type of types (the one whose ob_type is itself), tp_name at +12
data_rvas = [r for lo, hi in PY_DATA for r in range(lo, hi, 4)]
typetype = None
for r in data_rvas:
	if u32(r) == 1 and u32(r + 4) == BASE + r and u32(r + 12) and cstr(u32(r + 12) - BASE) == 'type':
		typetype = r
for r in data_rvas:
	if typetype and u32(r) == 1 and u32(r + 4) == BASE + typetype and u32(r + 12):
		tn = cstr(u32(r + 12) - BASE)
		if tn and len(types.get(tn, ())) == 1:
			name(r, next(iter(types[tn])), 'tp_name')
		elif tn and tn in types:
			name(r, '/'.join(sorted(types[tn])), 'tp_name')
for tn, obj in (('None', '_Py_NoneStruct'), ('ellipsis', '_Py_EllipsisObject')):
	t = [r for r, (n, h) in names.items() if h == 'tp_name' and tn in types and n in types[tn]]
	for r in data_rvas:
		if t and u32(r + 4) == BASE + t[0] and u32(r) and r not in names:
			name(r, obj, 'ob_type')
			break

# globals next to strings: (string, name, how): 'store' the first store after the push, 'cmp' the last compare before
XREF = (('PYTHONDEBUG', 'Py_DebugFlag', 'store'), ('PYTHONVERBOSE', 'Py_VerboseFlag', 'store'),
	('PYTHONOPTIMIZE', 'Py_OptimizeFlag', 'store'),
	('WARNING: import %s (read)\n the library was not preloaded\n', 'g_PyPackImport', 'cmp'))
code = b[off(PY_CODE[0]):off(PY_CODE[1])]
for s, n, how in XREF:
	for at in relocs:
		if in_code(at) and u32(at) and cstr(u32(at) - BASE) == s:
			o = at - PY_CODE[0]
			if how == 'store':
				m = re.compile(rb'\x89[\x05\x0d\x15\x1d\x35\x3d](....)', re.S).search(code, o + 4, o + 64)
			else:
				ms = list(re.compile(rb'\x83\x3d(....)\x00', re.S).finditer(code, max(0, o - 32), o))
				m = ms[-1] if ms else None
			if m:
				name(struct.unpack('<I', m.group(1))[0] - BASE, n, 'xref')
			break
for r in data_rvas:
	if u32(r) == BASE + INITTAB:
		name(r, 'PyImport_Inittab', 'xref')

if '-c' not in sys.argv:
	for r in sorted(names):
		print('%06x\t%s\t%s' % (r, names[r][0], names[r][1]))
	sys.exit(0)

ext = collections.defaultdict(int)
for at in relocs:
	if in_code(at) or in_data(at):
		continue
	t = (u32(at) or 0) - BASE
	if in_code(t) or in_data(t):
		ext[t] += 1
bad = 0
for t in sorted(ext):
	if t not in names:
		print('pymap: rva %06x of the Python (%d pointers) has no name' % (t, ext[t]), file=sys.stderr)
		bad = 1
		continue
	n = names[t][0]
	print('\t0x%06x: \'%s\',\t# %d' % (t, n, ext[t]))
sys.exit(bad)
