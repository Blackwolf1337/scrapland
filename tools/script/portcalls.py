#!/usr/bin/env python3
# The call sites between script ports (stage 3 of docs/objlib-plan.md) on the preprocessed sources of src/game/Script
# (the macros of the files resolved, see ports.py): the progress measure and the end check of the stage.
#
#   tools/script/portcalls.py [files]               per file: the by-name calls left by kind, the PORT_CALL and
#                                                   PORT_OWN sites, the OBJLIB 3 marks; the arity check after it
#   tools/script/portcalls.py sites Mod.Func ...    every by-name and PORT site of these functions
#   tools/script/portcalls.py rewrite Mod.Func [files]
#                                                   the plain by-name sites of Mod.Func (LV_CALL, LV_OWN,
#                                                   Native_CallMod, Native_CallGlobal and the file's aliases of them
#                                                   with a literal module) become PORT_CALL or PORT_OWN in the source
#   tools/script/portcalls.py wrap [files]          the functions the PORT sites of the files call, as the argument
#                                                   of _pton (tools/test/regress/helpers/fol3.py)
#   --build <dir>                                   the build folder for the preprocessor (default build-linux64)
#
# Kinds: port (a function of another module with a ref), own (of a module of the file, through its attribute),
# global (through the module's global), noentry (a C table function without an entry yet), engine, lib, other (data
# functions of the .def files, variables, unknown names). Arity: the count of the fmt at a site against the count the
# entry takes (the number in include/scrap/scriptports.h).
import argparse, collections, os, re, sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import ports

ENGINE = {'Scrap', 'SScorer', 'SAI', 'SVec', 'SAct', 'SSound', 'SInput', 'SLogic', 'SNet', 'SWeap', 'SFX'}
LIB = {'string', '__builtin__', 'sys', 'math', 'whrandom', 'random', 'marshal', 'new', 'types', 'os', 'strop'}
GLOBAL = {'Native_CallGlobal', 'Sf_Own'}
# an argument of a call (one level of nested brackets: NULL is ((void *)0) after the preprocessor)
ARG = r'(?:[^,()]|\((?:[^()]|\([^()]*\))*\))+'

BYNAME = re.compile(r'\b(Native_CallMod|Native_CallGlobal|Script_Try|Script_Call|Script_Ask|Script_CallKw|Script_AskKw|'
	r'Mode_CallKw|Mode_CallIf|Mode_CallVarIf|LvN_OnEnd|LvN_ScorerCallG)\(\s*"(\w+)",\s*"(\w+)"(?:\s*,' + ARG + r',\s*"([a-zA-Z]*)")?')
FRAME = re.compile(r'\b(Sf_Ask|Sf_Run|Sf_Own)\(\s*&?\w+,\s*"(\w+)",\s*"(\w+)"')
SITE = re.compile(r'\bPort_Invoke\(\s*\(&\(static portSite_t\)\{ \.ref = &\((port_\w+)\), \.global = \((\d)\)[^}]*\}\)\s*,'
	+ ARG + r',\s*"([a-zA-Z]*)"')


def refsyms():
	"""port symbol -> (level, module, name, nargs) of the header"""
	out = {}
	for (level, module, name), n in ports.read_header().items():
		out['port_%s%s_%s' % (level + '_' if level else '', module, name)] = (level, module, name, n)
	return out


def owners(pp):
	"""file -> the modules its tables serve; (module, name) -> 1 for every C table function"""
	tabs = ports.tables(pp)
	own, cfunc = collections.defaultdict(set), set()
	for level, module, g in ports.registrations():
		if g in tabs:
			own[tabs[g][0]].add(module)
			cfunc.update((module, name) for name, _ in tabs[g][1])
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
				level, module, name, _ = syms.get(m.group(1), (None, '?' + m.group(1), '?', None))
				out.append((f, no, 'PORT', module, name, m.group(3), m.group(2) == '1'))
	return out


def index(refs):
	"""(module, name) -> [Ref]"""
	out = collections.defaultdict(list)
	for rs in refs.values():
		for r in rs:
			out[(r.module, r.name)].append(r)
	return out


def nargs_of(f, module, name, idx):
	"""the ref of module.name as file f calls it (a level module: the ref of the file's level), None without one"""
	for r in idx.get((module, name), ()):
		if r.level is None or r.file == f:
			return r
	return None


def kind(f, form, module, name, glob, idx, own, cfunc):
	if form == 'PORT':
		return 'PORT'
	if module in ENGINE:
		return 'engine'
	if module in LIB:
		return 'lib'
	if nargs_of(f, module, name, idx):
		return 'global' if glob else 'own' if module in own[f] else 'port'
	return 'noentry' if (module, name) in cfunc else 'other'


def marks(f):
	return open(os.path.join(ports.SCRIPT, f), errors='replace').read().count('OBJLIB 3')


def survey(pp, idx, files):
	own, cfunc = owners(pp)
	kinds = ['port', 'own', 'global', 'noentry', 'engine', 'lib', 'other', 'PORT']
	rows, total, bad = {}, collections.Counter(), []
	for f, no, form, module, name, fmt, glob in scan(pp):
		if files and f not in files:
			continue
		k = kind(f, form, module, name, glob, idx, own, cfunc)
		rows.setdefault(f, collections.Counter())[k] += 1
		r = nargs_of(f, module, name, idx)
		if fmt is not None and r and r.nargs is not None and len(fmt) != r.nargs:
			bad.append('%s:%d %s %s.%s: %d arguments, the entry takes %d' % (f, no, form, module, name, len(fmt), r.nargs))
	print('%-28s %s %8s' % ('file', ' '.join('%7s' % k for k in kinds), 'OBJLIB3'))
	for f in sorted(set(rows) | ({f for f in files} if files else set())):
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


def rewrite(idx, func, files):
	module, name = func.split('.')
	total = 0
	for f in files or ports.sources():
		path = os.path.join(ports.SCRIPT, f)
		text = open(path).read()
		r = nargs_of(f, module, name, idx)
		if not r:
			continue
		lpre = 'L' if r.level else ''
		if r.level and not re.search(r'^#define PORT_LEVEL %s$' % r.level, text, re.M):
			print('%s: a level module: add "#define PORT_LEVEL %s" before converting' % (f, r.level))
			continue
		n = 0
		for form, (glob, fixed) in aliases(text).items():
			pat = (r'\b%s\(\s*"%s",\s*' % (form, name)) if fixed == module else \
				(r'\b%s\(\s*"%s",\s*"%s",\s*' % (form, module, name)) if fixed is None else None
			if not pat:
				continue
			new = 'PORT_%s%s(%s, %s, ' % (lpre, 'OWN' if glob else 'CALL', module, name)
			for m in reversed(list(re.finditer(pat, text))):
				print('%s:%d %s -> %s' % (f, text.count('\n', 0, m.start()) + 1, form, new.split('(')[0]))
				text = text[:m.start()] + new + text[m.end():]
				n += 1
		if n:
			open(path, 'w').write(text)
			total += n
	print('rewrite %s: %d sites' % (func, total))


def wrap(pp, files):
	funcs = sorted({'%s.%s' % (m, n) for f, _, form, m, n, _, _ in scan(pp) if form == 'PORT' and (not files or f in files)})
	print(repr(tuple(funcs)))


def main():
	ap = argparse.ArgumentParser(description='the call sites between script ports')
	ap.add_argument('--build', default=os.path.join(ports.ROOT, 'build-linux64'))
	ap.add_argument('args', nargs='*')
	a = ap.parse_args()
	cmd, rest = (a.args[0], a.args[1:]) if a.args and a.args[0] in ('sites', 'rewrite', 'wrap') else ('survey', a.args)
	build = os.path.abspath(a.build)
	if cmd == 'rewrite':
		rewrite(index(ports.refs(build)[0]), rest[0], rest[1:])
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
