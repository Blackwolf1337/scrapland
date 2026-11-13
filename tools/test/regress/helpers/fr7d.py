# round 7d (level scripts of Scrapyard and GDB): helpers exec'd into __main__ after fsd.py and fr6b.py, again after
# every level load
import Scrap, sys
def _m(n):
	return sys.modules[n]
def _un():
	return Scrap.UsrEntity(0).Name
_YEV = ('TgType', 'EntityList', 'Money', 'Life', 'MaxLife', 'MinLife', 'OnDeath', 'OnDamage', 'Radius', 'TgTypeMarker', 'OnTake',
	'rTime', 'OnlyUser', 'Active', 'CamTgt', 'Invulnerable', 'UseFunc', 'ArrOpt0', 'ArrnOpt', 'ArrowCanUse', 'VRadius',
	'ActiveCMask', 'Ammo00', 'MPil0', 'LaunchTaunts', 'CanPhoto', 'AppearsIfFree', 'AutoFace', 'MoveTime', 'RadarShow')
def _ye(names, vs=_YEV):
	return _sdent(names, vs)
def _ypos(names):
	# positions (rounded to 10) of entities
	r = []
	for n in names:
		e = Scrap.GetEntity(n)
		if e:
			p = e.Pos
			r.append((n, int(p[0] / 10), int(p[1] / 10), int(p[2] / 10)))
		else:
			r.append((n, None))
	return r
def _ynew(lst, before):
	# names of list lst not in before
	return filter(lambda n, b=before: n not in b, _sdlist(lst))
def _ymods(m, vs):
	return _sdmod(m, vs)
def _ylair(d):
	k = d.keys()
	k.sort()
	return map(lambda k, d=d: (k, int(d[k][0] / 10), int(d[k][1] / 10), int(d[k][2] / 10)), k)
