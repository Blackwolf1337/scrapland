#!/usr/bin/env python3
"""objcase.py [TestNative.c]: the checks of the channel's "native objtest" (the table ot_script of
src/game/Global/TestNative.c) as the lines of a channel case on Python objects, written to stdout. The case runs in a
level that has Fx and the modules of the three classes loaded: CharsNPC and GateKeeper (the checks twin-spawn and
twin-gate: an indoor level, Townhall), VehiclesNPC (twin-ship: an outdoor level, DownTown); the checks of a module
the level does not have fail there with their ImportError. Each reply is the tuple that the line of the same check of
"native objtest" prints after its name (addresses apart). A check whose name starts with '!' has no Python twin and is
left out."""
import os
import re
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
SRC = sys.argv[1] if len(sys.argv) > 1 else os.path.join(HERE, '..', '..', 'src', 'game', 'Global', 'TestNative.c')
TWINS = {'cShipAI': 'VehiclesNPC', 'CharSpawnPos': 'CharsNPC', 'GateKeeperType': 'GateKeeper'}
HEAD = ("W = tc_obj('OL9a.W', {}); W.t = lambda f, *a: tc_catch(f, a); W.cb_pair = lambda *a: (len(a), a); "
		"W.cb_none = lambda *a: None; W.cb_one = lambda *a: 1; W.cb_str = lambda *a: 'S'; W.cb_strt = lambda *a: 'T'; "
		"W.cb_second = lambda *a: a[1]; "
		"W.cb_cmprev = lambda x, y: cmp(y, x); W.cb_bad = lambda x, y: 'x'; result = 1")


def steps():
	text = open(SRC).read()
	body = text[text.index('static const char *const ot_script[] = {'):]
	body = body[:body.index('\tNULL,\n};')]
	return [s.replace('\\\\', '\\') for s in re.findall(r'"((?:[^"\\]|\\.)*)"', body)]


def atom(t):
	# the channel reads no long literal
	return 'W.' + t[1:] if t.startswith('$') else re.sub(r'(?<![\w.])(-?\d+)L\b', r'long(\1)', t)


def expr(tok):
	op, a = tok[0], tok[1:]
	x = [atom(t) for t in a]
	if op in ('lit', 'val', 'rt'):
		return x[0]
	if op == 'tuple':
		return '(%s,)' % ', '.join(x)
	if op == 'class':
		return "tc_class('%s', %s)" % (a[0], a[1])
	if op == 'twin':
		return "__import__('%s').%s" % (TWINS[a[0]], a[0])
	if op == 'port':
		return "__import__('%s').%s" % tuple(a[0].split('.'))
	if op == 'cb':
		return 'W.cb_' + a[0]
	if op == 'mod':
		return "__import__('%s')" % a[0]
	if op in ('repr', 'str', 'len'):
		return '%s(%s)' % (op, x[0])
	if op == 'type':
		return 'type(%s).__name__' % x[0]
	if op == 'not':
		return 'not %s' % x[0]
	if op == 'hash':
		return 'hash(%s) == hash(%s)' % (x[0], x[0])
	if op == 'hasheq':
		return 'hash(%s) == hash(%s)' % (x[0], x[1])
	if op == 'get':
		return '%s[%s]' % (x[0], x[1])
	if op == 'put':
		return 'tc_set(%s, %s, %s)' % (x[0], x[1], x[2])
	if op == 'del':
		return 'tc_del(%s, %s)' % (x[0], x[1])
	if op == 'clear':
		return 'tc_clear(%s)' % x[0]
	if op == 'slice':
		return '%s[%s:%s]' % (x[0], '' if a[1] == '-' else x[1], '' if a[2] == '-' else x[2])
	if op == 'attr':
		return "getattr(%s, '%s')" % (x[0], a[1])
	if op == 'setattr':
		return "setattr(%s, '%s', %s)" % (x[0], a[1], x[2])
	if op == 'delattr':
		return "delattr(%s, '%s')" % (x[0], a[1])
	if op == 'call':
		return '%s(%s)' % (x[0], ', '.join(x[1:]))
	if op == 'meth':
		return '%s.%s(%s)' % (x[0] if a[0].startswith('$') else '(%s)' % x[0], a[1], ', '.join(x[2:]))
	if op == 'in':
		return '%s in %s' % (x[0], x[1])
	if op == 'cmp':
		return 'cmp(cmp(%s, %s), 0)' % (x[0], x[1])
	if op == 'eq':
		return '%s == %s' % (x[0], x[1])
	if op == 'is':
		return '%s is %s' % (x[0], x[1])
	raise SystemExit('objcase: no step %s' % op)


def main():
	out = ['# object library 9a: the checks of "native objtest" (ot_script of src/game/Global/TestNative.c) on Python '
		   'objects, made by tools/test/objcase.py; reply n is the tuple of line n of the self-test', HEAD]
	names, stmts, items, skip = [], [], [], True

	def flush():
		if not skip:
			out.append('; '.join(stmts + ['result = (%s,)' % ', '.join(items)]))
	for s in steps():
		if s.startswith('-- '):
			flush()
			stmts, items, skip = [], [], s[3] == '!'
			if not skip:
				names.append(s[3:])
			continue
		if skip:
			continue
		tok = s.split(' ')
		if tok[0].endswith('='):
			if items:
				raise SystemExit('objcase: a store after an item: %s' % s)
			stmts.append('W.%s = %s' % (tok[0][:-1], expr(tok[1:])))
		elif tok[0] in ('setslice', 'delslice'):
			if items:
				raise SystemExit('objcase: a store after an item: %s' % s)
			x = [atom(t) for t in tok[1:]]
			sl = '%s[%s:%s]' % (x[0], '' if tok[2] == '-' else x[1], '' if tok[3] == '-' else x[2])
			stmts.append('%s = %s' % (sl, x[3]) if tok[0] == 'setslice' else 'del ' + sl)
		else:
			items.append('W.t(lambda: %s)' % expr(tok))
	flush()
	sys.stdout.write('\n'.join(out) + '\n')
	sys.stderr.write('%d checks: %s\n' % (len(names), ' '.join(names)))


if __name__ == '__main__':
	main()
