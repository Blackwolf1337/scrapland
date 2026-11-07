# The check of the native values of the script ports (src/game/Script/ScriptNative.c) against Python 1.5.2, run in
# the game by tools/test/litfuzz.sh through the test channel's py command (Python 1.5.2 syntax). The channel command
# "native py" (src/game/Global/TestNative.c) has put _native_lit and _native_repr into __main__.
#
# run(n, seed, wide): n random literal texts (the generator and atoms of the nopy-4 fuzz of Script_Literal, with
# wide=1 more atoms) plus the repr texts of the repr cases and the extra texts. For each text: the native reader
# takes it exactly when Script_Literal does; a taken text evals without error to the same value (type and value,
# float repr), and the native repr, str and check() texts are those of repr, backquotes, str and fmt of sdlib.py of
# that value. The repr of every taken value is checked as a text too.
# reprs(): every repr case copied into a native value and back gives the same value and the same four texts.
import string

OUT = 'litfuzz-sdlib.out'
execfile('sdlib.py')	# fmt: the check() text of the golden test

class _Rand:
	def __init__(self):
		self.x = 1
	def seed(self, a, b, c):
		self.x = (a * 65536 + b * 256 + c) % 2147483647 + 1
	def randint(self, lo, hi):
		self.x = (self.x * 48271) % 2147483647
		return lo + self.x % (hi - lo + 1)
	def choice(self, l):
		return l[self.randint(0, len(l) - 1)]
whrandom = _Rand()

def _same(a, b):
	if type(a) != type(b):
		return 0
	if type(a) in (type([]), type(())):
		if len(a) != len(b):
			return 0
		for i in range(len(a)):
			if not _same(a[i], b[i]):
				return 0
		return 1
	if type(a) == type({}):
		# the order of the keys is checked by the repr texts
		if len(a) != len(b):
			return 0
		for k in a.keys():
			if not b.has_key(k) or not _same(a[k], b[k]):
				return 0
		return 1
	if type(a) == type(1.0):
		return repr(a) == repr(b) and (a == b or repr(a) == 'nan' or repr(a) == '-nan')
	return a == b

ATOMS = ['0', '1', '7', '99', '100', '-1', '-5', '- 3', '-\t4', '1000', '2147483647', '2147483648', '-2147483648',
	'9223372036854775807', '9223372036854775808', '-9223372036854775808', '0.5', '1.', '12.25', '0.1', '3.14159265358979',
	'-0.0', '0.0', '0.', '-0.', '1e5', '1.5e-3', '010', '0x10', '08', '00', '1L', '5j', '.5', "'a'", '"b"', "''", '""',
	"'it''s'", "'say \"x\"'", '"it\'s"', "'a b'", "'a\\nb'", "'\\xe9'", "'\351t\351'", "'Vulcan'", "'Tesla'", "'x' 'y'",
	'None', 'foo', 'none', "'''t'''", "r'x'", '{}', '`1`', '1+2', "'#'", '#c', "'\t'", "'Upgrd01'", '1000', '1000',
	"'a'", "'a'", '2.5', '2.50', '0.10000000000000001', '1234567890.123456789', '00.5', '1..2', '1.2.3', '5abc',
	'None', '-None', '--1', '+1', '-(1)', '- -1', 'None.x', '1 2', "'\377'", "'\200'", '"\x7f"', '123456789012345678901234567890',
	'1e-05', '-1.2e-05', '2.5E+12', '1E5', '1e+300', '1e400', '-1e400', '1.e3', '0.e1', '0e1', '00e1', '1e', '1e+', '12.5e-3',
	'7L', '0L', '-12345678901234567890L', '123l', '00L', '017L', '1Le',
	"'a\\'b'", "'a\\\\b'", "'\\n'", "'\\t\\r\\a\\b\\f\\v'", "'\\351t\\351'", "'\\0'", "'\\000'", "'\\1234'",
	"'\\x41'", "'\\x4'", "'\\xg'", "'\\x'", "'\\x414243'", "'\\q'", "'\\8'", "'\\\\'", '"\\""', "'x\\\"y'", "'\\\''",
	"'Upg\\162d'", "'\\101BC'", "'a\\\nb'", "'\\\t'", "'\\\377'", "'\\\200'",
	'{}', '{1: 2}', "{'a': 1, 'b': [1, 2]}", '{1: 1, 1.0: 2}', "{'k': {'n': (1, 2)}}", '{[1]: 2}', '{1: 2,}', '{1:2, 3:4}',
	'{ 1 : 2 , }', '{,}', '{1}', '{1:}', '{:1}', '{1: 2 3: 4}', "{'x': 1.5, 1.5: 'x'}", '{None: None}', '{(1, 2): -3}',
	'{1: 2}x', '{-1: -0.0}', "{'a': 'a', 'a': 'b'}"]
# more texts for the native values (run with wide=1 they are atoms of the generator too): dicts that grow, collide
# (hash(-1) is hash(-2)) or replace a key when they grow, keys of every hashable type, long ints and floats beyond 32
# bits as keys, nesting at the limit and beyond, long texts and numbers
EXTRA = ['{1: 1, 2: 2, 3: 3, 1: 5}', '{-1: 1, -2: 2, -3: 3}', "{1: 'a', 1L: 'b', 1.0: 'c'}", '{4294967296.0: 1, 4294967296L: 2}',
	'{1e20: 1, 100000000000000000000L: 2}', "{(1, 'a', 2.5, None, 7L): 0}", '{(1, [2]): 3}', '{{}: 1}', '{0: 0, -0.0: 1}',
	"{'a': 1, 'b': 2, 'c': 3, 'd': 4, 'e': 5, 'f': 6, 'g': 7, 'h': 8, 'i': 9, 'j': 10, 'k': 11}",
	'{0: 0, 8: 8, 16: 16, 24: 24, 32: 32, 40: 40, 48: 48, 56: 56, 64: 64, 72: 72, 80: 80, 88: 88}',
	'{1e400: 1}', '{-1e400: 1, 1e400: 2}', '[' * 32 + '1' + ']' * 32, '[' * 33 + '1' + ']' * 33,
	'(' * 31 + '{1: 2}' + ')' * 31, '(' * 32 + '{1: 2}' + ')' * 32, "'" + 'x' * 300 + "'", "'" + '\\n' * 200 + "'",
	'123456789012345678901234567890123456789012345678901234567890123L',
	'1234567890123456789012345678901234567890123456789012345678901234L', '1e-400', '4.9e-324', '1.7976931348623157e308']
SEPS = [',', ', ', ' ,', ' , ', ',\t', ',,', ' ', '']

def _gen(d):
	k = whrandom.randint(0, 9)
	if d > 3 or k < 6:
		return whrandom.choice(_atoms)
	n = whrandom.randint(0, 4)
	items = []
	for i in range(n):
		items.append(_gen(d + 1))
	body = ''
	for i in range(len(items)):
		if i:
			body = body + whrandom.choice(SEPS[:5]) + items[i]
		else:
			body = items[i]
	if whrandom.randint(0, 5) == 0:
		body = body + ','
	if whrandom.randint(0, 3) == 0:
		body = ' ' + body + ' '
	if whrandom.randint(0, 40) == 0:
		body = body + whrandom.choice(SEPS)
	if k < 8:
		return '[' + body + ']'
	if k == 9:
		pairs = []
		for i in range(n):
			pairs.append(whrandom.choice(_atoms) + whrandom.choice([':', ': ', ' : ', ':\t', '']) + _gen(d + 1))
		body = ''
		for i in range(len(pairs)):
			if i:
				body = body + whrandom.choice(SEPS[:5]) + pairs[i]
			else:
				body = pairs[i]
		if whrandom.randint(0, 5) == 0:
			body = body + ','
		return '{' + body + '}'
	return '(' + body + ')'

def _top():
	if whrandom.randint(0, 4) == 0:
		s = _gen(0)
		for i in range(whrandom.randint(1, 3)):
			s = s + whrandom.choice([',', ', ']) + _gen(0)
		if whrandom.randint(0, 2) == 0:
			s = s + ','
	else:
		s = _gen(0)
	if whrandom.randint(0, 6) == 0:
		s = s + whrandom.choice([' ', '\t', '  ', '\n', ' #x', ';'])
	if whrandom.randint(0, 20) == 0:
		s = whrandom.choice([' ', '\t']) + s
	return s

def _values():
	inf = 1e300 * 1e300
	nan = inf - inf
	big = {}
	for i in range(40):
		big[i * 7 - 50] = i
	dele = {}
	for i in range(20):
		dele[i] = i
	for i in range(0, 20, 3):
		del dele[i]
	return [[1, 2, 3], ['Vulcan', 'Tesla'], (1.5, -2.25, 0.1), [], (), [0.1, 0.2, 0.30000000000000004], [(1, 'a'), (2, 'b')],
		[1e-05, 123456789.0, 1e+22, -1e-300, 2.0**52], ['a b', "it's", 'q"q'], [None, 0, None], 2147483647, -3,
		[[[[[[1]]]]]], ([], ()), [1.0 / 3], [-0.0], ['\351', 'x\ty'], 1e-05, -1.234e-07, 3.0e15, [1e-09, (2e-08, -4e+20)],
		{}, {'a': 1}, {1: 'x', 2: [1.5, 'y']}, {'k': {'j': (1, None)}}, {(1, 2): 3, 'z': -1e-06}, 7L, -12345678901234567890L,
		["it's", 'say "x"', 'both \' and "', 'back\\slash', 'nl\nnl', '\000\001\037', '\177\200\377'],
		{'it\'s': 'q"q', 'n\n': '\t'}, map(chr, range(256)), string.join(map(chr, range(256)), ''),
		# added for the native values
		None, 0, -1, sys.maxint, -sys.maxint - 1, 0L, -0L, 2L**64, -(2L**200), 1.0, -1.0, 0.0, -0.0, inf, -inf, nan, -nan,
		1e16, 1e15, 123456789012.0, 1234567890123.0, 0.1 + 0.2, 1e-310, 2.0**63, -2.0**31, 4294967296.5,
		"\000'", "'\000\"", 'x' * 300, '\303\251t\303\251', '\342\202\254', (1,), ((),), ([1],), (None, (None,)),
		[(1, [2, {3: (4,)}])], big, dele, {1: 1, 1.5: 2, 'x': 3, (1, 2): 4, None: 5, 2L**40: 6, -1: 7, -2: 8},
		{1e20: 'a', 2.0**40: 'b', -2.0**33: 'c', inf: 'd'}, {2**40: 'int', 2L**40: 'long'}, {0: 'a', 0.0: 'b', 0L: 'c'},
		{'': 0, '\000': 1, ' ': 2}, (1.0, 2.5, -3.75), [inf, -inf, nan]]

def _reprs():
	out = []
	for v in _values():
		out.append(repr(v))
		out.append(`v`)
	return out

def _texts(t, e, nat):
	"""the differences of the native texts (repr, str, check) of value e"""
	bad = []
	if nat[1] != repr(e) or nat[1] != `e`:
		bad.append(('repr', t, nat[1], repr(e)))
	if nat[2] != str(e):
		bad.append(('str', t, nat[2], str(e)))
	if nat[3] != fmt(e):
		bad.append(('check', t, nat[3], fmt(e)))
	return bad

def _one(t, g, bad, again):
	"""one text: 1 when the native reader took it"""
	r = _native_lit(t, g)
	try:
		e = eval(t, g)
		ok = 1
	except:
		ok = 0
	if r[0] != r[1]:
		bad.append(('taken native %d Script_Literal %d' % (r[0], r[1]), t))
	if not r[0]:
		return 0
	if not ok:
		bad.append(('taken but eval raised', t))
	elif not _same(r[2][0], e):
		bad.append(('value', t, r[2][0], e))
	else:
		bad[len(bad):] = _texts(t, e, r[2])
		if again is not None:
			again.append(repr(e))
	return 1

def run(n, seed, wide=0):
	global _atoms
	_atoms = ATOMS
	if wide:
		_atoms = ATOMS + EXTRA
	whrandom.seed(seed % 256, (seed / 256) % 256, 7)
	g = globals()
	tests = _reprs() + EXTRA
	for i in range(n):
		tests.append(_top())
	took = 0
	bad = []
	again = []
	for t in tests:
		took = took + _one(t, g, bad, again)
	retook = 0
	for t in again:
		retook = retook + _one(t, g, bad, None)
	o = ['seed %d%s: texts %d taken %d, their reprs %d taken %d, bad %d' % (seed, wide and ' wide' or '', len(tests), took, len(again), retook,
		len(bad))]
	for b in bad[:20]:
		o.append(repr(b))
	return string.join(o, '\n')

def reprs():
	bad = []
	vals = _values()
	for v in vals:
		r = _native_repr(v)
		if r is None:
			bad.append(('no native form', v))
		elif not _same(r[0], v):
			bad.append(('value', r[0], v))
		else:
			bad[len(bad):] = _texts(repr(v), v, r)
	o = ['repr cases %d, bad %d' % (len(vals), len(bad))]
	for b in bad[:20]:
		o.append(repr(b))
	return string.join(o, '\n')
