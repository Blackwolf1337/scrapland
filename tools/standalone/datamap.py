#!/usr/bin/env python3
# datamap.py: the map of the data of Scrap.exe 1.6.1 that ScrapC.exe uses (docs/standalone.md, stage 3b2).
#
#   datamap.py [-e Scrap.exe] [-c ScrapC.exe] [-s objects.txt] [-o datamap.txt] [dataprobe logs ...]
#       writes the map (default tools/standalone/datamap.txt) from the static scan and the logs of the data probe
#       (src/standalone/dataprobe.c, run with the same ScrapC.exe). Without logs the rows the probe used in the map
#       before stay used.
#   datamap.py --why <rva> ...
#       prints why the piece at rva is used, along the chain of pointers
#   datamap.py --check [-e Scrap.exe] [-c ScrapC.exe] [-o datamap.txt]
#       the check of the build after the link of ScrapC.exe: every rva the C and the tests use lies in data the map
#       keeps (datagen.py builds in only that data)
#
# What counts as used:
#   - the rvas in the code of ScrapC.exe (game_base plus an immediate, from GAME_DATA and Game_Ptr) with the width of
#     the access, and the rvas in its named data (tables of rvas) that the original or the code points to too; the C
#     objects of SD_AT (C already)
#   - the numbers in the test scripts (tools/test/py) that lie in the data, 4 bytes each
#   - the import directory, its name tables and the import address table (main.c), the resources (found by id)
#   - each unit the probe logged, and every page it retired as a whole (it was hot, its later accesses are not seen)
#   - everything a pointer (.reloc) in used data leads to, but for the pointers datagen.py sets to the real Python
#     and the pointers before a vtable to its RTTI, unless the C or the probe read them
#
# The data is cut into pieces at every target of a pointer of the original (code or data), every rva of the C or the
# tests and the end of an access of known width, the ends of the strings the original points to and the edges of the
# sections and the C objects. A piece with a used byte is used as a whole. An access of unknown extent (an address
# taken, an indexed access, a pointer target, a table of rvas) also uses the pieces after it, up to a pointer target
# of the data, a C object, the end of the section or a piece only other functions of the original pointed to (the
# original reached the fields of a struct or an array one by one, which cut it at its own targets); a string ends
# with its NUL. Those are the margins.
import bisect, collections, os, re, struct, subprocess, sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from sdexe import BASE, PY_DATA, PY_SYMBOLS, ROOT, TEXT_KEEP, Exe, ExeError, coff_sd, room_pieces

OBJDUMP = os.environ.get('OBJDUMP', 'i686-w64-mingw32-objdump')
NM = os.environ.get('NM', 'i686-w64-mingw32-nm')
MAP = os.path.join(ROOT, 'tools', 'standalone', 'datamap.txt')

# who the code of the original belongs to (rva ranges of .text): a piece of data only that code reached is the data
# of that library
CODE_OWNERS = [
	(0x001000, 0x002070, 'dinput8.lib'),
	(0x002070, 0x03e800, 'd3dx8'),	# with the ASE SDK, zlib, libpng and libjpeg
	(0x03e800, 0x05f6d8, None),	# the dynamic initializers of the game
	(0x05f6d8, 0x2d6010, None),
	(0x2d6010, 0x2da770, None),	# descomp_afin.cpp (M3D)
	(0x2da770, 0x313634, 'python'),
	(0x313634, 0x3179c6, 'crt'),
	(0x3179c6, 0x317b2e, None),	# the import thunks of the game
	(0x317b2e, 0x3761d0, 'crt'),
	(0x3761d0, 0x3761dc, None),
	(0x3761dc, 0x37b3d0, 'crt'),
	(0x37b3d0, 0x38f097, None),	# the unwind funclets and the atexit destructors of the game
]
OWNER_STARTS = [lo for lo, hi, o in CODE_OWNERS]

PAGE = 0x1000
INDEXED = -1	# the width of an indexed access: an array from there on

# the rvas of the C start at .rdata: a smaller number in its code is an offset or a count (the data formats of
# dinput8.lib in .text are reached through the pointers in .rdata)
DATA_START = 0x3a1000

# data of ScrapC.exe with numbers that only look like rvas: the 16 bit tables of the shader assembler, byte tables,
# the hook and probe tables (sizes, the ranges of the room)
NOT_RVAS = re.compile(r'^(yy\w+|kb_defaults|pad_defaults|mouse_gameDefaults|hooks|watch\.\d+)$')


def fail(msg):
	sys.exit('datamap.py: %s' % msg)


def code_owner(rva):
	i = bisect.bisect_right(OWNER_STARTS, rva) - 1
	if i >= 0 and rva < CODE_OWNERS[i][1]:
		return CODE_OWNERS[i][2] or 'game'
	return None


def clean(sym):
	"""the C name of a symbol of ScrapC.exe"""
	sym = re.sub(r'@\d+$', '', sym)
	return sym[1:] if sym[:1] in ('_', '@') else sym


class Map:
	"""the regions of the data of the original: (lo, hi, section, what) with what data (built in), bss or code"""

	def __init__(self, x):
		self.regions = []
		for lo, hi in room_pieces(x):
			what = 'code' if (lo, hi) in TEXT_KEEP[1:] else 'data'
			self.regions.append((lo, hi, x.section(lo), what))
		for name, (va, n, ptr, vsize) in x.secs.items():
			if name not in ('.text', '.reloc') and vsize > n:
				self.regions.append((va + n, va + vsize, name, 'bss'))
		self.regions.sort()
		self.starts = [r[0] for r in self.regions]
		self.targets = set()	# where the pointers of the original lead (code and data)
		for at in x.relocs():
			if x.off(at) is not None:
				self.targets.add(x.u32(at) - BASE)

	def region(self, rva):
		i = bisect.bisect_right(self.starts, rva) - 1
		if i >= 0 and rva < self.regions[i][1]:
			return self.regions[i]
		return None


def width(mn, ops):
	"""the bytes an instruction of objdump (AT&T) reads or writes at its memory operand"""
	if re.match(r'^(mov|and|or|xor|add|sub|mul|div|min|max|sqrt|cmp|ucomi|comi|rcp|rsqrt|unpck|shuf)[a-z]*p[sd]$', mn) \
			or mn in ('movdqu', 'movdqa', 'pxor', 'pand', 'por', 'paddd', 'psubd', 'pcmpeqd', 'movups', 'movaps'):
		return 16
	if mn.endswith('sd') or mn in ('movq', 'movlps', 'movhps', 'movlpd', 'movhpd', 'cvtps2pd', 'fldl', 'fstpl', 'fstl',
			'faddl', 'fmull', 'fsubl', 'fsubrl', 'fdivl', 'fdivrl', 'fcompl', 'fcoml', 'fildll', 'fistpll'):
		return 8
	if mn.endswith('ss') or mn in ('movd', 'cvtss2sd'):
		return 4
	if mn in ('fldt', 'fstpt'):
		return 10
	if mn.startswith(('movzb', 'movsb')) or mn.startswith('set'):
		return 1
	if mn.startswith(('movzw', 'movsw')):
		return 2
	if mn[-1:] in 'bwlq' and mn not in ('call', 'jl', 'jb', 'jbe', 'jle', 'shl', 'sal', 'rol', 'sub', 'sbb', 'imul',
			'mul'):
		return {'b': 1, 'w': 2, 'l': 4, 'q': 8}[mn[-1]]
	regs = re.findall(r'%([a-z0-9]+)', re.sub(r'\([^)]*\)', '', ops))
	for r in regs:
		if r in ('al', 'bl', 'cl', 'dl', 'ah', 'bh', 'ch', 'dh'):
			return 1
		if r in ('ax', 'bx', 'cx', 'dx', 'si', 'di', 'bp', 'sp'):
			return 2
		if r.startswith('xmm'):
			return 4
	return 4


class Symbols:
	"""the symbols of ScrapC.exe by address: its code and data, with the section symbols of the objects (.text,
	.rdata ...), which mark the anonymous parts (string literals, constants)"""

	def __init__(self, path):
		out = subprocess.run([NM, '-n', '--defined-only', path], capture_output=True, text=True, check=True).stdout
		self.addr, self.name = [], []
		named = {}
		for line in out.splitlines():
			p = line.split()
			if len(p) == 3 and p[1] in 'TtDdRrBb':
				a = int(p[0], 16)
				self.addr.append(a)
				self.name.append(clean(p[2]) if not p[2].startswith('.') else p[2])
				if not p[2].startswith('.'):
					named.setdefault(a, self.name[-1])
		self.name = [named.get(a, n) for a, n in zip(self.addr, self.name)]
		va, n, ptr, vsize = Exe(path).secs['.text']
		self.text = (BASE + va, BASE + va + vsize)	# the code of ScrapC.exe

	def at(self, va):
		"""the symbol va lies in, a name with a dot for an anonymous part"""
		i = bisect.bisect_right(self.addr, va) - 1
		return self.name[i] if i >= 0 else '.?'


def scan_c(m, cpath, syms):
	"""the rvas of the room in the code and the data of ScrapC.exe: [(rva, width or None, C function)], width None
	for an address that is taken or indexed (its extent is not known)"""
	try:
		cx = Exe(cpath)
	except ExeError as e:
		fail(str(e))
	crel = sorted(r + BASE for r in cx.relocs())
	dis = subprocess.run([OBJDUMP, '-d', '--no-show-raw-insn', '-j', '.text', cpath], capture_output=True, text=True,
		check=True).stdout
	insns = []	# (address, mnemonic, operands, function)
	func = '?'
	for line in dis.splitlines():
		if line.endswith('>:'):
			func = clean(line[line.index('<') + 1:-2])
			continue
		t = line.split('\t')
		if len(t) >= 2 and t[0].strip().endswith(':'):
			ops = t[1].split(None, 1)
			if ops:
				insns.append((int(t[0].strip()[:-1], 16), ops[0], ops[1] if len(ops) > 1 else '', func))
	seeds = []
	imm = re.compile(r'(\$?)(-?)0x([0-9a-f]+)(\([^)]*\))?')
	for k, (at, mn, ops, func) in enumerate(insns):
		if '0x' not in ops:
			continue
		nxt = insns[k + 1][0] if k + 1 < len(insns) else at + 16
		for dollar, neg, v, regs in imm.findall(ops):
			v = int(v, 16)
			if neg or v < DATA_START or not m.region(v):
				continue
			i = bisect.bisect_left(crel, at)
			if i < len(crel) and crel[i] < nxt:
				continue	# an address of ScrapC.exe itself
			if dollar:
				# game_base + rva; an immediate of another instruction is a number (a color, a count)
				if mn == 'add' and re.search(r',%e[a-z]{2}$', ops):
					seeds.append((v, None, func))
			elif regs and ',' in regs:
				seeds.append((v, INDEXED, func))
			elif mn == 'lea':
				seeds.append((v, None, func))
			elif regs:
				seeds.append((v, width(mn, ops), func))
	# tables of rvas in the data of ScrapC.exe: a number counts when the original points there too or the code of
	# ScrapC.exe has it (the data has numbers and text that only look like rvas)
	code_rvas = {s[0] for s in seeds}
	rel = set(cx.relocs())
	for name in ('.data', '.rdata'):
		if name not in cx.secs:
			continue
		va, n, ptr, vsize = cx.secs[name]
		b = cx.bytes(va, va + n)
		for o in range(0, n - 3, 4):
			v = struct.unpack_from('<I', b, o)[0]
			if va + o in rel or v < DATA_START or not m.region(v) or (v not in m.targets and v not in code_rvas):
				continue
			sym = syms.at(va + o + BASE)
			if not sym.startswith('.') and not NOT_RVAS.match(sym):
				seeds.append((v, None, sym))
	return seeds


def scan_tests(m):
	"""the rvas of the room in the test scripts"""
	seeds = []
	d = os.path.join(ROOT, 'tools', 'test', 'py')
	for f in sorted(os.listdir(d)):
		if f.endswith('.py'):
			for v in re.findall(r'\b0x([0-9a-fA-F]+)\b', open(os.path.join(d, f)).read()):
				v = int(v, 16)
				if v >= DATA_START and m.region(v):
					# what a test reads through SDTest the probe sees (it runs in the C), so a number that only
					# looks like an rva keeps no more than its unit
					seeds.append((v, 4, 'test ' + f[:-3]))
	return seeds


def scan_imports(x):
	"""what main.c reads and writes of the imports: the directory, the names and the import address table"""
	out = []
	lo, size = x.dirs[1]
	out.append((lo, lo + size))
	at = lo
	while x.u32(at + 12):
		name = x.u32(at + 12)
		out.append((name, name + x.bytes(name, name + 256).index(b'\0') + 1))
		for table in (x.u32(at), x.u32(at + 16)):
			t = table
			while table and x.u32(t):
				e = x.u32(t)
				if not e & 0x80000000:
					out.append((e, e + 2 + x.bytes(e + 2, e + 258).index(b'\0') + 1))
				t += 4
			if table:
				out.append((table, t + 4))
		at += 20
	lo, size = x.dirs[12]
	out.append((lo, lo + size))
	return out


def read_probe(paths, syms):
	"""the units of the probe logs: {unit rva: set of users}, and the retired pages"""
	units = collections.defaultdict(set)
	hot = set()
	for path in paths:
		mods = []
		for line in open(path, errors='replace'):
			p = line.split()
			if not p:
				continue
			if p[0] == 'm' and len(p) >= 3:
				mods.append((int(p[1], 16), os.path.basename(' '.join(p[2:]).replace('\\', '/')).lower()))
				mods.sort()
			elif p[0] == 'u' and len(p) == 5:
				rva, eip, ret = int(p[1], 16), int(p[3], 16), int(p[4], 16)
				if not syms.text[0] <= eip < syms.text[1]:
					mod = [n for b, n in mods if b <= eip]
					who = (syms.at(ret) + ' via ' if ret else '') + (mod[-1] if mod else '%x' % eip)
				else:
					who = syms.at(eip)
				units[rva & ~3].add(who)
			elif p[0] == 'p' and len(p) == 3:
				hot.add(int(p[1], 16))
	return units, hot


def read_functions():
	"""the functions of the original (symbols/functions.tsv): their rvas and sizes, sorted"""
	rows = []
	for line in open(os.path.join(ROOT, 'symbols', 'functions.tsv')):
		p = line.split('\t')
		if p[0] != 'rva':
			rows.append((int(p[0], 16), int(p[1])))
	rows.sort()
	return [r[0] for r in rows], [r[1] for r in rows]


def string_end(x, t, hi):
	"""the end of the string at t (after its NUL and the zero padding up to 4), None if no string starts there"""
	b = x.bytes(t, min(hi, t + 4096))
	n = b.find(b'\0')
	if n < 1 or not all(0x20 <= c < 0x7f or c in (9, 10, 13) or c >= 0xa0 for c in b[:n]) \
			or sum(1 for c in b[:n] if c >= 0xa0) * 4 > n:
		return None
	end = t + n + 1
	while end % 4 and end < hi and b[end - t:end - t + 1] == b'\0':
		end += 1
	return end


def guid_like(b):
	"""16 bytes that look like a GUID: version 1 or 4 with the variant of RFC 4122, or an OLE GUID
	(xxxxxxxx-xxxx-0000-C000-000000000046)"""
	if b[6:] == b'\0\0\xc0\0\0\0\0\0\0\x46':
		return True
	return b[7] >> 4 in (1, 4) and 0x80 <= b[8] < 0xc0


def guid_runs(x, lo, hi):
	"""the runs of at least 4 GUIDs in lo..hi (the GUID tables of the libraries, uuid.lib and dxguid.lib, which
	nothing points to but the code that uses one): [(start, count)]"""
	b = x.bytes(lo, hi)
	out = []
	p = (4 - lo % 4) % 4
	while p + 64 <= len(b):
		n = 0
		while p + 16 * (n + 1) <= len(b) and guid_like(b[p + 16 * n:p + 16 * n + 16]):
			n += 1
		if n >= 4:
			out.append((lo + p, n))
			p += 16 * n
		else:
			p += 4
	return out


def is_strings(b):
	"""bytes that are NUL terminated text, zero padded"""
	t = b.rstrip(b'\0')
	if not t or len(t) == len(b):
		return False
	text = sum(1 for c in t if 0x20 <= c < 0x7f or c in (9, 10, 13) or c >= 0xa0)
	return text + t.count(b'\0') == len(t) and text >= len(t) * 3 // 4 and b'\0\0\0\0' not in t


def plausible_float(v):
	e = v >> 23 & 0xff
	return v & 0x7fffffff == 0 or 0x60 <= e <= 0x9f


def plausible_double(v):
	e = v >> 52 & 0x7ff
	return v & 0x7fffffffffffffff == 0 or 0x3c0 <= e <= 0x440


class Piece:
	__slots__ = ('lo', 'hi', 'section', 'what', 'used', 'users', 'kind', 'owners', 'first')

	def __init__(self, lo, hi, section, what):
		self.lo, self.hi, self.section, self.what = lo, hi, section, what
		self.used, self.users, self.kind, self.owners, self.first = False, set(), None, set(), None


def build(x, cobjs, sd_symbols, seeds, probe_units, hot, probe_rows):
	"""the pieces (objects) of the data with their use and kind"""
	m = Map(x)
	relocs = sorted(x.relocs())
	code_targets = collections.defaultdict(set)	# target: owners of the code that points there
	data_targets = collections.defaultdict(set)	# target: rvas of the data pointers to it
	code_funcs = collections.defaultdict(set)	# target: the functions of the original that point there
	func_starts, func_sizes = read_functions()
	for at in relocs:
		try:
			t = x.u32(at) - BASE
		except ExeError:
			continue
		if not m.region(t):
			continue
		if at < x.secs['.text'][0] + x.secs['.text'][1] and not m.region(at):
			code_targets[t].add(code_owner(at))
			f = bisect.bisect_right(func_starts, at) - 1
			if f >= 0 and at < func_starts[f] + func_sizes[f]:
				code_funcs[t].add(f)
		elif m.region(at):
			data_targets[t].add(at)
	strong = set(data_targets)
	cuts = set(code_targets) | strong | {s[0] for s in seeds}
	# an access of a known width is a piece of its own
	cuts |= {rva + w for rva, w, who in seeds if w is not None and w > 0}
	for lo, hi, sec, what in m.regions:
		cuts |= {lo, hi}
	for o in cobjs:
		cuts |= {o.lo, o.hi}
		strong |= {o.lo, o.hi}
	for lo, hi in probe_rows:
		cuts |= {lo, hi}
	# in the GUID tables each GUID is an object of its own
	guids = []
	for lo, hi, sec, what in m.regions:
		if what == 'data' and sec == '.rdata':
			for start, n in guid_runs(x, lo, hi):
				guids.append((start, start + 16 * n))
				cuts |= {start + 16 * k for k in range(n + 1)}
				strong |= {start + 16 * k for k in range(n + 1)}
	# a string the original points to is an object from its start to its NUL and the zeros up to the next 4
	strings = {}
	for t in set(code_targets) | set(data_targets):
		r = m.region(t)
		if r[3] == 'data':
			end = string_end(x, t, r[1])
			if end:
				cuts |= {t, end}
				strings[t] = end
	pieces = []
	for lo, hi, sec, what in m.regions:
		inner = sorted(c for c in cuts if lo <= c <= hi)
		for a, z in zip(inner, inner[1:]):
			pieces.append(Piece(a, z, sec, what))
	starts = [p.lo for p in pieces]

	def find(rva):
		i = bisect.bisect_right(starts, rva) - 1
		return i if i >= 0 and rva < pieces[i].hi else None

	# the C objects
	names = {rva: name for name, rva in sd_symbols.items()}
	for o in cobjs:
		i = find(o.lo)
		while i is not None and i < len(pieces) and pieces[i].lo < o.hi:
			pieces[i].kind = 'C object'
			pieces[i].used = True
			pieces[i].users.add(clean(names.get(o.lo, o.name)))
			i += 1
	# the seeds the probe and the C read themselves, for the pointers to the RTTI below
	direct = {rva & ~3 for rva, w, who in seeds} | set(probe_units)

	strong_sorted = sorted(strong)

	work = []

	def use(i, who, extent):
		"""piece i is used by who; extent 'object' or 'array': also the pieces after it up to the next start of an
		object. That is a pointer target from the data, a C object or the end of the region; a piece the original
		code pointed to also ends it unless a function that pointed to the first piece pointed there too (a field,
		the next element). An object that starts with a string ends with it, an array goes on."""
		if i is None:
			return
		funcs = code_funcs.get(pieces[i].lo, set())
		limit = strings.get(pieces[i].lo) if extent == 'object' else None
		while i < len(pieces):
			p = pieces[i]
			if p.kind == 'C object':
				return
			new = not p.used
			p.used = True
			if new:
				p.first = who
			if who not in p.users and len(p.users) < 64:
				p.users.add(who)
			if new:
				work.append(i)
			if not extent or (limit and p.hi >= limit):
				return
			j = bisect.bisect_right(strong_sorted, p.lo)
			if p.hi >= p_end(i) or (j < len(strong_sorted) and strong_sorted[j] <= p.hi):
				return
			nxt = code_funcs.get(p.hi)
			if nxt and not nxt & funcs:
				return
			i += 1

	def p_end(i):
		lo, hi, sec, what = m.region(pieces[i].lo)
		return hi

	for rva, w, who in seeds:
		i = find(rva)
		if i is None:
			continue
		if w is None or w == INDEXED:
			use(i, who, 'object' if w is None else 'array')
		else:
			while i is not None and i < len(pieces) and pieces[i].lo < rva + w:
				use(i, who, False)
				i += 1
	for unit, users in probe_units.items():
		i = find(unit)
		while i is not None and i < len(pieces) and pieces[i].lo < unit + 4:
			for who in sorted(users):
				use(i, 'probe ' + who, False)
			i += 1
	for lo, hi in probe_rows:
		i = find(lo)
		while i is not None and i < len(pieces) and pieces[i].lo < hi:
			use(i, 'probe', False)
			i += 1
	for page in hot:
		i = find(page)
		if i is None:
			i = bisect.bisect_left(starts, page)
		while i < len(pieces) and pieces[i].lo < page + PAGE:
			if pieces[i].what == 'data':
				use(i, 'probe hot page %06x' % page, False)
			i += 1

	# pointers of used data. The pointer before a vtable to its complete object locator (RTTI) is followed only
	# when the C or the probe read it: the margins (hot pages, extents) cover many of them, the C reads no RTTI
	cols = set()
	tds = {t for t in set(data_targets) | set(code_targets) if x.off(t) is not None
		and x.bytes(t + 8, t + 12) in (b'.?AV', b'.?AU')}
	for t in data_targets:
		if x.off(t) is not None and x.off(t + 16) is not None and x.u32(t) == 0 and t + 12 in x.relocs() \
				and x.u32(t + 12) - BASE in tds:
			cols.add(t)
	while work:
		i = work.pop()
		p = pieces[i]
		if p.what != 'data':
			continue
		k = bisect.bisect_left(relocs, p.lo)
		while k < len(relocs) and relocs[k] < p.hi:
			at = relocs[k]
			k += 1
			t = x.u32(at) - BASE
			if t in PY_SYMBOLS or (t in cols and at & ~3 not in direct):
				continue
			j = find(t)
			if j is not None and pieces[j].what != 'code':
				use(j, 'ptr %06x' % at, 'object')

	kinds(x, pieces, code_targets, data_targets, guids)
	return m, pieces


def kinds(x, objs, code_targets, data_targets, guids):
	"""the kind of each object: bss, code, resource, import, rtti, guid, library data (only library code or library
	data points to it), else by its bytes"""
	starts = [o.lo for o in objs]
	gstarts = [g[0] for g in guids]

	def in_guids(o):
		i = bisect.bisect_right(gstarts, o.lo) - 1
		return i >= 0 and o.hi <= guids[i][1]

	rel = x.relocs()

	def find(rva):
		i = bisect.bisect_right(starts, rva) - 1
		return i if i >= 0 and rva < objs[i].hi else None

	def ptr(at):
		return x.u32(at) - BASE if at in rel else None

	# the RTTI: type descriptors (".?AV" after the vtable pointer and the spare word), the complete object locators
	# that point to them, their class hierarchies, base class arrays and descriptors
	rtti = set()
	for t in set(data_targets) | set(code_targets):
		if x.off(t) is not None and x.bytes(t + 8, t + 12) in (b'.?AV', b'.?AU'):
			rtti.add(t)
	tds = set(rtti)
	chds = []
	for t in data_targets:
		if x.off(t) is not None and x.off(t + 20) is not None and x.u32(t) == 0 and ptr(t + 12) in tds:
			rtti.add(t)
			if ptr(t + 16) is not None:
				chds.append(ptr(t + 16))
	seen = set()
	while chds:
		c = chds.pop()
		if c in seen or x.off(c) is None:
			continue
		seen.add(c)
		rtti.add(c)
		bca = ptr(c + 12)
		if bca is None:
			continue
		rtti.add(bca)
		for n in range(min(x.u32(c + 8), 64)):
			bcd = ptr(bca + 4 * n)
			if bcd is not None:
				rtti.add(bcd)
				if ptr(bcd + 24) is not None:
					chds.append(ptr(bcd + 24))
	rtti = {find(t) for t in rtti} - {None}
	imports = set()
	for lo, hi in scan_imports(x):
		i = find(lo)
		while i is not None and i < len(objs) and objs[i].lo < hi:
			imports.add(i)
			i += 1
	for i, o in enumerate(objs):
		if o.kind:
			continue
		if o.what in ('bss', 'code'):
			o.kind = o.what
		elif o.section == '.rsrc':
			o.kind = 'resource'
		elif i in imports:
			o.kind = 'import'
		elif i in rtti:
			o.kind = 'rtti'
		elif in_guids(o):
			o.kind = 'guid'
		else:
			o.owners = set(code_targets.get(o.lo, ()))
			if any(lo <= o.lo < hi for lo, hi in PY_DATA):
				o.owners = {'python'}
	# library data: reached only by library code, or only from library data; a piece nothing points to belongs to
	# the one before it
	changed = True
	while changed:
		changed = False
		for i, o in enumerate(objs):
			if o.kind or o.owners:
				continue
			src = data_targets.get(o.lo)
			if src:
				own = set()
				for at in src:
					j = find(at)
					own |= objs[j].owners if j is not None and objs[j].owners else {'game'}
				if own - {'game'} and 'game' not in own:
					o.owners = own
					changed = True
			elif i and not code_targets.get(o.lo) and objs[i - 1].owners and 'game' not in objs[i - 1].owners \
					and objs[i - 1].section == o.section:
				o.owners = set(objs[i - 1].owners)
				changed = True
	for o in objs:
		if o.kind:
			continue
		libs = o.owners - {'game'}
		if libs and 'game' not in o.owners:
			o.kind = 'library ' + '+'.join(sorted(libs))
		else:
			o.kind = content_kind(x, o)


def content_kind(x, p):
	b = x.bytes(p.lo, p.hi)
	if not b.strip(b'\0'):
		return 'zero'
	rel = sum(1 for a in range(p.lo, p.hi) if a in x.relocs())
	if rel and rel * 4 * 2 >= len(b):
		return 'pointer table'
	if is_strings(b):
		return 'string'
	if not rel and len(b) % 4 == 0 and p.lo % 4 == 0:
		w = struct.unpack('<%dI' % (len(b) // 4), b)
		if all(plausible_float(v) for v in w):
			return 'float'
		if len(b) % 8 == 0 and all(plausible_double(v) for v in struct.unpack('<%dQ' % (len(b) // 8), b)):
			return 'double'
	if rel:
		return 'unknown'
	return 'int table'


def label(users):
	if not users:
		return '-'
	u = sorted(users)
	if len(u) > 1:
		u = [n for n in u if not n.startswith('probe hot page')] or u
	if len(u) > 3:
		return ', '.join(u[:3]) + ' +%d' % (len(u) - 3)
	return ', '.join(u)


def rows_of(pieces):
	"""the pieces merged into rows of the same section, kind and users"""
	rows = []
	for p in pieces:
		lab = label(p.users) if p.used else '-'
		if rows and rows[-1][1] == p.lo and rows[-1][2] == p.section and rows[-1][3] == lab and rows[-1][4] == p.kind:
			rows[-1][1] = p.hi
		else:
			rows.append([p.lo, p.hi, p.section, lab, p.kind])
	return rows


def read_map(path):
	"""the rows of a map: [(lo, hi, section, used by, kind)]"""
	rows = []
	for line in open(path):
		if line.startswith('#') or not line.strip():
			continue
		p = line.rstrip('\n').split('\t')
		lo, size = int(p[0], 16), int(p[1], 16)
		rows.append((lo, lo + size, p[2], p[3], p[4]))
	return rows


def kept(rows):
	"""the ranges of built-in data the map keeps, merged, 4 aligned"""
	out = []
	for lo, hi, sec, users, kind in rows:
		if users == '-' or kind in ('bss', 'code'):
			continue
		lo, hi = lo & ~3, (hi + 3) & ~3
		if out and out[-1][1] >= lo:
			out[-1][1] = max(out[-1][1], hi)
		else:
			out.append([lo, hi])
	return [tuple(r) for r in out]


def main():
	args = sys.argv[1:]
	exe = os.path.join(ROOT, 'game', 'Bin', 'Scrap.exe')
	cpath = os.path.join(ROOT, 'build', 'ScrapC.exe')
	objects = os.path.join(ROOT, 'build', 'standalone', 'objects.txt')
	out = MAP
	check = False
	logs = []
	why = []
	while args:
		a = args.pop(0)
		if a == '--check':
			check = True
		elif a == '--why' and args:
			why.append(int(args.pop(0), 16))
		elif a in ('-e', '-c', '-s', '-o') and args:
			v = args.pop(0)
			if a == '-e':
				exe = v
			elif a == '-c':
				cpath = v
			elif a == '-s':
				objects = v
			else:
				out = v
		elif a.startswith('-'):
			fail('usage: datamap.py [--check] [--why rva] [-e Scrap.exe] [-c ScrapC.exe] [-s objects.txt] '
				'[-o datamap.txt] [dataprobe logs ...]')
		else:
			logs.append(a)
	try:
		x = Exe(exe)
	except ExeError as e:
		fail(str(e))
	m = Map(x)
	syms = Symbols(cpath)
	seeds = scan_c(m, cpath, syms) + scan_tests(m)

	if check:
		try:
			keep = kept(read_map(out))
		except (OSError, ValueError, IndexError) as e:
			fail('cannot read %s (%s)' % (out, e))
		starts = [lo for lo, hi in keep]

		def inside(a, b):
			i = bisect.bisect_right(starts, a) - 1
			return i >= 0 and b <= keep[i][1]

		bad = collections.defaultdict(set)
		for rva, w, who in seeds:
			r = m.region(rva)
			if r[3] == 'data' and not inside(rva, rva + (w if w and w > 0 else 1)):
				bad[rva].add(who)
		if bad:
			for rva in sorted(bad)[:20]:
				print('datamap.py: rva %06x (%s) is used by %s, but the map leaves it out' % (rva, m.region(rva)[2],
					label(bad[rva])), file=sys.stderr)
			fail('%d rvas of the C or the tests lie in data that %s leaves out: run tools/standalone/datamap.py '
				'(docs/standalone.md, stage 3b2)' % (len(bad), out))
		return

	cobjs = []
	sd_symbols = {}
	try:
		for line in open(objects):
			if line.strip():
				cobjs += coff_sd(line.strip(), sd_symbols)
	except (OSError, ExeError) as e:
		fail(str(e))
	cobjs.sort(key=lambda o: o.lo)
	seeds += [(lo, hi - lo, 'static: imports (main.c)') for lo, hi in scan_imports(x)]
	rs = x.secs['.rsrc']
	seeds.append((rs[0], rs[1], 'static: resources'))
	seeds += [(lo, hi - lo, 'static: import thunks of the game') for lo, hi in TEXT_KEEP[1:]]
	probe_units, hot = read_probe(logs, syms)
	probe_rows = []
	if not logs and os.path.exists(out):
		probe_rows = [(lo, hi) for lo, hi, sec, users, kind in read_map(out) if 'probe' in users]
	m, pieces = build(x, cobjs, sd_symbols, seeds, probe_units, hot, probe_rows)
	if why:
		starts = [p.lo for p in pieces]
		for rva in why:
			for n in range(40):
				p = pieces[bisect.bisect_right(starts, rva) - 1]
				print('%06x: piece %06x..%06x %s, %s' % (rva, p.lo, p.hi, p.kind, p.first or 'not used'))
				if not p.first or not p.first.startswith('ptr '):
					break
				rva = int(p.first[4:], 16)
		return
	rows = rows_of(pieces)

	# the summary per section and kind: all bytes and the used ones
	total = collections.Counter()
	used = collections.Counter()
	for lo, hi, sec, users, kind in rows:
		if kind in ('bss', 'code'):
			continue
		k = kind.split(' (')[0]
		total[sec, k] += hi - lo
		if users != '-':
			used[sec, k] += hi - lo
	lines = ['# tools/standalone/datamap.py: the data of Scrap.exe 1.6.1 that ScrapC.exe uses (docs/standalone.md, stage '
		'3b2), from', '# the static scan and %s. datagen.py builds in the rows with a user (4 aligned) but bss and code.'
		% ('%d probe logs' % len(logs) if logs else 'the probe rows of the map before'),
		'# columns: rva, size (hex), section, used by (C functions, test scripts, probe, ptr <rva of the pointer>,',
		'# static), kind', '#']
	for sec in sorted({s for s, k in total}, key=lambda s: x.secs[s][0]):
		for k in sorted(k for s, k in total if s == sec):
			lines.append('# %-8s %-24s %7d bytes, %7d used' % (sec, k, total[sec, k], used[sec, k]))
	lines.append('#')
	for lo, hi, sec, users, kind in rows:
		lines.append('%06x\t%x\t%s\t%s\t%s' % (lo, hi - lo, sec, users, kind))
	open(out, 'w').write('\n'.join(lines) + '\n')
	keep = kept(rows)
	print('datamap.py: %d rows, %d pieces, %d bytes of the data kept of %d' % (len(rows), len(pieces),
		sum(hi - lo for lo, hi in keep), sum(p.hi - p.lo for p in pieces if p.what == 'data')))


if __name__ == '__main__':
	main()
