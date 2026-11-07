# round 7b (Press, Junkyard): shortcuts on top of fsd.py and fr6b.py (exec'd after them)
import Scrap, sys
def _c(m, f, *a, **kw):
	# _sd of module m's function f
	return apply(_sd, (getattr(sys.modules[m], f),) + a, kw)
class _fusr:
	def __init__(self, ctrl, name='Player0'):
		self.ActCtrl = ctrl
		self.Name = name
_fuorig = [None]
def _fuon(ctrl):
	# Scrap.UsrEntity(i) gives a fake player of the class ctrl
	if _fuorig[0] is None:
		_fuorig[0] = Scrap.UsrEntity
	Scrap.UsrEntity = lambda i, c=ctrl: _fusr(c)
	return 1
def _fuoff():
	if _fuorig[0] is not None:
		Scrap.UsrEntity = _fuorig[0]
		_fuorig[0] = None
	return 1
_rorig = []
def _ron(names):
	# Speech.SpeechRoutines[name] recorded
	import Speech
	for n in names:
		_rorig.append((n, Speech.SpeechRoutines.get(n)))
		Speech.SpeechRoutines[n] = _sdrec('Routine.' + n)
	return 1
def _roff():
	import Speech
	while _rorig:
		n, v = _rorig[-1]
		del _rorig[-1]
		if v is None:
			del Speech.SpeechRoutines[n]
		else:
			Speech.SpeechRoutines[n] = v
	return 1
def _cb(name):
	# the callbacks of a mission character
	import CharsNPC
	if not CharsNPC.CharRes.has_key(name):
		return '-'
	return _sdn(getattr(CharsNPC.CharRes[name], 'CallbackFunc', '-'))
def _hk(*pairs):
	r = []
	for m, v in pairs:
		if sys.modules.has_key(m):
			r.append((m, v, _sdn(getattr(sys.modules[m], v, '-'))))
		else:
			r.append((m, v, '-'))
	return r
_SPK = ('Speech.AddNI', 'Speech.AutoSpeech', 'MissionsFuncs.SetMissionTarget', 'MissionsFuncs.ActivateTarget', 'MissionsFuncs.SetNextMission',
	'Scrap.ConsoleError', 'SSound.PlaySound', 'Scrap.DelSaveVars')
def _last(n):
	# the arguments of the last recorded call n of the last _sd
	r = None
	for x in _sdr:
		if x[0] == n:
			r = x[1]
	return r
