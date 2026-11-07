# round 6b (level scripts of Commercial, GamblinDen, Orbit): recorders and state dumps, exec'd into __main__ after
# fsd.py (execfile), again after every level load
import Scrap, sys, string
def _bfn(m):
	# the functions of a module: C (builtin) or Python
	mod = sys.modules[m]
	k = mod.__dict__.keys()
	k.sort()
	r = []
	for n in k:
		v = mod.__dict__[n]
		if type(v) == type(len):
			r.append((n, 'C'))
		elif type(v) == type(_bfn):
			r.append((n, 'py'))
	return r
def _bsv(names):
	return map(lambda n: (n, Scrap.GetSaveVar(n)), names)
def _bgv(names):
	r = []
	for n in names:
		try:
			r.append((n, _sdn(Scrap.Get(n))))
		except:
			r.append((n, '?'))
	return r
_BEV = ('ActCtrl', 'UseFunc', 'Descriptor', 'LLogicDefaultState', 'LLogicDefaultTarget', 'LLogicState', 'LLogicTarget',
	'LLogicIdle', 'LLogicProbTalkWhenWalking', 'LLogicKeepWorking', 'ArrowCanUse', 'ArrowCanMission', 'RadarShow', 'Money',
	'OnObjSlot', 'Active', 'TgType', 'EntityList', 'OnDeath', 'OnDamage', 'Life', 'MaxLife', 'ArrnOpt', 'ArrOpt0', 'DefLoop')
def _bent(names, vs=_BEV):
	return _sdent(names, vs)
def _bres():
	import CharsNPC
	k = CharsNPC.CharRes.keys()
	k.sort()
	return map(lambda k, r=CharsNPC.CharRes: (k, r[k].ActCtrl, _sdn(getattr(r[k], 'CallbackFunc', '-')), _sdn(getattr(r[k], 'DancePos', '-'))), k)
_BHOOKS = (('Speech', 'OnEndOfSpeech'), ('Speech', 'OnEndOfSpeechParams'), ('Speech', 'PlayerName'), ('Speech', 'NPCName'),
	('Speech', 'OnOutOfRange'), ('Alarm', 'TempAbortMissionCallback'), ('Alarm', 'TempRestoreMissionCallback'),
	('DTritusDesktop', 'DialogController'), ('Menu', 'MissionScorerCall'), ('Scene', 'EndOfSceneCallback'),
	('Parking', 'SwitchVehicleMissionCallback'), ('Parking', 'SwitchCharMissionCallback'), ('MissionsFuncs', 'currentTarget'),
	('MissionsFuncs', 'targetsList'), ('Init', 'Outdoor'), ('Init', 'OutArea'), ('CharsNPC', 'AfterBrowseNPC'),
	('CharConversor', 'AvailableChars'), ('GateKeeper', 'LocalCallFunc'), ('GateKeeper', 'LocalParams'))
def _bhooks():
	r = []
	for m, v in _BHOOKS:
		if not sys.modules.has_key(m):
			r.append((m, v, '-'))
		else:
			r.append((m, v, _sdn(getattr(sys.modules[m], v, '-'))))
	return r
def _broutines():
	import Speech
	k = Speech.SpeechRoutines.keys()
	k.sort()
	return map(lambda k, r=Speech.SpeechRoutines: (k, _sdn(r[k])), k)
# recorders for the calls with effects that need AI timing, load levels or play scenes; the originals come back with
# _broff() before a level load
_BREC = ('Speech.AddRemoteCom', ('Speech.OnSpch', 1), 'Speech.NISpeech', 'Speech.AutoAddNI', 'Speech.RunNI', 'Speech.StdAnsLst',
	'Speech.StdOnSpch', 'Speech.RestoreState', 'Speech.StartObjSpch', 'Speech.OffVoice', 'Speech.ForceRemoteCom',
	('Speech.RemoteComunication', 1), ('Speech.CalculateSpeechTime', 2.5), 'SaveGame.QLoad', 'SaveGame.StartGame',
	'SaveGame.MarkWayPoint', 'SaveGame.QWayPoint', 'SaveGame.MMDesc', 'RaceMaker.ShowSystemMessage', 'Scene.Play',
	'MissionsFuncs.StartSuperDeal', 'PhoneCab.UseAudioCom', 'CharsNPC.PlayOption', 'CharsNPC.GenericNPCSpeech', 'Menu.MovieScorer',
	'SSound.SetMusic', 'SSound.SetMusicVolume', 'Scrap.Achievement')
_borig = []
def _bon(extra=None):
	# the recorders of _BREC (or only those of extra); modules the level has not loaded are skipped
	global _borig
	for n in extra or _BREC:
		ret = None
		if type(n) == type(()):
			n, ret = n
		m, f = string.split(n, '.')
		if not sys.modules.has_key(m):
			continue
		mod = sys.modules[m]
		_borig.append((mod, f, getattr(mod, f)))
		setattr(mod, f, _sdrec(n, ret))
	return 1
def _broff():
	global _borig
	while _borig:
		mod, f, v = _borig[-1]
		del _borig[-1]
		setattr(mod, f, v)
	return 1
# SScorer variables of scorer items (id, names, attrs), '?' when one cannot be read
def _bitems(id, names, attrs):
	import SScorer
	r = []
	for n in names:
		row = [n]
		for a in attrs:
			try:
				row.append(_sdn(SScorer.Get(id, n, a)))
			except:
				row.append('?')
		r.append(tuple(row))
	return r
_BTEXT = ('X', 'Y', 'Text', 'Font', 'Align', 'Red', 'Green', 'Blue', 'Alpha', 'Visible')
def _bnear(n, d):
	# the player next to the entity n (d along x)
	u = Scrap.UsrEntity(0)
	p = Scrap.GetEntity(n).Pos
	u.Pos = (p[0] + d, p[1], p[2])
	return 1
def _bprof(p):
	d = p.__dict__
	k = d.keys()
	k.sort()
	return map(lambda k, d=d: (k, _sdn(d[k])), k)
_BSCEN = ('Scene.Load', 'Scene.AddGeneric', 'Scene.AddText', 'Scene.ActorAction', 'Scene.ObjAction', 'Scene.ActorBreakChannel',
	'Scene.AddCharActor', 'Scene.AddLocalActor', 'Scene.CreateLocalAction', 'Scene.SetDefSounds', 'MakeChar.CreateRelaxAction',
	'SAct.SetCls', 'SAct.SetAct', ('SAct.GetAct', 1.5), 'SAct.GetClass', 'SAct.CreateAction', 'Scrap.SetDebrisSys')
def _btarget():
	# the name of the current mission target
	import MissionsFuncs
	t = MissionsFuncs.currentTarget
	if type(t) != type(''):
		t = getattr(t, 'Name', t)
	return t
_bposv = [None]
def _bsave():
	_bposv[0] = Scrap.UsrEntity(0).Pos
	return 1
def _bback():
	Scrap.UsrEntity(0).Pos = _bposv[0]
	return 1
_basv = [None]
def _bas():
	import SInput
	_basv[0] = SInput.GetActionSet()
	return 1
def _basr():
	import SInput
	SInput.SetActionSet(_basv[0])
	return 1
class _bfakeusr:
	# Scrap.UsrEntity(0) as a functionary (the talks that check the player's character)
	ActCtrl = 'Functionary'
	Name = 'Player0'
_BDEN = ('MayorAtGambinDen', 'FunctionaryTwinsAtGambinDen', 'BankersAtGambinDen', 'BishopsAtGambinDen', 'PolicesAtGambinDen', 'PoliceBossAtPolice')
# the talk of an arranging mission through all its options (accept restores the action set)
def _barrange(fn, prefix):
	u = Scrap.UsrEntity(0).Name
	r = []
	for o in ('Talk', prefix + '_1', prefix + '_1_2', 'Other'):
		r.append(_sd(fn, u, 'CrazyGambler_Default', o, 'x'))
	_bas()
	r.append(_sd(fn, u, 'CrazyGambler_Default', prefix + '_1_1', 'x'))
	_basr()
	r.append(_bhooks()[:4])
	return r
# the talk of a mercenary through all its options
def _bmerc(fn, name, prefix):
	u = Scrap.UsrEntity(0).Name
	r = []
	for o in ('Talk', prefix + '_1', prefix + '_1_1', prefix + '_1_1_1', 'Other'):
		r.append(_sd(fn, u, name, o, 'x'))
	r.append(_bhooks()[:4])
	return r
class _brand:
	# Scrap.Rand(a, b) as a + frac * (b - a), recorded (the live loops of the level draw from it between the lines too)
	def __init__(self, frac):
		self.frac = frac
	def __call__(self, a, b):
		_sdr.append(('Scrap.Rand', (a, b)))
		return a + self.frac * (b - a)
def _brandon(frac):
	_borig.append((Scrap, 'Rand', Scrap.Rand))
	Scrap.Rand = _brand(frac)
	return 1
