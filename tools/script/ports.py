#!/usr/bin/env python3
# The refs of the direct calls between script ports (stage 3 of docs/objlib-plan.md, PORT_CALL in
# include/scrap/scriptnative.h): writes include/scrap/scriptports.h from the PyMethodDef tables of src/game/Script,
# their registrations in Script.c and Levels.c and the native entries (<adapter>_E, made by MODE_FN and LV_FN or by
# hand), found in the preprocessed sources (compile_commands.json of a build folder).
#
#   tools/script/ports.py                 write the header (run it after a table or an entry changes, and to resolve a
#                                         merge of the header)
#   tools/script/ports.py --check         exit 1 when the header is not what the tool writes or a file with refs has
#                                         no PORTS(<file>) line
#   tools/script/ports.py --lines         add the missing PORTS(<file>) lines (at the end of the file)
#   --build <dir>                         the build folder for the preprocessor (default build-linux64)
#
# One ref per table function with an entry: port_<Module>_<Func>, or port_<level>_<Module>_<Func> for the modules
# of a level (Map, Mission_X: Levels.c entries). A keyword entry whose file defines the names of its slots as
# char *<adapter>_N[] (MODE_KW, the Mode_KwCall adapters) gets PORT_DEFK or PORT_DEFLK: its ref takes keyword calls.
# portcalls.py imports this file.
import argparse, collections, concurrent.futures, json, os, re, shlex, subprocess, sys

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
SCRIPT = os.path.join(ROOT, 'src/game/Script')
HEADER = os.path.join(ROOT, 'include/scrap/scriptports.h')

TABLE = re.compile(r'\bPyMethodDef\s+(g_\w+)\s*\[\s*\]\s*=\s*\{')
# a row: { "Name", adapter, ... }, the adapter maybe cast as (PyCFunction) or (PyCFunction)(void (*)(void)) (keywords)
ROW = re.compile(r'\{\s*"(\w+)",\s*(?:\(PyCFunction\)\s*)?(?:\(\s*void\s*\(\s*\*\s*\)\s*\(\s*void\s*\)\s*\)\s*)?(\w+)')
ENTRY = re.compile(r'\bbyte\s+(\w+)_E\s*\(\s*const\s+nativeVal_t\s*\*\s*\w+\s*,\s*int\s+\w+\s*,\s*nativeVal_t\s*\*\s*\w+\s*\)\s*\{')
NAMES = re.compile(r'\bchar\s*\*\s*(\w+)_N\s*\[\s*\]\s*=')
ARITY = re.compile(r'Port_Arity\(\s*\w+\s*,\s*\(?(\d+)\)?\s*\)')
Ref = collections.namedtuple('Ref', 'file level module name adapter nargs kw')


def sources():
	"""the C files of the ports, relative to src/game/Script"""
	out = []
	for d, _, files in os.walk(SCRIPT):
		out += [os.path.relpath(os.path.join(d, f), SCRIPT) for f in files if f.endswith('.c')]
	return sorted(out)


def tables(pp):
	"""g_name -> (file, [(Python name, adapter)]) of the tables in the preprocessed files"""
	tabs = {}
	for f, lines in pp.items():
		text = '\n'.join(x for _, x in lines)
		for m in TABLE.finditer(text):
			depth, i = 1, m.end()
			while depth and i < len(text):
				depth += {'{': 1, '}': -1}.get(text[i], 0)
				i += 1
			tabs[m.group(1)] = (f, ROW.findall(text[m.end():i]))
	return tabs


def registrations():
	"""[(level or None, module, g_name)] in the order of Script.c and Levels.c"""
	regs = [(None, m.group(1), m.group(2)) for m in
		re.finditer(r'\{ "(\w+)", (g_\w+) \}', open(os.path.join(SCRIPT, 'Script.c')).read())]
	regs += [(m.group(1), m.group(2), m.group(3)) for m in
		re.finditer(r'\{ "(\w+)", "(\w+)", (g_\w+) \}', open(os.path.join(SCRIPT, 'Levels.c')).read())]
	return regs


def _pp1(cmd, path):
	args = shlex.split(cmd)
	if '-o' in args:
		i = args.index('-o')
		del args[i:i + 2]
	args = [a for a in args if a != '-c' and a != path] + ['-E', path]
	r = subprocess.run(args, capture_output=True, text=True, errors='replace', cwd=ROOT)
	if r.returncode:
		sys.exit('ports: the preprocessor failed on %s:\n%s' % (path, r.stderr[-2000:]))
	# the lines of the file itself with their source line numbers (from the line markers)
	lines, cur, no = [], None, 0
	for t in r.stdout.split('\n'):
		m = re.match(r'# (\d+) "([^"]*)"', t)
		if m:
			no, cur = int(m.group(1)), m.group(2)
			continue
		if cur == path and t.strip():
			# the pieces of one source line (a macro expansion) as one line
			if lines and lines[-1][0] == no:
				lines[-1] = (no, lines[-1][1] + ' ' + t.strip())
			else:
				lines.append((no, t))
		no += 1
	return lines


def preprocess(files, build):
	"""file -> [(source line, preprocessed text)] of the lines of the file itself"""
	db = os.path.join(build, 'compile_commands.json')
	if not os.path.exists(db):
		sys.exit('ports: no %s (configure the build folder first)' % db)
	cmds = {e['file']: e.get('command') or shlex.join(e['arguments']) for e in json.load(open(db))}
	jobs = {}
	with concurrent.futures.ThreadPoolExecutor(os.cpu_count() or 4) as ex:
		for f in files:
			path = os.path.join(SCRIPT, f)
			if path not in cmds:
				sys.exit('ports: %s is not in %s (configure the build folder again)' % (path, db))
			jobs[f] = ex.submit(_pp1, cmds[path], path)
	return {f: j.result() for f, j in jobs.items()}


def entries(lines):
	"""adapter -> nargs (None when the entry checks no count) of the entries in preprocessed lines, and the adapters
	with the names of their keyword slots (<adapter>_N)"""
	text = '\n'.join(t for _, t in lines)
	out, kw = {}, set(NAMES.findall(text))
	for m in ENTRY.finditer(text):
		# the count in the entry's own body (an entry without one is followed by other functions)
		depth, i = 1, m.end()
		while depth and i < len(text):
			depth += {'{': 1, '}': -1}.get(text[i], 0)
			i += 1
		a = ARITY.search(text, m.end(), i)
		out[m.group(1)] = int(a.group(1)) if a else None
	return out, kw


def refs(build, pp=None):
	"""file -> [Ref] of the table functions with an entry (in table order), and the problems found (pp: the
	preprocessed sources when the caller has them)"""
	pp = pp or preprocess(sources(), build)
	tabs, problems = tables(pp), []
	ents, kws = {}, {}
	for f, lines in pp.items():
		ents[f], kws[f] = entries(lines)
	seen, dup = {}, set()
	for level, module, g in registrations():
		if g not in tabs:
			continue
		f, rows = tabs[g]
		for name, adapter in rows:
			key = (level, module, name)
			if key in seen:
				problems.append('%s.%s%s is in two tables (%s, %s): no ref' % (module, name,
					' of ' + level if level else '', seen[key].file if seen[key] else '?', f))
				dup.add(key)
			elif adapter in ents[f]:
				seen[key] = Ref(f, level, module, name, adapter, ents[f][adapter], adapter in kws[f])
			else:
				seen[key] = None
	out = collections.defaultdict(list)
	for key, r in seen.items():
		if r and key not in dup:
			out[r.file].append(r)
	return out, problems


def listname(f):
	return 'PORTS_' + f[:-2].replace('/', '_')


def header(byfile):
	o = ['/*',
		' * The refs of the port functions that have a native entry (PORT_CALL and PORT_OWN, include/scrap/scriptnative.h),',
		' * one per table function: made by tools/script/ports.py from the PyMethodDef tables of src/game/Script, their',
		' * registrations in Script.c and Levels.c and the entries. Do not edit: run the tool after a table or an entry',
		' * changes, and to resolve a merge. The number after each ref is the count of arguments its entry takes.',
		' */',
		'#ifndef SCRAP_SCRIPTPORTS_H',
		'#define SCRAP_SCRIPTPORTS_H',
		'']
	for f in sorted(byfile):
		rs = byfile[f]
		o.append('/* %s */' % f)
		for r in rs:
			d = 'PORT_DECLL(%s, %s, %s)' % (r.level, r.module, r.name) if r.level else 'PORT_DECL(%s, %s)' % (r.module, r.name)
			o.append('%s /* %s */' % (d, '?' if r.nargs is None else r.nargs))
		o.append('#define %s \\' % listname(f))
		for r in rs:
			k = 'K' if r.kw else ''
			if r.level:
				o.append('\tPORT_DEFL%s(%s, %s, %s, %s) \\' % (k, r.level, r.module, r.name, r.adapter))
			else:
				o.append('\tPORT_DEF%s(%s, %s, %s) \\' % (k, r.module, r.name, r.adapter))
		o[-1] = o[-1][:-2]
		o.append('')
	o.append('#endif')
	return '\n'.join(o) + '\n'


def read_header():
	"""(level, module, name) -> nargs of the refs in the header as it is"""
	out = {}
	if os.path.exists(HEADER):
		for m in re.finditer(r'^PORT_DECL(L?)\(([\w, ]+)\) /\* (\S+) \*/', open(HEADER).read(), re.M):
			p = [x.strip() for x in m.group(2).split(',')]
			key = tuple(p) if m.group(1) else (None, p[0], p[1])
			out[key] = None if m.group(3) == '?' else int(m.group(3))
	return out


def missing_lines(byfile):
	return [f for f in sorted(byfile) if not re.search(r'^PORTS\(%s\)$' % listname(f)[6:],
		open(os.path.join(SCRIPT, f)).read(), re.M)]


def add_line(f):
	"""the PORTS line at the end of the file (after its adapters, entries and tables)"""
	path = os.path.join(SCRIPT, f)
	t = open(path).read().rstrip('\n')
	open(path, 'w').write(t + '\n\nPORTS(%s)\n' % listname(f)[6:])


def main():
	ap = argparse.ArgumentParser(description='the header of the port refs (include/scrap/scriptports.h)')
	ap.add_argument('--build', default=os.path.join(ROOT, 'build-linux64'))
	ap.add_argument('--check', action='store_true')
	ap.add_argument('--lines', action='store_true')
	a = ap.parse_args()
	byfile, problems = refs(os.path.abspath(a.build))
	for p in problems:
		print('ports: ' + p, file=sys.stderr)
	text = header(byfile)
	missing = missing_lines(byfile)
	n = sum(len(v) for v in byfile.values())
	if a.check:
		old = open(HEADER).read() if os.path.exists(HEADER) else ''
		for f in missing:
			print('ports: %s has no PORTS(%s) line' % (f, listname(f)[6:]))
		if old != text:
			print('ports: %s is not up to date (run tools/script/ports.py)' % os.path.relpath(HEADER, ROOT))
		ok = old == text and not missing
		print('ports: %d refs in %d files%s' % (n, len(byfile), '' if ok else ', not up to date'))
		return 0 if ok else 1
	open(HEADER, 'w').write(text)
	if a.lines:
		for f in missing:
			add_line(f)
		missing = []
	for f in missing:
		print('ports: %s has no PORTS(%s) line (--lines adds it)' % (f, listname(f)[6:]))
	print('ports: %d refs in %d files written to %s' % (n, len(byfile), os.path.relpath(HEADER, ROOT)))
	return 0


if __name__ == '__main__':
	sys.exit(main())
