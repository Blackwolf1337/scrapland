# Menu checks: calls engine script functions and writes the results to sdtest.out (Python 1.5.2 syntax).
# tools/test/difftest.sh runs it with the C functions and with the originals and compares the outputs. The game
# reads it from its root directory through -PythonExecute:execfile(...); the engine's command line drops quotes
# and splits at spaces, so difftest.sh spells the file name with chr().
OUT = 'sdtest.out'
execfile('sdlib.py')
import Scrap, SWeap, SVec, SAct, SFX, SLogic

check('Scrap.GetMoney()', lambda: Scrap.GetMoney())
check('Scrap.GetMoney(1)', lambda: Scrap.GetMoney(1))
check('Scrap.GetLevelPath()', lambda: Scrap.GetLevelPath())
check('Scrap.GetLevelPath("x")', lambda: Scrap.GetLevelPath("x"))
check('SWeap.GetNWeap()', lambda: SWeap.GetNWeap())
check('SWeap.GetNWeap(None)', lambda: SWeap.GetNWeap(None))
# SVec: vector math, compared to the last bit (fmt prints 17 digits)
inf = 1e308 * 10.0
nan = inf - inf
vecs = [(0.0, 0.0, 0.0), (1.0, 0.0, 0.0), (0.0, 1.0, 0.0), (1.0, 2.0, 3.0), (-3.5, 0.25, 7.125),
	(1e-7, 0.0, 0.0), (5e-7, 5e-7, 5e-7), (1234.5678, -9876.54321, 0.001), (0.1, 0.2, 0.3), (1e20, -1e20, 1e-20),
	(inf, 0.0, 0.0), (nan, 1.0, 1.0)]
for v in vecs:
	for name in ('Mod', 'ModSqr', 'Norm', 'GetRotAng'):
		check('SVec.%s%s' % (name, v), lambda f=getattr(SVec, name), v=v: f(v))
	check('SVec.GetRotAng%s flat' % (v,), lambda v=v: SVec.GetRotAng(v[0], v[1], v[2]))
	for f in (-2.5, 0.0, 3.0):
		check('SVec.Prod(%s, %s)' % (v, f), lambda v=v, f=f: SVec.Prod(v, f))
	for w in vecs[:10]:
		for name in ('Add', 'Sub', 'DProd', 'CProd', 'GetAngle', 'Rotate3D'):
			check('SVec.%s(%s, %s)' % (name, v, w), lambda f=getattr(SVec, name), v=v, w=w: f(v, w))
for a in (0.0, 45.0, 180.0, -180.0, 181.0, 359.999, 720.0, -1000.25, 1e10, inf, nan):
	check('SVec.NormAng(%s)' % a, lambda a=a: SVec.NormAng(a))
check('SVec.GetAngle((1,0,0), (1,0,0))', lambda: SVec.GetAngle((1, 0, 0), (1, 0, 0)))
check('SVec.GetAngle((1,0,0), (-1,0,0))', lambda: SVec.GetAngle((1, 0, 0), (-1, 0, 0)))
check('SVec.GetAngle((1,1,0), (2.000001,2,0))', lambda: SVec.GetAngle((1, 1, 0), (2.000001, 2, 0)))
check('SVec.Add bad', lambda: SVec.Add((1, 2), (3, 4, 5)))
check('SVec.Mod bad', lambda: SVec.Mod('abc'))
check('SVec.GetRotAng bad', lambda: SVec.GetRotAng(1, 2))
check('SVec.Prod bad', lambda: SVec.Prod((1, 2, 3)))
# SWeap: in the menu both tables are empty; Init may run only once (CHECK(!p) in Table.h), so only then.
# An index out of range stops the original on int3, so none of those here.
check('SWeap.GetNWeap()', lambda: SWeap.GetNWeap())
check('SWeap.GetNAmmo()', lambda: SWeap.GetNAmmo())
check('SWeap.GetFirstiWeap()', lambda: SWeap.GetFirstiWeap())
check('SWeap.InitWeap(0)', lambda: SWeap.InitWeap(0))
check('SWeap.InitWeap(-5)', lambda: SWeap.InitWeap(-5))
if SWeap.GetNWeap() == 0:
	check('SWeap.InitWeap(3)', lambda: SWeap.InitWeap(3))
if SWeap.GetNAmmo() == 0:
	check('SWeap.InitAmmo(2)', lambda: SWeap.InitAmmo(2))
check('SWeap.GetNWeap() after', lambda: SWeap.GetNWeap())
check('SWeap.GetNAmmo() after', lambda: SWeap.GetNAmmo())
wfloat = ('MinAmmo', 'iPrev', 'iNext', 'iAmmo', 'Upgrd00', 'Upgrd05', 'upgrd07', 'Upgrd', 'Upgrd1x', 'Bogus')
wstr = ('Name', 'OnPress', 'OnRelease', 'OnFire', 'OnHit', 'OnStart', 'OnEnd', 'SetFx', 'Bogus')
for i in range(SWeap.GetNWeap()):
	for v in wfloat:
		check('SWeap.GetFWeap(%d, %s)' % (i, v), lambda i=i, v=v: SWeap.GetFWeap(i, v))
	for v in wstr:
		check('SWeap.GetSWeap(%d, %s)' % (i, v), lambda i=i, v=v: SWeap.GetSWeap(i, v))
for v in ('MinAmmo', 'iPrev', 'iNext', 'iAmmo', 'Bogus'):
	check('SWeap.SetFWeap(1, %s, 2.5)' % v, lambda v=v: SWeap.SetFWeap(1, v, 2.5))
	check('SWeap.GetFWeap(1, %s) set' % v, lambda v=v: SWeap.GetFWeap(1, v))
for v in wstr:
	check('SWeap.SetSWeap(2, %s)' % v, lambda v=v: SWeap.SetSWeap(2, v, 'val_' + v))
	check('SWeap.GetSWeap(2, %s) set' % v, lambda v=v: SWeap.GetSWeap(2, v))
for i in range(SWeap.GetNAmmo()):
	for v in ('Max', 'Charger', 'max', 'Bogus'):
		check('SWeap.GetFAmmo(%d, %s)' % (i, v), lambda i=i, v=v: SWeap.GetFAmmo(i, v))
	for v in ('Name', 'OnPress', 'OnRelease', 'OnFire', 'OnHit', 'OnStart', 'Bogus'):
		check('SWeap.GetSAmmo(%d, %s)' % (i, v), lambda i=i, v=v: SWeap.GetSAmmo(i, v))
check('SWeap.SetFAmmo(0, Max, 99.75)', lambda: SWeap.SetFAmmo(0, 'Max', 99.75))
check('SWeap.SetFAmmo(0, Bogus, 1)', lambda: SWeap.SetFAmmo(0, 'Bogus', 1))
check('SWeap.GetFAmmo(0, Max) set', lambda: SWeap.GetFAmmo(0, 'Max'))
check('SWeap.SetSAmmo(1, Name)', lambda: SWeap.SetSAmmo(1, 'Name', 'Bullets'))
check('SWeap.GetSAmmo(1, Name) set', lambda: SWeap.GetSAmmo(1, 'Name'))
check('SWeap.SetSAmmo(1, Bogus)', lambda: SWeap.SetSAmmo(1, 'Bogus', 'x'))
check('SWeap.SetSAmmo(5, Name)', lambda: SWeap.SetSAmmo(5, 'Name', 'x'))
# SWeap.FillPriority() crashes the original too with tables made here but not filled by the weapon scripts
check('SWeap.GetFirstiWeap() after', lambda: SWeap.GetFirstiWeap())
check('SWeap.NetExec(Unknown)', lambda: SWeap.NetExec('Unknown'))
check('SWeap.NetExec(Unknown, x)', lambda: SWeap.NetExec('Unknown', 'x'))
check('SWeap.NetExec bad', lambda: SWeap.NetExec(1))
check('SWeap.GetFWeap bad', lambda: SWeap.GetFWeap('a', 'b'))
check('SWeap.SetSWeap bad', lambda: SWeap.SetSWeap(0, 'Name'))
# SAct: own test class (setting ModelFilePath loads the model: the original hangs on a missing file); GetClass of a missing class sets the current class to NULL, so nothing points to the
# class after DelClass. CreateClass/GetClass return None on bad arguments with the Python error left set:
# SVec.GetRotAng with a tuple clears it (PyErr_Clear between its two parse attempts).
check('SAct.GetCls(x) start', lambda: SAct.GetCls('x'))
check('SAct.CreateClass(SDTestClass)', lambda: SAct.CreateClass('SDTestClass'))
for name, value in (('Speed', 2.5), ('Model', 'x'), ('SoundDispX', 2.5), ('FlySpeed', -3.75), ('CanFly', 1),
		('CanJump', 2.9), ('DefAction', 'Walk'), ('Name', ''), ('Node1_2x', 0.5)):
	check('SAct.SetCls(%s, %s)' % (name, repr(value)), lambda n=name, v=value: SAct.SetCls(n, v))
for name in ('Speed', 'SoundDispX', 'sounddispx', 'FlySpeed', 'CanFly', 'CanJump', 'ModelFilePath', 'DefAction',
		'Name', 'WRadius', 'Missing'):
	check('SAct.GetCls(%s)' % name, lambda n=name: SAct.GetCls(n))
check('SAct.SetCls bad', lambda: SAct.SetCls('Speed', (1, 2)))
check('SAct.GetAct(x) no action', lambda: SAct.GetAct('x'))
check('SAct.SetAct(x, 1) no action', lambda: SAct.SetAct('x', 1))
check('SAct.CreateAction(Walk)', lambda: SAct.CreateAction('Walk'))
for name, value in (('Anim', 'walk01'), ('AnmName', 'walk01'), ('AnmFPS', 29.97), ('isLoop', 1), ('MaxSpeed', 0.125),
		('UseDisp', 3.7)):
	check('SAct.SetAct(%s, %s)' % (name, repr(value)), lambda n=name, v=value: SAct.SetAct(n, v))
for name in ('Anim', 'AnmName', 'AnmFPS', 'isLoop', 'MaxSpeed', 'UseDisp', 'RotSpeed', 'Missing'):
	check('SAct.GetAct(%s)' % name, lambda n=name: SAct.GetAct(n))
check('SAct.SetAct bad', lambda: SAct.SetAct('Anim', [1]))
check('SAct.GetAction(Missing)', lambda: SAct.GetAction('Missing'))
check('SAct.GetAction(Walk)', lambda: SAct.GetAction('Walk'))
check('SAct.GetAct(AnmName) again', lambda: SAct.GetAct('AnmName'))
check('SAct.GetClass(SDTestClass)', lambda: SAct.GetClass('SDTestClass'))
check('SAct.GetClass(NoSuchClass)', lambda: SAct.GetClass('NoSuchClass'))
check('SAct.GetCls(Speed) no class', lambda: SAct.GetCls('Speed'))
check('SAct.SetCls(Speed, 1) no class', lambda: SAct.SetCls('Speed', 1))
check('SAct.CreateAction(x) no class', lambda: SAct.CreateAction('x'))
check('SAct.GetAction(x) no class', lambda: SAct.GetAction('x'))
check('SAct.DelClass(SDTestClass)', lambda: SAct.DelClass('SDTestClass'))
check('SAct.DelClass(SDTestClass) again', lambda: SAct.DelClass('SDTestClass'))
check('SAct.GetClass(SDTestClass) deleted', lambda: SAct.GetClass('SDTestClass'))
check('SAct.CreateClass bad', lambda: SAct.CreateClass(1))
check('clear error', lambda: SVec.GetRotAng((0.0, 0.0, 1.0)))
check('SAct.DelClass bad', lambda: SAct.DelClass(None))
# SFX: functions that take entity names need a level (a missing entity fails a CHECK and ends the game), so
# only ShipExplosion and argument errors here (nflames -1 fails a CHECK inside the original effect code)
for args in (((0, 0, 0), (0, 0, 0), 1.0, 1.0, 0, 0), ((10.5, -3, 200), (1, 2, 3), 2.5, 0.25, 4, 1)):
	check('SFX.ShipExplosion%s' % (args,), lambda a=args: apply(SFX.ShipExplosion, a))
check('SFX.ShipExplosion bad', lambda: SFX.ShipExplosion((0, 0, 0), (0, 0, 0), 1.0, 1.0, 0))
check('SFX.CharacterConversor bad', lambda: SFX.CharacterConversor('a', 'b'))
check('SFX.CharacterConversion bad', lambda: SFX.CharacterConversion('a', 2, 1.0))
check('SFX.BishopSellLife bad', lambda: SFX.BishopSellLife('a'))
check('SFX.MoneyTransfer bad', lambda: SFX.MoneyTransfer('a', 'b', 'c'))
check('SFX.EmbeddedSet bad', lambda: SFX.EmbeddedSet(None, 'x'))
# SLogic: argument errors only, the real calls are in sdlevel.py
check('SLogic.SetCharState bad', lambda: SLogic.SetCharState('a', 'b', 'c'))
check('SLogic.SendSentinelToWatch bad', lambda: SLogic.SendSentinelToWatch((1, 2)))
check('SLogic.GearAttack bad', lambda: SLogic.GearAttack(5))
check('SLogic.IsEnemyActive bad', lambda: SLogic.IsEnemyActive(1))
check('SLogic.GetNearestShip bad', lambda: SLogic.GetNearestShip((1, 2, 3), 4))
check('SLogic.SetOnFloor bad', lambda: SLogic.SetOnFloor())
check('SLogic.Flash bad', lambda: SLogic.Flash((1, 2, 3)))
check('SLogic.SetShipToRegenerate bad', lambda: SLogic.SetShipToRegenerate('a', 1))
check('SLogic.SetDominationZones bad', lambda: SLogic.SetDominationZones())
check('SLogic.ChangeZoneState bad', lambda: SLogic.ChangeZoneState(1))
check('SLogic.ChangeBatonState bad', lambda: SLogic.ChangeBatonState((1, 2, 3), 'x'))
check('SLogic.SetCharState(Nobody)', lambda: SLogic.SetCharState('Nobody', 0, ''))
check('SLogic.SetShipToRegenerate(Nobody)', lambda: SLogic.SetShipToRegenerate('Nobody', 1, 2))
check('SLogic.UpdateTauntEndTime(5)', lambda: SLogic.UpdateTauntEndTime(5))
finish()
