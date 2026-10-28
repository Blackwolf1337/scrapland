#!/usr/bin/env python3
# strings.py [-w] [Scrap.exe]: the strings of Scrap.exe 1.6.1 as C for ScrapC.exe (docs/standalone.md, stage 3b4).
#
# A string of the original is a run of text bytes with its NUL at a multiple of 4, padded with zeros to the next 4,
# in the game's rows of the data map (tools/standalone/datamap.txt) of the kinds string, int table, float, double or
# zero (the map cuts strings where the C or an inlined strcmp or strcpy of the original reads them). Each one is
# found from its start, which the original or the C points to (at it or in its first 4 bytes: MSVC inlines strcmp
# with a compare of the first 2 bytes). It is no string when the original reads it as a float or double there (SSE,
# x87), when the C reads it only as another type (GAME_DATA or a pointer cast of a type that is no char), or when it
# is a single character in .data (the ints of a table). Strings nothing points to count when they lie in string rows
# only and are plain text. Packed strings (a literal with NULs in it, a filter of the file dialog) stay one object.
# Zero words after a string that nothing reads are its padding (in .data the rest of a char buffer); a zero word of
# .rdata the C reads as a string is the empty string.
#
# Left out: the strings of the libraries (their rows are kinds library *), and the strings inside other structures,
# which stage 3b5 does with the globals and tables: of the data that is built in that is only the name of the RTTI
# type descriptor of galaxy::api::IError (Gog_CatchError).
#
# Each string or packed run becomes one SD_AT object, named after its use: str_GVar_<name>, str_GVarDesc_<name> and
# str_GVarValue_<name> for the name, the help text and the default (or the script function) of the engine variables
# of src/game/Global/StaticInit.inc, str_NativeFunc_<name> for the script functions of the C, str_PyMethod_<module>_
# <name> and str_PyDoc_<module>_<name> for the method tables of the Python modules, else str_<rva>.
#
# Without -w it prints the counts and checks that every GAME_STR(name, rva) of src/ and include/ names the object at
# rva. With -w it writes src/game/Global/Strings*.c (one per 64 KB of rvas) and src/game/Global/StringsAt.h (the
# object of each string rva StaticInit.inc uses, for GAME_STR_AT).
import bisect, collections, os, re, sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from sdexe import BASE, ROOT, Exe, ExeError
from datamap import MAP, read_map

OUT_DIR = 'src/game/Global'
AT_H = 'src/game/Global/StringsAt.h'
GLOBALS_AT_H = 'src/game/Global/GlobalsAt.h'
STATIC_INIT = 'src/game/Global/StaticInit.inc'

# the kinds of the map a string may lie in
KINDS = ('string', 'int table', 'float', 'double', 'zero')
# the names by hand
NAMES = {
	0x3a3228: 'str_Empty',
}


def fail(msg):
	sys.exit('strings.py: %s' % msg)


def text(c):
	return c >= 0x20 and c != 0x7f or c in (9, 10, 13)


def plain(c):
	"""the characters of a string nothing points to: ASCII text and the letters of the Spanish texts"""
	return 0x20 <= c < 0x7f or c in (9, 10, 13) or c in b'\xe1\xe9\xed\xf3\xfa\xf1\xc1\xc9\xcd\xd3\xda\xd1\xbf\xa1\xfc'


class Str:
	"""one object: rva lo..hi, the strings in it (their rvas) and the size of the C array (hi without the padding of
	the assembler, None for the literal's own size)"""

	def __init__(self, lo, hi, sec, starts):
		self.lo, self.hi, self.sec, self.starts = lo, hi, sec, starts
		self.size = None
		self.name = None


class Strings:
	def __init__(self, x, rows, c_rvas):
		self.x = x
		self.rows = rows
		self.row_starts = [r[0] for r in rows]
		self.targets = collections.defaultdict(set)	# rva: the rvas of the pointers of the original to it
		for at in x.relocs():
			if x.off(at) is not None:
				self.targets[x.u32(at) - BASE].add(at)
		self.c_rvas = c_rvas
		self.text_va, self.text_n = x.secs['.text'][0], x.secs['.text'][1]
		self.data = {}
		for name in ('.rdata', '.data'):
			va, n, ptr, vsize = x.secs[name]
			self.data[name] = (va, x.bytes(va, va + n))
		self.excluded = []	# (Str, why)
		self.objs = self.find()

	def byte(self, a):
		va, b = self.data[self.x.section(a)]
		return b[a - va]

	def row(self, a):
		i = bisect.bisect_right(self.row_starts, a) - 1
		return self.rows[i] if i >= 0 and a < self.rows[i][1] else None

	def kind(self, a):
		r = self.row(a)
		return r[4] if r else None

	def parse(self, t, hi):
		"""the strings from t: (end at a multiple of 4, [starts]) or None"""
		e = t
		starts = []
		while True:
			starts.append(e)
			while e < hi and self.byte(e):
				if not text(self.byte(e)):
					return None
				e += 1
			if e >= hi:
				return None
			e += 1
			if e % 4 and e < hi and self.byte(e):
				continue	# packed: the next string right after one NUL
			while e % 4 and e < hi and not self.byte(e):
				e += 1
			return (e, starts) if e % 4 == 0 else None

	def access(self, at):
		"""'f' when the instruction of the original with the address at at reads a float or double there"""
		if not self.text_va <= at < self.text_va + self.text_n:
			return 'c'
		b = self.x.bytes(at - 4, at)
		if b[3] & 0xc7 != 0x05:	# no [disp32] operand
			return 'c'
		op = b[2]
		if b[1] == 0x0f:
			if b[0] == 0xf3 and op in (0x10, 0x11, 0x2a, 0x2c, 0x2d, 0x51, 0x52, 0x53, 0x58, 0x59, 0x5a, 0x5c, 0x5d,
					0x5e, 0x5f, 0xc2):
				return 'f'	# the scalar single ops (movq 7e and movdqu 6f copy strings)
			if b[0] in (0xf2, 0x66) and op not in (0x6f, 0x7e, 0x7f, 0xd6):
				return 'f'
			if op in (0x28, 0x29, 0x2e, 0x2f, 0x54, 0x55, 0x56, 0x57, 0x58, 0x59, 0x5a, 0x5c, 0x5d, 0x5e, 0x5f):
				return 'f'
		if op in (0xd8, 0xd9, 0xdc, 0xdd):
			return 'f'	# x87
		return 'c'

	def pointed(self, a, n=1):
		return any(t in self.targets or t in self.c_rvas for t in range(a, a + n))

	def floaty(self, a):
		return any(self.access(r) == 'f' for t in range(a, a + 4) for r in self.targets.get(t, ()))

	def find(self):
		runs = []	# the runs of rows of KINDS
		for lo, hi, sec, users, kind in self.rows:
			if sec in self.data and kind in KINDS:
				if runs and runs[-1][1] == lo:
					runs[-1][1] = hi
				else:
					runs.append([lo, hi, sec])
		objs = []
		for lo, hi, sec in runs:
			p = (lo + 3) & ~3
			while p < hi:
				u = self.parse(p, hi) if self.byte(p) else None
				if u and self.accept(p, u):
					objs.append(Str(p, u[0], sec, u[1]))
					p = u[0]
					# the zero words after it that nothing reads: its padding
					while p + 4 <= hi and self.x.bytes(p, p + 4) == b'\0' * 4 and not self.pointed(p, 4):
						p += 4
					if p != objs[-1].hi:
						objs[-1].size = p - objs[-1].lo
						objs[-1].hi = p
				elif sec == '.rdata' and p + 4 <= hi and self.x.bytes(p, p + 4) == b'\0' * 4 \
						and self.c_rvas.get(p) == 'str':
					objs.append(Str(p, p + 4, sec, [p]))	# the empty string
					p += 4
				else:
					p += 4
		return objs

	def accept(self, p, u):
		end, starts = u
		b = self.x.bytes(p, end)
		first = b[:b.index(b'\0')]
		if not any(self.kind(a) == 'string' for a in range(p, end)):
			return False
		if any(a in self.x.relocs() for a in range(p - 3, end)) or first.startswith(b'.?A'):
			return False	# pointers, the name of an RTTI type descriptor
		if self.floaty(p):
			return False
		uses = {self.c_rvas[a] for a in range(p, end) if a in self.c_rvas}
		if 'other' in uses and 'str' not in uses:
			return False	# the C reads it as something else
		if self.x.section(p) == '.data' and len(first) < 2 and 'str' not in uses:
			return False	# the ints of a table
		if sum(1 for c in first if 0x20 <= c < 0x7f or c in (9, 10, 13)) * 2 < len(first):
			return False
		if self.pointed(p, 4):
			return True
		# nothing points to it: only plain text in string rows
		return len(first) >= 4 and all(plain(c) or c == 0 for c in b) \
			and all(self.kind(a) == 'string' for a in range(p, end))


ESCAPES = {0x5c: '\\\\', 0x22: '\\"', 0x0a: '\\n', 0x09: '\\t', 0x0d: '\\r'}


def c_literal(b):
	"""the characters of a C literal of the bytes (without the NUL the compiler adds), one item per byte: the build has
	-fexec-charset=CP1252 and UTF-8 sources, so the Latin-1 letters 0xa1..0xff are UTF-8 (the same code points in
	Windows-1252), everything else that is not ASCII text an octal escape of 3 digits"""
	out = []
	for c in b:
		if c in ESCAPES:
			out.append(ESCAPES[c])
		elif 0x20 <= c < 0x7f:
			out.append('\\?' if c == 0x3f and out and out[-1] == '?' else chr(c))	# no trigraphs
		elif c >= 0xa1 and c != 0xad:
			out.append(chr(c))
		else:
			out.append('\\%03o' % c)
	return out


def c_sources():
	"""the C of src/ and include/ but the vendored Python, what this tool writes and the globals of globals.py (stage
	3b5, made after the strings)"""
	for top in ('src', 'include'):
		for d, _, fs in os.walk(os.path.join(ROOT, top)):
			if os.path.relpath(d, ROOT).startswith('src/python'):
				continue
			for f in sorted(fs):
				path = os.path.join(d, f)
				if f.endswith(('.c', '.h', '.inc')) and not re.match(r'^(Strings|Globals)[0-9a-f]{2}\.c$', f) \
						and os.path.relpath(path, ROOT) not in (AT_H, GLOBALS_AT_H):
					yield path


# the forms of the C that read a string by its rva
STR_USE = re.compile(r'\(\s*(?:const\s+)?(?:unsigned\s+)?char\s*\*\s*\)\s*Game_Ptr\(\s*(0x[0-9a-fA-F]+)\s*\)'
	r'|GAME_STR\(\s*\w+\s*,\s*(0x[0-9a-fA-F]+)\s*\)|GAME_STR_AT\((0x[0-9a-fA-F]+)\)')
GVAR_ARGS = {	# the positions of the string arguments of the lines of StaticInit.inc
	'INIT_GVAR_INT': (3, 5), 'INIT_GVAR_FLOAT': (3, 5), 'INIT_GVAR_FLOAT_DATA': (3, 5), 'INIT_GVAR_FLOAT_MULDIV': (3, 5),
	'INIT_GVAR_FLOAT_ADD': (3, 5), 'INIT_GVAR_STRING': (4, 6, 7), 'INIT_GVAR_FUNCTION': (4, 6, 7),
	'INIT_NATIVE_FUNC': (3,),
}


def static_init():
	"""the strings of StaticInit.inc: [(rva, kind, name)] with kind GVar, GVarDesc, GVarValue or NativeFunc"""
	out = []
	for line in open(os.path.join(ROOT, STATIC_INIT)):
		m = re.match(r'^(\w+)\((.*)\)$', line.strip())
		if not m or m.group(1) not in GVAR_ARGS:
			continue
		args = [a.strip() for a in m.group(2).split(',')]
		fn = args[1]
		for i, k in zip(GVAR_ARGS[m.group(1)], ('', 'Desc', 'Value')):
			if args[i] != '0':
				if fn.startswith('Init_GVar_'):
					out.append((int(args[i], 16), 'GVar' + k, fn[len('Init_GVar_'):]))
				else:
					out.append((int(args[i], 16), 'NativeFunc', fn[len('Init_NativeFunc_'):]))
	return out


# the forms of the C that read something else there: GAME_DATA, the globals of stage 3b5 (GAME_GLOBAL, GAME_ARRAY and
# their _IN views) or a pointer cast of a type that is no char or void
OTHER_USE = re.compile(r'GAME_(?:DATA|GLOBAL|ARRAY|GLOBAL_IN|ARRAY_IN)\(\s*([^,()]+?)\s*,(?:\s*\w+\s*,){0,2}'
	r'\s*(0x[0-9a-fA-F]+)\s*\)'
	r'|\(\s*((?:const\s+)?\w+)\s*\*\s*\)\s*Game_Ptr\(\s*(0x[0-9a-fA-F]+)\s*\)')


def c_uses(lo, hi):
	"""the rvas lo..hi in the C: {rva: 'str' for a use as a string, 'other' for a use as another type, '' else}"""
	uses = {}
	for path in c_sources():
		t = open(path, encoding='utf-8').read()
		for m in OTHER_USE.finditer(t):
			typ, v = (m.group(1), m.group(2)) if m.group(2) else (m.group(3), m.group(4))
			v = int(v, 16)
			if lo <= v < hi and not re.search(r'\b(char|void)\b', typ):
				uses.setdefault(v, 'other')
		for v in re.findall(r'\b0x([0-9a-fA-F]+)\b', t):
			v = int(v, 16)
			if lo <= v < hi:
				uses.setdefault(v, '')
		for m in STR_USE.finditer(t):
			uses[int(next(g for g in m.groups() if g), 16)] = 'str'
	for rva, k, name in static_init():
		uses[rva] = 'str'
	return uses


def method_tables():
	"""the method tables of the Python modules and types the C names: [(module, rva)]"""
	out = []
	for path in c_sources():
		t = open(path, encoding='utf-8').read()
		out += [(n, int(v, 16)) for n, v in re.findall(r'^SD_PY_MODULE\((\w+), (0x[0-9a-fA-F]+)\)', t, re.M)]
		out += [(n, int(v, 16)) for n, v in
			re.findall(r'#define (\w+)_methods \(&GAME_(?:DATA|GLOBAL|GLOBAL_IN)\(PyMethodDef, (?:\w+, ){0,2}'
				r'(0x[0-9a-fA-F]+)\)\)', t)]
	return sorted(set(out))


def ident(s):
	return re.sub(r'\W', '_', s)


def name_all(x, objs):
	"""names the objects after their use"""
	by_lo = {o.lo: o for o in objs}
	starts = [o.lo for o in objs]
	taken = set()

	def give(rva, name):
		i = bisect.bisect_right(starts, rva) - 1
		if i < 0 or rva >= objs[i].hi or objs[i].name:
			return
		o = objs[i]
		if rva != o.lo:
			return	# a string inside a packed run or after an inlined compare: the run keeps the name of its start
		if name in taken:
			name = '%s_%06x' % (name, rva)
		o.name = name
		taken.add(name)

	for rva, name in NAMES.items():
		give(rva, name)
	for rva, kind, name in static_init():
		give(rva, 'str_%s_%s' % (kind, name))
	for module, table in method_tables():
		at = table
		while x.u32(at):
			name = x.u32(at) - BASE
			o = by_lo.get(name)
			if o:
				meth = ident(x.bytes(o.lo, o.hi).split(b'\0')[0].decode('latin-1'))
				give(name, 'str_PyMethod_%s_%s' % (module, meth))
				if x.u32(at + 12):
					give(x.u32(at + 12) - BASE, 'str_PyDoc_%s_%s' % (module, meth))
			at += 16
	for o in objs:
		if not o.name:
			o.name = 'str_%06x' % o.lo


def c_body(x, o):
	"""the definition of the object o"""
	b = x.bytes(o.lo, o.hi)
	lit = b.rstrip(b'\0')
	size = '' if o.size is None and (len(lit) + 4) & ~3 == len(b) else '%d' % len(b)
	qual = 'const ' if o.sec == '.rdata' else ''
	head = '%schar %s[%s] SD_AT(0x%06x) =' % (qual, o.name, size, o.lo)
	chars = c_literal(lit)
	if len(head) + sum(len(c) for c in chars) + 4 <= 120:
		return ['%s "%s";' % (head, ''.join(chars))]
	# long texts in pieces of up to 100 columns, after a line break of the text where there is one
	pieces = []
	while chars:
		n = w = brk = 0
		while n < len(chars) and w + len(chars[n]) <= 100:
			w += len(chars[n])
			n += 1
			if chars[n - 1] == '\\n' and w > 20:
				brk = n
		if n < len(chars) and brk:
			n = brk
		pieces.append(''.join(chars[:n]))
		chars = chars[n:]
	return [head] + ['\t"%s"%s' % (p, ';' if i == len(pieces) - 1 else '') for i, p in enumerate(pieces)]


def at_header(objs):
	"""StringsAt.h: SD_STR_<rva> for each string rva of StaticInit.inc"""
	starts = [o.lo for o in objs]
	lines = [
		'/*',
		' * The C object of each string rva of src/game/Global/StaticInit.inc in ScrapC.exe (docs/standalone.md, stage',
		' * 3b4), generated by tools/standalone/strings.py, do not edit. GAME_STR_AT(rva) of scrap/game.h reads it.',
		' */',
		'#define SD_STR_0 ((char *)NULL)	/* no string (a script variable without a function) */',
	]
	seen = set()
	# and the string arguments of the constructors (initpat.py writes them as GAME_STR_AT)
	args = [int(v, 16) for v in re.findall(r'\bGAME_STR_AT\((0x[0-9a-fA-F]+)\)', open(os.path.join(ROOT,
		STATIC_INIT)).read())]
	for rva in [s[0] for s in static_init()] + args:
		if rva in seen:
			continue
		seen.add(rva)
		i = bisect.bisect_right(starts, rva) - 1
		if i < 0 or rva >= objs[i].hi:
			fail('StaticInit.inc: the string at rva %06x is no C object' % rva)
		o = objs[i]
		ref = 'GAME_STR(%s, 0x%06x)' % (o.name, o.lo)
		lines.append('#define SD_STR_0x%06x %s' % (rva, ref if rva == o.lo else '(%s + %d)' % (ref, rva - o.lo)))
	return lines


def check_uses(objs):
	"""every GAME_STR(name, rva) of the C names the object at rva"""
	byname = {o.name: o.lo for o in objs}
	bad = uses = 0
	for path in c_sources():
		for name, rva in re.findall(r'GAME_STR\((\w+), (0x[0-9a-fA-F]+)\)', open(path, encoding='utf-8').read()):
			uses += 1
			if byname.get(name) != int(rva, 16):
				bad += 1
				print('strings.py: %s: GAME_STR(%s, %s), the string there is %s' % (os.path.relpath(path, ROOT), name,
					rva, next((n for n, r in byname.items() if r == int(rva, 16)), 'none')))
	return uses, bad


def main():
	args = [a for a in sys.argv[1:] if a != '-w']
	try:
		x = Exe(args[0] if args else os.path.join(ROOT, 'game/Bin/Scrap.exe'))
	except ExeError as e:
		fail(str(e))
	try:
		rows = read_map(MAP)
	except (OSError, ValueError, IndexError) as e:
		fail('cannot read %s (%s)' % (MAP, e))
	lo, hi = x.secs['.rdata'][0], x.secs['.data'][0] + x.secs['.data'][3]
	s = Strings(x, rows, c_uses(lo, hi))
	objs = s.objs
	name_all(x, objs)
	count = collections.Counter(o.sec for o in objs)
	size = collections.Counter()
	for o in objs:
		size[o.sec] += o.hi - o.lo
	named = sum(1 for o in objs if not re.match(r'^str_[0-9a-f]{6}$', o.name))
	print('strings.py: %d strings in .rdata (%d bytes), %d in .data (%d bytes), %d named after their use'
		% (count['.rdata'], size['.rdata'], count['.data'], size['.data'], named))
	# the rows of kind string the objects leave: floats and ints the map took for text, the strings of other structures
	cov = set()
	for o in objs:
		cov.update(range(o.lo, o.hi))
	left = collections.Counter()
	for lo, hi, sec, users, kind in rows:
		if kind == 'string':
			left[sec] += sum(1 for a in range(lo, hi) if a not in cov)
	print('strings.py: left of the string rows of the map: %s' % ', '.join('%s %d bytes' % (sec, n)
		for sec, n in sorted(left.items())))
	uses, bad = check_uses(objs)
	print('strings.py: %d GAME_STR uses' % uses)
	if bad:
		fail('%d GAME_STR uses name another string' % bad)
	if '-w' not in sys.argv:
		return

	files = collections.defaultdict(list)
	for o in objs:
		files[o.lo >> 16].append(o)
	old = [f for f in os.listdir(os.path.join(ROOT, OUT_DIR)) if re.match(r'^Strings[0-9a-f]{2}\.c$', f)]
	for f in old:
		os.remove(os.path.join(ROOT, OUT_DIR, f))
	for k, group in sorted(files.items()):
		lines = [
			'/*',
			' * The strings of Scrap.exe 1.6.1 at rva 0x%06x..0x%06x as C objects of ScrapC.exe at the same rvas'
				% (k << 16, (k + 1) << 16),
			' * (docs/standalone.md, stage 3b4), generated by tools/standalone/strings.py, do not edit. The bytes are the',
			' * original\'s (Windows-1252, the build has -fexec-charset=CP1252): the letters as UTF-8, the rest as octal',
			' * escapes. An array with a size holds the zeros after the text too. The hook DLL uses the exe\'s own',
			' * strings. The C reaches them through GAME_STR (scrap/game.h).',
			' */',
			'#include "scrap/shared.h"',
			'',
			'#ifdef SD_STANDALONE',
			'',
		]
		for o in group:
			lines += c_body(x, o)
		lines += ['', '#endif', '']
		path = os.path.join(OUT_DIR, 'Strings%02x.c' % k)
		open(os.path.join(ROOT, path), 'w', encoding='utf-8').write('\n'.join(lines))
		print('strings.py: wrote %s (%d strings)' % (path, len(group)))
	open(os.path.join(ROOT, AT_H), 'w').write('\n'.join(at_header(objs)) + '\n')
	print('strings.py: wrote %s' % AT_H)


if __name__ == '__main__':
	main()
