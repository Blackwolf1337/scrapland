import Scrap, SScorer, SAct
_calls = []
class _Rec:
	def __init__(self, name, ret):
		self.name = name
		self.ret = ret
	def __call__(self, *a, **kw):
		_calls.append((self.name, a, kw))
		return self.ret
def _rec(mod, func, ret=None):
	setattr(__import__(mod), func, _Rec(mod + '.' + func, ret))
def _takecalls():
	r = _calls[:]
	del _calls[:]
	return r
def _sc(items, attrs=('X','Y','W','H','Red','Green','Blue','Alpha','Text','Visible','OnAccept','Hint','SizeX','SizeY','Font','Type','File')):
	r = []
	for i in items:
		v = []
		for a in attrs:
			try:
				v.append(SScorer.Get(0, i, a))
			except:
				v.append('?')
		r.append((i, v))
	return r
def _el(name):
	try:
		return Scrap.EntityListGet(name, 'Num')
	except:
		return '?'
def _entl(name, vs):
	r = []
	e = Scrap.GetEntity(Scrap.EntityListGet(name, 'First'))
	while e:
		r.append(_ent(e.Name, vs))
		e = Scrap.GetEntity(e.NextInList)
	return r
def _prof(k):
	import VehiclesNPC
	p = VehiclesNPC.AIProf[k]
	d = p.__dict__.items()
	d.sort()
	return d
def _cldump(names):
	import string
	SAct.DumpEvents('misev.txt')
	r = []
	on = 0
	for l in open('misev.txt').readlines():
		if l[:6] == 'class ':
			on = string.strip(l[6:]) in names
		if on:
			r.append(string.rstrip(l))
	return r
