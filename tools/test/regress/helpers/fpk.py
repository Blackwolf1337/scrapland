# helpers for the Parking, ShipMaker, Sputnik and GateKeeper cases (round 4a)
import Scrap, SAct
exec(open('fsnd.py').read())
exec(open('fweap.py').read())
_SHIPV = ('Model', 'MPil0', 'ActCtrl', 'Mass', 'Style', 'Engine', 'MinLife', 'MaxLife', 'BoostTime', 'VerticalBalance',
	'MEng0', 'MEng1', 'MEng2', 'MEng3', 'NEng0', 'NEng1', 'NEng2', 'NEng3', 'EngAnmF0', 'EngAnmF1', 'EngAnmF2', 'EngAnmF3',
	'EngAnmM0', 'EngAnmM1', 'EngAnmM2', 'EngAnmM3', 'EngFxClass0', 'EngFxClass1', 'EngFxClass2', 'EngFxClass3', 'WeapList')
_PARKV = ('ArrowCanUse', 'ArrnOpt', 'ArrOpt0', 'UseFunc', 'UsrControl', 'TgType', 'EntityList', 'OnDeath', 'Life', 'Active',
	'ActiveCMask', 'MoveTime', 'WRadius', 'VRadius', 'Descriptor', 'isWpn02', 'isWpn03', 'isWpn04', 'isWpn05')
def _rnd(x):
	if type(x) == type(0.0):
		return round(x, 3)
	if type(x) == type(()):
		return tuple(map(_rnd, x))
	return x
def _v(n, vs=None):
	vs = vs or _SHIPV
	e = Scrap.GetEntity(n)
	if not e:
		return (n, None)
	r = [n]
	for v in vs:
		try:
			r.append((v, _rnd(getattr(e, v))))
		except:
			r.append((v, '?'))
	return r
def _ships(vs=None):
	# every ship of the level, one entry per model and variables (the traffic and the parked ships pick models at random)
	d = {}
	for x in _vehs():
		row = _v(x, vs)
		k = `row[1:]`
		d[k] = 1
	k = d.keys()
	k.sort()
	return k
def _pk():
	P = __import__('Parking')
	r = []
	for v in ('PlayerShip', 'DtritusPlate', 'LastCamera', 'LastPark'):
		e = getattr(P, v, '?')
		r.append((v, e and e != '?' and e.Name or e))
	for v in ('SetWeaponOnShip', 'AllShips', 'ShipTypes', 'ParkingZones', 'LastParkingZone', 'SwitchCharMissionCallback',
		'SwitchVehicleMissionCallback', 'ParkedOutCallback', 'OnCreatePlayerShip'):
		r.append((v, getattr(P, v, '?')))
	r.append(('LocalShip', P.__dict__.has_key('LocalShip') and P.LocalShip.Name))
	return r
def _spt():
	S = __import__('Sputnik')
	r = []
	for v in ('spt', 'ply'):
		e = getattr(S, v, '?')
		r.append((v, e and e != '?' and e.Name or e))
	for v in ('LastPos', 'LastAng', 'LastHangar', 'UseParams'):
		r.append((v, _rnd(getattr(S, v, '?'))))
	if S.spt:
		r.append(_v(S.spt.Name, ('Pos', 'Ang', 'Action', 'Active', 'ActiveCMask', 'ArrowCanUse', 'ArrnOpt', 'ArrOpt0', 'UseFunc', 'MoveTime')))
	return r
def _cam():
	c = Scrap.GetEntity(Scrap.GetCam(0))
	r = [Scrap.GetCam(0)]
	for v in ('Target', 'MainTarget', 'EntityLink', 'CamDist', 'nWayPoint', 'OnEnd', 'TgtCam00', 'TgtCam01', 'TgtCam03', 'Time00', 'FadeIn00', 'FadeOut00', 'Pos00', 'Ang00', 'Pos01', 'Ang01', 'FOV01', 'Pos02', 'Ang02'):
		try:
			r.append((v, _rnd(getattr(c, v))))
		except:
			r.append((v, '?'))
	return r
def _usr():
	u = Scrap.UsrEntity(0)
	return u and _v(u.Name, ('ActCtrl', 'Model', 'MPil0', 'UsrControl', 'Active', 'TgType', 'EntityList', 'OnDeath', 'AcWeap', 'Descriptor', 'Pos'))
class _wrapT:
	# a function that stops the game clock (Menu.InitMenuSys(0)), with the clock running again after it
	def __init__(self, f):
		self.f = f
	def __call__(self, *a):
		try:
			return apply(self.f, a)
		finally:
			Scrap.SetTimeSpeed(1)
def _std(key, name):
	# a ship with the standard parts of key (Vehicles.SetStdProperties, which calls ShipMaker.SetShipParams)
	import Vehicles
	v = Scrap.GetEntity(name)
	if not v:
		p = Scrap.UsrEntity(0).Pos
		v = Scrap.CreateEntity(name, p[0] + 3000.0, p[1] + 3000.0, p[2], 'Car')
	Vehicles.SetStdProperties(v.Name, key, 'P_Police')
	return _v(v.Name, _SHIPV + ('AnmP0',))
def _its():
	# the items of the level by name (fweap.py replaces _items)
	n = []
	def c(x, n=n):
		n.append(x)
	__import__('Init')._itemcollect = c
	Scrap.SphereCall(0.0, 0.0, 0.0, 300000.0, 'i', 'Init._itemcollect')
	n.sort()
	return n
def _pk():
	P = __import__('Parking')
	r = []
	for v in ('PlayerShip', 'DtritusPlate', 'LastCamera', 'LastPark'):
		e = getattr(P, v, '?')
		try:
			r.append((v, e and e != '?' and e.Name or e))
		except:
			r.append((v, 'deleted'))
	for v in ('SetWeaponOnShip', 'AllShips', 'ShipTypes', 'ParkingZones', 'LastParkingZone', 'SwitchCharMissionCallback',
		'SwitchVehicleMissionCallback', 'ParkedOutCallback', 'OnCreatePlayerShip'):
		r.append((v, getattr(P, v, '?')))
	try:
		r.append(('LocalShip', P.__dict__.has_key('LocalShip') and P.LocalShip.Name))
	except:
		r.append(('LocalShip', 'deleted'))
	return r
