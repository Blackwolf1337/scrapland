# helpers for the ship editor tab cases (round 5b): scorer items, module variables, the edited ship, save variables
_ta = ('Visible','X','Y','W','H','Up','Down','Left','Right','Hint','OnAccept','OnGainFocus','OnLooseFocus','OnRender','MaxHintLineWidth','Alpha','Red','Green','Blue','SpriteIndex','FocusSpriteIndex','Highlight','Text','ScaleX','PivotX','PivotY','Rotate','Mirror','File','Type','Value','MinValue','MaxValue','MaxValueForced','TextColorRed','TextColorGreen','TextColorBlue','MinX','MultiPress','Unit')
def _tfn(v):
	if type(v) == type('') and v[:11] in ('EnginesTab.', 'WeaponsTab.'):
		return v[11:]
	if type(v) == type('') and v[:8] == 'HullTab.':
		return v[8:]
	return v
def _tget(n, a):
	import SScorer
	try:
		v = SScorer.Get(0, n, a)
	except:
		return '?'
	if a[:2] == 'On':
		return _tfn(v)
	if type(v) == type(0.0):
		return round(v, 3)
	return v
def _tpre(n, ps):
	for p in ps:
		if n[:len(p)] == p:
			return 1
	return 0
_tsk = ('Tab_',)
def _ti(ps, attrs=None):
	import SScorer
	if attrs is None:
		attrs = _ta
	r = []
	for n in SScorer.Items(0):
		if not _tpre(n, ps):
			continue
		row = [n]
		for a in attrs:
			v = _tget(n, a)
			if v is not None:
				row.append((a, v))
		r.append(tuple(row))
	return r
_tE = ('Engine', 'IconEngine', 'Recycle', 'PrevEngine', 'NextEngine', 'SPoli', 'SMerc', 'SMayor', 'SBank', 'SBishop', 'SArch', 'SFunc', 'SBerto', 'SBetty', 'SHump', 'SBoss', 'SLife')
_tW = ('Weapon', 'Vulcan', 'Devastator', 'Tesla', 'ATPC', 'Inferno', 'Swarm', 'Upgrade', 'Slot', 'SlotsBox', 'UpgradesBox')
_tH = ('Hull',)
_tC = ('MoneyPredict', 'Money', 'Weight', 'Boost', 'Speed', 'MaxSpeed', 'Exit', 'ShipEditTab')
def _tord():
	import SScorer
	return SScorer.Items(0)
def _tm():
	r = []
	for m, vs in (('EnginesTab', ('enginesList','canBeMounted','engineIndex','currentEngineGroup','nEngSlots')), ('WeaponsTab', ('controlsToLinkList','prevWeapon')), ('ShipEdit', ('prevTab',)), ('HangarTab', ('currentHangar','prevItem'))):
		mod = __import__(m)
		for v in vs:
			try:
				r.append((m, v, getattr(mod, v)))
			except:
				r.append((m, v, '?'))
	return r
_tsv = ['Name','Model','AcMass','MaxLife','MinLife','Life','nSlot00','nSlot01','nSlot02']
for _i in range(4):
	_tsv = _tsv + ['MEng%d' % _i, 'nEng%d' % _i, 'NEng%d' % _i]
for _w in ('00','01','02','03','04','05'):
	_tsv = _tsv + ['Upgrd' + _w, 'UAmmo' + _w]
	for _s in 'ABCD':
		_tsv.append('Weap' + _s + _w)
def _tship():
	import HangarTab
	s = HangarTab.prevShip
	if s is None:
		return None
	r = []
	for v in _tsv:
		try:
			x = getattr(s, v)
			if type(x) == type(0.0):
				x = round(x, 3)
			r.append((v, x))
		except:
			r.append((v, '?'))
	return r
def _thv():
	import Scrap
	r = [Scrap.GetMoney()]
	for v in ('Hangar.iHangarShip', 'Hangar.DestroyedShips', 'Hangar.availableEnginesList'):
		r.append((v, Scrap.GetSaveVar(v)))
	for i in range(1, 10):
		r.append(Scrap.GetSaveVar('Hangar.HangarShip%d' % i))
	r.append(Scrap.GetSaveVar('Hangar.HangarShipAux'))
	return r
def _tall(ps):
	return (_tm(), _tship(), _thv(), _ti(ps), _ti(_tC))
