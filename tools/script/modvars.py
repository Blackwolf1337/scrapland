#!/usr/bin/env python3
# The registry of the module variables (stage 5 of docs/objlib-plan.md, include/scrap/modvars.h): writes
# src/game/Script/ModVarTable.c from the .mod files of the pack modules (tools/modules/modules.tsv) and the C state the
# ports keep for them (found in the preprocessed sources of src/game/Script, as ports.py preprocesses them).
#
#   tools/script/modvars.py               write the table (run it after a .mod file or the C state of a port changes,
#                                         and to resolve a merge of it)
#   tools/script/modvars.py --check       exit 1 when the table is not what the tool writes
#   tools/script/modvars.py --list        the names per module with kind, types and place (for a look)
#   --build <dir>                         the build folder for the preprocessor (default build-linux64)
#
# One table per module file (a module of a level has one per level: Map, MapSnd, Mission_X), its names sorted as
# strcmp orders them, then the table of the modules sorted by module and level (the file of every level first). A
# name is what the module code puts into the module dict at load time by a line of its own:
#   let x = <expr>      a variable (MVK_VAR); its types are those of the values the let lines give it (a literal:
#                       its type, None NV_NONE, any other expression every type)
#   def f ...           a function (MVK_FUNC)
#   import A ...        a module (MVK_MODULE)
#   from M import a     a name of another module (MVK_IMPORT); "from M import *" names nothing here
#   class C             a class (MVK_CLASS, only in the dict until stage 7); its body goes to the class
# plus __builtins__, __doc__, __file__ and __name__ of every module (MVK_SYS), the functions of the C tables of the
# module that no def line names (MVK_FUNC: the loader puts every table function in after the module code,
# Script_ModuleExtras) and the names the C state of a port keeps that no line gives (MVK_VAR of every type: a C hook
# or a load time call makes them). Flags: MVF_COND (only a
# line in a branch of an if gives it), MVF_DEL (a del line of the module code removes it again). The place is the C
# state of the port that keeps the variable (the file and the field): a slot the Mode_Read/Mode_Put/Mode_DictPut
# family reads and writes (&<static struct>.<field>), a row of a nativeVar_t table of the file, or a computed name
# (COMPUTED below). Names a mod or the game adds at run time are not here: they go to the overflow list of the
# module (src/game/Script/ModVars.c).
import argparse, collections, os, re, sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import ports

ROOT = ports.ROOT
SCRIPT = ports.SCRIPT
TSV = os.path.join(ROOT, 'tools/modules/modules.tsv')
TABLEFILE = os.path.join(SCRIPT, 'ModVarTable.c')

# kinds in the order of the header (the C names)
KINDS = ['MVK_VAR', 'MVK_FUNC', 'MVK_MODULE', 'MVK_IMPORT', 'MVK_CLASS', 'MVK_SYS']
TYPES = ['NONE', 'INT', 'FLOAT', 'STR', 'VEC', 'LONG', 'LIST', 'TUPLE', 'DICT', 'OBJ']
ANY = (1 << len(TYPES)) - 1
SYS = ['__builtins__', '__doc__', '__file__', '__name__']
# names the C builds at run time, not as a literal: (module, prefix, file, array of the suffixes, place with %d)
COMPUTED = [('Fx', 'FXMOTOR_', 'Fx.c', 'fx_motors', 'fx.motor[%d]')]

Name = collections.namedtuple('Name', 'kind types flags place')


# ---- the .mod files ----

def statements(text):
	"""the statements of a .mod text: (indent, text) with the lines inside open brackets joined, comments dropped"""
	out, cur, depth, quote = [], None, 0, None
	for raw in text.split('\n'):
		if cur is None:
			if not raw.strip() or raw.lstrip().startswith('#'):
				continue
			cur = raw
		else:
			cur += '\n' + raw
		line = raw
		i = 0
		while i < len(line):
			c = line[i]
			if quote:
				if c == '\\':
					i += 1
				elif c == quote:
					quote = None
			elif c in '\'"':
				quote = c
			elif c in '([{':
				depth += 1
			elif c in ')]}':
				depth -= 1
			elif c == '#':
				break
			i += 1
		if depth <= 0 and not quote:
			indent = len(cur) - len(cur.lstrip('\t'))
			out.append((indent, cur.strip()))
			cur, depth = None, 0
	if cur is not None:
		sys.exit('modvars: an open bracket at the end of a .mod text: %r' % cur[:80])
	return out


def strip_parens(e):
	"""an expression without brackets around the whole of it"""
	while e.startswith('(') and e.endswith(')'):
		depth, quote, i = 0, None, 0
		for i, c in enumerate(e):
			if quote:
				if c == '\\':
					continue
				if c == quote:
					quote = None
			elif c in '\'"':
				quote = c
			elif c in '([{':
				depth += 1
			elif c in ')]}':
				depth -= 1
				if depth == 0 and i < len(e) - 1:
					return e
		inner = e[1:-1].strip()
		if not inner or top_comma(inner):
			return e
		e = inner
	return e


def top_comma(e):
	depth, quote, prev = 0, None, ''
	for c in e:
		if quote:
			if prev == '\\':
				prev = ''
				continue
			if c == quote:
				quote = None
		elif c in '\'"':
			quote = c
		elif c in '([{':
			depth += 1
		elif c in ')]}':
			depth -= 1
		elif c == ',' and depth == 0:
			return True
		prev = c
	return False


STR1 = r"(?:'(?:[^'\\]|\\.)*'|\"(?:[^\"\\]|\\.)*\")"


def expr_types(e):
	"""the NV_ mask of the value of a let expression (every type for an expression that is no literal)"""
	e = strip_parens(e.strip())
	if e == 'None':
		return 1 << TYPES.index('NONE')
	if re.fullmatch(r'-?(?:0[xX][0-9a-fA-F]+|\d+)[lL]', e):
		return 1 << TYPES.index('LONG')
	if re.fullmatch(r'-?(?:0[xX][0-9a-fA-F]+|\d+)', e):
		return 1 << TYPES.index('INT')
	if re.fullmatch(r'-?(?:\d+\.\d*|\.\d+|\d+)(?:[eE][-+]?\d+)?', e):
		return 1 << TYPES.index('FLOAT')
	if re.fullmatch(r'%s(?:\s*%s)*' % (STR1, STR1), e, re.S):
		return 1 << TYPES.index('STR')
	if e.startswith('(') and e.endswith(')') and strip_parens(e) == e:
		return 1 << TYPES.index('TUPLE')
	# a list or dict display (not one followed by an index or a call)
	for o, c, t in (('[', ']', 'LIST'), ('{', '}', 'DICT')):
		if e.startswith(o) and e.endswith(c) and closes_at_end(e):
			return 1 << TYPES.index(t)
	return ANY


def closes_at_end(e):
	depth, quote, prev = 0, None, ''
	for i, c in enumerate(e):
		if quote:
			if prev == '\\':
				prev = ''
				continue
			if c == quote:
				quote = None
		elif c in '\'"':
			quote = c
		elif c in '([{':
			depth += 1
		elif c in ')]}':
			depth -= 1
			if depth == 0 and i < len(e) - 1:
				return False
		prev = c
	return True


def mod_names(path):
	"""name -> Name of the module code of a .mod file"""
	names = {}

	def add(name, kind, types=0, flags=0):
		old = names.get(name)
		if old is None:
			names[name] = Name(kind, types, flags, None)
			return
		k = old.kind
		# a variable that is also something else is a variable of every type
		if k != kind:
			types = ANY
			if kind == 'MVK_VAR' or k == 'MVK_VAR':
				k = 'MVK_VAR'
		# MVF_COND only while every line that gives it is in a branch
		f = (old.flags | flags) & ~1 | (old.flags & flags & 1)
		names[name] = Name(k, old.types | types, f, None)

	blocks = []
	for indent, s in statements(open(path, encoding='latin1').read()):
		word = s.split(None, 1)[0]
		rest = s[len(word):].strip()
		if word == 'end':
			blocks.pop()
			continue
		if word == 'else':
			continue
		in_class = 'class' in blocks
		cond = 1 if 'if' in blocks else 0
		if word == 'if':
			blocks.append('if')
			continue
		if word == 'class':
			if not in_class:
				add(re.match(r'\w+', rest).group(0), 'MVK_CLASS', 0, cond)
			blocks.append('class')
			continue
		if in_class:
			continue
		if word == 'import':
			for n in rest.split():
				add(n, 'MVK_MODULE', 0, cond)
		elif word == 'from':
			m = re.match(r'(\w+)\s+import\s+(.*)$', rest)
			if m.group(2).strip() != '*':
				for n in m.group(2).split():
					add(n, 'MVK_IMPORT', 0, cond)
		elif word == 'def':
			for n in rest.split():
				add(n, 'MVK_FUNC', 0, cond)
		elif word == 'let':
			m = re.match(r'(\w+)\s*=\s*(.*)$', rest, re.S)
			add(m.group(1), 'MVK_VAR', expr_types(m.group(2)), cond)
		elif word == 'del':
			for n in rest.split():
				if n in names:
					names[n] = names[n]._replace(flags=names[n].flags | 2)
		elif word not in ('set', 'call', 'c'):
			sys.exit('modvars: %s: unknown statement %r' % (path, s[:60]))
	if blocks:
		sys.exit('modvars: %s: if or class without end' % path)
	for n in SYS:
		if n not in names:
			names[n] = Name('MVK_SYS', 0, 0, None)
	return names


def module_files():
	"""[(module, level or None, path of the .mod file)] of tools/modules/modules.tsv"""
	out = []
	for line in open(TSV):
		if not line.strip() or line.startswith('#'):
			continue
		module, level, _ = line.rstrip('\n').split('\t')
		rel = ('modules/%s.mod' % module.lower()) if level == '-' else ('levels/%s/%s.mod' % (level, module.lower()))
		out.append((module, None if level == '-' else level, os.path.join(ROOT, 'data', rel)))
	return out


# ---- the C state of the ports ----

SLOT = re.compile(r'\bMode_(?:Read|Put|DictPut|DictDel)\s*\(\s*"(\w+)"\s*,\s*"(\w+)"\s*,\s*&\s*(\w+)\s*\.\s*(\w+(?:\s*\[\s*\w+\s*\])?)')
VARTAB = re.compile(r'\bnativeVar_t\s+\w+\s*\[\s*\]\s*=\s*\{')
VARROW = re.compile(r'\{\s*"(\w+)"\s*,\s*&?\s*(\w+)\s*\.\s*(\w+(?:\s*\[\s*\w+\s*\])?)\s*,\s*(\w+)')
NATIVEMOD = re.compile(r'\bNative_(?:Import|ImportGlobals|Put)\s*\(\s*"(\w+)"')
STATICSTRUCT = re.compile(r'^static struct \{.*?^\} (\w+)\b', re.S | re.M)


def c_sources(build):
	"""file -> (raw text, preprocessed text) of the port files with a static struct (C state) or a PyMethodDef table"""
	raws = {f: open(os.path.join(SCRIPT, f)).read() for f in ports.sources()}
	files = [f for f in sorted(raws) if STATICSTRUCT.search(raws[f]) or 'PyMethodDef' in raws[f]]
	pp = ports.preprocess(files, build)
	return {f: (raws[f], pp[f]) for f in files}


def c_functions(src):
	"""(level or None, module) -> [name] of the functions of the C tables (Script.c and Levels.c registrations): the
	loader puts each into the module dict after the module code (Script_ModuleExtras), also one the script has not"""
	tabs = ports.tables({f: pp for f, (_, pp) in src.items()})
	out = collections.defaultdict(list)
	for level, module, g in ports.registrations():
		if g in tabs:
			out[(level, module)] += [name for name, _ in tabs[g][1]]
	return out


def c_places(src):
	"""(module, name) -> [place] of the C state the ports keep"""
	places = collections.defaultdict(set)
	for f, (raw, pp) in src.items():
		structs = set(STATICSTRUCT.findall(raw))
		if not structs:
			continue
		text = '\n'.join(t for _, t in pp)
		base = os.path.basename(f)
		for m in SLOT.finditer(text):
			if m.group(3) in structs:
				places[(m.group(1), m.group(2))].add('%s %s.%s' % (base, m.group(3), re.sub(r'\s+', '', m.group(4))))
		mods = set(NATIVEMOD.findall(text))
		for t in VARTAB.finditer(text):
			end = text.index('};', t.end())
			for r in VARROW.finditer(text, t.end(), end):
				if r.group(2) not in structs:
					continue
				if len(mods) != 1:
					sys.exit('modvars: %s: the nativeVar_t rows name no single module (%s)' % (f, sorted(mods)))
				count = '' if r.group(4) == '0' else '[%s]' % r.group(4)
				places[(next(iter(mods)), r.group(1))].add('%s %s.%s%s' % (base, r.group(2), r.group(3), count))
	for module, prefix, f, array, fmt in COMPUTED:
		raw = open(os.path.join(SCRIPT, f)).read()
		m = re.search(r'\b%s\s*\[\s*\]\s*=\s*\{([^}]*)\}' % array, raw)
		for i, s in enumerate(re.findall(r'"(\w+)"', m.group(1))):
			places[(module, prefix + s)].add('%s %s' % (f, fmt % i))
	return places


# ---- the table ----

def collect(build):
	"""[(module, level, {name: Name})] in the order of the table"""
	src = c_sources(build)
	places, funcs = c_places(src), c_functions(src)
	mods = []
	for module, level, path in module_files():
		mods.append((module, level, mod_names(path)))
	known = {m for m, _, _ in mods}
	for (level, module), fs in funcs.items():
		# a table of a module without a .mod file (a C test module) is no pack module
		for names in [names for m, l, names in mods if m == module and (level is None or l == level)]:
			for n in fs:
				names.setdefault(n, Name('MVK_FUNC', 0, 0, None))
	for (module, name), ps in places.items():
		if module not in known:
			sys.exit('modvars: C state of %s.%s, a module without a .mod file' % (module, name))
		for m, _, names in mods:
			if m != module:
				continue
			old = names.get(name) or Name('MVK_VAR', ANY, 0, None)
			names[name] = old._replace(place='; '.join(sorted(ps)))
	mods.sort(key=lambda x: (x[0].encode(), (x[1] or '').encode()))
	return mods


def types_c(t):
	if t == 0:
		return '0'
	if t == ANY:
		return 'MVT_ANY'
	return ' | '.join('MVT_' + n for i, n in enumerate(TYPES) if t >> i & 1)


def flags_c(f):
	return ' | '.join(n for b, n in ((1, 'MVF_COND'), (2, 'MVF_DEL')) if f & b) or '0'


def cstr(s):
	return '"%s"' % s.replace('\\', '\\\\').replace('"', '\\"')


def tabname(module, level):
	return 'mv_%s_%s' % (level, module) if level else 'mv_%s' % module


def table(mods):
	o = ['/*',
		' * The registry of the module variables (stage 5 of docs/objlib-plan.md, include/scrap/modvars.h): the names the',
		' * module code of each pack module gives, from its .mod file, with kind, types, flags and the C state of the port',
		' * that keeps it. Made by tools/script/modvars.py from tools/modules/modules.tsv, the .mod files and the sources',
		' * of the ports. Do not edit: run the tool after a .mod file or the C state of a port changes, and to resolve a',
		' * merge.',
		' */',
		'#include "scrap/modvars.h"',
		'']
	for module, level, names in mods:
		o.append('static const modVar_t %s[] = {' % tabname(module, level))
		for n in sorted(names, key=lambda x: x.encode()):
			v = names[n]
			o.append('\t{ %s, %s, %s, %s, %s },' % (cstr(n), v.kind, types_c(v.types), flags_c(v.flags),
				cstr(v.place) if v.place else 'NULL'))
		o.append('};')
	o.append('')
	o.append('const modVarModule_t g_ModVarModules[] = {')
	for module, level, names in mods:
		t = tabname(module, level)
		o.append('\t{ %s, %s, %s, sizeof(%s) / sizeof(%s[0]) },' % (cstr(module), cstr(level) if level else 'NULL', t, t, t))
	o.append('};')
	o.append('const int g_ModVarModuleCount = sizeof(g_ModVarModules) / sizeof(g_ModVarModules[0]);')
	return '\n'.join(o) + '\n'


def main():
	ap = argparse.ArgumentParser(description='the registry of the module variables (src/game/Script/ModVarTable.c)')
	ap.add_argument('--build', default=os.path.join(ROOT, 'build-linux64'))
	ap.add_argument('--check', action='store_true')
	ap.add_argument('--list', action='store_true')
	a = ap.parse_args()
	mods = collect(os.path.abspath(a.build))
	nvars = sum(1 for _, _, ns in mods for v in ns.values() if v.kind == 'MVK_VAR')
	nplaces = sum(1 for _, _, ns in mods for v in ns.values() if v.place)
	summary = '%d modules (%d files), %d names, %d variables, %d with C state' % (len({m for m, _, _ in mods}),
		len(mods), sum(len(ns) for _, _, ns in mods), nvars, nplaces)
	if a.list:
		for module, level, names in mods:
			print('== %s%s' % (module, ' (%s)' % level if level else ''))
			for n in sorted(names, key=lambda x: x.encode()):
				v = names[n]
				print('%s\t%s\t%s\t%s\t%s' % (n, v.kind[4:].lower(), types_c(v.types), flags_c(v.flags), v.place or ''))
		print('modvars: ' + summary)
		return 0
	tab = table(mods)
	if a.check:
		old = open(TABLEFILE).read() if os.path.exists(TABLEFILE) else ''
		if old != tab:
			print('modvars: %s is not up to date (run tools/script/modvars.py)' % os.path.relpath(TABLEFILE, ROOT))
		print('modvars: %s%s' % (summary, '' if old == tab else ', not up to date'))
		return 0 if old == tab else 1
	open(TABLEFILE, 'w').write(tab)
	print('modvars: %s written to %s' % (summary, os.path.relpath(TABLEFILE, ROOT)))
	return 0


if __name__ == '__main__':
	sys.exit(main())
