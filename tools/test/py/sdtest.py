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
	out.flush() # a crash in the game must not lose the lines before it

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
# SWeap: in the menu both tables are empty; Init may run only once (CHECK(!p) in Table.h), so only then.
# An index out of range stops the original on int3, so none of those here.
check('SWeap.GetNWeap()', lambda: SWeap.GetNWeap())
check('SWeap.GetNAmmo()', lambda: SWeap.GetNAmmo())
check('SWeap.GetFirstiWeap()', lambda: SWeap.GetFirstiWeap())
check('SWeap.InitWeap(0)', lambda: SWeap.InitWeap(0))
check('SWeap.InitWeap(-5)', lambda: SWeap.InitWeap(-5))
if SWeap.GetNWeap() == 0:
	check('SWeap.InitWeap(3)', lambda: SWeap.InitWeap(3))
if SWeap.GetNAmmo() == 0:
	check('SWeap.InitAmmo(2)', lambda: SWeap.InitAmmo(2))
check('SWeap.GetNWeap() after', lambda: SWeap.GetNWeap())
check('SWeap.GetNAmmo() after', lambda: SWeap.GetNAmmo())
wfloat = ('MinAmmo', 'iPrev', 'iNext', 'iAmmo', 'Upgrd00', 'Upgrd05', 'upgrd07', 'Upgrd', 'Upgrd1x', 'Bogus')
wstr = ('Name', 'OnPress', 'OnRelease', 'OnFire', 'OnHit', 'OnStart', 'OnEnd', 'SetFx', 'Bogus')
for i in range(SWeap.GetNWeap()):
	for v in wfloat:
		check('SWeap.GetFWeap(%d, %s)' % (i, v), lambda i=i, v=v: SWeap.GetFWeap(i, v))
	for v in wstr:
		check('SWeap.GetSWeap(%d, %s)' % (i, v), lambda i=i, v=v: SWeap.GetSWeap(i, v))
for v in ('MinAmmo', 'iPrev', 'iNext', 'iAmmo', 'Bogus'):
	check('SWeap.SetFWeap(1, %s, 2.5)' % v, lambda v=v: SWeap.SetFWeap(1, v, 2.5))
	check('SWeap.GetFWeap(1, %s) set' % v, lambda v=v: SWeap.GetFWeap(1, v))
for v in wstr:
	check('SWeap.SetSWeap(2, %s)' % v, lambda v=v: SWeap.SetSWeap(2, v, 'val_' + v))
	check('SWeap.GetSWeap(2, %s) set' % v, lambda v=v: SWeap.GetSWeap(2, v))
for i in range(SWeap.GetNAmmo()):
	for v in ('Max', 'Charger', 'max', 'Bogus'):
		check('SWeap.GetFAmmo(%d, %s)' % (i, v), lambda i=i, v=v: SWeap.GetFAmmo(i, v))
	for v in ('Name', 'OnPress', 'OnRelease', 'OnFire', 'OnHit', 'OnStart', 'Bogus'):
		check('SWeap.GetSAmmo(%d, %s)' % (i, v), lambda i=i, v=v: SWeap.GetSAmmo(i, v))
check('SWeap.SetFAmmo(0, Max, 99.75)', lambda: SWeap.SetFAmmo(0, 'Max', 99.75))
check('SWeap.SetFAmmo(0, Bogus, 1)', lambda: SWeap.SetFAmmo(0, 'Bogus', 1))
check('SWeap.GetFAmmo(0, Max) set', lambda: SWeap.GetFAmmo(0, 'Max'))
check('SWeap.SetSAmmo(1, Name)', lambda: SWeap.SetSAmmo(1, 'Name', 'Bullets'))
check('SWeap.GetSAmmo(1, Name) set', lambda: SWeap.GetSAmmo(1, 'Name'))
check('SWeap.SetSAmmo(1, Bogus)', lambda: SWeap.SetSAmmo(1, 'Bogus', 'x'))
check('SWeap.SetSAmmo(5, Name)', lambda: SWeap.SetSAmmo(5, 'Name', 'x'))
# SWeap.FillPriority() crashes the original too with tables made here but not filled by the weapon scripts
check('SWeap.GetFirstiWeap() after', lambda: SWeap.GetFirstiWeap())
check('SWeap.NetExec(Unknown)', lambda: SWeap.NetExec('Unknown'))
check('SWeap.NetExec(Unknown, x)', lambda: SWeap.NetExec('Unknown', 'x'))
check('SWeap.NetExec bad', lambda: SWeap.NetExec(1))
check('SWeap.GetFWeap bad', lambda: SWeap.GetFWeap('a', 'b'))
check('SWeap.SetSWeap bad', lambda: SWeap.SetSWeap(0, 'Name'))
out.write('done\n')
out.close()
