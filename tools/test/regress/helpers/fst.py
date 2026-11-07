import Scrap, SAct, Init
def _ent(n, vs):
	e = Scrap.GetEntity(n)
	r = []
	for v in vs:
		try:
			r.append((v, getattr(e, v)))
		except:
			r.append((v, '?'))
	return r
def _gv(vs):
	return map(lambda v: (v, Scrap.Get(v)), vs)
def _mod(m, vs):
	r = []
	mod = __import__(m)
	for v in vs:
		try:
			r.append((v, `getattr(mod, v)`))
		except:
			r.append((v, '?'))
	return r
def _cls(c, v):
	SAct.GetClass(c)
	return SAct.GetCls(v)
Init._ent = _ent
Init._gv = _gv
Init._mod = _mod
Init._cls = _cls
def _items():
	n = []
	def c(x, n=n):
		n.append(x)
	Init._itemcollect = c
	Scrap.SphereCall(0.0, 0.0, 0.0, 300000.0, 'i', 'Init._itemcollect')
	n.sort()
	return n
Init._items = _items
def _veh():
	n = []
	def c(x, n=n):
		n.append(x)
	Init._vehcollect = c
	Scrap.SphereCall(0.0, 0.0, 0.0, 300000.0, 'v', 'Init._vehcollect')
	n.sort()
	r = []
	for x in n:
		e = Scrap.GetEntity(x)
		row = [x]
		for v in ('Model', 'FxClass', 'ShipMotorColorR', 'ShipMotorColorG', 'ShipMotorColorB', 'ShipMotorScale', 'ShipMotorOffset'):
			try:
				row.append(getattr(e, v))
			except:
				row.append('?')
		r.append(row)
	return r
Init._veh = _veh
def _itemvars():
	r = []
	for x in _items():
		e = Scrap.GetEntity(x)
		row = [x]
		for v in ('NetRes', 'Radius', 'TakeSound', 'TakeSoundVol', 'OnTake', 'rTime', 'Owner', 'OnReset', 'BackTime'):
			try:
				row.append(getattr(e, v))
			except:
				row.append('?')
		r.append(row)
	return r
Init._itemvars = _itemvars
