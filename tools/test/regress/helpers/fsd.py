# helpers of the super deal cases (round 4): names instead of object reprs, recorders that keep the arguments, a
# call with the scheduler recorded in the same line, state dumps
import Scrap, sys
_sdr = []
def _sdn(x):
	t = type(x)
	if t == type(()):
		return tuple(map(_sdn, x))
	if t == type([]):
		return map(_sdn, x)
	if t == type({}):
		d = {}
		for k in x.keys():
			d[k] = _sdn(x[k])
		return d
	if t == type(1.0):
		return round(x, 2)
	if str(t) == "<type 'cPyEntity'>":
		try:
			return 'ent:' + x.Name
		except:
			return 'ent:?'
	if t != type('') and hasattr(x, '__name__'):
		return 'fn:' + x.__name__
	if str(t) == "<type 'instance'>":
		return 'inst:' + x.__class__.__name__
	return x
class _sdrec:
	def __init__(self, n, ret=None):
		self.n = n
		self.ret = ret
	def __call__(self, *a, **kw):
		_sdr.append((self.n, _sdn(a), _sdn(kw)))
		return self.ret
def _sdsch(t, fn, args, name=None):
	_sdr.append(('sched', round(t - Scrap.GetTime(), 2), _sdn(fn), _sdn(args)))
def _sd(f, *a, **kw):
	global _sdr
	_sdr = []
	old = Scrap.AddScheduledFunc
	Scrap.AddScheduledFunc = _sdsch
	try:
		try:
			v = _sdn(apply(f, a, kw))
		except:
			v = 'error: %s: %s' % (sys.exc_type, sys.exc_value)
	finally:
		Scrap.AddScheduledFunc = old
		Scrap.SetTimeSpeed(1)
	return (v, _sdr)
def _sdon():
	import Speech, MissionsFuncs, SaveGame, RaceMaker
	Speech.AddRemoteCom = _sdrec('AddRemoteCom')
	Speech.ForceRemoteCom = _sdrec('ForceRemoteCom')
	MissionsFuncs.OnSuperDealFinish = _sdrec('OnSuperDealFinish')
	SaveGame.Restart = _sdrec('Restart')
	RaceMaker.ShowSystemMessage = _sdrec('ShowSystemMessage')
	return 1
def _sdmod(m, vs):
	mod = __import__(m)
	r = []
	for v in vs:
		if mod.__dict__.has_key(v):
			r.append((v, _sdn(mod.__dict__[v])))
		else:
			r.append((v, '-'))
	return r
_sdev = ('TgType', 'EntityList', 'Invulnerable', 'Active', 'LLogicIsLeader', 'RadarShow', 'OnTake', 'Money', 'LaunchTaunts', 'WeapList')
def _sdent(names, vs=_sdev):
	r = []
	for n in names:
		e = Scrap.GetEntity(n)
		if not e:
			r.append((n, None))
			continue
		row = [n]
		for v in vs:
			try:
				row.append(_sdn(getattr(e, v)))
			except:
				row.append('?')
		r.append(tuple(row))
	return r
def _sdhooks():
	import Parking, Chars, Vehicles, Menu, MissionsFuncs
	return _sdn((Parking.SwitchVehicleMissionCallback, Parking.SwitchCharMissionCallback, Parking.OnCreatePlayerShip,
		Chars.OnCreateMainChar, Chars.OnMainCharDeath, Vehicles.OnDeathCallback, Vehicles.DelPlayerShipOnDeath,
		Menu.MissionScorerCall, MissionsFuncs.cameraData))
def _sdlist(l):
	r = []
	e = Scrap.GetEntity(Scrap.EntityListGet(l, 'First'))
	while e:
		r.append(e.Name)
		e = Scrap.GetEntity(e.NextInList)
	r.sort()
	return r
def _sdcfn(m):
	mod = __import__(m)
	n = 0
	c = 0
	for k in mod.__dict__.keys():
		v = mod.__dict__[k]
		if type(v) == type(len):
			c = c + 1
		elif type(v) == type(_sdn):
			n = n + 1
	return (c, n)
def _sdfreeze(names):
	# the ships static and invulnerable, so the AI takes no flags and kills nobody between the lines
	import SAI
	for n in names:
		e = Scrap.GetEntity(n)
		if e:
			e.Invulnerable = 1
			SAI.SetStateVehicle(0, n)
	return len(names)
def _sdallitems():
	import Init
	n = []
	def c(x, n=n):
		n.append(x)
	Init._sdcollect = c
	Scrap.SphereCall(0.0, 0.0, 0.0, 300000.0, 'i', 'Init._sdcollect')
	n.sort()
	return n
def _sditems(prefix):
	r = []
	for n in _sdallitems():
		if n[:len(prefix)] == prefix:
			r.append(n)
	return _sdent(r, ('OnTake', 'RadarShow', 'Radius', 'rTime', 'BackTime', 'OnReset', 'Active'))
