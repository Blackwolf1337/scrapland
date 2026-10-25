# Calls engine script functions and writes the results to sdtest.out (Python 1.5.2 syntax). tools/test/difftest.sh
# runs it with the C functions and with the originals and compares the two outputs. The game reads it from its
# root directory through -PythonExecute:execfile(...); the engine's command line drops quotes and splits at spaces,
# so difftest.sh spells the file name with chr().
import sys
import Scrap, SWeap, SVec

out = open('sdtest.out', 'w')

# floats with 17 digits: repr() of Python 1.5.2 shows only 12, which hides differences in the last bits.
# Python 1.5.2 has no string methods, hence no join.
def fmt(v):
	if type(v) == type(0.0):
		return '%.17g' % v
	if type(v) == type(()):
		r = ''
		for x in v:
			if r:
				r = r + ', '
			r = r + fmt(x)
		return '(' + r + ')'
	return repr(v)

def check(name, fn):
	try:
		r = fmt(fn())
	except:
		r = 'exception %s: %s' % (sys.exc_type, sys.exc_value)
	out.write('%s = %s\n' % (name, r))

check('Scrap.GetMoney()', lambda: Scrap.GetMoney())
check('Scrap.GetMoney(1)', lambda: Scrap.GetMoney(1))
check('Scrap.GetLevelPath()', lambda: Scrap.GetLevelPath())
check('Scrap.GetLevelPath("x")', lambda: Scrap.GetLevelPath("x"))
check('SWeap.GetNWeap()', lambda: SWeap.GetNWeap())
check('SWeap.GetNWeap(None)', lambda: SWeap.GetNWeap(None))
# SVec: vector math, compared to the last bit (fmt prints 17 digits)
inf = 1e308 * 10.0
nan = inf - inf
vecs = [(0.0, 0.0, 0.0), (1.0, 0.0, 0.0), (0.0, 1.0, 0.0), (1.0, 2.0, 3.0), (-3.5, 0.25, 7.125),
	(1e-7, 0.0, 0.0), (5e-7, 5e-7, 5e-7), (1234.5678, -9876.54321, 0.001), (0.1, 0.2, 0.3), (1e20, -1e20, 1e-20),
	(inf, 0.0, 0.0), (nan, 1.0, 1.0)]
for v in vecs:
	for name in ('Mod', 'ModSqr', 'Norm', 'GetRotAng'):
		check('SVec.%s%s' % (name, v), lambda f=getattr(SVec, name), v=v: f(v))
	check('SVec.GetRotAng%s flat' % (v,), lambda v=v: SVec.GetRotAng(v[0], v[1], v[2]))
	for f in (-2.5, 0.0, 3.0):
		check('SVec.Prod(%s, %s)' % (v, f), lambda v=v, f=f: SVec.Prod(v, f))
	for w in vecs[:10]:
		for name in ('Add', 'Sub', 'DProd', 'CProd', 'GetAngle', 'Rotate3D'):
			check('SVec.%s(%s, %s)' % (name, v, w), lambda f=getattr(SVec, name), v=v, w=w: f(v, w))
for a in (0.0, 45.0, 180.0, -180.0, 181.0, 359.999, 720.0, -1000.25, 1e10, inf, nan):
	check('SVec.NormAng(%s)' % a, lambda a=a: SVec.NormAng(a))
check('SVec.GetAngle((1,0,0), (1,0,0))', lambda: SVec.GetAngle((1, 0, 0), (1, 0, 0)))
check('SVec.GetAngle((1,0,0), (-1,0,0))', lambda: SVec.GetAngle((1, 0, 0), (-1, 0, 0)))
check('SVec.GetAngle((1,1,0), (2.000001,2,0))', lambda: SVec.GetAngle((1, 1, 0), (2.000001, 2, 0)))
check('SVec.Add bad', lambda: SVec.Add((1, 2), (3, 4, 5)))
check('SVec.Mod bad', lambda: SVec.Mod('abc'))
check('SVec.GetRotAng bad', lambda: SVec.GetRotAng(1, 2))
check('SVec.Prod bad', lambda: SVec.Prod((1, 2, 3)))
out.write('done\n')
out.close()
