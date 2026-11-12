#!/usr/bin/env python3
# The call sites between script ports (stage 3 of docs/objlib-plan.md) on the preprocessed sources of src/game/Script
# (the macros of the files resolved, see ports.py): the progress measure and the end check of the stage, and the
# rewrite of the sites that have a form through a ref.
#
#   tools/script/portcalls.py [files]               per file: the by-name calls left by kind, the sites through refs
#                                                   (PORT), the OBJLIB 3 marks (without files also those of the
#                                                   sources and headers that have no call site); the arity check
#                                                   after it
#   tools/script/portcalls.py sites Mod.Func ...    every by-name and PORT site of these functions
#   tools/script/portcalls.py rewrite [--dry] <Mod.Func | all> [files]
#                                                   the sites of Mod.Func (all: of every function with a ref) in the
#                                                   files (default: all files) that have a form through the ref become
#                                                   that form in the source; the sites it leaves are listed with the
#                                                   reason (--dry: only the list of what it would do)
#   tools/script/portcalls.py wrap [files]          the functions the PORT sites of the files call, as the argument
#                                                   of _pton (tools/test/regress/helpers/fol3.py)
#   --build <dir>                                   the build folder for the preprocessor (default build-linux64)
#
# Kinds: port (a function of another module with a ref), own (of a module of the file, through its attribute),
# global (through the module's global), noentry (a C table function without an entry yet), engine (a function of an
# engine module with a native entry and its ref: convertible), engslow (an engine function without a native entry
# yet: stays by name), lib,
# other (data functions of the .def files, variables, unknown names). Arity: the count of the positional letters at a
# site against the count the entry takes (the number in include/scrap/scriptports.h).
#
# What rewrite converts (the module a literal "Mod" or a constant of the file, #define X "Mod"; the function a literal):
#
#   LV_CALL, Native_CallMod (and the file's aliases of them)   PORT_CALL, PORT_LCALL (level module)
#   LV_OWN, Native_CallGlobal (and aliases)                    PORT_OWN, PORT_LOWN
#   Script_Try(m, f, "(fmt)", ...)                             PORT_CALL(M, F, NULL, fmt', ...)
#   Script_Ask(m, f, "(fmt)", ...)                             PORT_ASK(M, F, fmt', ...)
#   Script_Call(m, f, "(fmt)", ...)                            MODE_PRINTED(M, F, fmt', ...)
#   Sf_Ask, Sf_Run, Sf_Own(fr, m, f, "(fmt)", ...)             SF_CALL, SF_RUN, SF_OWN (SF_L... of a level module)
#   Mode_PrintedCall(m, f, id)                                 MODE_PRINTED(M, F, "i", (long)id)
#   Mode_Printed(Native_CallMod(m, f, NULL, fmt, ...), m, f)   MODE_PRINTED(M, F, fmt, ...)
#   Mode_CallKw(m, f, out, args, n, Py_BuildValue("{...}", ...))
#                                                              PORT_CALL(M, F, out, "v..;key=x,...", &args[0], ...,
#                                                              values): args NULL, &x or an array name, n a number,
#                                                              the dict of pairs "s" + i, s, d or O with literal keys
#   LvN_OnEnd(m, f, params), LvN_ScorerCallG(m, f)             LV_ONEND(M, F, params), LV_SCORERG(M, F) (a level
#                                                              module), LvN_OnEndRef / LvN_ScorerRef(&PORT_REF(M, F))
#   LvN_Com(key, who, m, f, params), LvN_Callback(name, m, f), LvN_MissionChar(name, m, f)
#                                                              LV_COM, LV_CALLBACK, LV_MISSIONCHAR (the same way)
#
# The functions of the engine modules (Scrap, SAI, ...) are converted the same way when they have a native entry (a ref
# of include/scrap/scriptports.h): PORT_CALL(Scrap, SetCam, ...), whose seam sees a function a case put there instead
# and calls it by name. The others are listed (no entry yet).
#
# fmt' is the Py_BuildValue format turned into the letters of Native_Call: i, b, h, l as i (the argument cast to long,
# the va_arg of Port_Invoke), s and z as s (a NULL text is None for both), d and f as d, O and S as O; other letters
# and nested tuples or lists stay by name (listed). A level module needs "#define PORT_LEVEL <level>" in the file (the
# level files have it; a file with several levels defines it again before each part); the level of a site is the last
# define before it. Names only known at run time, conditional names and other forms stay by name and are listed: they
# are converted by hand (or stay until stage 4, see docs/objlib-plan.md 1.6). Run the rewrite again after it changed a
# site that held another convertible call in its arguments (it converts those in the next run).
import argparse, collections, os, re, sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import ports

ENGINE = {'Scrap', 'SScorer', 'SAI', 'SVec', 'SAct', 'SSound', 'SInput', 'SLogic', 'SNet', 'SWeap', 'SFX'}
LIB = {'string', '__builtin__', 'sys', 'math', 'whrandom', 'random', 'marshal', 'new', 'types', 'os', 'strop'}
GLOBAL = {'Native_CallGlobal', 'Sf_Own'}
# an argument of a call (one level of nested brackets: NULL is ((void *)0) after the preprocessor)
ARG = r'(?:[^,()]|\((?:[^()]|\([^()]*\))*\))+'

BYNAME = re.compile(r'\b(Native_CallMod|Native_CallGlobal|Script_Try|Script_Call|Script_Ask|Script_CallKw|Script_AskKw|'
	r'Mode_CallKw|Mode_CallIf|Mode_CallVarIf|Mode_PrintedCall|LvN_OnEnd|LvN_ScorerCallG)\(\s*"(\w+)",\s*"(\w+)"(?:\s*,' + ARG +
	r',\s*"([a-zA-Z]*)")?')
FRAME = re.compile(r'\b(Sf_Ask|Sf_Run|Sf_Own)\(\s*&?\w+,\s*"(\w+)",\s*"(\w+)"')
SITEREF = r'\(&\(static portSite_t\)\{ \.ref = &\((port_\w+)\), \.global = \((\d)\)[^}]*\}\)'
SITE = re.compile(r'\b(?:Port_Invoke\(\s*' + SITEREF + r'\s*,' + ARG + r'|Port_Ask\(\s*' + SITEREF +
	r'|Sf_Port(?:Run)?\(\s*' + ARG + r',\s*' + SITEREF + r')\s*,\s*"([a-zA-Z;=,]*)"')
VREF = re.compile(r'\b(LvN_OnEndRef|LvN_ScorerRef|LvN_ComRef|LvN_CallbackRef|LvN_MissionCharRef)\((?:' + ARG + r',\s*)*&\((port_\w+)\)')


def refsyms():
	"""port symbol -> (level, module, name, nargs) of the header, and the native twins of string and math (declared in
	scriptmodes.h, no seam)"""
	out = {}
	for (level, module, name), n in ports.read_header().items():
		out['port_%s%s_%s' % (level + '_' if level else '', module, name)] = (level, module, name, n)
	for module, name in re.findall(r'^PORT_DECL\((\w+), (\w+)\)', open(os.path.join(ports.ROOT, 'include/scrap/scriptmodes.h')).read(), re.M):
		out['port_%s_%s' % (module, name)] = (None, module, name, None)
	return out


def owners(pp):
	"""file -> the modules its tables serve; (module, name) -> 1 for every C table function"""
	tabs = ports.tables(pp)
	own, cfunc = collections.defaultdict(set), set()
	for level, module, g in ports.registrations():
		if g in tabs:
			own[tabs[g][0]].add(module)
			cfunc.update((module, row[0]) for row in tabs[g][1])
	return own, cfunc


def scan(pp):
	"""[(file, line, form, module, name, fmt or None, global)] of the by-name sites and the PORT sites"""
	syms, out = refsyms(), []
	for f, lines in pp.items():
		for no, text in lines:
			for m in BYNAME.finditer(text):
				out.append((f, no, m.group(1), m.group(2), m.group(3), m.group(4), m.group(1) in GLOBAL))
			for m in FRAME.finditer(text):
				out.append((f, no, m.group(1), m.group(2), m.group(3), None, m.group(1) in GLOBAL))
			for m in SITE.finditer(text):
				sym, glob = next((m.group(i), m.group(i + 1)) for i in (1, 3, 5) if m.group(i))
				level, module, name, _ = syms.get(sym, (None, '?' + sym, '?', None))
				out.append((f, no, 'PORT', module, name, m.group(7), glob == '1'))
			for m in VREF.finditer(text):
				level, module, name, _ = syms.get(m.group(2), (None, '?' + m.group(2), '?', None))
				out.append((f, no, 'PORT', module, name, None, True))
	return out


def index(refs):
	"""(module, name) -> [Ref]"""
	out = collections.defaultdict(list)
	for rs in refs.values():
		for r in rs:
			out[(r.module, r.name)].append(r)
	return out


_levels = {}


def level_marks(f):
	"""[(offset, line, level or None)] of the PORT_LEVEL defines and undefs of the file"""
	if f not in _levels:
		text = open(os.path.join(ports.SCRIPT, f), errors='replace').read()
		_levels[f] = [(m.start(), text.count('\n', 0, m.start()) + 1, m.group(2) if m.group(1) == 'define' else None)
			for m in re.finditer(r'^#(define|undef) PORT_LEVEL\b ?(\w*)', text, re.M)]
	return _levels[f]


def level_at(f, line=None, offset=None):
	"""the PORT_LEVEL of the file at a line (or an offset of its text)"""
	lv = None
	for off, no, level in level_marks(f):
		if (offset is not None and off < offset) or (line is not None and no < line):
			lv = level
	return lv


def ref_for(idx, module, name, level):
	"""the ref of module.name: of a module of the engine's tables, or of the level's module"""
	cands = idx.get((module, name), ())
	return next((r for r in cands if r.level is None), None) or next((r for r in cands if r.level == level), None)


def nargs_of(f, module, name, idx, line=None):
	"""the ref of module.name as file f calls it at a line (a level module: the ref of the level there; without a
	PORT_LEVEL the file's own), None without one"""
	r = ref_for(idx, module, name, level_at(f, line) if line is not None else None)
	if r is None and not level_marks(f):
		r = next((r for r in idx.get((module, name), ()) if r.file == f), None)
	return r


def engine_ref(idx, module, name):
	"""the ref of an engine function with a native entry (its calls can take the fast path), else None"""
	r = ref_for(idx, module, name, None)
	return r


def kind(f, no, form, module, name, glob, idx, own, cfunc):
	if form == 'PORT':
		return 'PORT'
	if module in ENGINE:
		return 'engine' if engine_ref(idx, module, name) else 'engslow'
	if module in LIB:
		return 'lib'
	if nargs_of(f, module, name, idx, no):
		return 'global' if glob else 'own' if module in own[f] else 'port'
	return 'noentry' if (module, name) in cfunc else 'other'


def marks(f):
	"""the OBJLIB 3 marks of a source of src/game/Script or a header (include/scrap/x.h)"""
	path = os.path.join(ports.ROOT, f) if f.startswith('include/') else os.path.join(ports.SCRIPT, f)
	return open(path, errors='replace').read().count('OBJLIB 3')


def marked():
	"""the sources and headers with OBJLIB 3 marks, also those without call sites"""
	inc = os.path.join(ports.ROOT, 'include/scrap')
	hs = ['include/scrap/' + h for h in sorted(os.listdir(inc)) if h.endswith('.h')]
	srcs = []
	for d, _, fs in os.walk(ports.SCRIPT):
		srcs += [os.path.relpath(os.path.join(d, x), ports.SCRIPT) for x in fs if x.endswith('.h')]
	return {f for f in ports.sources() + sorted(srcs) + hs if marks(f)}


def survey(pp, idx, files):
	own, cfunc = owners(pp)
	kinds = ['port', 'own', 'global', 'noentry', 'engine', 'engslow', 'lib', 'other', 'PORT']
	rows, total, bad = {}, collections.Counter(), []
	for f, no, form, module, name, fmt, glob in scan(pp):
		if files and f not in files:
			continue
		k = kind(f, no, form, module, name, glob, idx, own, cfunc)
		rows.setdefault(f, collections.Counter())[k] += 1
		r = nargs_of(f, module, name, idx, no)
		pos = fmt.split(';')[0] if fmt is not None else None
		if pos is not None and r and r.nargs is not None and len(pos) != r.nargs:
			bad.append('%s:%d %s %s.%s: %d arguments, the entry takes %d' % (f, no, form, module, name, len(pos), r.nargs))
	print('%-28s %s %8s' % ('file', ' '.join('%7s' % k for k in kinds), 'OBJLIB3'))
	for f in sorted(set(rows) | ({f for f in files} if files else marked())):
		c = rows.get(f, collections.Counter())
		m = marks(f)
		total.update(c)
		total['OBJLIB3'] += m
		print('%-28s %s %8d' % (f, ' '.join('%7d' % c[k] for k in kinds), m))
	print('%-28s %s %8d' % ('total', ' '.join('%7d' % total[k] for k in kinds), total['OBJLIB3']))
	print('arity: %d sites with another count than the entry' % len(bad))
	for b in bad:
		print('  ' + b)


def sites(pp, funcs):
	for f, no, form, module, name, fmt, glob in sorted(scan(pp), key=lambda s: s[:2]):
		if '%s.%s' % (module, name) in funcs:
			print('%s:%d %s%s %s.%s %s' % (f, no, form, ' global' if glob else '', module, name,
				'"%s"' % fmt if fmt is not None else ''))


# ---- the rewrite ----

def call_args(text, i):
	"""text[i] is the '(' of a call: ([(start, end) of each argument, stripped], the index after the ')'), or None"""
	depth, j, start, out = 0, i, i + 1, []
	while j < len(text):
		c = text[j]
		if c in '"\'':
			k = j + 1
			while k < len(text) and text[k] != c:
				k += 2 if text[k] == '\\' else 1
			j = k
		elif text.startswith('/*', j):
			j = text.index('*/', j) + 1
		elif c in '([{':
			depth += 1
		elif c in ')]}':
			depth -= 1
			if depth == 0:
				out.append((start, j))
				break
		elif c == ',' and depth == 1:
			out.append((start, j))
			start = j + 1
		j += 1
	else:
		return None
	spans = []
	for s, e in out:
		while s < e and text[s].isspace():
			s += 1
		while e > s and text[e - 1].isspace():
			e -= 1
		spans.append((s, e))
	if len(spans) == 1 and spans[0][0] == spans[0][1]:
		spans = []
	return spans, j + 1


def literal(tok):
	m = re.fullmatch(r'"(\w*)"', tok)
	return m.group(1) if m else None


def long_arg(e):
	"""an argument of Py_BuildValue's "i" as the long that Port_Invoke reads"""
	if re.fullmatch(r'-?\d+', e):
		return e + 'L'
	if re.fullmatch(r'[\w.]+(->\w+)*(\[\w+\])?', e):
		return '(long)' + e
	return '(long)(%s)' % e


def bv_format(fmt, args):
	"""a Py_BuildValue tuple format and its arguments as the letters and arguments of Native_Call, or (None, why)"""
	if not (fmt.startswith('(') and fmt.endswith(')')) or re.search(r'[()\[\]{}]', fmt[1:-1]):
		return None, 'Py_BuildValue format "%s" is no tuple' % fmt
	letters, out = '', []
	body = fmt[1:-1]
	if len(body) != len(args):
		return None, 'format "%s" with %d arguments' % (fmt, len(args))
	for c, a in zip(body, args):
		if c in 'ibhl':
			letters += 'i'
			out.append(long_arg(a))
		elif c in 'sz':
			letters += 's'
			out.append(a)
		elif c in 'df':
			letters += 'd'
			out.append(a)
		elif c in 'OS':
			letters += 'O'
			out.append(a)
		else:
			return None, 'format letter %s of "%s"' % (c, fmt)
	return (letters, out), None


def kw_format(text, span):
	"""Py_BuildValue("{...}", key, value, ...) as ([(key, letter, value)], None) or (None, why)"""
	expr = text[span[0]:span[1]]
	m = re.match(r'Py_BuildValue\s*\(', expr)
	if not m:
		return None, 'keywords not a Py_BuildValue dict'
	a = call_args(expr, m.end() - 1)
	if not a or a[1] != len(expr):
		return None, 'keywords not a Py_BuildValue dict'
	toks = [expr[s:e] for s, e in a[0]]
	f = literal(toks[0].replace('{', '').replace('}', '').replace(':', '').replace(',', '')) if toks else None
	if f is None or not toks[0].startswith('"{') or len(f) % 2 or len(f) != len(toks) - 1:
		return None, 'keyword dict format %s' % (toks[0] if toks else '?')
	out = []
	for k in range(0, len(f), 2):
		key, c, v = literal(toks[1 + k]), f[k + 1], toks[2 + k]
		if f[k] != 's' or not key or c not in 'isdOz':
			return None, 'keyword dict format %s' % toks[0]
		out.append((key, {'z': 's'}.get(c, c), long_arg(v) if c == 'i' else v))
	return out, None


class File:
	"""one source file being rewritten"""

	def __init__(self, f, idx, funcs):
		self.f, self.idx, self.funcs = f, idx, funcs
		self.path = os.path.join(ports.SCRIPT, f)
		self.text = open(self.path).read()
		self.consts = dict(re.findall(r'^#define (\w+) "(\w+)"$', self.text, re.M))
		self.edits, self.left = [], []

	def line(self, pos):
		return self.text.count('\n', 0, pos) + 1

	def module(self, tok):
		return literal(tok) or self.consts.get(tok)

	def ref(self, module, name, pos):
		if self.funcs is not None and '%s.%s' % (module, name) not in self.funcs:
			return None
		return ref_for(self.idx, module, name, level_at(self.f, offset=pos))

	def want(self, module, name):
		return self.funcs is None or '%s.%s' % (module, name) in self.funcs

	def target(self, mtok, ntok, pos, form):
		"""the ref of the site, or None (the reason listed)"""
		module, name = self.module(mtok), literal(ntok)
		if module is None or name is None:
			if module is not None and self.funcs is None:
				self.leave(pos, form, '%s.%s: the function is no literal' % (module, ntok))
			elif module is None and self.funcs is None and literal(ntok):
				self.leave(pos, form, '%s.%s: the module is no literal' % (mtok, ntok))
			return None
		if not self.want(module, name):
			return None
		if module in LIB:
			return None
		if module in ENGINE:
			r = self.ref(module, name, pos)
			if r:
				return r
			self.leave(pos, form, '%s.%s: no native entry yet' % (module, name))
			return None
		r = self.ref(module, name, pos)
		if r is None:
			cands = self.idx.get((module, name), ())
			if cands and not level_at(self.f, offset=pos):
				self.leave(pos, form, ('%s.%s: a level module, add "#define PORT_LEVEL %s"' % (module, name, cands[0].level))
					if self.f.startswith('levels/') else '%s.%s: the module of the current level (stage 4)' % (module, name))
			return None
		return r

	def leave(self, pos, form, why):
		self.left.append((self.line(pos), form, why))

	def macro(self, r, base):
		"""PORT_CALL -> PORT_LCALL for a level module, SF_RUN -> SF_LRUN"""
		if not r.level:
			return base
		return base.replace('PORT_', 'PORT_L').replace('SF_', 'SF_L')

	def edit(self, s, e, new):
		self.edits.append((s, e, new))

	def toks(self, spans):
		return [self.text[a:b] for a, b in spans]

	def calls(self, form):
		for m in re.finditer(r'\b%s\s*\(' % form, self.text):
			a = call_args(self.text, m.end() - 1)
			if a:
				yield m.start(), a[1], self.toks(a[0]), a[0]

	def plain(self):
		for form, (glob, fixed) in aliases(self.text).items():
			for s, e, t, spans in self.calls(form):
				if fixed:
					if len(t) < 3:
						continue
					mtok, ntok, rest = '"%s"' % fixed, t[0], t[1:]
				else:
					if len(t) < 4:
						continue
					mtok, ntok, rest = t[0], t[1], t[2:]
				r = self.target(mtok, ntok, s, form)
				if r:
					base = 'PORT_OWN' if glob else 'PORT_CALL'
					self.edit(s, e, '%s(%s, %s, %s)' % (self.macro(r, base), r.module, r.name, ', '.join(rest)))

	def buildvalue(self, form, base, out):
		"""Script_Try, Script_Ask, Script_Call"""
		for s, e, t, spans in self.calls(form):
			if len(t) < 3:
				continue
			r = self.target(t[0], t[1], s, form)
			if not r:
				continue
			fmt = re.fullmatch(r'"([^"]*)"', t[2])
			conv, why = bv_format(fmt.group(1), t[3:]) if fmt else (None, 'format is no literal')
			if not conv:
				self.leave(s, form, '%s.%s: %s' % (r.module, r.name, why))
				continue
			args = ([out] if out else []) + ['"%s"' % conv[0]] + conv[1]
			self.edit(s, e, '%s(%s, %s, %s)' % (self.macro(r, base), r.module, r.name, ', '.join(args)))

	def frame(self):
		for form, base in (('Sf_Ask', 'SF_CALL'), ('Sf_Run', 'SF_RUN'), ('Sf_Own', 'SF_OWN')):
			for s, e, t, spans in self.calls(form):
				if len(t) < 4:
					continue
				r = self.target(t[1], t[2], s, form)
				if not r:
					continue
				fmt = re.fullmatch(r'"([^"]*)"', t[3])
				conv, why = bv_format(fmt.group(1), t[4:]) if fmt else (None, 'format is no literal')
				if not conv:
					self.leave(s, form, '%s.%s: %s' % (r.module, r.name, why))
					continue
				self.edit(s, e, '%s(%s, %s, %s, %s)' % (self.macro(r, base), t[0], r.module, r.name,
					', '.join(['"%s"' % conv[0]] + conv[1])))

	def printed(self):
		for s, e, t, spans in self.calls('Mode_PrintedCall'):
			if len(t) == 3:
				r = self.target(t[0], t[1], s, 'Mode_PrintedCall')
				if r:
					self.edit(s, e, 'MODE_PRINTED(%s, %s, "i", %s)' % (r.module, r.name, long_arg(t[2])))
		for s, e, t, spans in self.calls('Mode_Printed'):
			if len(t) != 3:
				continue
			m = re.match(r'(Native_CallMod|LV_CALL)\s*\(', t[0])
			if not m:
				continue
			inner = call_args(t[0], m.end() - 1)
			if not inner or inner[1] != len(t[0]):
				continue
			it = [t[0][a:b] for a, b in inner[0]]
			if len(it) < 4 or it[2] != 'NULL' or self.module(it[0]) != self.module(t[1]) or literal(it[1]) != literal(t[2]):
				continue
			r = self.target(it[0], it[1], s, 'Mode_Printed')
			if r and not r.level:
				self.edit(s, e, 'MODE_PRINTED(%s, %s, %s)' % (r.module, r.name, ', '.join(it[3:])))

	def callkw(self):
		for s, e, t, spans in self.calls('Mode_CallKw'):
			if len(t) != 6:
				continue
			r = self.target(t[0], t[1], s, 'Mode_CallKw')
			if not r:
				continue
			arr, n = t[3], t[4]
			if arr == 'NULL' and n == '0':
				pos = []
			elif re.fullmatch(r'&\w+', arr) and n == '1':
				pos = [arr]
			elif re.fullmatch(r'\w+', arr) and re.fullmatch(r'\d+', n):
				pos = ['&%s[%d]' % (arr, i) for i in range(int(n))]
			else:
				self.leave(s, 'Mode_CallKw', '%s.%s: arguments %s, %s' % (r.module, r.name, arr, n))
				continue
			kws, why = kw_format(self.text, spans[5])
			if kws is None:
				self.leave(s, 'Mode_CallKw', '%s.%s: %s' % (r.module, r.name, why))
				continue
			fmt = 'v' * len(pos) + ';' + ','.join('%s=%s' % (k, c) for k, c, _ in kws)
			args = [t[2], '"%s"' % fmt] + pos + [v for _, _, v in kws]
			self.edit(s, e, '%s(%s, %s, %s)' % (self.macro(r, 'PORT_CALL'), r.module, r.name, ', '.join(args)))

	def values(self):
		# form, its macro, its ref function, the index of the module among the arguments
		for form, macro, ref, k in (('LvN_OnEnd', 'LV_ONEND', 'LvN_OnEndRef', 0), ('LvN_ScorerCallG', 'LV_SCORERG', 'LvN_ScorerRef', 0),
			('LvN_Com', 'LV_COM', 'LvN_ComRef', 2), ('LvN_Callback', 'LV_CALLBACK', 'LvN_CallbackRef', 1),
			('LvN_MissionChar', 'LV_MISSIONCHAR', 'LvN_MissionCharRef', 1)):
			for s, e, t, spans in self.calls(form):
				if len(t) < k + 2 or t[k + 1] == 'NULL':
					continue
				r = self.target(t[k], t[k + 1], s, form)
				if not r:
					continue
				pre, rest = ''.join(x + ', ' for x in t[:k]), ''.join(', ' + x for x in t[k + 2:])
				if r.level:
					self.edit(s, e, '%s(%s%s, %s%s)' % (macro, pre, r.module, r.name, rest))
				else:
					self.edit(s, e, '%s(%s&PORT_REF(%s, %s)%s)' % (ref, pre, r.module, r.name, rest))

	def run(self):
		self.printed()
		self.callkw()
		self.values()
		self.buildvalue('Script_Try', 'PORT_CALL', 'NULL')
		self.buildvalue('Script_Ask', 'PORT_ASK', None)
		self.buildvalue('Script_Call', 'MODE_PRINTED', None)
		self.frame()
		self.plain()
		# the outer edit of nested ones (the inner call is in its arguments: the next run takes it)
		done, text = [], self.text
		for s, e, new in sorted(self.edits, key=lambda x: (x[0], -x[1])):
			if done and s < done[-1][1]:
				continue
			done.append((s, e, new))
		for s, e, new in reversed(done):
			text = text[:s] + new + text[e:]
		return done, text


def aliases(text):
	"""the forms of a file that call by name with a literal module: name -> (global, fixed module or None)"""
	forms = {'LV_CALL': (0, None), 'Native_CallMod': (0, None), 'LV_OWN': (1, None), 'Native_CallGlobal': (1, None)}
	consts = dict(re.findall(r'^#define (\w+) "(\w+)"$', text, re.M))
	for m in re.finditer(r'^#define (\w+)\(\s*(\w+),\s*(\w+),\s*(\w+),\s*\.\.\.\)\s*Native_Call(Mod|Global)\(\s*\2,\s*\3,', text, re.M):
		forms[m.group(1)] = (m.group(5) == 'Global', None)
	for m in re.finditer(r'^#define (\w+)\(\s*(\w+),\s*(\w+),\s*\.\.\.\)\s*Native_Call(Mod|Global)\(\s*("\w+"|\w+),\s*\2,', text, re.M):
		mod = m.group(5).strip('"') if m.group(5).startswith('"') else consts.get(m.group(5))
		if mod:
			forms[m.group(1)] = (m.group(4) == 'Global', mod)
	return forms


def rewrite(idx, func, files, dry):
	funcs = None if func == 'all' else set(func.split(','))
	total = 0
	for f in files or ports.sources():
		x = File(f, idx, funcs)
		done, text = x.run()
		for s, e, new in done:
			print('%s:%d %s -> %s' % (f, x.line(s), x.text[s:e].split('(')[0].strip(), new.split('(')[0]))
		for no, form, why in sorted(set(x.left)):
			print('%s:%d %s left: %s' % (f, no, form, why))
		if done and not dry:
			open(x.path, 'w').write(text)
		total += len(done)
	print('rewrite %s: %d sites%s' % (func, total, ' (dry run)' if dry else ''))


def wrap(pp, files):
	"""the seamed functions of the PORT sites (not the twins of string and math: nothing replaces them)"""
	funcs = sorted({'%s.%s' % (m, n) for f, _, form, m, n, _, _ in scan(pp) if form == 'PORT' and (not files or f in files)
		and m not in LIB})
	print(repr(tuple(funcs)))


def main():
	ap = argparse.ArgumentParser(description='the call sites between script ports')
	ap.add_argument('--build', default=os.path.join(ports.ROOT, 'build-linux64'))
	ap.add_argument('--dry', action='store_true')
	ap.add_argument('args', nargs='*')
	a = ap.parse_intermixed_args()
	cmd, rest = (a.args[0], a.args[1:]) if a.args and a.args[0] in ('sites', 'rewrite', 'wrap') else ('survey', a.args)
	build = os.path.abspath(a.build)
	if cmd == 'rewrite':
		rewrite(index(ports.refs(build)[0]), rest[0], rest[1:], a.dry)
		return 0
	pp = ports.preprocess(ports.sources(), build)
	if cmd == 'sites':
		sites(pp, set(rest))
	elif cmd == 'wrap':
		wrap(pp, set(rest))
	else:
		survey(pp, index(ports.refs(build, pp)[0]), set(rest))
	return 0


if __name__ == '__main__':
	sys.exit(main())
