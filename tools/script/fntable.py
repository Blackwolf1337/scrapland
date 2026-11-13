#!/usr/bin/env python3
# The conversion of the PyMethodDef tables of a port file into function tables (stage 7e of docs/objlib-plan.md,
# portFn_t of include/scrap/scriptnative.h), the mechanical part of the recipe:
#
#   tools/script/fntable.py [--dry] <file.c> ...    the files relative to src/game/Script (levels/Bank.c) or absolute
#
# Per file: every table "PyMethodDef g_X[] = { ... };" with literal rows becomes "const portFn_t g_X[] = { ... };"
#   { "Name", adapter, 1, NULL }                    PORT_FN(Name, adapter)
#   { "Name", (PyCFunction)(void (*)(void))adapter, 3 or METH_VARARGS | METH_KEYWORDS, NULL }
#                                                   PORT_FNK(Name, adapter)
#   { NULL, NULL, 0, NULL }                         PORT_FNEND
# the lines MODE_ADAPTER(pre, name) and MODE_REFADAPTER(pre, name) go (the entry pre_name_E stays), the declaration of
# the table in include/scrap/script.h or src/game/Script/levels/Levels.h becomes "extern const portFn_t g_X[];" and
# its registration in Script.c ({ "Mod", g_X, NULL }) or Levels.c ({ "level", "Mod", g_X, NULL }) puts the table in
# the fns slot ({ "Mod", NULL, g_X }).
#
# A table with a row it cannot read (a macro row, a row of another form) is left as it is and named; hand adapters
# (static PyObject *f(PyObject *self, PyObject *args[, PyObject *kw])) are named, not removed. The rest of the recipe
# (the hand adapters, keyword names, macros, ports.py) is in the hand-back of batch 7e2a.
import argparse, os, re, sys

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
SCRIPT = os.path.join(ROOT, 'src/game/Script')
HEADERS = [os.path.join(ROOT, 'include/scrap/script.h'), os.path.join(SCRIPT, 'levels/Levels.h')]
REGS = [os.path.join(SCRIPT, 'Script.c'), os.path.join(SCRIPT, 'Levels.c')]

TABLE = re.compile(r'^(static )?PyMethodDef (g_\w+)\[\] = \{', re.M)
CAST = r'(?:\(PyCFunction\)\s*)?(?:\(\s*void\s*\(\s*\*\s*\)\s*\(\s*void\s*\)\s*\)\s*)?'
ROW = re.compile(r'^\{ "(\w+)", ' + CAST + r'(\w+), (1|3|METH_VARARGS|METH_VARARGS \| METH_KEYWORDS), NULL \}$')
END = re.compile(r'^\{ NULL, NULL, 0, NULL \}$|^\{ NULL \}$|^\{ 0 \}$')
ADAPTER_LINE = re.compile(r'^MODE_(?:REF)?ADAPTER\(\w+, \w+\)\n', re.M)
HAND = re.compile(r'^(?:static )?PyObject \*(\w+)\(PyObject \*self, PyObject \*args(?:, PyObject \*kw)?\) \{', re.M)


def table_span(text, m):
	"""the end of the table that m starts (after its ';' and newline) and its body (between the braces)"""
	depth, i = 1, m.end()
	while depth and i < len(text):
		depth += {'{': 1, '}': -1}.get(text[i], 0)
		i += 1
	end = text.index(';', i) + 1
	return end + (text[end:end + 1] == '\n'), text[m.end():i - 1]


def convert_table(m, body):
	"""the function table for the PyMethodDef table of m, or None when a row is of another form"""
	out = []
	rest = re.sub(r'/\*.*?\*/', '', body, flags=re.S)
	for r in re.finditer(r'\{[^{}]*\}', rest):
		row = ' '.join(r.group(0).split())
		x = ROW.match(row)
		if x:
			kw = x.group(3) in ('3', 'METH_VARARGS | METH_KEYWORDS')
			out.append('\tPORT_FN%s(%s, %s),' % ('K' if kw else '', x.group(1), x.group(2)))
		elif END.match(row):
			out.append('\tPORT_FNEND')
		else:
			return None
	if re.sub(r'\{[^{}]*\}', '', rest).strip(' \t\n,') or not out or out[-1] != '\tPORT_FNEND':
		return None
	return '%sconst portFn_t %s[] = {\n%s\n};\n' % (m.group(1) or '', m.group(2), '\n'.join(out))


def fix_decl(text, g):
	"""the declaration of g in a header text as a function table"""
	single = re.compile(r'^extern PyMethodDef %s\[\];\n' % g, re.M)
	if single.search(text):
		return single.sub('extern const portFn_t %s[];\n' % g, text), True
	# a list: extern PyMethodDef g_A[], g_B[], ...; (one or more lines)
	for m in re.finditer(r'^extern PyMethodDef ([^;]*);\n', text, re.M | re.S):
		names = [x.strip() for x in m.group(1).replace('\n', ' ').split(',')]
		if g + '[]' not in names:
			continue
		names.remove(g + '[]')
		rest = ''
		if names:
			rest = wrap('extern PyMethodDef ' + ', '.join(names) + ';')
		return text[:m.start()] + rest + 'extern const portFn_t %s[];\n' % g + text[m.end():], True
	return text, False


def wrap(line):
	"""a declaration wrapped at 120 columns (tabs count 8), continuation lines indented by one tab"""
	out, cur = [], ''
	for word in line.split(' '):
		if cur and len(cur) + 1 + len(word) > 120:
			out.append(cur)
			cur = '\t' + word
		else:
			cur = cur + ' ' + word if cur else word
	out.append(cur)
	return '\n'.join(out) + '\n'


def fix_reg(text, g):
	text, n = re.subn(r'(\{ "\w+", (?:"\w+", )?)%s, NULL \}' % g, r'\1NULL, %s }' % g, text)
	return text, n


def main():
	ap = argparse.ArgumentParser(description='PyMethodDef tables of port files into function tables')
	ap.add_argument('--dry', action='store_true')
	ap.add_argument('files', nargs='+')
	a = ap.parse_args()
	heads = {h: open(h).read() for h in HEADERS}
	regs = {r: open(r).read() for r in REGS}
	for f in a.files:
		path = f if os.path.isabs(f) else os.path.join(SCRIPT, f)
		text = open(path).read()
		done, pos = [], 0
		while True:
			m = TABLE.search(text, pos)
			if not m:
				break
			end, body = table_span(text, m)
			t = convert_table(m, body)
			if t is None:
				print('%s: %s has a row of another form: left as it is (comments in a table are dropped)' % (f, m.group(2)))
				pos = end
				continue
			done.append(m.group(2))
			text = text[:m.start()] + t + text[end:]
			pos = m.start() + len(t)
		if done:
			text = ADAPTER_LINE.sub('', text)
		for g in done:
			found = False
			for h in heads:
				heads[h], ok = fix_decl(heads[h], g)
				found = found or ok
			if not found and not re.search(r'^static const portFn_t %s\[\]' % g, text, re.M):
				print('%s: no declaration of %s found' % (f, g))
			n = 0
			for r in regs:
				regs[r], k = fix_reg(regs[r], g)
				n += k
			if not n:
				print('%s: %s is registered nowhere (a table of the file itself?)' % (f, g))
		for h in HAND.findall(text):
			print('%s: hand adapter %s left (remove it, its entry %s_E stays)' % (f, h, h))
		print('%s: %d tables converted' % (f, len(done)))
		if not a.dry:
			open(path, 'w').write(text)
	if not a.dry:
		for h, t in heads.items():
			open(h, 'w').write(t)
		for r, t in regs.items():
			open(r, 'w').write(t)
	return 0


if __name__ == '__main__':
	sys.exit(main())
