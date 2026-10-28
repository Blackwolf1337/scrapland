#!/usr/bin/env python3
# testsyms.py [-w] [-v] [-l]: the test scripts of tools/test/py address the game by name (docs/standalone.md, stage 3c0).
#
# It rewrites the rvas of the scripts to names where the C has one and writes the table of the names they use,
# src/core/testsyms.inc, which SDTest.Sym of src/core/testmod.c reads:
#   - a function rva of src/hooks.inc (or an entry of the original's CRT that ScrapC.exe points to its C) becomes the
#     name of its C function: 'Str_Trim' as the first argument of SDTest.Call, sym('Str_Trim') anywhere else. A C
#     function that backs several copies of the original names the first by its name and the others name@rva
#   - a data rva becomes the C object of SD_AT there (the objects of build/standalone/objects.txt, so build ScrapC
#     first), else the name of a GAME_DATA or Game_Ptr define, else the name of symbols/globals.tsv: 'g_Name' as the
#     first argument of SDTest.Peek and Poke, sym('g_Name') + offset inside an object
#   - an absolute address (rva + 0x400000) becomes symva('name'), symva('name') + offset
# The numbers that stay are counted by kind (-v lists them). The rewrite is idempotent and the run without -w only
# reports: exit code 1 when the scripts or the table would change. -l lists the rewrites outside the first argument of
# SDTest (in tables, comparisons, arithmetic), the ones to look at.
#
# A number is a candidate where it can be an address: not an offset added to something else (x + 0x24), not an
# operand of * / % & | ^ << >> or a unary minus. 0x400000 + rva keeps the 0x400000.
import bisect, collections, glob, io, os, re, sys, tokenize

ROOT = os.path.normpath(os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', '..'))
sys.path.insert(0, os.path.join(ROOT, 'tools', 'standalone'))

TESTS = os.path.join(ROOT, 'tools', 'test', 'py')
TABLE = os.path.join(ROOT, 'src', 'core', 'testsyms.inc')
OBJECTS = os.path.join(ROOT, 'build', 'standalone', 'objects.txt')
BASE = 0x400000
CODE = (0x1000, 0x390000)	# .text, below it the headers
DATA = (0x390000, 0x55e000)	# .bss_ste .. .reloc
UNSIZED = 0x1000	# how far into an object of unknown size (a struct, an array of Game_Ptr) an offset may reach
SDTEST_RVA = ('Call', 'Peek', 'Poke', 'ConvU')	# the SDTest functions whose first argument is an rva
NOT_ADDRESS_OPS = ('*', '/', '%', '&', '|', '^', '<<', '>>', '**', '~')

# sizes of the GAME_DATA types the C uses most; another type leaves the size unknown
TYPE_SIZES = {
	'char': 1, 'byte': 1, 'uint8_t': 1, 'int8_t': 1, 'bool': 1,
	'short': 2, 'uint16_t': 2, 'int16_t': 2, 'ushort': 2,
	'int': 4, 'uint': 4, 'unsigned': 4, 'uint32_t': 4, 'int32_t': 4, 'float': 4, 'DWORD': 4, 'long': 4,
	'double': 8, 'int64_t': 8, 'uint64_t': 8, 'vec3_t': 12,
}


class Sym:
	def __init__(self, key, cname, rva, kind, size=None):
		self.key, self.cname, self.rva, self.kind, self.size = key, cname, rva, kind, size
		self.ghidra = False	# a name of symbols/globals.tsv only


def fail(msg):
	sys.exit('testsyms.py: ' + msg)


def read(path):
	return open(path, encoding='utf-8', errors='replace').read()


def type_size(t, fnptrs):
	t = re.sub(r'\b(const|volatile|struct)\b', '', t).strip()
	if t.endswith('*') or t in fnptrs:
		return 4
	return TYPE_SIZES.get(t)


def functions():
	"""the C functions by rva: the SD_HOOKs of src/hooks.inc, then the entries of the original's CRT of ScrapC.exe (the
	SD_IMPORT_CRT of src/imports.inc and the SD_HOOKs of src/standalone/crthooks.inc)"""
	out = {}
	seen = set()

	def add(name, rva, kind):
		if rva in out:
			return
		key = name if name not in seen else '%s@%x' % (name, rva)
		seen.add(name)
		out[rva] = Sym(key, name, rva, kind)

	hook = re.compile(r'^SD_HOOK\((\w+), *(0x[0-9a-fA-F]+), *\d+\)', re.M)
	for name, rva in hook.findall(read(os.path.join(ROOT, 'src', 'hooks.inc'))):
		add(name, int(rva, 16), 'FUNC')
	crt = re.compile(r'^SD_IMPORT_CRT(?:_STDCALL)?\((\w+), *(?:\d+, *)?(0x[0-9a-fA-F]+)\)', re.M)
	for name, rva in crt.findall(read(os.path.join(ROOT, 'src', 'imports.inc'))):
		add(name, int(rva, 16), 'CRT')
	for name, rva in hook.findall(read(os.path.join(ROOT, 'src', 'standalone', 'crthooks.inc'))):
		add(name, int(rva, 16), 'CRT')
	return out


def c_objects():
	"""the C objects of SD_AT by rva, from the object files of scrapcode_sa (objects.txt of the build)"""
	from sdexe import ExeError, coff_sd
	if not os.path.exists(OBJECTS):
		fail('no %s: build ScrapC first (cmake --build build)' % os.path.relpath(OBJECTS))
	out = {}
	try:
		for path in open(OBJECTS).read().split():
			names = {}
			objs = coff_sd(path, names)
			by_rva = {}
			for name, rva in names.items():
				by_rva.setdefault(rva, name[1:] if name.startswith('_') else name)	# the i386 COFF name has a _
			for o in objs:
				if o.rva in by_rva:
					out[o.rva] = Sym(by_rva[o.rva], by_rva[o.rva], o.rva, 'OBJ', o.size)
	except ExeError as e:
		fail(str(e))
	return out


def defines():
	"""the names of the data in the C: #define name GAME_DATA(type, rva), #define name ((type *)Game_Ptr(rva)) and the
	GAME_GLOBAL forms of stage 3b5,
	by rva. Several names of one rva: the first that is not all capitals, in the order of the paths"""
	found = collections.defaultdict(list)
	gd = re.compile(r'^[ \t]*#[ \t]*define[ \t]+(\w+)[ \t]+GAME_DATA\(([^\n]*?), *(0x[0-9a-fA-F]+)\)[ \t]*(?:/\*.*)?$', re.M)
	gp = re.compile(r'^[ \t]*#[ \t]*define[ \t]+(\w+)[ \t]+\(\(([\w ]+?\**)[ \t]*\)[ \t]*Game_Ptr\((0x[0-9a-fA-F]+)\)\)', re.M)
	fp = re.compile(r'\btypedef\b[^;{}]*?\(\s*(?:__\w+\s+|WINAPI\s+)?\*\s*(\w+)\s*\)\s*\(')
	# the forms of stage 3b5: GAME_GLOBAL(type, name, rva), GAME_GLOBAL_IN(type, owner, off, rva), GAME_ARRAY(...),
	# GAME_ARRAY_IN(...) and GAME_PTR(owner, off, rva); the last argument is the rva
	gg = re.compile(r'^[ \t]*#[ \t]*define[ \t]+(\w+)[ \t]+\(?GAME_(GLOBAL|GLOBAL_IN|ARRAY|ARRAY_IN|PTR)\(([^\n]*?), *'
		r'(0x[0-9a-fA-F]+)\)\)?[ \t]*(?:/\*.*)?$', re.M)
	paths = sorted(glob.glob(os.path.join(ROOT, 'include', '**', '*.h'), recursive=True) +
		glob.glob(os.path.join(ROOT, 'src', '**', '*.[ch]'), recursive=True))
	texts = [read(path) for path in paths]
	fnptrs = set()
	for s in texts:
		fnptrs.update(fp.findall(s))
	for s in texts:
		if 'GAME_DATA' not in s and 'Game_Ptr' not in s and 'GAME_' not in s:
			continue
		for name, form, args, rva in gg.findall(s):
			if name.startswith('SD_'):
				continue	# SD_DATA_<rva> and the other aliases of GlobalsAt.h and StringsAt.h
			size = type_size(args.split(',')[0].strip(), fnptrs) if form in ('GLOBAL', 'GLOBAL_IN') else None
			found[int(rva, 16)].append((name, size))
		for name, t, rva in gd.findall(s):
			found[int(rva, 16)].append((name, type_size(t, fnptrs)))
		for name, t, rva in gp.findall(s):
			found[int(rva, 16)].append((name, None))
	out = {}
	for rva, names in found.items():
		names.sort(key=lambda n: n[0].isupper())
		out[rva] = Sym(names[0][0], names[0][0], rva, 'DATA', names[0][1])
	return out


def ghidra_globals():
	"""the names of symbols/globals.tsv that a person or the RTTI gave (not DAT_, the strings of Ghidra or mangled
	names), :: as _"""
	out = {}
	for line in read(os.path.join(ROOT, 'symbols', 'globals.tsv')).splitlines()[1:]:
		c = line.split('\t')
		if len(c) < 3 or c[1] not in ('USER_DEFINED', 'IMPORTED') or 'RTTI' in c[2] or len(c) > 3 and (
				c[3].startswith('/RTTI') or c[3].startswith('/TypeDescriptor')):
			continue
		name = c[2].replace('::', '_')
		if not re.fullmatch(r'[A-Za-z_]\w*', name) or re.match(r'(DAT|PTR|s|u|LAB|FUN)_', name):
			continue
		rva = int(c[0], 16)
		size = None
		m = re.fullmatch(r'/(pointer|int|uint|float|dword|undefined4)(?:\[(\d+)\])?', c[3] if len(c) > 3 else '')
		if m:
			size = 4 * int(m.group(2) or 1)
		out.setdefault(rva, Sym(name, name, rva, 'DATA', size))
		out[rva].ghidra = True
	return out


class Names:
	def __init__(self):
		self.funcs = functions()
		data = ghidra_globals()
		data.update(defines())
		# a C object takes the rva, but where a define names the same rva the tests keep the define's name (what the
		# C calls it; the object may be a generated group or data_ object) and the table gets the object's C name
		for rva, obj in c_objects().items():
			d = data.get(rva)
			if d is not None and not d.ghidra:
				data[rva] = Sym(d.key, obj.cname, rva, obj.kind, obj.size if obj.size is not None else d.size)
			else:
				data[rva] = obj
		self.data = data
		self.starts = sorted(data)
		self.by_key = {}
		for s in list(self.funcs.values()) + list(data.values()):
			if s.key in self.by_key and self.by_key[s.key].rva != s.rva:
				# a name of two places: the data one gets its rva
				if s.kind in ('FUNC', 'CRT'):
					fail('%s names rva %x and %x' % (s.key, s.rva, self.by_key[s.key].rva))
				s.key = '%s@%x' % (s.key, s.rva)
			self.by_key[s.key] = s

	def data_at(self, rva):
		"""(Sym, offset) of the named data that holds rva, or (None, why)"""
		if rva in self.data:
			return self.data[rva], 0
		k = bisect.bisect_right(self.starts, rva) - 1
		if k < 0:
			return None, 'data without a name'
		s = self.data[self.starts[k]]
		if s.size is not None:
			if rva < s.rva + s.size:
				return s, rva - s.rva
			return None, 'data without a name'
		# up to the next name: an array or a struct whose fields the C does not name one by one
		if rva - s.rva < UNSIZED and (k + 1 == len(self.starts) or rva < self.starts[k + 1]):
			return s, rva - s.rva
		return None, 'data in an object of unknown size'


def tokens(src):
	return list(tokenize.generate_tokens(io.StringIO(src).readline))


SKIP = (tokenize.NL, tokenize.COMMENT, tokenize.INDENT, tokenize.DEDENT, tokenize.NEWLINE)


def significant(toks, i, step):
	i += step
	while 0 <= i < len(toks) and toks[i].type in SKIP:
		i += step
	return i


def rewrite(src, names, remain, path):
	"""src with the rvas as names; counts the numbers that stay in remain[kind]"""
	toks = tokens(src)
	edits = []
	for i, t in enumerate(toks):
		if t.type != tokenize.NUMBER or not re.fullmatch(r'0[xX][0-9a-fA-F]+[lL]?', t.string):
			continue
		v = int(t.string.rstrip('lL'), 16)
		p = significant(toks, i, -1)
		n = significant(toks, i, 1)
		ps = toks[p].string if p >= 0 else ''
		ns = toks[n].string if n < len(toks) else ''
		based = False
		if ps in ('+', '-'):
			pp = significant(toks, p, -1)
			pps = toks[pp].string if pp >= 0 else ''
			if ps == '+' and pps.lower() == '0x400000':
				based = True
			else:
				continue	# an offset, or a unary minus
		if ps in NOT_ADDRESS_OPS or ns in NOT_ADDRESS_OPS:
			continue
		# the first argument of SDTest.Call/Peek/Poke/ConvU, and for Peek the kind after it
		first = (not based and ps == '(' and p >= 3 and toks[p - 1].string in SDTEST_RVA and toks[p - 2].string == '.'
			and toks[p - 3].string == 'SDTest' and ns in (',', '+', '-'))
		peek_at = False
		if first and toks[p - 1].string == 'Peek' and ns == ',':
			k = significant(toks, n, 1)
			peek_at = toks[k].type == tokenize.STRING and toks[k].string[1:2] == '@'
		certain = first or based or peek_at
		if peek_at:
			absolute = True
		elif based:
			absolute = False
		elif DATA[0] <= v < DATA[1]:
			# a data rva, or a code address above 0x401000 where no data has a name
			absolute = not first and names.data_at(v)[0] is None and v - BASE in names.funcs
		else:
			absolute = v >= BASE + CODE[0]
		rva = v - BASE if absolute else v
		if v == BASE and not based:
			continue
		if CODE[0] <= rva < CODE[1]:
			s = names.funcs.get(rva)
			off = 0
			why = 'function without a C function' if rva in tsv_functions() else 'code without a function start'
		elif DATA[0] <= rva < DATA[1]:
			s, off = names.data_at(rva)
			why = off
			if s and off and not certain and rva % 4:
				s, why = None, 'data inside an object, not aligned'	# more likely a number
			elif s and s.ghidra and not certain:
				s, why = None, 'data with a name of symbols/globals.tsv only'	# a color 0x80ff00 hit the RTTI
		else:
			continue
		if not s:
			remain[(certain, why)].append((path, t.start[0], t.string))
			continue
		if absolute:
			new = "symva('%s')" % s.key
		elif first and not off and ns == ',' and not peek_at:
			new = "'%s'" % s.key
		else:
			new = "sym('%s')" % s.key
		if off:
			new += ' + 0x%x' % off
		edits.append((t.start, t.end, new))
		if not first:
			LISTED.append((path, t.start[0], t.string, new, t.line.strip()))
	if not edits:
		return src
	lines = src.splitlines(True)
	starts = [0]
	for line in lines:
		starts.append(starts[-1] + len(line))
	out = src
	for (sl, sc), (el, ec), new in reversed(edits):
		a = starts[sl - 1] + sc
		b = starts[el - 1] + ec
		out = out[:a] + new + out[b:]
	return out


_tsv = None
LISTED = []	# the rewrites outside the first argument of SDTest, for -l


def tsv_functions():
	global _tsv
	if _tsv is None:
		_tsv = {}
		for line in read(os.path.join(ROOT, 'symbols', 'functions.tsv')).splitlines()[1:]:
			c = line.split('\t')
			_tsv[int(c[0], 16)] = c
	return _tsv


# the names in a test script: the first argument of SDTest.Call/Peek/Poke/ConvU, sym() and symva()
NAME_USE = re.compile(r"(?:SDTest\.(?:%s)\(\s*|\bsym(?:va)?\(\s*)'([^']+)'" % '|'.join(SDTEST_RVA))


def used_names(src):
	return NAME_USE.findall(src)


def table(names, used):
	syms = sorted((names.by_key[k] for k in used), key=lambda s: s.key.encode())
	out = ['/*',
		' * Generated by tools/test/testsyms.py -w from the test scripts of tools/test/py: the names they use for the',
		' * functions and data of the original, sorted by name for the binary search of SDTest.Sym (src/core/testmod.c).',
		' *   SD_TEST_FUNC(key, name, rva)  the C function name of src/hooks.inc (key name@rva for a further copy it backs)',
		' *   SD_TEST_CRT(key, name, rva)   an entry of the original\'s C runtime, in ScrapC.exe the C function name',
		' *   SD_TEST_OBJ(key, name, rva)   the C object name of SD_AT (ScrapC.exe)',
		' *   SD_TEST_DATA(key, name, rva)  data of the original: a GAME_DATA or Game_Ptr define, else a name of',
		' *                                 symbols/globals.tsv (no C object)',
		' */']
	for s in syms:
		out.append('SD_TEST_%s("%s", %s, 0x%06x)' % (s.kind, s.key, s.cname, s.rva))
	return '\n'.join(out) + '\n'


def read_table(path=TABLE):
	"""{key: (kind, rva)} of src/core/testsyms.inc"""
	out = {}
	if os.path.exists(path):
		for kind, key, rva in re.findall(r'^SD_TEST_(\w+)\("([^"]+)", \w+, (0x[0-9a-f]+)\)', read(path), re.M):
			out[key] = (kind, int(rva, 16))
	return out


def numbers(src, syms):
	"""the numbers a script names (for the scan of tools/standalone/datamap.py): its hex numbers, and the rva of each
	name of syms ({key: (kind, rva)}) in sym(), symva() (the address) and the first argument of SDTest.Call/Peek/Poke/
	ConvU. A name followed by + 0x.. also gives the rva with that offset: the rewrite writes an rva inside an object
	that way, the same form as an offset of the test itself"""
	out = [int(v, 16) for v in re.findall(r'\b0x([0-9a-fA-F]+)\b', src)]
	for m in re.finditer(r"(?:\b(symva|sym)\(|SDTest\.(?:%s)\(\s*)'([^']+)'\)?(?:\s*\+\s*0x([0-9a-fA-F]+)\b)?"
			% '|'.join(SDTEST_RVA), src):
		if m.group(2) in syms:
			rva = syms[m.group(2)][1] + (BASE if m.group(1) == 'symva' else 0)
			out.append(rva)
			if m.group(3):
				out.append(rva + int(m.group(3), 16))
	return out


def main():
	write = '-w' in sys.argv[1:]
	verbose = '-v' in sys.argv[1:]
	listing = '-l' in sys.argv[1:]
	names = Names()
	remain = collections.defaultdict(list)
	changed = []
	used = set()
	uses = 0
	for path in sorted(glob.glob(os.path.join(TESTS, '*.py'))):
		src = open(path).read()
		new = rewrite(src, names, remain, os.path.basename(path))
		for k in used_names(new):
			uses += 1
			if k not in names.by_key:
				fail('%s: no rva has the name %s' % (os.path.basename(path), k))
			used.add(k)
		if new != src:
			changed.append(path)
			if write:
				open(path, 'w').write(new)
	tab = table(names, used)
	old = read(TABLE) if os.path.exists(TABLE) else ''
	if tab != old and write:
		open(TABLE, 'w').write(tab)
	kinds = collections.Counter(names.by_key[k].kind for k in used)
	print('testsyms.py: %d uses of %d names (%s), %d scripts %s' % (uses, len(used),
		', '.join('%d %s' % (n, k.lower()) for k, n in sorted(kinds.items())), len(changed),
		'rewritten' if write else 'to rewrite'))
	for (certain, kind), where in sorted(remain.items(), key=lambda r: (not r[0][0], r[0][1])):
		print('testsyms.py: %5d numbers stay, %s: %s' % (len(where), 'addresses' if certain else 'maybe addresses',
			kind))
		if verbose:
			for path, line, num in where:
				print('  %s:%d: %s' % (path, line, num))
	if listing:
		for path, line, num, new, text in LISTED:
			print('  %s:%d: %s -> %s: %s' % (path, line, num, new, text))
	if not write and (changed or tab != old):
		print('testsyms.py: the scripts or %s are not up to date, run tools/test/testsyms.py -w'
			% os.path.relpath(TABLE, ROOT))
		sys.exit(1)


if __name__ == '__main__':
	main()
