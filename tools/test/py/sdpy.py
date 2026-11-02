# The interpreter itself (execfile'd near the end of sdtest.py, Python 1.5.2): ScrapC.exe runs its own build of
# Python 1.5.2 (gcc, src/python), the original the MSVC build in Scrap.exe, and the game's scripts depend on both
# behaving the same. Numbers at their edges, float formatting, strings and strop, hashes and dict order, sort and
# cmp, marshal, regex, math, string.py, exceptions, eval, exec and compile errors. In the hook DLL runs both sides
# use the exe's Python, so only the ScrapC.exe run compares something.
# Not here, because they stop the original: INT_MIN / -1 and INT_MIN % -1 (idiv traps), pow(x, y, -1) when a
# product is INT_MIN, long() and hash() of inf and nan (PyLong_FromDouble loops on them), '%.Nd' with a large N
# (formatint writes into a 120 byte buffer), pow() of longs with the modulus 0L (long_pow releases the quotient
# that the failed l_divmod did not set), marshal.loads of a complex ('x') cut short (r_string copies -1 bytes) or
# of a string with a huge length (the pool allocation fails), the regex a** (regexpr.c loops).
import marshal, math, regex, strop

def spyTry(fn):
	try:
		return fn()
	except:
		return 'exception %s: %s' % (sys.exc_type, sys.exc_value)

spyMin = -2147483647 - 1
spyMax = 2147483647
spyInf = 1e308 * 10.0
spyNan = spyInf - spyInf

# integers: every pair of the edge values through the operators that check overflow
spyInts = (0, 1, -1, 2, -2, 3, -3, 7, -7, 46340, 46341, -46341, 65535, 65536, -65536, 1073741824, -1073741824,
	spyMax, spyMax - 1, spyMin, spyMin + 1)
for spyA in spyInts:
	check('int neg abs inv %d' % spyA, lambda a=spyA: (spyTry(lambda a=a: -a), spyTry(lambda a=a: abs(a)), ~a,
		spyTry(lambda a=a: +a), hex(a), oct(a), str(a), float(a), long(a)))
	for spyB in spyInts:
		check('int + - * %d %d' % (spyA, spyB), lambda a=spyA, b=spyB: (spyTry(lambda a=a, b=b: a + b),
			spyTry(lambda a=a, b=b: a - b), spyTry(lambda a=a, b=b: a * b), a & b, a | b, a ^ b, cmp(a, b)))
		if spyA != spyMin or spyB != -1:
			check('int / %% divmod %d %d' % (spyA, spyB), lambda a=spyA, b=spyB: (spyTry(lambda a=a, b=b: a / b),
				spyTry(lambda a=a, b=b: a % b), spyTry(lambda a=a, b=b: divmod(a, b))))
	for spyB in (0, 1, 2, 15, 16, 30, 31, 32, 33, 100, -1):
		check('int << >> %d %d' % (spyA, spyB), lambda a=spyA, b=spyB: (spyTry(lambda a=a, b=b: a << b),
			spyTry(lambda a=a, b=b: a >> b)))
	for spyB in (0, 1, 2, 3, 5, 16, 30, 31, 32, -1):
		check('int pow %d %d' % (spyA, spyB), lambda a=spyA, b=spyB: spyTry(lambda a=a, b=b: pow(a, b)))
		for spyZ in (7, -7, 65521, -65521, spyMax, spyMin, 0, 1):
			check('int pow %d %d %d' % (spyA, spyB, spyZ),
				lambda a=spyA, b=spyB, z=spyZ: spyTry(lambda a=a, b=b, z=z: pow(a, b, z)))
check('int pow -1 big', lambda: (pow(-1, spyMax), pow(-1, spyMax - 1), pow(1, spyMax), pow(0, spyMax), pow(2, 0)))
check('int coerce', lambda: (coerce(1, 2.5), coerce(1, 2L), coerce(1.5, 2L), 7 / 2.0, 7 % 2.5, -7 / 2L, 2 ** -1.0))

# longs
spyLongs = (0L, 1L, -1L, 7L, -7L, 2L ** 31, -(2L ** 31), 2L ** 32 - 1, 2L ** 63, -(2L ** 63) + 5, 2L ** 100 + 12345,
	-(3L ** 70), 10L ** 30)
for spyA in spyLongs:
	check('long %s' % str(spyA), lambda a=spyA: (str(a), repr(a), hex(a), oct(a), -a, abs(a), ~a, hash(a),
		spyTry(lambda a=a: int(a)), spyTry(lambda a=a: float(a)), a >> 3, a << 40, spyTry(lambda a=a: a >> -1)))
	for spyB in spyLongs:
		check('long ops %s %s' % (str(spyA), str(spyB)), lambda a=spyA, b=spyB: (a + b, a - b, a * b,
			spyTry(lambda a=a, b=b: a / b), spyTry(lambda a=a, b=b: a % b), spyTry(lambda a=a, b=b: divmod(a, b)),
			a & b, a | b, a ^ b, cmp(a, b)))
	for spyB in (0L, 1L, 2L, 3L, 17L, -1L):
		check('long pow %s %s' % (str(spyA), str(spyB)), lambda a=spyA, b=spyB: (spyTry(lambda a=a, b=b: pow(a, b)),
			spyTry(lambda a=a, b=b: pow(a, b, 1000000007L)), spyTry(lambda a=a, b=b: pow(a, b, -97L))))
for spyF in (0.0, -0.0, 0.5, -0.5, 1.5, -1.5, 1e10, -1e10, 2.0 ** 31, -(2.0 ** 31), 2.0 ** 52 + 1, 2.0 ** 63,
		1.2345e100, -9.87e300, 1e-300, 1.7976931348623157e308):
	check('long(%.17g)' % spyF, lambda f=spyF: spyTry(lambda f=f: long(f)))
	check('int(%.17g)' % spyF, lambda f=spyF: spyTry(lambda f=f: int(f)))
for spyF in (2147483647.9, 2147483648.0, -2147483648.5, -2147483649.0, spyInf, -spyInf, spyNan):
	check('int(%.17g)' % spyF, lambda f=spyF: spyTry(lambda f=f: int(f)))
check('long big', lambda: (2L ** 1023 * 2 / 3, float(2L ** 1023), spyTry(lambda: float(2L ** 1024)),
	spyTry(lambda: float(-(2L ** 2000))), long(1e300) % 1000000007L, str(long(2.0 ** 200))))
check('long mixed', lambda: (1 + 2L, 2L * 3.5, cmp(1, 1L), cmp(2L ** 40, 1e12), cmp(1.5, 1L), 5L / 2, -5L / 2, -5L % 3,
	5L % -3, divmod(-(2L ** 64), 7), divmod(2L ** 64, -7)))

# int(), long() and float() of strings
for spyS in ('0', '12', ' 12 ', '-12', '+12', '2147483647', '2147483648', '-2147483648', '-2147483649',
		'4294967296', '99999999999', '0x10', '010', '1.5', '', ' ', '12abc', '1e3', '00000000000000000000012',
		'-0', '\t7\n'):
	check('int(%s)' % repr(spyS), lambda s=spyS: spyTry(lambda s=s: int(s)))
	check('long(%s)' % repr(spyS), lambda s=spyS: spyTry(lambda s=s: long(s)))
for spyS in ('12L', '12l', '123456789012345678901234567890', '-123456789012345678901234567890', '1' * 300):
	check('long(%s)' % repr(spyS), lambda s=spyS: spyTry(lambda s=s: long(s)))
spyFloatStrs = ('0', '1.5', ' 1.5 ', '-0.0', '0.1', '1e308', '1.7976931348623157e308', '1.7976931348623158e308',
	'1.7976931348623159e308', '1e309', '-1e309', '2.2250738585072014e-308', '2.2250738585072011e-308', '1e-310',
	'4.9406564584124654e-324', '4.9e-324', '2.5e-324', '2.4e-324', '1e-400', '-1e-400', 'inf', '-inf', 'INF',
	'infinity', 'nan', '-nan', 'NaN', 'nan(ind)', '1.#INF', '1.#QNAN', '0x1p3', '0x1.8p1', '0X10', '1e', '1e+', '.',
	'.5', '5.', '', ' ', '1.5abc', '1d5', '1E5', '9007199254740993', '123456789012345678901234567890',
	'1.00000000000000011102230246251565404236316680908203125', '0.1e-1', '1' + '0' * 400, '0.' + '0' * 400 + '1',
	'1_0', '+.5e+2', '\t-3.25\n')
for spyS in spyFloatStrs:
	check('float(%s)' % repr(spyS), lambda s=spyS: spyTry(lambda s=s: float(s)))
	check('float(%s) str' % repr(spyS), lambda s=spyS: spyTry(lambda s=s: '%.17g' % float(s)))
	check('strop.atof(%s)' % repr(spyS), lambda s=spyS: spyTry(lambda s=s: '%.17g' % strop.atof(s)))
for spyS in ('0.1', '1e-320', '2.2250738585072011e-308', '4.9e-324', '1.7976931348623158e308',
		'1.7976931348623159e308', '1e999', '0777', '0xffffffff', '0x7fffffff + 1', '2147483647 + 1', '2147483648',
		'-2147483648', '4294967296', '1L << 100', '017777777777', '037777777777', '040000000000', '0x100000000',
		'1j * 1j', '2 ** 10', '"\\x41\\101\\777\\xg"', "'a' 'b'", '3 / 2', '-3 / 2', '-3 % 2', '1 / 0', '1.0 % 0',
		'divmod(1, 0)', '1e16', '123456789012.0', '.1 + .2', '`1e22`', '`1e-5`'):
	check('eval(%s)' % repr(spyS), lambda s=spyS: spyTry(lambda s=s: eval(s)))
	check('eval(%s) str' % repr(spyS), lambda s=spyS: spyTry(lambda s=s: str(eval(s))))

# floats: arithmetic
spyFloats = (0.0, -0.0, 1.0, -1.0, 0.5, 3.0, -7.5, 0.1, 1e308, -1e308, 2.2250738585072014e-308, 1e-310, 5e-324,
	spyInf, -spyInf, spyNan)
for spyA in spyFloats:
	check('float neg abs %.17g' % spyA, lambda a=spyA: (-a, abs(a), +a, a == a, a < 0, cmp(a, 0.0)))
	for spyB in spyFloats:
		check('float ops %.17g %.17g' % (spyA, spyB), lambda a=spyA, b=spyB: (a + b, a - b, a * b,
			spyTry(lambda a=a, b=b: a / b), spyTry(lambda a=a, b=b: a % b), spyTry(lambda a=a, b=b: divmod(a, b)),
			spyTry(lambda a=a, b=b: a ** b), cmp(a, b), a == b, a < b, a > b, max(a, b), min(a, b)))
for spyA in (2.0, -2.0, 10.0, -8.0, 0.5, 1e300, 1e-300):
	for spyB in (0.0, 0.5, -0.5, 1.0, 2.0, 3.0, -3.0, 1.0 / 3.0, 9999.0, 10000.0, -10001.0, 300.0, 1e10, -1e10):
		check('float pow %.17g %.17g' % (spyA, spyB), lambda a=spyA, b=spyB: spyTry(lambda a=a, b=b: a ** b))
for spyA in (0.5, 1.5, 2.5, -0.5, -2.5, 1.005, 2.675, 1234.5678, -1234.5678, 1e300, 1e-300, 0.285, 5e-324,
		spyInf, spyNan):
	for spyB in (0, 1, 2, 3, -1, -2, 20, 400, -400):
		check('round(%.17g, %d)' % (spyA, spyB), lambda a=spyA, b=spyB: round(a, b))
check('float misc', lambda: (0.1 + 0.2, 1.0 / 3.0, 2.0 / 3.0, 1e308 * 10.0 - 1e308 * 10.0 == 0, 1e-308 / 1e10,
	5e-324 / 2, 5e-324 * 0.5, 1e300 * 1e300, 1e-300 * 1e-300, 4.35 * 100, 1.1 * 1.1, 1e16 + 1, 2.0 ** 53 + 1.0,
	float(spyMax) + 1, divmod(-7.5, 2), -7.5 % 2, 7.5 % -2, 1e300 % 3, -1e-300 % 1e300, 5.0 % spyInf,
	spyTry(lambda: spyInf % 3), spyTry(lambda: 1.0 % 0.0), spyTry(lambda: 0.0 / 0.0), coerce(1.5, 2)))
check('complex', lambda: (str(complex(1, 2) * complex(3, -4)), repr(complex(1.5, -0.25)), abs(complex(3, 4)),
	str((1 + 2j) / (3 - 4j)), str(1j ** 2), str((1 + 2j) ** 0.5), str((-8 + 0j) ** (1.0 / 3)),
	spyTry(lambda: (1 + 2j) / 0j), spyTry(lambda: 0j ** -1), str(complex(1e300, 1e300) * 10),
	str(divmod(7 + 3j, 2 + 1j)), str((5 + 1j) % (2 - 1j)), hash(complex(1.5, 0)), hash(complex(2, 0)),
	str(complex(spyInf, spyNan)), str(complex(0.1, 1e22)), repr(complex(1e-310, -0.0))))

# floats: formatting
spyFmtFloats = (0.0, -0.0, 1.0, -1.0, 0.1, 0.5, 1.5, 2.5, 0.125, 1.0 / 3.0, 2.0 / 3.0, 1e-5, 1e-4, 1.234e-5,
	0.0001234, 1e-7, 123.456, 123456789012.0, 1234567890123.0, 999999999999.5, 1e15, 1e16, 1e17, 1e18, 1e19, 1e20,
	1e21, 1e22, 1e23, 9.999999999999999e22, 1e49, 9.9e49, 1e50, 1e100, 1e300, 1.7976931348623157e308,
	2.2250738585072014e-308, 2.225073858507201e-308, 1e-310, 1e-320, 4.9406564584124654e-324, 0.30000000000000004,
	5e-324 * 3, 2.0 ** 63, -2.0 ** 63, 4.35, 0.285, 1.005, 2.675, 5e-5, 0.00005, 9.5, 10.5, 0.05, 1e-50, spyInf,
	-spyInf, spyNan, -spyNan)
spyFmts = ('%f', '%e', '%g', '%E', '%G', '%.17g', '%.12g', '%.0f', '%.1f', '%.2f', '%.3e', '%.0e', '%.20e',
	'%10.3f', '%-12.3e|', '%+f', '% f', '%+g', '%#g', '%#.0f', '%#.0e', '%#x', '%010.2f', '%-010.2f|', '%.50f',
	'%.50e', '%.50g', '%.30g', '%.60f', '%d', '%x', '%r')
for spyA in spyFmtFloats:
	check('float str %.17g' % spyA, lambda a=spyA: (str(a), repr(a), `a`, '%s' % a))
	for spyF in spyFmts:
		check('float %s %.17g' % (spyF, spyA), lambda a=spyA, f=spyF: spyTry(lambda a=a, f=f: f % a))
check('float marshal str', lambda: (marshal.dumps(0.1), marshal.dumps(1e22), marshal.dumps(5e-324),
	marshal.dumps(spyInf), marshal.dumps(spyNan), marshal.dumps(-0.0), marshal.dumps(1.0 / 3.0)))

# string formatting of the other kinds
for spyA in (0, 1, -1, 7, -7, 255, 256, 65, 1000000, spyMax, spyMin, 2L ** 40, -(2L ** 40), 1.9, -1.9, 1e10, 'x',
		None, (1,)):
	for spyF in ('%d', '%i', '%u', '%x', '%X', '%o', '%#x', '%#X', '%#o', '%08d', '%-8d|', '%+d', '% d', '%.5d',
			'%5.3d', '%8.5x', '%-#10o|', '%c', '%s', '%10s|', '%-10s|', '%.2s', '%5.1s|', '%*d', '%.*d'):
		if spyF[1] == '*':
			check('fmt %s 6 %s' % (spyF, repr(spyA)), lambda a=spyA, f=spyF: spyTry(lambda a=a, f=f: f % (6, a)))
		else:
			check('fmt %s %s' % (spyF, repr(spyA)), lambda a=spyA, f=spyF: spyTry(lambda a=a, f=f: f % (a,)))
for spyA in ('a', '', 'ab', '\000', '\377', -1, -129, 300, 65.5):
	check('fmt %%c %s' % repr(spyA), lambda a=spyA: spyTry(lambda a=a: '%c' % (a,)))
for spyS in ("'%(a)s %(b)d %(a)r' % {'a': 'x', 'b': 3}", "'%(a)s' % {}", "'%s %s' % 'a'", "'%s' % ('a', 'b')",
		"'%d' % 'x'", "'%q' % 1", "'%' % 1", "'%%%s%%' % 'x'", "'%*.*f' % (10, 3, 3.14159)", "'%-*.*e|' % (12, 2, -0.000123)",
		"'%*d' % ('x', 1)", "'%5%|' % ()", "'%s' % [1, 'a']", "'%s' % ({1: 2},)", "'%s' % (1L,)", "'%.3s' % 12345",
		"'%100s|' % 'x'", "'%-100.50s|' % ('y' * 80)", "'%.100s' % 'z'", "'%(a)s %s' % {'a': 1}", "'%s %(a)s' % {'a': 1}",
		"'%(a' % {'a': 1}", "'%-+ #0d' % 5", "'%h' % 1", "'%ld' % 7", "'%-5c|' % 'x'", "'%03c' % 'x'", "'%(a)*d' % {'a': 1}"):
	check('fmt %s' % spyS, lambda s=spyS: spyTry(lambda s=s: eval(s)))

# strings and strop
spyStrs = ('', 'a', 'abc', 'Hello, World', '  padded\t\n', 'a,b,,c,', 'x' * 40, '\000\001\177\200\377', 'MiXeD 123',
	'tab\tsep\tline', 'line1\nline2\r\nline3', 'aaa', 'ab' * 7)
for spyS in spyStrs:
	check('str %s' % repr(spyS), lambda s=spyS: (repr(s), str(s), len(s), hash(s), s[1:3], s[-2:], s * 2, s * -1,
		strop.upper(s), strop.lower(s), strop.swapcase(s), strop.capitalize(s), strop.strip(s), strop.lstrip(s),
		strop.rstrip(s), strop.split(s), strop.split(s, ','), strop.split(s, 'a', 1), strop.splitfields(s, 'a'),
		strop.find(s, 'a'), strop.rfind(s, 'a'), strop.find(s, 'a', 2), strop.find(s, ''), strop.count(s, 'a'),
		strop.replace(s, 'a', 'XY'), strop.replace(s, 'a', '', 1), strop.expandtabs(s, 4), strop.joinfields(
		strop.split(s), '-'), cmp(s, 'abc'), s < 'b', 'a' in s))
check('strop tables', lambda: (strop.whitespace, strop.lowercase, strop.uppercase,
	strop.maketrans('abc', 'xyz'), strop.translate('abcabc', strop.maketrans('abc', 'xyz'), 'b')))
spyAll = ''
for spyI in range(256):
	spyAll = spyAll + chr(spyI)
check('strop 256', lambda: (strop.upper(spyAll), strop.lower(spyAll), strop.swapcase(spyAll), strop.split(spyAll),
	strop.strip(spyAll), hash(spyAll), repr(spyAll)))
for spyS in ('0', '12', '-12', ' 12', '12 ', '0x1f', '0X1F', '077', '09', 'z', 'Z', '2147483647', '2147483648',
		'-2147483648', '-2147483649', '4294967295', '99999999999', '', '-', '+5', '1L'):
	for spyB in (10, 0, 16, 8, 36, 2):
		check('strop.atoi(%s, %d)' % (repr(spyS), spyB), lambda s=spyS, b=spyB: spyTry(lambda s=s, b=b: strop.atoi(s, b)))
		check('strop.atol(%s, %d)' % (repr(spyS), spyB), lambda s=spyS, b=spyB: spyTry(lambda s=s, b=b: strop.atol(s, b)))
for spyB in (1, 37, -1):
	check('strop.atoi base %d' % spyB, lambda b=spyB: spyTry(lambda b=b: strop.atoi('5', b)))
check('str misc', lambda: (chr(0), chr(255), spyTry(lambda: chr(256)), spyTry(lambda: chr(-1)), ord('\377'),
	spyTry(lambda: ord('ab')), 'abc' < 'abd', 'a\377' > 'a\001', '' < '\000', cmp('ab', 'abc'), 'abc'[-1],
	spyTry(lambda: 'abc'[10]), 'abc'[10:20], 'abc'[-10:2], 'x' + 'y' * 3, spyTry(lambda: 'a' + 1), str(None),
	repr('it\'s "q"'), repr("'"), repr('\\'), `'\t\n\r\v\f\a\b'`, intern('spyintern') == 'spyintern', hex(-1),
	oct(-1), hex(spyMin), oct(spyMin), hex(0), oct(0)))

# string.py of the packs (scripts/python)
try:
	import string
	spyString = string
except:
	spyString = None
check('import string', lambda: spyString != None)
if spyString:
	for spyS in ('1.5', ' -2.5e3 ', '1e400', '1e-400', 'x', '0x10'):
		check('string.atof(%s)' % repr(spyS), lambda s=spyS: spyTry(lambda s=s: '%.17g' % spyString.atof(s)))
	for spyS in ('12', '-12', '0x10', '010', 'x', '99999999999'):
		check('string.atoi(%s)' % repr(spyS), lambda s=spyS: spyTry(lambda s=s: spyString.atoi(s)))
		check('string.atoi(%s, 0)' % repr(spyS), lambda s=spyS: spyTry(lambda s=s: spyString.atoi(s, 0)))
		check('string.atol(%s)' % repr(spyS), lambda s=spyS: spyTry(lambda s=s: spyString.atol(s)))
	check('string misc', lambda: (spyString.zfill('-12', 6), spyString.ljust('ab', 5), spyString.rjust('ab', 5),
		spyString.center('ab', 7), spyString.split('a b  c', None, 1), spyString.join(['a', 'b'], '+'),
		spyString.capwords('hello  big world'), spyString.digits, spyString.hexdigits, spyString.letters,
		spyString.whitespace, spyString.index('abcabc', 'c', 3), spyTry(lambda: spyString.index('abc', 'z')),
		spyString.zfill('3.5', 6), spyString.expandtabs('a\tb', 8)))

# hashes and the order of dicts (not of a NaN: frexp leaves its exponent unset, so that hash is stack garbage)
for spyV in (0, 1, -1, -2, 2, spyMax, spyMin, 1000000, 0.5, -0.5, 1.0, -1.0, 1.5, 0.1, 1e10, -1e10, 1e100, 1e-300,
		5e-324, 2L ** 40, -(2L ** 40), 2L ** 100, 0L, -1L, (), (1,), (1, 2), (1, 2, 3), ('a', 1.5, (2,)),
		(0.1, -1, 2L), '', 'a', 'b', 'ab', 'ba', 'Scrap', 'Scrapland', 'x' * 100, '\377\376', 1 + 2j, 1.5 + 0j, 0.1 + 0.2j,
		complex(-7.25, 1e-300), complex(1e10, 0.3), 1.0 / 3.0, -2.0 / 3.0, 1e-5, 123.456, -0.75):
	check('hash %s' % repr(spyV), lambda v=spyV: hash(v))
spyD = {}
for spyI in range(40):
	spyD[spyI * 7 - 50] = spyI
check('dict ints', lambda: (spyD.keys(), spyD.values()))
for spyI in range(0, 40, 3):
	del spyD[spyI * 7 - 50]
for spyI in range(100, 120):
	spyD[spyI * spyI] = -spyI
check('dict ints changed', lambda: (spyD.items(), len(spyD)))
spyD = {}
for spyS in ('Scrap', 'SWeap', 'SVec', 'SAct', 'SFX', 'SLogic', 'SSound', 'SInput', 'SAI', 'SNet', 'SScorer', 'Name',
		'Pos', 'Angle', 'Life', 'Money', 'Ammo', 'Player', 'Ship', 'Mesh', 'a', 'b', 'c', 'aa', 'ab', 'ba', ''):
	spyD[spyS] = len(spyD)
check('dict strs', lambda: spyD.keys())
spyD['Life'] = None
del spyD['Pos']
del spyD['SAI']
spyD['SDNew'] = 1
spyD['SDNew2'] = 2
check('dict strs changed', lambda: spyD.items())
spyD = {}
for spyV in (1, 1.0, 1L, 2.5, -3, (1, 2), 'x', (), 0, 0.0, -0.0, 2L ** 40, 1e10, 'y', (1, (2, 3)), 1.5 + 0j, 4 + 1j):
	spyD[spyV] = repr(spyV)
check('dict mixed', lambda: (spyD.keys(), spyD.values(), spyD.has_key(1.0), spyD.get(2.5), spyD.get(9, 'no')))
check('dict misc', lambda: ({'b': 1, 'a': 2, 'c': 3}.keys(), {3: 'a', 2: 'b', 1: 'c'}.items(), cmp({1: 2}, {1: 3}),
	cmp({1: 2}, {2: 1}), cmp({}, {1: 1}), {1: 2} == {1: 2}, str({'a': 1, 'b': [1, 2]}), spyTry(lambda: {}['x']),
	spyTry(lambda: {[]: 1}), spyTry(lambda: hash([]))))
spyD = {}
for spyI in range(200):
	spyD['k%d' % spyI] = spyI
	if spyI % 3 == 0:
		del spyD['k%d' % (spyI / 2)]
check('dict churn', lambda: (spyD.keys(), len(spyD)))
spyD = {}
for spyI in range(100):
	spyD[(spyI, spyI * 0.5)] = spyI
check('dict tuples', lambda: spyD.keys())
spyD = {}
for spyI in range(60):
	spyD[spyI * 0.1 + 0.05] = spyI
	spyD[complex(spyI * 0.3, 1.1)] = -spyI
check('dict floats', lambda: spyD.values())

# sort and cmp
spyL = []
spyR = 12345
for spyI in range(300):
	spyR = (spyR * 1103 + 12345) % 65536
	spyL.append(spyR % 1000)
spyM = spyL[:]
spyM.sort()
check('sort ints', lambda: spyM)
spyM = spyL[:]
spyM.sort(lambda a, b: cmp(b, a))
check('sort cmp reverse', lambda: spyM)
spyM = []
for spyI in range(len(spyL)):
	spyM.append((spyL[spyI], spyI))
spyM.sort(lambda a, b: cmp(a[0] / 100, b[0] / 100))
check('sort cmp equal keys', lambda: spyM)
spyM = spyL[:50]
spyM.sort(lambda a, b: cmp(a % 7, b % 7))
check('sort cmp small', lambda: spyM)
spyM = []
for spyI in range(len(spyL)):
	spyM.append(spyL[spyI] * 0.37 - 100)
spyM.sort()
check('sort floats', lambda: spyM[:40])
spyM = [3, 'a', 2.5, None, (1,), [2], {}, 1L, 'B', -1, (), [], 0.0, 'a', 2L ** 40, len, 1 + 1j]
spyM.sort()
check('sort mixed', lambda: map(lambda x: type(x).__name__ + ':' + str(x), spyM))
check('sort raises', lambda: spyTry(lambda: [3, 1, 2].sort(lambda a, b: 1 / 0)))
check('sort bad cmp', lambda: spyTry(lambda: [3, 1, 2].sort(lambda a, b: 'x')))
for spyA, spyB in ((1, 'a'), ('a', (1,)), ([1], (1,)), (None, 0), (None, ''), (1, 1.0), (1L, 1.5), ({}, []),
		('abc', 'abd'), ((1, 2), (1, 2, 0)), (spyMin, 2L ** 40), (spyNan, 1.0), (1.0, spyNan), (spyNan, spyNan),
		([1, 2], [1, 3]), ((), []), (len, 1), (1 + 0j, 1), ('', ()), (0, -0.0), (2L ** 1100, 1e308), (spyInf, 2L ** 1100)):
	check('cmp %s %s' % (repr(spyA), repr(spyB)), lambda a=spyA, b=spyB: spyTry(lambda a=a, b=b: (cmp(a, b), a == b,
		a < b, cmp(b, a))))
check('id', lambda: (id(None) == id(None), id(spyL) == id(spyL), id(spyL) != id(spyM)))
check('list misc', lambda: (range(-5, 5, 3), range(10, -10, -4), spyTry(lambda: range(1, 2, 0)), len(xrange(0, 100, 7)),
	[1, 2, 3] * 2, spyTry(lambda: [].remove(1)), spyTry(lambda: [].index(1)), [1, 2, 3][-5:5], spyTry(lambda: [][0]),
	reduce(lambda a, b: a * b, range(1, 13)), filter(None, [0, 1, '', 'a', None, ()]), map(None, [1, 2], [3]),
	max(3, 1L, 2.5), min('b', 'a'), spyTry(lambda: max([])), apply(divmod, (7, 3)), tuple([1, 2]), list('abc')))

# marshal: the bytes and the round trip
spyCode = compile('def f(a, b=2):\n\treturn a * b + 1.5\nx = f(3)\n', 'spyfile', 'exec')
for spyV in (0, 1, -1, spyMax, spyMin, 2L ** 100, -(2L ** 100), 0L, 1.5, 0.1, 1e300, -0.0, 5e-324, 'abc', '',
		'\000\377' * 3, (), (1, 'a', 2.5), [1, [2, [3]]], {'a': 1}, {1: 'b', 'c': (2,)}, None, 1 + 2j, -1.5e-300j,
		spyCode, compile('1 / 0', 'spyerr', 'eval'), spyInf, spyNan):
	check('marshal %s' % repr(type(spyV)), lambda v=spyV: marshal.dumps(v))
	check('marshal loads %s' % repr(type(spyV)), lambda v=spyV: spyTry(lambda v=v: marshal.loads(marshal.dumps(v)) == v))
check('marshal loads str', lambda: (repr(marshal.loads(marshal.dumps(spyInf))), repr(marshal.loads(marshal.dumps(spyNan))),
	repr(marshal.loads('f\0031e5')), repr(marshal.loads('f\004-nan')), repr(marshal.loads('f\0101.#INF000'))))
for spyS in ('', 'q', 'i\001\002', 'f\005abc', 'l\002\000\000\000\001\000', 's\003\000\000\000ab', '(\002\000\000\000i',
		'{i\001\000\000\000', 'c'):
	check('marshal.loads(%s)' % repr(spyS), lambda s=spyS: spyTry(lambda s=s: marshal.loads(s)))
spyD = {}
exec spyCode in spyD
check('marshal code run', lambda: (spyD['x'], spyD['f'](2, 3), spyD['f'].func_code.co_name, spyD['f'].func_code.co_varnames,
	spyD['f'].func_code.co_consts, spyD['f'].func_code.co_code, spyD['f'].func_code.co_firstlineno))

# regex
for spyP, spyS in (('a(b*)c', 'xabbbcy'), ('a\\(b*\\)c', 'xabbbcy'), ('[0-9]+', 'abc 123 def'), ('^ab', 'xab'),
		('x*', 'aaa'), ('\\(a\\|b\\)+', 'cabbad'), ('[^a-z]', 'abcD'), ('a.c', 'a\nc'), ('\\bword\\b', 'a word here'),
		('\\(\\w+\\) \\1', 'hello hello'), ('[]a]', ']'), ('a\\{2\\}', 'aaa'), ('\\<b', 'a b'), ('\\(', 'x'),
		('a\\)', 'x'), ('[a-', 'x'), ('*a', '*a'), ('\\(\\(\\(a\\)\\)\\)', 'a'), ('', '')):
	check('regex %s %s' % (repr(spyP), repr(spyS)), lambda p=spyP, s=spyS: spyTry(lambda p=p, s=s: (regex.match(p, s),
		regex.search(p, s))))
	check('regex groups %s %s' % (repr(spyP), repr(spyS)), lambda p=spyP, s=spyS: spyTry(lambda p=p, s=s: (lambda r, s=s:
		(r.search(s), r.regs, r.group(0), r.last, r.givenpat, r.realpat))(regex.compile(p))))
check('regex symcomp', lambda: spyTry(lambda: (lambda r: (r.match('ab12'), r.group('word'), r.group('num'), r.groupindex))(
	regex.symcomp('\\(<word>[a-z]+\\)\\(<num>[0-9]+\\)'))))
check('regex casefold', lambda: spyTry(lambda: regex.compile('abc', regex.casefold).search('xABCx')))
spyOld = regex.set_syntax(0)
regex.set_syntax(spyOld)
check('regex syntax', lambda: spyOld)

# math at its edges: the C runtime is ucrtbase in both, so these should match bit for bit
for spyName in ('sqrt', 'exp', 'log', 'log10', 'sin', 'cos', 'tan', 'asin', 'acos', 'atan', 'sinh', 'cosh', 'tanh',
		'floor', 'ceil', 'fabs', 'frexp', 'modf'):
	for spyA in (0.0, -0.0, 1.0, -1.0, 0.5, 2.0, -2.0, 1.5707963267948966, 3.141592653589793, 10.0, 100.0, 709.0,
			710.0, -745.0, -746.0, 1e22, 1e300, -1e300, 1e-310, 5e-324, spyInf, -spyInf, spyNan):
		spyF = getattr(math, spyName)
		if spyName == 'frexp' and spyA in (spyInf, -spyInf):
			# the exponent of an infinity is unspecified (stack garbage): the mantissa only
			spyF = lambda a, f=spyF: f(a)[0]
		check('math.%s(%.17g)' % (spyName, spyA), lambda f=spyF, a=spyA: spyTry(lambda f=f, a=a: f(a)))
for spyName in ('pow', 'atan2', 'fmod', 'hypot', 'ldexp'):
	for spyA in (0.0, -0.0, 1.0, -1.0, 2.0, -8.0, 10.0, 1e308, 1e-310, spyInf, -spyInf, spyNan):
		for spyB in (0.0, -0.0, 1.0, -1.0, 0.5, 3.0, 1.0 / 3.0, 1024.0, -1080.0, 1e308, spyInf, spyNan):
			check('math.%s(%.17g, %.17g)' % (spyName, spyA, spyB), lambda f=getattr(math, spyName), a=spyA, b=spyB:
				spyTry(lambda f=f, a=a, b=b: f(a, b)))
check('math consts', lambda: (math.pi, math.e, spyTry(lambda: math.sqrt('x')), spyTry(lambda: math.ldexp(1.0, 1.5))))

# exceptions and their messages
def spyExc(code):
	try:
		exec code in {}
	except:
		return 'exception %s: %s' % (sys.exc_type, sys.exc_value)
	return 'no exception'
for spyS in ('1 / 0', '1.0 / 0', '1 % 0', '1L / 0', 'divmod(1, 0)', '[][1]', '{}["x"]', '"a" + 1', 'len(1)',
		'spyundefined', 'getattr(1, "x")', '(1, 2)[5]', 'x = []; x.remove(1)', 'chr(300)', 'raise "custom"',
		'raise ValueError, "arg"', 'raise ValueError, ("a", 2)', 'import spynomodule', 'from math import spynone',
		'"%d" % "x"', 'int("x")', 'float("x")', 'x = 1; x()', '[1].foo', 'None.x', 'def f(a): pass\nf()',
		'def f(a): pass\nf(1, 2)', 'def f(a): pass\nf(b=1)', 'def f(a): pass\nf(1, a=2)', 'class C: pass\nC().x',
		'class C:\n\tdef m(self): pass\nC.m()', 'del spyundef', 'a, b = 1, 2, 3', 'a, b = 1', 'x = 1; del x; x',
		'def f():\n\tx\n\tx = 1\nf()', '[1, 2] + (3,)', '1 < 2 < "a"', 'assert 0, "msg"', 'assert 0', '-"a"',
		'~1.5', '1 << 1.5', '"abc" * 1.5', '[] * []', 'hash({})', 'x = {}; x[[]] = 1', 'coerce(1, "a")',
		'abs("a")', 'apply(1)', 'reduce(None, [])', 'reduce(lambda a, b: a, [])', 'map()', 'range(1.5)',
		'xrange(10)[20]', 'open("spynofile.txt")', 'long("x")', 'pow(2, 3, 0)', 'pow(2.0, 3, 1)', '2 ** -1',
		'0.0 ** -1', '(-8.0) ** (1.0 / 3)', '10.0 ** 400', 'tuple(1)', 'list(1)', 'int([])', 'float(None)',
		'complex("x")', 'compile("x", "f", "bad")', 'eval(1)', 'exec 1', 'globals()[1]', 'vars(1)', 'cmp(1)',
		'"abc"[1:2] = "x"', '"abc".x', 'import string; string.atoi("1", 99)', 'import strop; strop.join(1)',
		'import marshal; marshal.dumps(len)', 'import regex; regex.compile(1)', 'import math; math.log(0)', 'import math; math.sqrt(-1)', 'import math; math.exp(1000)',
		'import math; math.acos(2)', 'import math; math.pow(0, -1)', 'import math; math.ldexp(1, 2000)', 'import math; math.fmod(1, 0)'):
	check('exc %s' % repr(spyS), lambda s=spyS: spyExc(s))

# eval and exec of code strings
spyD = {}
exec 'x = 5\ny = x * 2\ndef g(n):\n\tif n < 2: return n\n\treturn g(n - 1) + g(n - 2)\nz = g(15)\n' in spyD
check('exec', lambda: (spyD['x'], spyD['y'], spyD['z']))
check('eval misc', lambda: (eval('1 + 2 * 3'), eval('x * 3', {'x': 4}), eval('y', {}, {'y': 5}), eval('(lambda a: a + 1)(2)'),
	eval('[1, 2][1:]'), eval('{"a": (1, 2)}'), eval('`1.1`'), eval('-1 ** 2'), eval('not 1 or 2 and 3'),
	eval('2 < 3 < 4'), eval('"%s" % 0.1'), eval(' 1'), eval('1\n')))
spyD = {}
exec compile('a = 1\nb = a + 1\n', 'spyexec', 'exec') in spyD
check('exec compiled', lambda: (spyD['a'], spyD['b']))

# compile errors: message, file name, line, column and text
for spyS in ('a = (', 'if 1:\nx', 'def f(:', 'x = 1 +', '\tx = 1\n  y = 2', 'x =\n', 'return 1', 'break',
		'continue', 'def f(a, a): pass', 'a b', '1 +* 2', '"unterminated', '"""abc', 'f(x=1, 2)', 'del 1',
		'[x for x in y]', 'x += 1', 'print >>f, x', 'if 1:\n  a\n b\n', 'a = 1\n\n\n  b = 2\n', 'def f():\n\treturn\n\tyield 1',
		'1 = 2', 'f() = 1', '(a, 1) = 2', 'lambda: (yield)', 'class C(: pass', 'x = [1, 2', 'x = {1: 2', 'x = 1)',
		'for in x: pass', 'while 1\n\tpass', 'try:\n\tpass\n', 'try:\n\tpass\nfinally:\n\tpass\nexcept:\n\tpass',
		'def f(*a, b): pass', 'def f(a=1, b): pass', 'def f(**a, *b): pass', 'import', 'from x import', 'global',
		'exec', 'x = 0777777777777777777777', 'x = 99999999999999999999', 'x = 0xfffffffff', '$', 'x = `1',
		'if 1:\n\tif 2:\n\t\tpass\n\t  else:\n', 'a = 1;;', '@', '\\', 'x = 1\\\n+ 2\n', 'x = """\n\n\n', 'def f():\n',
		'  x = 1', 'x\n  y', 'a.1', '1.e', '1e+', '0x', '1L.x', 'not', 'f(**)', 'f(a for a in b)'):
	for spyMode in ('exec', 'eval', 'single'):
		check('compile %s %s' % (repr(spyS), spyMode), lambda s=spyS, m=spyMode: spyTry(lambda s=s, m=m:
			type(compile(s, 'spyfile', m)).__name__))

# the modules of the module table that hold plain values
import errno, signal
# the codes of errno are the platform's (Windows has the Winsock ones too): only the common ones
check('errno', lambda: (errno.ENOENT, errno.EACCES, errno.EEXIST, errno.EINVAL, errno.ERANGE, errno.EDOM))
# NSIG is the platform's (23 on Windows, 65 on Linux)
check('signal', lambda: (signal.SIGINT, signal.SIGTERM, signal.SIG_DFL, signal.SIG_IGN))
check('sys', lambda: (sys.maxint, sys.version, sys.platform, sys.builtin_module_names, sys.hexversion))
check('builtins', lambda: dir(__builtins__))
