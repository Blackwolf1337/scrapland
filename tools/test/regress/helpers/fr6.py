# round 6a (level scripts of Bank, Police, Townhall): recorders and state dumps, exec'd into __main__ after fsd.py
# (execfile), again after every level load
import Scrap, sys
_R6HOOKS = (('Speech', 'OnEndOfSpeech'), ('Speech', 'OnEndOfSpeechParams'), ('Speech', 'PlayerName'), ('Speech', 'NPCName'),
	('CharAct', 'TakenPhoto'), ('CharAct', 'TakenPhotoMap'), ('CharAct', 'onNPCDeath'), ('CharConversor', 'OnPossession'),
	('CharConversor', 'afterExitConversor'), ('Alarm', 'RestoreMissionCallback'), ('Alarm', 'AbortMissionCallback'),
	('DTritusDesktop', 'DialogController'), ('Menu', 'MissionScorerCall'), ('GateKeeper', 'AfterGateKeeperSpeech'),
	('Elevator', 'OnElevatorReachUp'), ('Scene', 'EndOfSceneCallback'), ('Chars', 'OnMainCharDeath'),
	('MissionsFuncs', 'currentTarget'), ('MissionsFuncs', 'targetsList'), ('Messenger', 'MakeMDP'), ('Init', 'Outdoor'),
	('Init', 'OutArea'), ('CharsNPC', 'AfterBrowseNPC'), ('CharsNPC', 'RespawnChars'), ('CharsNPC', 'ReposChars'),
	('CharConversor', 'AvailableChars'), ('InTraffic', 'N_TrafficList'), ('InTraffic', 'N_NumTraffic'),
	('InTraffic', 'M_TrafficList'), ('InTraffic', 'M_NumTraffic'), ('InTraffic', 'H_TrafficList'), ('InTraffic', 'H_NumTraffic'))
def _r6hooks():
	r = []
	for m, v in _R6HOOKS:
		if not sys.modules.has_key(m):
			r.append((m, v, '-'))
		else:
			r.append((m, v, _sdn(getattr(sys.modules[m], v, '-'))))
	return r
def _r6res():
	import CharsNPC
	k = CharsNPC.CharRes.keys()
	k.sort()
	return map(lambda k, r=CharsNPC.CharRes: (k, r[k].ActCtrl, _sdn(getattr(r[k], 'CallbackFunc', '-')), _sdn(getattr(r[k], 'Pos', '-'))), k)
def _r6sv(names):
	return map(lambda n: (n, Scrap.GetSaveVar(n)), names)
def _r6gv(names):
	r = []
	for n in names:
		try:
			r.append((n, Scrap.Get(n)))
		except:
			r.append((n, '?'))
	return r
# entity variables of the characters and objects the missions touch
_R6EV = ('ActCtrl', 'UseFunc', 'Descriptor', 'LLogicDefaultState', 'LLogicDefaultTarget', 'LLogicState', 'LLogicTarget',
	'LLogicProbTalkWhenWalking', 'LLogicEscapeWithinRadius', 'ArrowCanUse', 'ArrowCanMission', 'RadarShow', 'Money',
	'MoneyReward', 'Action', 'OnTake', 'Radius', 'OnlyUser', 'rTime', 'Blink', 'Active', 'Invulnerable', 'ArrOpt0')
def _r6ent(names, vs=_R6EV):
	return _sdent(names, vs)
# the functions of a level module: C (builtin) or Python, and the names
def _r6fn(m):
	mod = sys.modules[m]
	k = mod.__dict__.keys()
	k.sort()
	r = []
	for n in k:
		v = mod.__dict__[n]
		if type(v) == type(len):
			r.append((n, 'C'))
		elif type(v) == type(_r6fn):
			r.append((n, 'py'))
	return r
# recorders for the calls whose effects need AI timing or load levels, keeping the arguments
def _r6on():
	import Speech, SaveGame, RaceMaker, CharsNPC, Scene, Menu, MissionsFuncs, Elevator, CharConversor
	for n in ('AddRemoteCom', 'OnSpch', 'NISpeech', 'AutoAddNI', 'AddNI', 'RunNI', 'StdAnsLst', 'RestoreState', 'OnObjSpch',
			'AutoSpeech', 'ForceRemoteCom'):
		setattr(Speech, n, _sdrec(n))
	Speech.RemoteComunication = _sdrec('RemoteComunication', 1)
	Speech.SpeechRoutines = {'Police': _sdrec('Routine.Police'), 'PoliceBoss': _sdrec('Routine.PoliceBoss')}
	for n in ('QLoad', 'StartGame', 'MarkWayPoint', 'QWayPoint', 'MMDesc', 'SaveMoney', 'CheckAllBlueprints'):
		setattr(SaveGame, n, _sdrec(n))
	RaceMaker.ShowSystemMessage = _sdrec('ShowSystemMessage')
	CharsNPC.GenericNPCSpeech = _sdrec('GenericNPCSpeech')
	Scene.Play = _sdrec('Scene.Play')
	Menu.ShowWarning = _sdrec('ShowWarning')
	Elevator.UseElevator = _sdrec('UseElevator')
	CharConversor.ConvertChar = _sdrec('ConvertChar')
	MissionsFuncs.FinishGame = _sdrec('FinishGame')
	return 1
# Scrap.CreateEntity recorded; the entity is made under a new name R6_<n>_<name> (an existing name is not made twice)
class _r6ce:
	def __init__(self, real):
		self.n = 0
		self.real = real
	def __call__(self, *a):
		_sdr.append(('CreateEntity',) + _sdn(a))
		self.n = self.n + 1
		return apply(self.real, ('R6_' + `self.n` + '_' + a[0],) + a[1:])
if not globals().has_key('_r6realce'):
	_r6realce = Scrap.CreateEntity
def _r6ceon():
	Scrap.CreateEntity = _r6ce(_r6realce)
	return 1
def _r6ceoff():
	Scrap.CreateEntity = _r6realce
	return 1
# SScorer variables of scorer items (id, names, attrs), '?' when one cannot be read
def _r6items(id, names, attrs):
	import SScorer
	r = []
	for n in names:
		row = [n]
		for a in attrs:
			try:
				row.append(SScorer.Get(id, n, a))
			except:
				row.append('?')
		r.append(tuple(row))
	return r
