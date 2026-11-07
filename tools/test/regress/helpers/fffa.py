# helpers of the Ffa cases (round 8a): recorders of the network, HUD and menu functions, a call with the scheduler
# recorded (with the schedule names), state dumps. exec'd into Init's dict.
import Scrap, sys
_fr = []
def _fn_(x):
	t = type(x)
	if t == type(()):
		return tuple(map(_fn_, x))
	if t == type([]):
		return map(_fn_, x)
	if t == type({}):
		ks = x.keys()
		ks.sort()
		return map(lambda k, x=x: (k, _fn_(x[k])), ks)
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
class _frec:
	def __init__(self, n, ret=None):
		self.n = n
		self.ret = ret
	def __call__(self, *a, **kw):
		if self.n in ('SetMsgText', 'SetLabelText') and len(a) == 2 and type(a[1]) == type(1.0):
			a = (a[0], 'now+%.1f' % (a[1] - Scrap.GetTime()))
		if kw:
			_fr.append((self.n, _fn_(a), _fn_(kw)))
		else:
			_fr.append((self.n, _fn_(a)))
		return self.ret
def _fsch(t, fn, args, name=None):
	_fr.append(('sched', round(t - Scrap.GetTime(), 2), _fn_(fn), _fn_(args), name))
_fsaved = {}
def _fset(mod, name, v):
	m = __import__(mod)
	k = (mod, name)
	if not _fsaved.has_key(k):
		_fsaved[k] = getattr(m, name)
	setattr(m, name, v)
def _fput(mod, name):
	setattr(__import__(mod), name, _fsaved[(mod, name)])
def _fon():
	for mn in (('SNet', 'SendUsrString'), ('SNet', 'ModifyUsrData'), ('Scrap', 'ConsoleError'), ('SScorer', 'SetMsgText'),
		('SScorer', 'SetLabelText'), ('SScorer', 'SetMPFunc'), ('SScorer', 'Fade'), ('SInput', 'Bind'),
		('Scrap', 'DeleteScheduledFuncs'), ('Vehicles', 'OnDeath'), ('Vehicles', 'OnClientDeath'),
		('RacerScorer', 'ShowScorer'), ('RacerScorer', 'Create'), ('RacerScorer', 'MissionScorer'),
		('RacerScorer', 'MissionScorerTeam'), ('Menu', 'BackToGame'), ('Menu', 'StartNewMenu'), ('Menu', 'VerticalMenu'),
		('Menu', 'DrawBackOptionMenu'), ('Menu', 'ShowWarning'), ('SScorer', 'Set')):
		_fset(mn[0], mn[1], _frec(mn[1]))
	return 1
# the net flags: (client, server, dedicated)
def _fnet(c, s):
	import SNet
	_fset('Scrap', 'GetNetFlags', lambda c=c, s=s: (c, s, 0))
	_fset('SNet', 'IsClient', lambda c=c: c)
	_fset('SNet', 'IsServer', lambda s=s: s)
	return Scrap.GetNetFlags()
def _fnetoff():
	for mn in (('Scrap', 'GetNetFlags'), ('SNet', 'IsClient'), ('SNet', 'IsServer')):
		if _fsaved.has_key(mn):
			_fput(mn[0], mn[1])
	return Scrap.GetNetFlags()
def _f(f, *a, **kw):
	global _fr
	_fr = []
	old = Scrap.AddScheduledFunc
	Scrap.AddScheduledFunc = _fsch
	try:
		try:
			v = _fn_(apply(f, a, kw))
		except:
			v = 'error: %s: %s' % (sys.exc_type, sys.exc_value)
	finally:
		Scrap.AddScheduledFunc = old
		Scrap.SetTimeSpeed(1)
	return (v, _fr)
def _fst():
	F = __import__('Ffa')
	R = __import__('RacerScorer')
	M = __import__('Menu')
	P = __import__('PCMenu')
	import SInput
	c = Scrap.GetEntity('NetCam')
	r = []
	for v in ('Scorer', 'GameScorer', 'YourPosition', 'YourScore', 'Winner', 'ScorerFraglimit', 'GameStart', 'isScorerVisible',
		'SpectatorMode', 'GameFinished', 'TotalRed', 'TotalGreen', 'MasterFlag'):
		r.append((v, _fn_(getattr(F, v, '?'))))
	r.append(('Top', R.ScorerTopMessage, 'Bot', R.ScorerBotMessage, 'Menu', M.MultiplayerMenu, 'PCMenu', P.MultiplayerMenu))
	if c:
		r.append(('cam', c.MainTarget, c.FreeCam, SInput.GetActionSet()))
	return r
def _fent(names):
	r = []
	for n in names:
		e = Scrap.GetEntity(n)
		if not e:
			r.append((n, None))
			continue
		row = [n]
		for v in ('Active', 'TgType', 'Descriptor', 'UsrControl', 'Model', 'ActCtrl'):
			try:
				row.append(_fn_(getattr(e, v)))
			except:
				row.append('?')
		r.append(row)
	return r
def _fcfg(name):
	return (Scrap.Get('PlayerModel'), Scrap.Get('PlayerMaxLife'), name, Scrap.Get('PilotModel'), Scrap.Get('Motor0Model'),
		Scrap.Get('Motor1Model'), Scrap.Get('Motor2Model'), Scrap.Get('Motor3Model'), Scrap.Get('WeaponBayList'), 0)
def _fgfs():
	import OutMap
	OutMap.SpawRes = 'net256'
	return ((100.0, 200.0, 300.0), 45.0)
