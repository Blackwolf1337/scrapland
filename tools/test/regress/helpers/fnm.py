# helpers of the online mode cases (round 8b): one list of recorded calls (names instead of object reprs), recorders
# that return fixed values, a list of values in turn or run the real function, the scheduler recorded for one call
import Scrap, sys
exec(open('fsd.py').read())
_nr = []
class _NRec:
	def __init__(self, n, ret=None, fn=None, seq=None):
		self.n = n
		self.ret = ret
		self.fn = fn
		self.seq = seq
	def __call__(self, *a):
		_nr.append((self.n, _sdn(a)))
		if self.fn:
			return apply(self.fn, a)
		if self.seq:
			v = self.seq[0]
			if len(self.seq) > 1:
				del self.seq[0]
			return v
		return self.ret
_nsaved = {}
def _non(mod, func, ret=None, run=0, seq=None):
	m = __import__(mod)
	if not _nsaved.has_key((mod, func)):
		_nsaved[(mod, func)] = getattr(m, func)
	fn = None
	if run:
		fn = _nsaved[(mod, func)]
	setattr(m, func, _NRec(mod + '.' + func, ret, fn, seq))
	return 1
def _noff():
	for k, f in _nsaved.items():
		setattr(__import__(k[0]), k[1], f)
	_nsaved.clear()
	return 1
class _NRand:
	# Scrap.Rand(a, b) as a + frac * (b - a), recorded
	def __init__(self, frac):
		self.frac = frac
	def __call__(self, a, b):
		_nr.append(('Scrap.Rand', (a, b)))
		return a + self.frac * (b - a)
def _nrand(frac):
	if not _nsaved.has_key(('Scrap', 'Rand')):
		_nsaved[('Scrap', 'Rand')] = Scrap.Rand
	Scrap.Rand = _NRand(frac)
	return 1
def _nsch(t, fn, args, name=None):
	_nr.append(('sched', round(t - Scrap.GetTime(), 2), _sdn(fn), _sdn(args), name))
def _nm(f, *a):
	global _nr
	_nr = []
	old = Scrap.AddScheduledFunc
	Scrap.AddScheduledFunc = _nsch
	try:
		try:
			v = _sdn(apply(f, a))
		except:
			v = 'error: %s: %s' % (sys.exc_type, sys.exc_value)
	finally:
		Scrap.AddScheduledFunc = old
		Scrap.SetTimeSpeed(1)
	return (v, _nr)
def _nload(names):
	r = []
	Scrap.OpenPack('Scripts/Net')
	for n in names:
		Scrap.PreloadLibrary(n, n + '.pyc')
		m = __import__(n)
		r.append((n, filter(lambda k, m=m: callable(getattr(m, k)) and k[0] != '_', dir(m))))
	Scrap.ClosePack()
	return r
def _nent(n, vs):
	e = Scrap.GetEntity(n)
	if not e:
		return (n, None)
	r = [n]
	for v in vs:
		try:
			r.append((v, _sdn(getattr(e, v))))
		except:
			r.append((v, '?'))
	return r
def _nbots():
	# NDeathMatchBots has no .pyc in the packs (init.py would stop in PreloadLibrary): its source compiled into a
	# .pyc next to the game, which PreloadLibrary reads from disk as a file not in the pack index
	import marshal, imp
	src = open('ndeathmatchbots.py', 'rb').read()
	import string
	src = string.replace(src, '\r\n', '\n') + '\n'
	co = compile(src, 'Scripts/Net/NDeathMatchBots.py', 'exec')
	f = open('nmbots.pyc', 'wb')
	f.write(imp.get_magic() + '\0\0\0\0')
	marshal.dump(co, f)
	f.close()
	Scrap.PreloadLibrary('NDeathMatchBots', 'nmbots.pyc')
	m = __import__('NDeathMatchBots')
	return filter(lambda k, m=m: callable(getattr(m, k)) and k[0] != '_', dir(m))
def _nmod(m, vs):
	mod = __import__(m)
	r = []
	for v in vs:
		try:
			x = getattr(mod, v)
			if type(x) == type({}):
				k = x.keys()
				k.sort()
				x = map(lambda k, x=x: (k, _sdn(x[k])), k)
			r.append((v, _sdn(x)))
		except:
			r.append((v, '?'))
	return r
