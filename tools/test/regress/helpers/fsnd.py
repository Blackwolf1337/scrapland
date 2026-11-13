import Scrap, SAct, SSound
exec(open('fmis.py').read())
# vol is the volume the engine sets while the ship moves (InTraffic sets it once: _sndvars(..., vs=_SNDVV))
_SNDV = ('name', 'loop', 'int0', 'int1', 'ini', 'end', 'dopp', 'pitch0', 'pitch1', 'pitch2', 'vol0', 'vol1', 'vol2', 'time0', 'time1', 'time2')
_SNDVV = _SNDV + ('vol',)
def _sndvars(n, pre='snd', count=32, vs=None):
	vs = vs or _SNDV
	e = Scrap.GetEntity(n)
	r = []
	for v in (pre + '_n',):
		try:
			r.append((v, getattr(e, v)))
		except:
			r.append((v, '?'))
	for i in range(count):
		row = []
		for v in vs:
			try:
				x = getattr(e, '%s%02d_%s' % (pre, i, v))
				if type(x) == type(0.0):
					x = round(x, 4)
				row.append(x)
			except:
				row.append('?')
		r.append(row)
	return r
def _allsnd(n):
	return (_sndvars(n, 'snd', 32), _sndvars(n, 'sdd', 9), _sndvars(n, 'sdl', 5))
def _vehs():
	n = []
	def c(x, n=n):
		n.append(x)
	__import__('Init')._vc = c
	Scrap.SphereCall(0.0, 0.0, 0.0, 3000000.0, 'v', 'Init._vc')
	n.sort()
	return n
def _vehbymodel():
	# the sound variables of every ship, one entry per model (the traffic picks models at random)
	d = {}
	for x in _vehs():
		e = Scrap.GetEntity(x)
		try:
			m = e.Model
		except:
			m = '?'
		s = `_allsnd(x)`
		if not d.has_key(m):
			d[m] = []
		if s not in d[m]:
			d[m].append(s)
	k = d.items()
	k.sort()
	return k
def _ps(names, vs=('Sound', 'X', 'Y', 'Z', 'Volume', 'Pitch', 'Loop', 'AttenIni', 'AttenEnd', 'Doppler', 'MaxInterval', 'MinInterval')):
	r = []
	for n in names:
		row = [n]
		for v in vs:
			try:
				x = SSound.GetPS(n, v)
				if type(x) == type(0.0):
					x = round(x, 3)
				row.append(x)
			except:
				row.append('?')
		r.append(row)
	return r
def _acts(cls, acts):
	# the events of actions of a class: (time, name, function)
	SAct.DumpEvents('sndev.txt')
	import string
	r = []
	on = 0
	for l in open('sndev.txt').readlines():
		if l[:6] == 'class ':
			on = string.strip(l[6:]) == cls
		elif on:
			r.append(string.rstrip(l))
	return r
_SNDG = ('SndLoopCharNPCMul', 'OnAcceptSound', 'OnAcceptSoundVol', 'OnFocusSound', 'OnFocusSoundVol', 'CharacterDeathSound', 'CharacterDeathSoundVol',
	'DangerLifeSound', 'DangerLifeSoundVol', 'RegenerationSound', 'RegenerationSoundVol', 'RegenerationSoundPitchMin', 'RegenerationSoundPitchMax',
	'InitRegenerationSound', 'InitRegenerationSoundVol', 'EndRegenerationSound', 'EndRegenerationSoundVol', 'TextTypingSound', 'TextTypingSoundVol',
	'CharacterConversionSound', 'CharacterConversionSoundVol', 'OutRelaxMusic', 'iOutRelaxMusic', 'OutRelaxMusicVolume', 'OutActionMusic',
	'iOutActionMusic', 'OutActionMusicVolume', 'isActionIndoor', 'ActionMusicTimeFade', 'MusicTimeChange', 'CharacterTeleportUpSound',
	'CharacterTeleportDownSound', 'CharacterTeleportSoundVol', 'SoundAutoplayVolume', 'SoundDopplerLevel', 'SoundDistAttenuation', 'NPCSndMotorVolFactor',
	'EMIThunderSoundVol', 'EMIThunderSoundAttIni', 'EMIThunderSoundAttEnd', 'EMIThunder1Sound', 'EMIThunder2Sound', 'SwarmBeeSoundAttIni',
	'SwarmBeeSoundAttEnd', 'SwarmBeeSoundVol', 'SwarmHitSoundAttIni', 'SwarmHitSoundAttEnd', 'SwarmHitSoundVol', 'SwarmEngineSoundAttIni',
	'SwarmEngineSoundAttEnd', 'SwarmEngineSoundVol', 'VulcanRic0SoundVol', 'VulcanRic0Sound', 'VulcanRic1SoundVol', 'VulcanRic1Sound',
	'VulcanRic2SoundVol', 'VulcanRic2Sound', 'DevastatorBoomSoundVol', 'DevastatorBoomSoundAttIni', 'DevastatorBoomSoundAttEnd', 'DevastatorBoomSound',
	'ATPCHitSoundVol', 'ATPCHitSound', 'ATPCCapsuleSoundVol', 'ATPCCapsuleSound', 'ATPCCapsuleExtendedSoundVol', 'ATPCCapsuleExtendedSound',
	'InfernoHitSoundVol', 'InfernoHitSoundAttIniRadius', 'InfernoHitSoundAttEndRadius', 'InfernoEngineSoundAttIni', 'InfernoEngineSoundAttEnd',
	'InfernoEngineSoundVol', 'LaserRicSoundVol', 'LaserRicSound', 'TargetScorerSwitchSound', 'TargetScorerSwitchSoundVol', 'CannonScorerNoAmmoSound',
	'CannonScorerNoAmmoSoundVol', 'WindFxScorerSound', 'WindFxScorerSoundVol', 'WindFxScorerSpeedVol', 'WindFxScorerSpeedPitch', 'WindFxScorerHorizontal',
	'WindFxScorerVertical', 'WindFxScorerFadeFactor', 'WindFxScorerFwAngle', 'WindFxScorerShow', 'FXElementsFireDropSound', 'FXElementsFireDropSoundVol',
	'FXElementsFireDropSoundAttIni', 'FXElementsFireDropSoundAttEnd')
_MAPG = ('FileNameMap2D', 'CharGrav', 'AtmosphereLimit', 'TopLimit', 'GridUseTopLimit', 'Max_Rad_Obj', 'CameraCloudDistance', 'CameraCloudPlaneSize',
	'CameraCloudThickness', 'CameraCloudAlpha', 'CameraCloudRed', 'CameraCloudGreen', 'CameraCloudBlue', 'OnTaunt', 'SuperDealAvaliable', 'EnergyBarActive',
	'ShipPathFmt')
_SNDM = ('WaySound', 'WayVolume', 'EneWaySound', 'EneWayVolume', 'PhotoSound', 'PhotoVolume', 'DeathSound', 'DeathVolume', 'TVOffSound', 'TVOffVolume',
	'RotateShipSound', 'RotateShipVolume', 'RotateShipFastSound', 'RotateShipFastVolume', 'HologramStartSound', 'HologramStartVolume', 'HologramLoopSound',
	'HologramLoopVolume', 'ShipEditLoadShipSound', 'ShipEditLoadShipVolume', 'ShipEditLoadWeaponSound', 'ShipEditLoadWeaponVolume',
	'ShipEditLoadEngineSound', 'ShipEditLoadEngineVolume', 'ShipEditUnLoadSound', 'ShipEditUnLoadVolume', 'FlashSound')
def _gvq(vs):
	r = []
	for v in vs:
		try:
			x = Scrap.Get(v)
			if type(x) == type(0.0):
				x = round(x, 4)
			r.append((v, x))
		except:
			r.append((v, '?'))
	return r
def _take():
	import Sound
	k = Sound.ItemTake.items()
	k.sort()
	return k
def _recall(calls):
	map(lambda n: _rec(n[0], n[1]), calls)
class _Keep:
	# a recorder that also runs the real function (for calls whose effect the case reads back)
	def __init__(self, name, fn):
		self.name = name
		self.fn = fn
	def __call__(self, *a, **kw):
		_calls.append((self.name, a, kw))
		return apply(self.fn, a, kw)
def _keep(mod, func):
	m = __import__(mod)
	f = getattr(m, func)
	if hasattr(f, "fn"):
		return
	setattr(m, func, _Keep(mod + '.' + func, f))
def _unkeep(mod, func):
	m = __import__(mod)
	f = getattr(m, func)
	if hasattr(f, 'fn'):
		setattr(m, func, f.fn)
def _psall(prefixes, n):
	# the point sounds prefix<i> (i from 1 to n) that exist
	names = []
	for p in prefixes:
		for i in range(1, n + 1):
			if SSound.GetPS(p + `i`, 'Sound') != None:
				names.append(p + `i`)
	return _ps(names)
class _Seq:
	# a recorder that returns the given values in turn (the last one again at the end)
	def __init__(self, name, values):
		self.name = name
		self.values = values
		self.i = 0
	def __call__(self, *a, **kw):
		_calls.append((self.name, a, kw))
		v = self.values[min(self.i, len(self.values) - 1)]
		self.i = self.i + 1
		return v
_saved = {}
def _seq(mod, func, values):
	m = __import__(mod)
	if not _saved.has_key((mod, func)):
		_saved[(mod, func)] = getattr(m, func)
	setattr(m, func, _Seq(mod + '.' + func, values))
def _rec2(mod, func, ret=None):
	m = __import__(mod)
	if not _saved.has_key((mod, func)):
		_saved[(mod, func)] = getattr(m, func)
	setattr(m, func, _Rec(mod + '.' + func, ret))
def _restore():
	for k, f in _saved.items():
		setattr(__import__(k[0]), k[1], f)
	_saved.clear()
class _FakeEnt:
	def __init__(self, name):
		self.Name = name
	def __repr__(self):
		d = self.__dict__.items()
		d.sort()
		return 'Ent' + `d`
class _MakeEnt:
	# Scrap.CreateEntity and GetEntity for CreatePlayers: fake entities that keep what is set on them
	def __init__(self, name):
		self.name = name
		self.made = []
	def __call__(self, *a):
		_calls.append((self.name, a))
		e = _FakeEnt(a[0])
		self.made.append(e)
		return e
def _model(name, model, out=0):
	# a ship with the model (OutTraffic models with out=1)
	import Traffic
	if out:
		prev = Scrap.Get('ShipPathFmt')
		Scrap.Set('ShipPathFmt', Traffic.Path + '%s.m3d')
	v = Scrap.GetEntity(name)
	if not v:
		v = Scrap.CreateEntity(name, 1000.0, 50000.0, 1000.0, 'Car')
	v.SetModel(model)
	if out:
		Scrap.Set('ShipPathFmt', prev)
	return v.Model
def _vs(model, out=0, tag=''):
	import OutSound
	n = 'SndT' + tag + model
	_model(n, model, out)
	r = OutSound.SetVehicleSounds(n)
	return (model, r, _allsnd(n))
def _t(f, *a, **kw):
	# f(*a, **kw), or the exception it raised
	import sys
	try:
		return apply(f, a, kw)
	except:
		t, v = sys.exc_info()[:2]
		return ('raised', str(t), str(v))
def _pk(pack, f, *a, **kw):
	# f(*a, **kw) with the sound pack open (as the loading function that calls it has it): (result, calls)
	Scrap.OpenPack(pack)
	_takecalls()
	r = apply(_t, (f,) + a, kw)
	c = _takecalls()
	Scrap.ClosePack()
	_takecalls()
	return (r, c)
