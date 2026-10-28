#!/usr/bin/env python3
# globals.py [-w] [-b build] [Scrap.exe]: the globals and tables of Scrap.exe 1.6.1 as typed C for ScrapC.exe
# (docs/standalone.md, stage 3b5).
#
# The objects come from what the C says about the data (the claims): the defines of GAME_DATA(type, rva) and
# GAME_GLOBAL, the pointer casts of Game_Ptr(rva) (an array of what the cast points to) and *(type *)Game_Ptr(rva) (an
# object), the objects of the dynamic initializers (src/game/Global/StaticInit.inc: the engine variables, native
# functions, vectors, hash tables) and the method tables of SD_PY_MODULE. The rest of the game's data in the map
# (tools/standalone/datamap.txt) becomes objects typed by its kind there, named after the first C function that uses
# it, or data_<rva>. Left out: the import directory, the import address table and the import names (ScrapC.exe has
# its own imports since stage 3c1a), the data of the libraries, the RTTI (with the pointers to it before the vtables), the data of the
# original's Python and what strings.py and vtables.py made C already. The map drops the C++ exception tables.
#
# One owner per byte: claims are sorted by rva, the larger first; a claim inside an earlier object (a field of a
# struct, the value of an engine variable, an element of a table, a second type at the same rva) is a view into
# that object. Objects that would share a word (the assembler pads each section to 4) are one group struct.
#
# Without -w it prints what it would write and checks it; with -w it writes src/game/Global/Globals*.c (one per 64
# KB of rvas), src/game/Global/GlobalsAt.h and rewrites the defines of the claims to GAME_GLOBAL, GAME_ARRAY,
# GAME_GLOBAL_IN, GAME_ARRAY_IN and GAME_PTR (include/scrap/game.h). The build (-b, default build) gives the C
# objects of the strings and the vtables and the symbols of the C functions.
import bisect, collections, decimal, os, re, struct, subprocess, sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from sdexe import BASE, PY_DATA, PY_SYMBOLS, ROOT, Exe, ExeError, c_names, coff_sd
from datamap import MAP, kept, read_map
import ctypeinfo
from strings import c_literal

OUT_DIR = 'src/game/Global'
OUT_RE = re.compile(r'^Globals[0-9a-f]{2}\.c$')
AT_H = 'src/game/Global/GlobalsAt.h'
STATIC_INIT = 'src/game/Global/StaticInit.inc'
NM = os.environ.get('NM', 'i686-w64-mingw32-nm')

# the sections of the game's data
SECTIONS = ('.bss_ste', '.rdata', '.data', '.gfids')
# the kinds of the map that stay bytes of the original
STAYS = ('import', 'library', 'rtti', 'code', 'resource', 'C object')


def fail(msg):
	sys.exit('globals.py: %s' % msg)


def ceil4(n):
	return (n + 3) & ~3


def c_sources():
	"""the C of src/ and include/ but the vendored Python, the loader and what strings.py, vtables.py and this tool
	write"""
	for top in ('src', 'include'):
		for d, _, fs in os.walk(os.path.join(ROOT, top)):
			rel = os.path.relpath(d, ROOT)
			if rel.startswith('src/python') or rel.startswith('src/standalone'):
				continue
			for f in sorted(fs):
				path = os.path.join(d, f)
				r = os.path.relpath(path, ROOT)
				if not f.endswith(('.c', '.h', '.inc')) or re.match(r'^Strings[0-9a-f]{2}\.c$', f) or OUT_RE.match(f) \
						or r in (AT_H, 'src/game/Global/StringsAt.h', 'src/game/Global/Vtables.c'):
					continue
				yield path


# ---- the claims ----

def call_args(t, at):
	"""the arguments of the call whose '(' is at t[at]: (end after ')', [args stripped]) or None"""
	depth = 0
	args = []
	start = at + 1
	i = at
	while i < len(t):
		c = t[i]
		if c in '([{':
			depth += 1
		elif c in ')]}':
			depth -= 1
			if depth == 0:
				args.append(t[start:i].strip())
				return i + 1, args
		elif c == ',' and depth == 1:
			args.append(t[start:i].strip())
			start = i + 1
		elif c == '\n' and (i == 0 or t[i - 1] != '\\'):
			return None
		i += 1
	return None


def back_paren(t, close):
	"""the index of the '(' that matches the ')' at t[close], or None"""
	depth = 0
	i = close
	while i >= 0:
		if t[i] == ')':
			depth += 1
		elif t[i] == '(':
			depth -= 1
			if depth == 0:
				return i
		elif t[i] == '\n':
			return None
		i -= 1
	return None


def deref(cast):
	"""the type a pointer type of a cast points to: 'T *' gives T, 'T (*)[N]' T[N], 'T (__thiscall **)(a)' the
	function pointer T (__thiscall *)(a); None when it is no pointer to data"""
	c = cast.strip()
	if c.endswith('*'):
		return c[:-1].rstrip()
	m = re.search(r'\((\s*(?:__\w+\s*)?)(\*+)\s*\)', c)
	if not m:
		return None
	stars = m.group(2)[1:]
	if stars:
		return (c[:m.start()].rstrip() + ' (%s%s)' % (m.group(1), stars) + c[m.end():]).strip()
	if m.group(1).strip():
		return None	# a function pointer type: code
	return (c[:m.start()].rstrip() + c[m.end():]).strip()


class Claim:
	"""a use of the C that says what lies at an rva: form 'global' (an object of etype), 'array' (an array of etype,
	count elements or unknown) or 'addr' (an address, the type unknown); name is the macro or a generated name; prio
	orders the claims for the owner and its name (lower first); span is the text the rewrite replaces"""

	def __init__(self, rva, etype, form, name, prio, path, span=None, count=None, comment=''):
		self.rva, self.etype, self.form, self.name, self.prio = rva, etype, form, name, prio
		self.path, self.span, self.count, self.comment = path, span, count, comment
		self.rvatok = '0x%06x' % rva	# the rva as the C writes it: a number or the name of a numeric define
		self.inline = False	# a use in the code, not a define: rewritten only when it has an object
		self.ctype = None	# ctypeinfo.CType of etype
		self.size = None
		self.obj = None		# the object that owns it
		self.skip = None	# why it is no object of this tool
		self.esize = None

	def where(self):
		return '%s at rva %06x (%s)' % (self.name or self.etype, self.rva, os.path.relpath(self.path, ROOT))


DEFINE = re.compile(r'^[ \t]*#[ \t]*define[ \t]+(\w+)(\(?)', re.M)
FORMS = re.compile(r'\b(GAME_DATA|GAME_GLOBAL|GAME_GLOBAL_IN|GAME_ARRAY|GAME_ARRAY_IN|GAME_PTR|Game_Ptr)\s*\(')
RVA = re.compile(r'^0x[0-9a-fA-F]+$')


NUMDEF = re.compile(r'^[ \t]*#[ \t]*define[ \t]+(\w+)[ \t]+(0x[0-9a-fA-F]+)\b', re.M)


def scan_text(path, t, data_lo, data_hi, numdefs):
	"""the claims of one source file; an rva is a number or the name of a numeric define (of the file, else one with a
	single value in the sources, numdefs)"""
	out = []
	local = {n: int(v, 16) for n, v in NUMDEF.findall(t)}

	def rva_of(tok):
		if RVA.match(tok):
			return int(tok, 16)
		if tok in local:
			return local[tok]
		v = numdefs.get(tok)
		return list(v)[0] if v is not None and len(v) == 1 else None

	defs = []	# (start, end, name) of the object-like defines
	for m in DEFINE.finditer(t):
		if m.group(2):
			continue
		end = m.end()
		while True:
			nl = t.find('\n', end)
			if nl < 0:
				nl = len(t)
			if t[nl - 1:nl] == '\\':
				end = nl + 1
				continue
			break
		defs.append((m.end(), nl, m.group(1)))
	starts = [d[0] for d in defs]
	header = path.endswith('.h')
	for m in FORMS.finditer(t):
		what = m.group(1)
		r = call_args(t, m.end() - 1)
		if r is None:
			continue
		end, args = r
		i = bisect.bisect_right(starts, m.start()) - 1
		in_def = i >= 0 and m.start() < defs[i][1]
		name = defs[i][2] if in_def else None
		prio = (0 if header else 1) if in_def else 5
		comment = re.sub(r'^[\s\)]*', '', t[end:t.find('\n', end)])
		start = len(out)
		if what == 'Game_Ptr':
			rva = rva_of(args[0]) if len(args) == 1 else None
			if rva is None or not data_lo <= rva < data_hi:
				continue
			# a cast before it, and a * before the cast for an object
			p = m.start() - 1
			while p >= 0 and t[p] in ' \t':
				p -= 1
			cast = None
			if p >= 0 and t[p] == ')':
				o = back_paren(t, p)
				if o is not None:
					cast = t[o + 1:p].strip()
					q = o - 1
					while q >= 0 and t[q] in ' \t':
						q -= 1
					if not re.match(r'^[\w\s\*\(\)\[\],]+$', cast) or not re.search(r'\*', cast):
						cast = None
			e = deref(cast) if cast else None
			if e is None or re.match(r'^(const\s+)?void$', e):
				out.append(Claim(rva, None, 'addr', name, prio, path, (m.start(), end)))
			elif q >= 0 and t[q] == '*' and (q == 0 or t[q - 1] in '( \t'):
				out.append(Claim(rva, e, 'global', name, prio, path, (q, end)))
			else:
				out.append(Claim(rva, e, 'array', name, prio, path, (o, end), comment=comment))
			mark(out[start:], args[0], in_def)
			continue
		n = {'GAME_DATA': 2, 'GAME_GLOBAL': 3, 'GAME_ARRAY': 3, 'GAME_GLOBAL_IN': 4, 'GAME_ARRAY_IN': 4, 'GAME_PTR': 3}
		if len(args) != n[what]:
			continue
		rva = rva_of(args[-1])
		if rva is None:
			continue
		span = (m.start(), end)
		if what == 'GAME_DATA':
			out.append(Claim(rva, args[0], 'global', name, prio, path, span, comment=comment))
		elif what in ('GAME_GLOBAL', 'GAME_ARRAY', 'GAME_GLOBAL_IN', 'GAME_ARRAY_IN'):
			out.append(Claim(rva, args[0], 'global' if what.startswith('GAME_GLOBAL') else 'array', name, prio, path,
				span, comment=comment))
		else:
			out.append(Claim(rva, None, 'addr', name, prio, path, span))
		mark(out[start:], args[-1], in_def)
	return out


def mark(claims, tok, in_def):
	for c in claims:
		c.rvatok = tok if not RVA.match(tok) else '0x%06x' % c.rva
		c.inline = not in_def


def scan_static_init():
	"""the objects the dynamic initializers of src/game/Global/StaticInit.inc construct"""
	out = []
	path = os.path.join(ROOT, STATIC_INIT)
	for line in open(path):
		m = re.match(r'^(\w+)\((.*)\)$', line.strip())
		if not m:
			continue
		kind, args = m.group(1), [a.strip() for a in m.group(2).split(',')]
		fn = args[1]
		if kind.startswith('INIT_GVAR_'):
			obj = args[3] if kind in ('INIT_GVAR_STRING', 'INIT_GVAR_FUNCTION') else args[2]
			out.append(Claim(int(obj, 16), 'cGVarFunction' if kind == 'INIT_GVAR_FUNCTION' else 'cGVar', 'global',
				'GVar_' + fn[len('Init_GVar_'):], 2, path))
			if kind.startswith('INIT_GVAR_FLOAT_') and RVA.match(args[6]):
				out.append(Claim(int(args[6], 16), 'float', 'global', None, 5, path))
		elif kind == 'INIT_NATIVE_FUNC':
			out.append(Claim(int(args[2], 16), 'cPy_NativeFunc', 'global', 'NativeFunc_' + fn[len('Init_NativeFunc_'):],
				2, path))
		elif kind == 'INIT_HASHTABLE':
			out.append(Claim(int(args[3], 16), 'HashTable', 'global', fn[len('Init_'):], 3, path))
		elif kind == 'INIT_VECTOR3':
			out.append(Claim(int(args[2], 16), 'vec3_t', 'global', fn[len('Init_'):], 3, path))
		elif kind in ('INIT_VECTOR', 'INIT_ARRAY'):
			obj = int(args[3 if kind == 'INIT_VECTOR' else 2], 16)
			size, count, ctor = (args[4], args[5], args[6]) if kind == 'INIT_VECTOR' else (args[3], args[4], args[5])
			t = ctor[:-len('_ctor')] if ctor.endswith('_ctor') else None
			c = Claim(obj, t, 'array' if t else 'addr', fn[len('Init_'):], 3, path, count=int(count, 0))
			c.esize = int(size, 0)
			out.append(c)
		elif kind in ('INIT_OBJECT', 'INIT_OBJECT_ARGS', 'INIT_CALL', 'INIT_CALL_ARGS'):
			obj, ctor = (args[3], args[4]) if kind.startswith('INIT_OBJECT') else (args[2], args[3])
			t = re.sub(r'_ctor\w*$', '', ctor) if '_ctor' in ctor else None
			out.append(Claim(int(obj, 16), t, 'global' if t else 'addr', fn[len('Init_'):], 3, path))
	return out


def scan(x):
	lo = x.secs['.bss_ste'][0]
	hi = x.secs['.data'][0] + x.secs['.data'][3]
	claims = []
	texts = {}
	for path in c_sources():
		texts[path] = open(path, encoding='utf-8').read()
	numdefs = collections.defaultdict(set)
	for t in texts.values():
		for n, v in NUMDEF.findall(t):
			numdefs[n].add(int(v, 16))
	for path, t in texts.items():
		claims += scan_text(path, t, lo, hi, numdefs)
		for name, rva in re.findall(r'^SD_PY_MODULE\((\w+), (0x[0-9a-fA-F]+)\)', t, re.M):
			claims.append(Claim(int(rva, 16), 'PyMethodDef', 'array', name + '_methods', 4, path))
		# the uses of a define M(rva) GAME_DATA(type, rva) in its file: objects of type
		for name, param, typ in re.findall(r'^[ \t]*#[ \t]*define[ \t]+(\w+)\((\w+)\)[ \t]+GAME_DATA\(\s*([^,]+?)\s*,'
				r'\s*\2\s*\)\s*$', t, re.M):
			for rva in re.findall(r'\b%s\(\s*(0x[0-9a-fA-F]+)\s*\)' % name, t):
				if lo <= int(rva, 16) < hi:
					claims.append(Claim(int(rva, 16), typ, 'global', None, 5, path))
	claims += scan_static_init()
	return claims, texts


# ---- the types ----

def all_headers():
	d = os.path.join(ROOT, 'include', 'scrap')
	return sorted(f for f in os.listdir(d) if f.endswith('.h'))


def numeric_macros(t):
	"""the object-like defines of a source text whose body is a number expression"""
	out = {}
	for name, body in re.findall(r'^[ \t]*#[ \t]*define[ \t]+(\w+)[ \t]+([^\n/]*?)\s*(?:/\*.*)?$', t, re.M):
		if re.match(r'^[\w\s\(\)\+\-\*/<>]+$', body) and re.search(r'\d', body):
			out[name] = body
	return out


def count_expr(c, texts, things=False):
	"""the count of elements a comment after a Game_Ptr array names ([N], N or N bytes, with things also N <things>),
	as a C expression"""
	m = re.match(r'^\s*/\*\s*\[\s*([^\]]+?)\s*\]', c.comment) or \
		re.match(r'^\s*/\*\s*(0x[0-9a-fA-F]+|\d+|[A-Z][A-Z0-9_]*)\s*(\*/|,|bytes\b%s)' % (r'|[a-z][a-z ]*\*/' if things else ''),
			c.comment)
	if not m:
		return None
	e = m.group(1)
	if re.match(r'^0x0*$|^0+$', e):
		return None
	# the numeric defines of the claim's own file first (the probe only sees the headers)
	local = numeric_macros(texts.get(c.path, ''))
	for _ in range(4):
		e = re.sub(r'\b[A-Za-z_]\w*\b', lambda w: '(%s)' % local[w.group(0)] if w.group(0) in local else w.group(0), e)
	bytes_ = len(m.groups()) > 1 and m.group(2) == 'bytes'
	return e, bytes_


def probe_types(claims, texts):
	"""the CType of each claim's etype and the counts of the comments, from one probe with all headers"""
	types = sorted({c.etype for c in claims if c.etype})
	counts = {}
	for c in claims:
		amp = c.form == 'global' and c.span and texts[c.path][:c.span[0]].rstrip().endswith('&')
		if (c.form == 'array' or amp) and c.count is None and c.comment:
			r = count_expr(c, texts, amp)
			if r:
				counts[c] = r
	exprs = sorted({e for e, b in counts.values()})
	head = ''.join('#include "scrap/%s"\n' % h for h in all_headers())
	decls = ['typeof(%s) sd_t%d;' % (t, i) for i, t in enumerate(types)]
	decls += ['char sd_n%d[(%s)];' % (i, e) for i, e in enumerate(exprs)]
	try:
		got, bad = ctypeinfo.probe(head, decls)
	except ctypeinfo.ProbeError as e:
		fail(str(e))
	out = {}
	for i, t in enumerate(types):
		if i not in bad:
			out[t] = got['sd_t%d' % i]
	nval = {}
	for i, e in enumerate(exprs):
		if i + len(types) not in bad:
			nval[e] = got['sd_n%d' % i].size
	for c in claims:
		if c.etype:
			c.ctype = out.get(c.etype)
		if c in counts and c.form == 'global':
			c.form = 'array'	# &GAME_DATA(T, rva) /* N */: a table of N
		if c in counts:
			e, b = counts[c]
			if e in nval and c.ctype and c.ctype.size:
				c.count = nval[e] // c.ctype.size if b else nval[e]
	return out


# ---- the objects ----

class Obj:
	"""a C object at rva lo..hi: decl is its type text, count the number of elements of an array of it (None for no
	array); kind the kind of the map for a generated one; members the objects of a group struct"""

	def __init__(self, lo, size, sec):
		self.lo, self.size, self.hi = lo, size, lo + size
		self.sec = sec
		self.name = None
		self.decl = None
		self.ctype = None	# CType of one element (or of the object)
		self.count = None
		self.claims = []
		self.kind = None
		self.users = None
		self.members = None
		self.group = None	# the group struct it is a member of

	def __repr__(self):
		return '%s %06x..%06x' % (self.name, self.lo, self.hi)


def simple(t):
	return re.match(r'^[\w\s\*]+$', t) is not None


class Layout:
	def __init__(self, x, rows, existing):
		self.x = x
		self.rows = rows
		self.row_starts = [r[0] for r in rows]
		self.keep = kept(rows)
		self.keep_starts = [k[0] for k in self.keep]
		self.data_end = x.secs['.data'][0] + x.secs['.data'][1]	# the end of the initialized .data
		# the ranges that are no objects of this tool: the imports, the libraries, the RTTI, the original's Python,
		# the C objects of strings.py and vtables.py
		ex = [(x.dirs[1][0], x.dirs[1][0] + x.dirs[1][1]), (x.dirs[12][0], x.dirs[12][0] + x.dirs[12][1])]
		for lo, hi, sec, users, kind in rows:
			if kind.startswith(STAYS):
				ex.append((lo, hi))
		ex += [(lo, hi) for lo, hi in PY_DATA]
		ex += [(o.lo, o.hi) for o in existing]
		# the pointers to the RTTI: before each vtable to its complete object locator (also of the library's vtables),
		# the parts of the hierarchies the map took for data
		rtti = sorted((lo, hi) for lo, hi, sec, users, kind in rows if kind == 'rtti')
		rstarts = [r[0] for r in rtti]
		for at in x.relocs():
			if x.section(at) not in ('.rdata', '.data') or at % 4:
				continue
			t = x.u32(at) - BASE
			i = bisect.bisect_right(rstarts, t) - 1
			if i >= 0 and t < rtti[i][1]:
				ex.append((at, at + 4))
		ex.sort()
		self.excluded = []
		for lo, hi in ex:
			if self.excluded and lo <= self.excluded[-1][1]:
				self.excluded[-1][1] = max(self.excluded[-1][1], hi)
			else:
				self.excluded.append([lo, hi])
		self.ex_starts = [e[0] for e in self.excluded]

	def section(self, rva):
		s = self.x.section(rva)
		return s if s in SECTIONS else None

	def sec_end(self, rva):
		va, n, ptr, vsize = self.x.secs[self.x.section(rva)]
		return va + max(n, vsize)

	def is_bss(self, rva):
		s = self.x.section(rva)
		return s == '.bss_ste' or (s == '.data' and rva >= self.data_end)

	def excluded_at(self, rva):
		i = bisect.bisect_right(self.ex_starts, rva) - 1
		return i >= 0 and rva < self.excluded[i][1]

	def next_excluded(self, rva):
		i = bisect.bisect_right(self.ex_starts, rva)
		return self.excluded[i][0] if i < len(self.excluded) else 1 << 32

	def row(self, rva):
		i = bisect.bisect_right(self.row_starts, rva) - 1
		return i if i >= 0 and rva < self.rows[i][1] else None

	def kept_end(self, rva):
		"""the end of the kept data from rva (for bss its section end)"""
		if self.is_bss(rva):
			return self.sec_end(rva)
		i = bisect.bisect_right(self.keep_starts, rva) - 1
		if i >= 0 and rva < self.keep[i][1]:
			return self.keep[i][1]
		return rva


def users_of(label):
	"""the C functions of the users column of a row"""
	out = set()
	for u in label.split(', '):
		u = u.split(' +')[0].strip()
		if u and u != '-' and not u.startswith(('probe', 'ptr ', 'test ', 'static')):
			out.add(u)
	return out


def method_count(x, rva):
	"""the entries of a PyMethodDef table with its NULL entry"""
	n = 1
	while x.u32(rva + 16 * (n - 1)):
		n += 1
	return n


def sizes(L, claims):
	"""the sizes of the claims; the arrays of unknown length run to the next claim they cannot hold (an element of
	the same size), a C object of the build, a range that stays bytes, the end of the section or of the kept data, or
	a row of the map no C function of the first row uses"""
	x = L.x
	starts = sorted(set(c.rva for c in claims))
	by_rva = collections.defaultdict(list)
	for c in claims:
		by_rva[c.rva].append(c)

	def absorbable(d, lo, esize):
		return d.form == 'global' and d.ctype is not None and d.ctype.kind in ('base', 'pointer', 'enum') \
			and d.ctype.size == esize and (d.rva - lo) % esize == 0

	def enclosing_end(lo, esize, me):
		"""the end of the fixed claim that holds lo (the claim lies in its fields)"""
		end = 1 << 32
		i = bisect.bisect_right(fixed_starts, lo) - 1
		k = 0
		while i >= 0 and k < 256:
			d = fixed[i]
			if d is not me and d.rva + d.size > lo and not (d.rva == lo and absorbable(d, lo, esize)):
				end = min(end, d.rva + d.size)
			i -= 1
			k += 1
		return end

	def limit(lo, esize, rows=True, me=None):
		hard = min(L.sec_end(lo), L.next_excluded(lo), L.kept_end(lo) if not L.is_bss(lo) else 1 << 32,
			enclosing_end(lo, esize, me))
		i = bisect.bisect_right(starts, lo)
		counted = me is not None and me.count is not None
		while i < len(starts) and starts[i] < hard:
			# an address the C takes inside an array of a known length is a view of it (stage 3c1b)
			if not all(absorbable(d, lo, esize) or (counted and d.form == 'addr') for d in by_rva[starts[i]]):
				hard = starts[i]
				break
			i += 1
		if not rows:
			return hard
		r = L.row(lo)
		if r is None:
			return hard
		first = users_of(L.rows[r][3])
		if not first:
			return hard
		k = r + 1
		while k < len(L.rows) and L.rows[k][0] < hard:
			if not users_of(L.rows[k][3]) & first:
				return L.rows[k][0]
			k += 1
		return hard

	# the fixed sizes first: the objects, the arrays of a known length
	arrays = []
	for c in claims:
		if c.skip:
			continue
		if c.etype and (c.ctype is None or (c.esize and c.ctype.size != c.esize)):
			c.form = 'addr'	# no type of that name, or not the size of the elements the initializer constructs
		if c.form == 'global' and c.ctype.kind == 'struct' and c.ctype.name == 'PyMethodDef':
			c.form = 'array'
		if c.form == 'global':
			c.size = c.ctype.size
			if not c.size:
				c.form = 'addr'
		if c.form == 'array':
			if not c.ctype.size:
				c.form = 'addr'
				continue
			if c.ctype.name == 'PyMethodDef':
				c.count = method_count(x, c.rva)
			arrays.append(c)
			if c.count is not None:
				c.size = c.count * c.ctype.size
	fixed = sorted((c for c in claims if not c.skip and c.size), key=lambda c: c.rva)
	fixed_starts = [c.rva for c in fixed]
	for c in arrays:
		esize = c.ctype.size
		hard = limit(c.rva, esize, rows=False, me=c)
		if c.count is None or c.rva + c.count * esize > hard:
			c.count = max(1, (limit(c.rva, esize, me=c) - c.rva) // esize)
		c.size = c.count * esize


def find_existing(build):
	"""the C objects of the build that are not this tool's (strings, vtables): [(CObj, symbol)]"""
	path = os.path.join(build, 'standalone', 'objects.txt')
	try:
		paths = [l.strip() for l in open(path) if l.strip()]
	except OSError as e:
		fail('cannot read %s (%s): build ScrapC.exe first' % (path, e.strerror))
	objs = []
	syms = {}
	for p in paths:
		if re.search(r'Globals[0-9a-f]{2}\.c\.obj$', p):
			continue
		try:
			objs += coff_sd(p, syms)
		except ExeError as e:
			fail(str(e))
	names = {}
	for s, rva in syms.items():
		names.setdefault(rva, s[1:] if s.startswith('_') else s)
	for o in objs:
		o.symbol = names.get(o.lo)
	return sorted(objs, key=lambda o: o.lo), paths


def owners(L, claims):
	"""the objects of the claims: sorted by rva, the larger and the better named first; a claim inside an object is
	a view of it"""
	cl = sorted((c for c in claims if not c.skip and c.size), key=lambda c: (c.rva, -c.size, c.prio, c.form != 'global'))
	objs = []
	cur = None
	for c in cl:
		if cur and c.rva < cur.hi:
			if c.rva + c.size > cur.hi:
				fail('%s (0x%x bytes) runs out of %s at rva %06x..%06x' % (c.where(), c.size, cur.claims[0].where(),
					cur.lo, cur.hi))
			cur.claims.append(c)
			c.obj = cur
			continue
		cur = Obj(c.rva, c.size, L.section(c.rva))
		cur.claims = [c]
		cur.ctype = c.ctype
		cur.count = c.count if c.form == 'array' and c.count != 1 else None
		cur.etype = c.etype
		c.obj = cur
		objs.append(cur)
	# the addresses without a type: a view where an object holds them
	starts = [o.lo for o in objs]
	for c in claims:
		if c.skip or c.size:
			continue
		i = bisect.bisect_right(starts, c.rva) - 1
		if i >= 0 and c.rva < objs[i].hi:
			objs[i].claims.append(c)
			c.obj = objs[i]
	return objs


def free_byte(L, rva):
	return L.section(rva) is not None and not L.excluded_at(rva)


def groups(L, objs):
	"""the top objects: an object of the claims on its own when it starts at a multiple of 4 and the rest of its last
	word is free and zero (the assembler pads with zeros), else a group struct of the objects that share words, with
	the bytes between them as filler members"""
	x = L.x
	tops = []
	i = 0
	while i < len(objs):
		k = i + 1
		hi = ceil4(objs[i].hi)
		while k < len(objs) and objs[k].lo < hi and objs[k].sec == objs[i].sec:
			hi = max(hi, ceil4(objs[k].hi))
			k += 1
		run = objs[i:k]
		lo = run[0].lo & ~3
		if any(not free_byte(L, a) for a in range(lo, run[0].lo)) or \
				any(not free_byte(L, a) for a in range(run[-1].hi, hi) if not any(o.lo <= a < o.hi for o in run)):
			for o in run:
				for c in o.claims:
					c.skip = 'shares a word with data that stays'
			i = k
			continue
		if len(run) == 1 and run[0].lo == lo and not x.bytes(run[0].hi, hi).strip(b'\0'):
			tops.append(run[0])
		else:
			g = Obj(lo, hi - lo, run[0].sec)
			g.members = []
			at = lo
			for o in run:
				if o.lo > at:
					f = Obj(at, o.lo - at, o.sec)
					f.kind = 'filler'
					f.group = g
					g.members.append(f)
				g.members.append(o)
				o.group = g
				at = o.hi
			if at < hi:
				f = Obj(at, hi - at, run[0].sec)
				f.kind = 'filler'
				f.group = g
				g.members.append(f)
			tops.append(g)
		i = k
	return tops


def is_text(b):
	"""a C string padded with zeros: text of at least 4 characters, half of them letters, its NUL, zeros"""
	t = b.rstrip(b'\0')
	letters = sum(1 for c in t if chr(c).isalpha())
	return len(t) >= 4 and len(t) < len(b) and all(0x20 <= c < 0x7f or c in (9, 10, 13) for c in t) and \
		letters * 2 >= len(t)


def fill(L, tops, claims):
	"""the objects of the data in the map that no claim covers, word by word: cut at the rows of the map that start
	at a multiple of 4 and at the addresses of the C, typed by the kind of their rows. A word that shares bytes with
	data that stays stays too (returned as left)"""
	cuts = {c.rva for c in claims if not c.skip and c.obj is None}
	foot = sorted((o.lo, o.hi) for o in tops)
	fstarts = [f[0] for f in foot]

	def covered(a):
		i = bisect.bisect_right(fstarts, a) - 1
		return i >= 0 and a < foot[i][1]

	# the runs of free used bytes
	runs = []
	for lo, hi, sec, users, kind in L.rows:
		if sec not in SECTIONS or kind.startswith(STAYS) or (kind == 'bss' and users == '-'):
			continue
		if kind != 'bss' and not any(a <= lo < b for a, b in [L.keep[max(0, bisect.bisect_right(L.keep_starts, lo) - 1)]]):
			continue
		a = lo
		while a < hi:
			if covered(a) or L.excluded_at(a):
				a += 1
				continue
			b = a
			while b < hi and not covered(b) and not L.excluded_at(b):
				b += 1
			if runs and runs[-1][1] == a:
				runs[-1][1] = b
			else:
				runs.append([a, b])
			a = b
	out = []
	left = 0
	for a, b in runs:
		a2, b2 = ceil4(a), b & ~3
		left += (a2 - a) + (b - b2) if b2 > a2 else b - a
		if b2 <= a2:
			continue
		r = L.row(a2)
		points = {a2, b2} | {c for c in cuts if a2 < c < b2 and c % 4 == 0}
		k = r
		while k is not None and k < len(L.rows) and L.rows[k][0] < b2:
			if L.rows[k][0] % 4 == 0 and a2 < L.rows[k][0]:
				points.add(L.rows[k][0])
			k += 1
		points = sorted(points)
		for lo, hi in zip(points, points[1:]):
			o = Obj(lo, hi - lo, L.section(lo))
			ks = []
			k = L.row(lo)
			while k is not None and k < len(L.rows) and L.rows[k][0] < hi:
				ks.append(L.rows[k])
				k += 1
			o.kinds = set(r[4] for r in ks)
			o.kind = ks[0][4] if len(o.kinds) == 1 else 'mixed'
			o.users = ks[0][3]
			out.append(o)
	return out, left


# ---- names ----

def first_user(label):
	"""the first C function of the users column of the map"""
	for u in label.split(', '):
		u = u.split(' +')[0].strip()
		if u and u != '-' and not u.startswith(('probe', 'ptr ', 'test ', 'static')):
			m = re.match(r'^([A-Za-z_]\w*?)(@\d+)?(\.\w+)*$', u)
			if m:
				return m.group(1)
	return None


def flat(tops):
	out = []
	for t in tops:
		out += t.members if t.members else [t]
	return out


def name_all(L, tops, taken):
	"""the names: the macro of the best claim at the start of the object with its type, else the first C function of
	the map's users with the rva, else data_<rva>; a name already taken gets the rva"""
	used = set()

	def give(o, name, rva=False):
		if rva or name in used or name in taken:
			name = '%s_%06x' % (name, o.lo)
		used.add(name)
		o.name = name

	for t in tops:
		if t.members:
			give(t, 'group', True)
	for o in flat(tops):
		if o.kind == 'filler':
			o.name = 'pad_%x' % (o.lo - o.group.lo)
			continue
		cands = sorted((c for c in o.claims if c.rva == o.lo and c.name and c.etype == o.etype and c.size == o.size),
			key=lambda c: (c.prio, c.name))
		if not cands:
			# the name of an address the C takes there (a GUID, a data format)
			cands = sorted((c for c in o.claims if c.rva == o.lo and c.name and c.form == 'addr'),
				key=lambda c: (c.prio, c.name))
		if cands:
			give(o, cands[0].name)
			continue
		r = L.row(o.lo)
		user = first_user(L.rows[r][3]) if r is not None else None
		give(o, user or 'data', True)


# ---- the C types ----

BASE_TYPES = ('float', 'double', 'uint32_t', 'char', 'void *', 'D3DGuid', 'byte')


def type_map(x, o, T):
	"""the type of an object of the map, by the kind of its rows and its bytes"""
	n = o.size
	rel = [a for a in range(o.lo, o.hi) if a in x.relocs()]
	if any(a % 4 for a in rel):
		fail('rva %06x..%06x: a pointer at rva %06x that is not at a multiple of 4' % (o.lo, o.hi, rel[0]))
	b = x.bytes(o.lo, o.hi)
	kinds = getattr(o, 'kinds', {o.kind})
	if len(kinds) > 1:
		kinds = kinds - {'zero'}
	if o.kind == 'bss' or 'bss' in kinds:
		e = 'uint32_t'
	elif kinds == {'float'} and not rel:
		e = 'float'
	elif kinds == {'double'} and not rel and n % 8 == 0:
		e = 'double'
	elif kinds == {'guid'} and not rel and n % 16 == 0:
		e = 'D3DGuid'
	elif not rel and is_text(b):
		e = 'char'
	elif rel and all(a in rel or not x.u32(a) for a in range(o.lo, o.hi, 4)):
		e = 'void *'
	else:
		e = 'uint32_t'
	o.etype = e
	o.ctype = T[e]
	o.count = n // o.ctype.size if n > o.ctype.size else None


def decl(o):
	"""the declaration of o without its attributes"""
	if o.members:
		return decl_group(o)
	t = o.etype
	arr = '[%d]' % o.count if o.count is not None else ''
	const = o.sec == '.rdata'
	m = re.match(r'^([\w\s\*]+?)\s*(\[\w+\])$', t)
	if m and arr:
		t, arr = m.group(1), arr + m.group(2)	# an array of arrays: T name[n][m]
	if simple(t):
		if const and t.endswith('*'):
			base = t + 'const'
		elif const and not re.match(r'^const\b', t):
			base = 'const ' + t
		else:
			base = t
		sep = '' if base.endswith('*') else ' '
		return '%s%s%s%s' % (base, sep, o.name, arr)
	return '%stypeof(%s) %s%s' % ('const ' if const else '', t, o.name, arr)


def decl_group(g):
	packed = any((m.lo - g.lo) % min(4, max(1, member_align(m))) for m in g.members)
	lines = ['struct%s {' % (' __attribute__((packed))' if packed else '')]
	for m in g.members:
		if m.kind == 'filler':
			lines.append('\tbyte %s[%d];' % (m.name, m.size))
		else:
			d = decl(m)
			if g.sec == '.rdata':
				d = re.sub(r'^const ', '', d)
			lines.append('\t%s;\t/* 0x%x */' % (d, m.lo - g.lo))
	lines.append('} %s' % g.name)
	return '\n'.join(lines)


def member_align(o):
	if o.kind == 'filler':
		return 1
	ct = o.ctype
	while ct.kind == 'array':
		ct = ct.target
	if ct.kind in ('struct', 'union'):
		return 4
	return ct.size


# ---- the initializers ----

class InitError(Exception):
	pass


def float_lit(bits):
	v = struct.unpack('<f', struct.pack('<I', bits))[0]
	if v != v:
		quiet = bits & 0x400000
		pay = bits & 0x3fffff
		s = '-' if bits >> 31 else ''
		return '%s__builtin_nan%sf("0x%x")' % (s, '' if quiet else 's', pay)
	if v in (float('inf'), float('-inf')):
		return '%s__builtin_inff()' % ('-' if v < 0 else '')
	for p in range(1, 10):
		t = '%.*g' % (p, v)
		try:
			if struct.unpack('<I', struct.pack('<f', float(t)))[0] == bits:
				break
		except OverflowError:
			pass
	return plain(t) + 'f'


def plain(t):
	"""a decimal literal without the exponent where it is short, with a point"""
	d = decimal.Decimal(t)
	if d and -5 <= d.adjusted() < 10:
		t = format(d, 'f')
	if 'e' not in t and '.' not in t:
		t += '.0'
	return t


def double_lit(bits):
	v = struct.unpack('<d', struct.pack('<Q', bits))[0]
	if v != v:
		return '%s__builtin_nan%s("0x%x")' % ('-' if bits >> 63 else '', '' if bits & (1 << 51) else 's',
			bits & ((1 << 51) - 1))
	if v in (float('inf'), float('-inf')):
		return '%s__builtin_inf()' % ('-' if v < 0 else '')
	return plain(repr(v))


def int_lit(v, size, signed):
	if signed:
		if v >= 1 << (8 * size - 1):
			v -= 1 << (8 * size)
		if size == 4 and v == -0x80000000:
			return '(-2147483647 - 1)'
		if size == 8:
			return '%dLL' % v
		return '%d' % v
	if size == 8:
		return '0x%xULL' % v
	return '%d' % v if v < 0x10000 else '0x%x' % v


class Resolver:
	"""the C expression of a pointer of the original: a C object at its target (or in it), a C function behind the
	hook of a code target, a symbol of the real Python, else the fixed address of the room (data that stays bytes,
	code without C)"""

	def __init__(self, tops, existing, labels):
		# an object with the zeros up to its next word (a pointer of the original may point there)
		self.index = sorted([(t.lo, ceil4(t.hi), t.name) for t in tops] +
			[(o.lo, o.hi, o.symbol) for o in existing if o.symbol])
		self.starts = [i[0] for i in self.index]
		self.hooks = c_names()
		self.labels = labels
		self.aliases = {}
		self.orig = collections.Counter()

	def alias(self, name, label):
		self.aliases[name] = label
		return 'sd_' + name

	def ptr(self, t, x):
		i = bisect.bisect_right(self.starts, t) - 1
		if i >= 0 and t < self.index[i][1]:
			lo, hi, name = self.index[i]
			a = self.alias(name, '_' + name)
			return '(void *)%s' % a if t == lo else '(void *)(%s + %d)' % (a, t - lo)
		if t in self.hooks:
			name = self.hooks[t]
			return '(void *)%s' % self.alias(name, self.labels.get(name, '_' + name))
		if t in PY_SYMBOLS:
			return '(void *)%s' % self.alias(PY_SYMBOLS[t], '_' + PY_SYMBOLS[t])
		# stage 3c1b: no pointer leads into bytes of the original any more (SD_ORIG is gone)
		self.orig[x.section(t) or '?'] += 1
		raise InitError('a pointer to rva %06x (%s), which is no C object, C function or Python symbol'
			% (t, x.section(t) or '?'))


class Init:
	"""the initializer of an object from the bytes of the original, along its CType"""

	def __init__(self, x, res):
		self.x, self.res = x, res

	def of(self, o):
		self.lo, self.hi = o.lo, o.hi
		self.b = self.x.bytes(o.lo, o.hi)
		self.cov = bytearray(o.size)
		self.relocs = {a for a in range(o.lo, o.hi) if a in self.x.relocs()}
		self.used = set()
		if o.members:
			v = []
			for m in o.members:
				if m.kind == 'filler':
					w = self.bytes_array(m.lo, m.size)
				else:
					w = self.whole(m)
				if w is not None:
					v.append(('.%s' % m.name, w))
			v = ('s', v) if v else None
		else:
			v = self.whole(o)
		left = [self.lo + k for k in range(len(self.b)) if self.b[k] and not self.cov[k]]
		if left:
			raise InitError('the bytes at rva %06x are in no member of the type (padding)' % left[0])
		if self.relocs - self.used:
			raise InitError('the pointer at rva %06x is in no pointer of the type' % min(self.relocs - self.used))
		return v

	def whole(self, o):
		if o.count is not None:
			arr = ctypeinfo.CType('array', o.ctype.size * o.count)
			arr.target, arr.count = o.ctype, o.count
			return self.value(arr, o.lo)
		return self.value(o.ctype, o.lo)

	def mark(self, rva, n):
		for k in range(rva - self.lo, rva - self.lo + n):
			self.cov[k] = 1

	def raw(self, rva, n):
		return self.b[rva - self.lo:rva - self.lo + n]

	def bytes_array(self, rva, n):
		self.mark(rva, n)
		b = self.raw(rva, n)
		if any(a in self.relocs for a in range(rva, rva + n)):
			raise InitError('a pointer at rva %06x in filler bytes' % rva)
		if not b.strip(b'\0'):
			return None
		t = b.rstrip(b'\0')
		return ('a', ['0x%02x' % c for c in t])

	def value(self, ct, rva):
		k = ct.kind
		if k in ('base', 'enum'):
			n = ct.size
			if rva in self.relocs:
				if n != 4 or ct.enc == 'float':
					raise InitError('a pointer at rva %06x in a %s' % (rva, ct.name or k))
				self.mark(rva, 4)
				self.used.add(rva)
				return '(uintptr_t)%s' % self.res.ptr(self.x.u32(rva) - BASE, self.x)
			if any(a in self.relocs for a in range(rva + 1, rva + n)):
				raise InitError('a pointer at rva %06x across a %s' % (rva, ct.name or k))
			self.mark(rva, n)
			b = self.raw(rva, n)
			if not b.strip(b'\0'):
				return None
			v = int.from_bytes(b, 'little')
			if ct.enc == 'float':
				return float_lit(v) if n == 4 else double_lit(v)
			return int_lit(v, n, ct.enc in ('signed', 'char') or k == 'enum')
		if k == 'pointer':
			self.mark(rva, 4)
			if rva in self.relocs:
				self.used.add(rva)
				return self.res.ptr(self.x.u32(rva) - BASE, self.x)
			if any(a in self.relocs for a in range(rva + 1, rva + 4)):
				raise InitError('a pointer at rva %06x across a pointer' % rva)
			v = int.from_bytes(self.raw(rva, 4), 'little')
			return '(void *)0x%x' % v if v else None
		if k == 'array':
			e = ct.target
			n = ct.count or 0
			if e.is_char() and n:
				b = self.raw(rva, n)
				t = b.rstrip(b'\0')
				if not any(a in self.relocs for a in range(rva, rva + n)) and t and len(t) < n and \
						sum(1 for c in t if 0x20 <= c < 0x7f) * 4 >= len(t) * 3:
					self.mark(rva, n)
					return '"%s"' % ''.join(c_literal(t))
			vals = [self.value(e, rva + i * e.size) for i in range(n)]
			while vals and vals[-1] is None:
				vals.pop()
			if not vals:
				return None
			if e.kind in ('struct', 'union', 'array') and sum(1 for v in vals if v is None) * 2 > len(vals):
				return ('a', [('[%d]' % i, v) for i, v in enumerate(vals) if v is not None])
			zero = '0' if e.kind in ('base', 'enum', 'pointer') else '{ 0 }'
			return ('a', [zero if v is None else v for v in vals])
		if k == 'struct':
			items = []
			self.members(ct, rva, items, '')
			return ('s', items) if items else None
		if k == 'union':
			return self.union(ct, rva)
		raise InitError('no initializer for a %s at rva %06x' % (k, rva))

	def members(self, ct, rva, items, prefix):
		i = 0
		ms = ct.members
		while i < len(ms):
			name, off, mt, bits, bitoff = ms[i]
			if name is None and mt.kind in ('struct', 'union'):
				if mt.kind == 'struct':
					self.members(mt, rva + off, items, prefix)
				else:
					v = self.union(mt, rva + off, flat=True)
					if v is not None:
						items += v[1]
				i += 1
				continue
			if bits is not None:
				# the bit fields of one storage unit
				j = i
				vals = []
				while j < len(ms) and ms[j][3] is not None:
					n2, o2, t2, b2, bo2 = ms[j]
					start = (o2 * 8 + bo2)
					nb = (start + b2 + 7) // 8 - o2
					raw = int.from_bytes(self.raw(rva + o2, nb), 'little')
					v = raw >> bo2 & ((1 << b2) - 1)
					self.mark_bits(rva + o2, bo2, b2)
					if v and n2:
						vals.append(('.%s' % n2, int_lit(v, 8, False) if v > 0xffff else '%d' % v))
					j += 1
				items += vals
				i = j
				continue
			v = self.value(mt, rva + off)
			if v is not None:
				items.append(('.%s' % name, v))
			i += 1

	def mark_bits(self, rva, bitoff, bits):
		"""a byte counts as covered when all its bits are in fields"""
		if not hasattr(self, 'bitcov'):
			self.bitcov = collections.defaultdict(int)
		for b in range(bitoff, bitoff + bits):
			a = rva + b // 8
			self.bitcov[a] |= 1 << (b % 8)
			if self.bitcov[a] == 0xff:
				self.mark(a, 1)

	def union(self, ct, rva, flat=False):
		b = self.raw(rva, ct.size)
		if not b.strip(b'\0') and not any(a in self.relocs for a in range(rva, rva + ct.size)):
			self.mark(rva, ct.size)
			return None
		saved = (bytearray(self.cov), set(self.used), dict(getattr(self, 'bitcov', {})), dict(self.res.aliases),
			collections.Counter(self.res.orig))
		for name, off, mt, bits, bitoff in ct.members:
			try:
				v = self.value(mt, rva + off)
			except InitError:
				v = None
				ok = False
			else:
				ok = all(self.cov[a - self.lo] or not self.b[a - self.lo] for a in range(rva, rva + ct.size)) and \
					all(a in self.used for a in self.relocs if rva <= a < rva + ct.size)
			if ok and name:
				items = [('.%s' % name, v)] if v is not None else []
				self.mark(rva, ct.size)
				return ('s', items) if flat else (('s', items) if items else None)
			self.cov, self.used = bytearray(saved[0]), set(saved[1])
			self.bitcov = collections.defaultdict(int, saved[2])
			self.res.aliases, self.res.orig = dict(saved[3]), collections.Counter(saved[4])
		raise InitError('no member of the union at rva %06x holds its bytes' % rva)


def fmt(v, indent, width=120):
	"""the C text of an initializer value"""
	if isinstance(v, str):
		return v
	kind, items = v

	def item(it, ind):
		if isinstance(it, tuple) and len(it) == 2 and isinstance(it[0], str) and it[0][:1] in '.[':
			return '%s = %s' % (it[0], fmt(it[1], ind))
		return fmt(it, ind)

	one = '{ %s }' % ', '.join(item(it, 0) for it in items)
	if '\n' not in one and len(one) + 8 * indent <= width:
		return one
	tabs = '\t' * (indent + 1)
	parts = [item(it, indent + 1) for it in items]
	lines = []
	cur = ''
	for p in parts:
		if '\n' in p or '{' in p or '=' in p:
			if cur:
				lines.append(cur)
				cur = ''
			lines.append(p + ',')
			continue
		if cur and len(cur) + len(p) + 2 + 8 * (indent + 1) > width:
			lines.append(cur)
			cur = ''
		cur = (cur + ' ' if cur else '') + p + ','
	if cur:
		lines.append(cur)
	return '{\n' + ''.join(tabs + l + '\n' for l in lines) + '\t' * indent + '}'


# ---- the output ----

def header_index():
	"""the header of include/scrap that defines each type and macro name"""
	out = {}
	for h in all_headers():
		t = open(os.path.join(ROOT, 'include', 'scrap', h), encoding='utf-8').read()
		for n in re.findall(r'^\s*}\s*(\w+)\s*;', t, re.M) + re.findall(r'\btypedef\s[^;{]*?\b(\w+)\s*;', t) + \
				re.findall(r'\btypedef\s[^;{]*?\(\s*\*\s*(\w+)\s*\)', t) + \
				re.findall(r'^\s*(?:typedef\s+)?(?:struct|union|enum)\s+(\w+)\s*\{', t, re.M) + \
				re.findall(r'^\s*#\s*define\s+(\w+)', t, re.M):
			out.setdefault(n, h)
	return out


KEYWORDS = {'const', 'volatile', 'struct', 'union', 'enum', 'unsigned', 'signed', 'int', 'char', 'short', 'long',
	'float', 'double', 'void', 'typeof', '__thiscall', '__stdcall', '__fastcall', '__cdecl'}


def includes(objs, hidx):
	need = {'shared.h'}
	for o in objs:
		for m in (o.members or [o]):
			if m.kind == 'filler' or not m.etype:
				continue
			for w in re.findall(r'[A-Za-z_]\w*', m.etype):
				if w in KEYWORDS:
					continue
				if w in hidx:
					need.add(hidx[w])
				elif w.startswith('Py'):
					need.add('python.h')
	if any(o.members for o in objs):
		need.add('shared.h')
	return sorted(need)


def write_files(x, L, tops, res, hidx, macro_names):
	init = Init(x, res)
	files = collections.defaultdict(list)
	for t in tops:
		files[t.lo >> 16].append(t)
	errors = []
	out = {}
	for k, group in sorted(files.items()):
		res.aliases = {}
		defs = []
		for t in group:
			bss = L.is_bss(t.lo)
			try:
				v = None if bss else init.of(t)
			except InitError as e:
				errors.append('%s at rva %06x: %s' % (t.name, t.lo, e))
				continue
			d = decl(t)
			at = 'SD_BSS(0x%06x)' % t.lo if bss else 'SD_AT(0x%06x)' % t.lo
			if v is None:
				defs.append('%s %s;' % (d, at))
			else:
				text = fmt(v, 0)
				defs.append('%s %s = %s;' % (d, at, text))
		undef = sorted({m.name for t in group for m in (t.members or []) + [t]} & macro_names)
		lines = [
			'/*',
			' * The globals and tables of Scrap.exe 1.6.1 at rva 0x%06x..0x%06x as C objects of ScrapC.exe at the same'
				% (k << 16, (k + 1) << 16),
			' * rvas (docs/standalone.md, stage 3b5), generated by tools/standalone/globals.py, do not edit. The initial',
			' * values are the original\'s; its pointers are the C objects, functions and Python symbols at their targets',
			' * (sd_<name>). The hook DLL uses the exe\'s own data.',
			' * The C reaches them through GAME_GLOBAL, GAME_ARRAY and their _IN views (scrap/game.h).',
			' */',
			'#include "scrap/shared.h"',
			'',
			'#ifdef SD_STANDALONE',
			'',
		]
		lines += ['#include "scrap/%s"' % h for h in includes(group, hidx) if h != 'shared.h']
		lines.append('')
		if res.aliases:
			lines += ['SD_SYM(%s);' % n if lab == '_' + n else 'SD_SYM_AS(%s, "%s");' % (n, lab)
				for n, lab in sorted(res.aliases.items())]
			lines.append('')
		if undef:
			lines += ['#undef %s' % n for n in undef]
			lines.append('')
		lines += defs
		lines += ['', '#endif', '']
		out['Globals%02x.c' % k] = '\n'.join(lines)
	return out, errors


def at_header(tops, x):
	"""GlobalsAt.h: SD_DATA_<rva> for each rva of the data in StaticInit.inc, the address in its C object
	(GAME_PTR_AT of scrap/game.h); an rva no object of this tool holds (a string) is the rva of the room"""
	starts = [t.lo for t in tops]
	lo, hi = x.secs['.bss_ste'][0], x.secs['.data'][0] + x.secs['.data'][3]
	lines = [
		'/*',
		' * The C object of each rva of the data in src/game/Global/StaticInit.inc in ScrapC.exe (docs/standalone.md,',
		' * stage 3b5), generated by tools/standalone/globals.py, do not edit. GAME_PTR_AT(rva) of scrap/game.h reads it.',
		' */',
	]
	seen = set()
	n = 0
	for v in re.findall(r'\b0x[0-9a-f]{6}\b', open(os.path.join(ROOT, STATIC_INIT)).read()):
		rva = int(v, 16)
		if v in seen or not lo <= rva < hi:
			continue
		seen.add(v)
		i = bisect.bisect_right(starts, rva) - 1
		if i >= 0 and rva < ceil4(tops[i].hi):
			t = tops[i]
			lines.append('#define SD_DATA_%s GAME_PTR(%s, %d, %s)' % (v, t.name, rva - t.lo, v))
			n += 1
	return lines, n


# ---- the rewrite of the defines ----

def rewrite(claims, texts):
	"""the new text of the define of each claim: GAME_GLOBAL or GAME_ARRAY for the object itself, the _IN forms
	for a view into an object, GAME_PTR for an address; a claim without an object goes back to GAME_DATA"""
	edits = collections.defaultdict(list)
	for c in claims:
		if not c.span or (c.inline and (c.skip or c.obj is None)):
			continue
		if c.skip or c.obj is None:
			if c.srcform == 'global':
				new = 'GAME_DATA(%s, %s)' % (c.etype, c.rvatok)
			elif c.srcform == 'array':
				new = '(%s *)Game_Ptr(%s)' % (c.etype if simple(c.etype) else 'typeof(%s)' % c.etype, c.rvatok)
			else:
				new = 'Game_Ptr(%s)' % c.rvatok
		else:
			o = c.obj
			base = o.group or o
			off = c.rva - base.lo
			whole = o.group is None and c.rva == o.lo and c.etype == o.etype and c.srcform == c.form and \
				(c.form != 'array' or c.size == o.size) and o.claims[0].form == c.form
			r = c.rvatok
			if c.srcform == 'addr':
				new = 'GAME_PTR(%s, %d, %s)' % (base.name, off, r)
			elif whole and c.srcform == 'global':
				new = 'GAME_GLOBAL(%s, %s, %s)' % (c.etype, o.name, r)
			elif whole:
				new = 'GAME_ARRAY(%s, %s, %s)' % (c.etype, o.name, r)
			elif c.srcform == 'global':
				new = 'GAME_GLOBAL_IN(%s, %s, %d, %s)' % (c.etype, base.name, off, r)
			else:
				new = 'GAME_ARRAY_IN(%s, %s, %d, %s)' % (c.etype, base.name, off, r)
		edits[c.path].append((c.span[0], c.span[1], new))
	out = {}
	for path, es in edits.items():
		t = texts[path]
		for a, b, new in sorted(es, reverse=True):
			t = t[:a] + new + t[b:]
		if t != texts[path]:
			out[path] = t
	return out


def symbols(build, paths):
	"""the global symbols of the C of the build but this tool's objects: {C name: label}, the taken names"""
	libs = [os.path.join(build, 'libpython152.a')]
	sa = []
	for d, _, fs in os.walk(os.path.join(build, 'CMakeFiles', 'ScrapC.dir')):
		sa += [os.path.join(d, f) for f in fs if f.endswith('.obj')]
	labels = {}
	taken = set()
	r = subprocess.run([NM, '--defined-only', '-g'] + [p for p in paths if not re.search(r'Globals[0-9a-f]{2}\.c\.obj$', p)]
		+ libs + sa, capture_output=True, text=True)
	for line in r.stdout.splitlines():
		p = line.split()
		if len(p) == 3:
			m = re.match(r'^[_@](\w+?)(@\d+)?$', p[2])
			if m:
				labels.setdefault(m.group(1), p[2])
				taken.add(m.group(1))
	return labels, taken


def main():
	args = sys.argv[1:]
	write = '-w' in args
	build = os.path.join(ROOT, 'build')
	rest = []
	while args:
		a = args.pop(0)
		if a == '-w':
			continue
		if a == '-b' and args:
			build = args.pop(0)
		else:
			rest.append(a)
	try:
		x = Exe(rest[0] if rest else os.path.join(ROOT, 'game/Bin/Scrap.exe'))
	except ExeError as e:
		fail(str(e))
	rows = read_map(MAP)
	existing, paths = find_existing(build)
	L = Layout(x, rows, existing)
	claims, texts = scan(x)
	for c in claims:
		c.srcform = c.form
	T = probe_types(claims + [Claim(0, t, 'none', None, 9, '') for t in BASE_TYPES], texts)
	for c in claims:
		if L.section(c.rva) is None:
			c.skip = 'not in the data'
		elif L.excluded_at(c.rva):
			c.skip = 'stays'
	sizes(L, claims)
	objs = owners(L, claims)
	tops = groups(L, objs)
	gen, left = fill(L, tops, claims)
	for o in gen:
		type_map(x, o, T)
	tops = sorted(tops + gen, key=lambda o: o.lo)
	# the addresses in the objects of the map
	starts = [t.lo for t in tops]
	for c in claims:
		if not c.skip and c.obj is None:
			i = bisect.bisect_right(starts, c.rva) - 1
			if i >= 0 and c.rva < tops[i].hi:
				tops[i].claims.append(c)
				c.obj = tops[i]
	labels, taken = symbols(build, paths)
	name_all(L, tops, taken)
	# SD_PY_MODULE names its method table <module>_methods
	for c in claims:
		if c.prio == 4 and (c.obj is None or c.obj.name != c.name or c.obj.group):
			fail('%s: the method table is no object named %s' % (c.where(), c.name))
	res = Resolver(tops, existing, labels)
	hidx = header_index()
	macro_names = {c.name for c in claims if c.name and c.span}
	files, errors = write_files(x, L, tops, res, hidx, macro_names)
	for e in errors:
		print('globals.py: %s' % e)
	valid = [c for c in claims if not c.skip]
	nb = collections.Counter()
	for t in tops:
		nb[t.sec, 'bss' if L.is_bss(t.lo) else 'data'] += t.size
	print('globals.py: %d claims, %d of them views or objects of this tool; %d objects (%d of the claims, %d of the '
		'map, %d groups); %d bytes in words shared with data that stays' % (len(claims), len(valid), len(tops),
		len(objs), len(gen), sum(1 for t in tops if t.members), left))
	for (sec, k), n in sorted(nb.items()):
		print('globals.py:   %-8s %-4s %7d bytes' % (sec, k, n))
	print('globals.py: pointers to data that stays or code without C: %s' % dict(res.orig))
	skipped = collections.Counter(c.skip for c in claims if c.skip)
	print('globals.py: claims left out: %s' % dict(skipped))
	if errors:
		fail('%d objects have no initializer' % len(errors))
	if not write:
		return
	d = os.path.join(ROOT, OUT_DIR)
	for f in os.listdir(d):
		if OUT_RE.match(f) and f not in files:
			os.remove(os.path.join(d, f))
	for f, t in files.items():
		open(os.path.join(d, f), 'w', encoding='utf-8').write(t)
	at, n = at_header(tops, x)
	open(os.path.join(ROOT, AT_H), 'w').write('\n'.join(at) + '\n')
	print('globals.py: wrote %d files %s and %s (%d rvas)' % (len(files), os.path.join(OUT_DIR, 'Globals*.c'), AT_H,
		n))
	new = rewrite(claims, texts)
	for path, t in new.items():
		open(path, 'w', encoding='utf-8').write(t)
	print('globals.py: rewrote the defines in %d files' % len(new))


if __name__ == '__main__':
	main()
