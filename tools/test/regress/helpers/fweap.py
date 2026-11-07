def _sv(get, i, vs):
	r = []
	for v in vs:
		try:
			r.append((v, get(i, v)))
		except:
			r.append((v, '?'))
	return r
def _wd():
	import SWeap
	ws = ('Name','OnPress','OnRelease','OnHit','OnFire','SetFX','OnEnd','OnStart')
	wf = ('iAmmo','Upgrd01','Upgrd02','Upgrd03','Upgrd04','Upgrd05','Upgrd06','Upgrd07','Upgrd08','Upgrd09','Upgrd10','Upgrd11','Upgrd12','MinAmmo')
	r = [SWeap.GetNWeap(), SWeap.GetNAmmo()]
	for i in range(SWeap.GetNWeap()):
		r.append((i, _sv(SWeap.GetSWeap, i, ws), _sv(SWeap.GetFWeap, i, wf)))
	for i in range(SWeap.GetNAmmo()):
		r.append((i, _sv(SWeap.GetSAmmo, i, ('Name','OnFire','OnPress','OnRelease')), _sv(SWeap.GetFAmmo, i, ('Charger','Max'))))
	return r
_wvars = ('ATPCAmmoCost','ATPCConvDist','ATPCDelay','ATPCExplosionRadius','ATPCExtraDamage','ATPCHitForce','ATPCIDAmmo','ATPCIDWeap','ATPCMainDamage','ATPCRange','ATPCShootFile','ATPCUpgradeDelay','CloudAmmoNeed','CloudFile','CloudIDAmmo','CloudLifeTime','CloudRechargeTime','CreateCloudFunc','CreateEMIFunc','CreateSonicFunc','DevastatorAmmoCost','DevastatorBulletFile','DevastatorDistLimit','DevastatorExplosionRadius','DevastatorExtraDamage','DevastatorHitFile','DevastatorHitForce','DevastatorIDAmmo','DevastatorIDWeap','DevastatorMinDamage','DevastatorRange','DevastatorRechargeTime','DevastatorShootFile','DevastatorSparksFile','DevastatorSpreadAngle','EMIAmmoNeed','EMIFile','EMIIDAmmo','EMILifeTime','EMILowRadiusFactor','EMIRadius','EMIRechargeTime','FXSonicRadius','FXSonicXpldeFile','HookAccel','HookPlugFunc','HookRange','InfernoExplosionFile','InfernoIDWeap','InfernoMissileFile','LaserAmmoCost','LaserDistLimit','LaserHitFile','LaserHitForce','LaserHookFile','LaserHookSparksFile','LaserIDAmmo','LaserIDWeap','LaserRange','LaserShootFile','LaserTimeDelay','SetUpCloudFunc','SetUpEMIFunc','SetUpSonicFunc','SonicAmmoNeed','SonicArmor','SonicIDAmmo','SonicLifeTime','SonicRechargeTime','SwarmBeeExplosionFile','SwarmExplosionFile','SwarmIDWeap','SwarmMissileFile','SwarmTurnSpeed','SwarmTurnSpeedUPG','TeslaAmmoCost','TeslaDistLimit','TeslaDownRangeTime','TeslaHitForce','TeslaIDAmmo','TeslaIDWeap','TeslaMaxAngRange','TeslaMinAngRange','TeslaMinAngRangeUpg','TeslaRange','TeslaShootFile','TeslaTrackForce','TeslaUpRangeTime','VulcanAmmoCost','VulcanBulletFile','VulcanDistLimit','VulcanDistLimitUPG','VulcanHitFile','VulcanHitForce','VulcanIDAmmo','VulcanIDWeap','VulcanRange','VulcanTimeDelay','VulcanTimeDelayUPG')
def _wv():
	r = []
	for v in _wvars:
		try:
			r.append((v, Scrap.Get(v)))
		except:
			r.append((v, '?'))
	return r
def _wm():
	r = []
	for m in ('Vulcan','Devastator','Tesla','ATPC','Laser','Swarm','Inferno','Cloud','EMI','Sonic'):
		mod = __import__(m)
		r.append((m, getattr(mod, 'IDWeap', '-'), getattr(mod, 'IDAmmo', '-'), _fn(getattr(mod, 'LastClientCreate', '-')), _fn(getattr(mod, 'LastClientAfterCreate', '-'))))
	n = __import__('Net')
	r.append(('Net', _fn(n.ClientCreate), _fn(n.ClientAfterCreate), _fn(n.ClientJoin)))
	return r
def _fn(v):
	if hasattr(v, '__name__'):
		return v.__name__
	return v
_r = []
class _recn:
	def __init__(self, n):
		self.n = n
	def __call__(self, *a):
		_r.append((self.n,) + a)
		return 1
def _t(f, *a):
	import sys
	try:
		try:
			return apply(f, a)
		except:
			return 'error: %s: %s' % (sys.exc_type, sys.exc_value)
	finally:
		Scrap.SetTimeSpeed(1)
def _sync(f, *a):
	global _r
	_r = []
	v = apply(f, a)
	return (v, map(lambda c: c[0], _r), _r)
def _items(id, names, attrs):
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
def _split():
	import Split
	return (Split.PlayersReady, Split.MortalProfiles, Split.Scorer, Split.FirstTime, Split.ModLib.__name__, _fn(__import__('Vehicles').OnDeathCallback), _fn(Split.FuncGetSpawnPos), _fn(Split.FuncGetReSpawnPos), _fn(Split.OnDeathFunc), Scrap.Get('EscapeEvent'), __import__('SInput').GetActionSet())
def _obj(o):
	d = o.__dict__
	k = d.keys()
	k.sort()
	return map(lambda k, d=d: (k, d[k]), k)
def _std():
	import VehiclesNPC
	d = VehiclesNPC.StdShips
	k = d.keys()
	k.sort()
	return map(lambda k, d=d: (k, d[k]), k)
class _Fake:
	pass
def _fakemake(*a):
	_r.append(('MakeAI',) + a)
	f = _Fake()
	f.args = a
	return f
