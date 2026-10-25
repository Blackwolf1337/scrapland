# Menu checks: calls engine script functions and writes the results to sdtest.out (Python 1.5.2 syntax).
# tools/test/difftest.sh runs it with the C functions and with the originals and compares the outputs. The game
# reads it from its root directory through -PythonExecute:execfile(...); the engine's command line drops quotes
# and splits at spaces, so difftest.sh spells the file name with chr().
OUT = 'sdtest.out'
execfile('sdlib.py')
import Scrap, SWeap, SVec, SAct, SFX, SLogic, SSound, SInput, SAI, SNet, SScorer

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
# SSound: the test runs without an audio device (Wine audio switched off), the calls still go through the
# engine's sound code. OpenVoice and SetMusic load files, not here.
psvars = ('Sound', 'Loop', 'MaxInterval', 'MinInterval', 'AttenEnd', 'AttenIni', 'Doppler', 'Volume', 'Pitch', 'X',
	'Y', 'Z', 'Missing')
check('SSound.GetPS(SDTestPS, Volume) before', lambda: SSound.GetPS('SDTestPS', 'Volume'))
check('SSound.CreatePS(SDTestPS)', lambda: SSound.CreatePS('SDTestPS'))
for name, value in (('Sound', 'Explosion'), ('Loop', 1), ('MaxInterval', 2.5), ('Volume', 0.75), ('Pitch', 1.25),
		('X', -3.5), ('Missing', 1), ('Missing', 'x')):
	check('SSound.SetPS(SDTestPS, %s, %s)' % (name, repr(value)), lambda n=name, v=value: SSound.SetPS('SDTestPS', n, v))
for v in psvars:
	check('SSound.GetPS(SDTestPS, %s)' % v, lambda v=v: SSound.GetPS('SDTestPS', v))
check('SSound.SetPS(NoPS, Volume, 1)', lambda: SSound.SetPS('NoPS', 'Volume', 1))
check('SSound.SetPS(NoPS, Sound, x)', lambda: SSound.SetPS('NoPS', 'Sound', 'x'))
check('SSound.GetPS(NoPS, Volume)', lambda: SSound.GetPS('NoPS', 'Volume'))
check('SSound.UsePS(1)', lambda: SSound.UsePS(1))
check('SSound.UsePS(0)', lambda: SSound.UsePS(0))
# voice numbers are not range checked: VoiceRemain(99, ...) crashes the original
for args in ((0, 7.5), (3, -1.0)):
	check('SSound.VoiceRemain%s' % (args,), lambda a=args: apply(SSound.VoiceRemain, a))
check('SSound.SetVoiceVolume(0, 0.5)', lambda: SSound.SetVoiceVolume(0, 0.5))
check('SSound.SetVoiceString(0, hello)', lambda: SSound.SetVoiceString(0, 'hello'))
check('SSound.StopVoice(0)', lambda: SSound.StopVoice(0))
check('SSound.SetMusicVolume(0.3)', lambda: SSound.SetMusicVolume(0.3))
check('SSound.Play(SDNoSound)', lambda: SSound.Play('SDNoSound'))
check('SSound.Play(SDNoSound, 0.5, -0.25)', lambda: SSound.Play('SDNoSound', 0.5, -0.25))
check('SSound.PlaySound(0,0,0, SDNoSound, 1)', lambda: SSound.PlaySound((0, 0, 0), 'SDNoSound', 1.0))
check('SSound.PlaySound full', lambda: SSound.PlaySound((1, 2, 3), 'SDNoSound', 0.5, 0.25, 0.75, 2.0))
check('SSound.StopAllSounds()', lambda: SSound.StopAllSounds())
check('SSound.Play bad', lambda: SSound.Play())
check('SSound.PlaySound bad', lambda: SSound.PlaySound((0, 0), 'x', 1.0))
check('SSound.SetPS bad', lambda: SSound.SetPS('a', 'b'))
check('SSound.GetPS bad', lambda: SSound.GetPS(1, 'b'))
check('SSound.VoiceRemain bad', lambda: SSound.VoiceRemain('x', 1.0))
check('SSound.OpenVoice bad', lambda: SSound.OpenVoice('x'))
check('SSound.PlayVoice bad', lambda: SSound.PlayVoice('x'))
check('SSound.SetMusic bad', lambda: SSound.SetMusic(1))
# SInput: player numbers are checked unsigned (4 and -1 are both out). CheckPadButton(4, ..) fails a CHECK.
aset = SInput.GetActionSet()
check('SInput.GetActionSet()', lambda: SInput.GetActionSet())
check('SInput.SetActionSet(current)', lambda a=aset: SInput.SetActionSet(a))
check('SInput.SetActionSet(NoSet)', lambda: SInput.SetActionSet('NoSet'))
for i in (0, 1, 3, 4, -1):
	check('SInput.GetString(%d)' % i, lambda i=i: SInput.GetString(i))
	check('SInput.GetCursorChar(%d)' % i, lambda i=i: SInput.GetCursorChar(i))
	check('SInput.SetString(%d)' % i, lambda i=i: SInput.SetString(i, 'text%d' % i))
	check('SInput.GetString(%d) set' % i, lambda i=i: SInput.GetString(i))
	check('SInput.SetInputFunc(%d)' % i, lambda i=i: SInput.SetInputFunc(i, ''))
	check('SInput.Rumble(%d)' % i, lambda i=i: SInput.Rumble(i, 0.0, 0.0, 0.0))
check('SInput.GetVirtualKeyboard()', lambda: SInput.GetVirtualKeyboard())
check('SInput.SetVirtualKeyboard(hello)', lambda: SInput.SetVirtualKeyboard('hello'))
check('SInput.GetVirtualKeyboard() set', lambda: SInput.GetVirtualKeyboard())
for a in (aset, 'NoSet'):
	for b, c in (('Fire', 'Key'), ('NoAction', 'NoDev')):
		check('SInput.GetEntry(%s, %s, %s)' % (a, b, c), lambda a=a, b=b, c=c: SInput.GetEntry(a, b, c))
		check('SInput.GetDefinedEntry(0, %s, %s, %s)' % (a, b, c), lambda a=a, b=b, c=c: SInput.GetDefinedEntry(0, a, b, c))
		check('SInput.GetDefinedEntry(0, %s, %s, %s, 1)' % (a, b, c), lambda a=a, b=b, c=c: SInput.GetDefinedEntry(0, a, b, c, 1))
	check('SInput.GetDefinedList(0, %s, Fire)' % a, lambda a=a: SInput.GetDefinedList(0, a, 'Fire'))
	check('SInput.GetDefinedList(1, %s, Fire, 1)' % a, lambda a=a: SInput.GetDefinedList(1, a, 'Fire', 1))
check('SInput.ListenToDefine(0, ...)', lambda a=aset: SInput.ListenToDefine(0, a, 'Fire', 'Key'))
check('SInput.AbortListenToDefine()', lambda: SInput.AbortListenToDefine())
check('SInput.ResetToSplit()', lambda: SInput.ResetToSplit())
check('SInput.CheckPadButton(0, 1)', lambda: SInput.CheckPadButton(0, 1))
check('SInput.CheckPadButton(3, 99)', lambda: SInput.CheckPadButton(3, 99))
check('SInput.SetActionSet bad', lambda: SInput.SetActionSet(1))
check('SInput.GetEntry bad', lambda: SInput.GetEntry('a', 'b'))
check('SInput.AssingEntry bad', lambda: SInput.AssingEntry('a', 'b', 'c', 'd', 'e'))
check('SInput.GetDefinedList bad', lambda: SInput.GetDefinedList('a', 'b', 'c'))
check('SInput.Rumble bad', lambda: SInput.Rumble(0, 1, 2))
check('SInput.Bind bad', lambda: SInput.Bind(0, 'a', 'b'))
check('SInput.SetVirtualKeyboard bad', lambda: SInput.SetVirtualKeyboard(None))
# SAI: argument errors only (no AI objects in the menu)
check('SAI.IniAI bad', lambda: SAI.IniAI(1, 'x'))
check('SAI.BuildGraph bad', lambda: SAI.BuildGraph('x', 1, 2, 3))
check('SAI.AnalizeMap bad', lambda: SAI.AnalizeMap('x'))
check('SAI.AnalizeTraffic bad', lambda: SAI.AnalizeTraffic(1))
check('SAI.GetStateVehicle bad', lambda: SAI.GetStateVehicle(1))
check('SAI.GetStateChar(Nobody)', lambda: SAI.GetStateChar('Nobody'))
check('SAI.EnableAIVehicle bad', lambda: SAI.EnableAIVehicle('x', 1))
check('SAI.EnableAIChar bad', lambda: SAI.EnableAIChar('x'))
check('SAI.GetNextRacePoint bad', lambda: SAI.GetNextRacePoint((1, 2), 1, 2))
check('SAI.GetNearestItemLife bad', lambda: SAI.GetNearestItemLife(None))
check('SAI.GetReposCharPos bad', lambda: SAI.GetReposCharPos((1, 2, 3)))
check('SAI.AddVehicleRace(Nobody)', lambda: SAI.AddVehicleRace('Nobody'))
check('SAI.SetRotStaticObj bad', lambda: SAI.SetRotStaticObj('x', 1))
check('SAI.SetInertia(Nobody)', lambda: SAI.SetInertia('Nobody', 1))
check('SAI.SetStateVehicle()', lambda: SAI.SetStateVehicle())
check('clear error', lambda: SVec.GetRotAng((0.0, 0.0, 1.0)))
check('SAI.SetStateVehicle(10)', lambda: SAI.SetStateVehicle(10))
check('SAI.GetOD()', lambda: SAI.GetOD())
# SNet in the menu (no server, no client). Nothing that sends over the network: no InitServer, InitClient,
# InitBrowser, PingInetSvrs, SendMasterString.
for f in ('IsServer', 'IsClient', 'IsDedicated', 'IsMaster', 'GetServerData', 'GetClientData', 'GetMyClientShip',
		'GetBotName', 'GetObjName'):
	check('SNet.%s()' % f, lambda f=getattr(SNet, f): f())
check('SNet.GetPing(Player0)', lambda: SNet.GetPing('Player0'))
check('SNet.GetPing(Nobody)', lambda: SNet.GetPing('Nobody'))
check('SNet.Kick(Nobody)', lambda: SNet.Kick('Nobody'))
check('SNet.SendChatString(0, hi)', lambda: SNet.SendChatString(0, 'hi'))
check('SNet.SendCtrlString(x)', lambda: SNet.SendCtrlString('x'))
check('SNet.SendUsrString(0, x)', lambda: SNet.SendUsrString(0, 'x'))
check('SNet.ModifyUsrData(1)', lambda: SNet.ModifyUsrData(1))
check('SNet.ServerChangeLevel(x)', lambda: SNet.ServerChangeLevel('x'))
check('SNet.AddResource(x)', lambda: SNet.AddResource('x'))
check('SNet.CloseServer(bye)', lambda: SNet.CloseServer('bye'))
check('SNet.CloseClient()', lambda: SNet.CloseClient())
check('SNet.CloseClient(bye)', lambda: SNet.CloseClient('bye'))
check('SNet.DoneBrowser()', lambda: SNet.DoneBrowser())
check('SNet.InitServer bad', lambda: SNet.InitServer('a', 1))
check('SNet.InitClient bad', lambda: SNet.InitClient(1, 1))
check('SNet.InitBrowser bad', lambda: SNet.InitBrowser('x'))
check('SNet.SendChatString bad', lambda: SNet.SendChatString('x', 0))
check('SNet.GetPing bad', lambda: SNet.GetPing())

# SScorer in the menu (the scorer of player 0 is the menu)
for i in (0, 1, 3, 4, -1):
	check('SScorer.GetDefault(%d)' % i, lambda i=i: SScorer.GetDefault(i))
	check('SScorer.GetActual(%d)' % i, lambda i=i: SScorer.GetActual(i))
	check('SScorer.GetMenuAccept(%d)' % i, lambda i=i: SScorer.GetMenuAccept(i))
cur = SScorer.GetActual(0)
if cur:
	check('SScorer.GetItem(0, actual)', lambda c=cur: SScorer.GetItem(0, c) is not None)
	for v in ('Text', 'X', 'Y', 'Visible', 'Alpha', 'Font', 'Missing'):
		check('SScorer.Get(0, actual, %s)' % v, lambda c=cur, v=v: SScorer.Get(0, c, v))
check('SScorer.GetItem(0, NoItem)', lambda: SScorer.GetItem(0, 'NoItem'))
check('SScorer.New(0, SDItem, Text)', lambda: SScorer.New(0, 'SDItem', 'Text') is not None)
check('SScorer.Add(0, SDItem2, Text)', lambda: SScorer.Add(0, 'SDItem2', 'Text'))
check('SScorer.Add(5, x, Text)', lambda: SScorer.Add(5, 'x', 'Text'))
check('SScorer.New(0, SDBad, NoType)', lambda: SScorer.New(0, 'SDBad', 'NoType'))
# cPyMenuEntity attributes (tp_getattr/tp_setattr) and tp_print. Not here: a value of another type than the
# variable (the original passes the other values uninitialized).
mi = SScorer.GetItem(0, 'SDItem')
for v in ('Text', 'X', 'Y', 'Visible', 'Alpha', 'Font', 'Missing'):
	check('SDItem.%s' % v, lambda v=v, mi=mi: getattr(mi, v))
for name, value in (('Text', 'Hi'), ('X', 0.5), ('Y', 7), ('Missing', 1), ('Missing', 2.5), ('Missing', 'x'),
		('X', []), ('Text', None)):
	check('SDItem.%s = %s' % (name, repr(value)), lambda n=name, v=value, mi=mi: setattr(mi, n, v))
for v in ('Text', 'X', 'Y'):
	check('SDItem.%s set' % v, lambda v=v, mi=mi: getattr(mi, v))
sys.stdout = out
print mi
sys.stdout = sys.__stdout__
for name, value in (('Text', 'Hello'), ('X', 0.25), ('Y', 3), ('Missing', 1), ('Missing', 'x')):
	check('SScorer.Set(0, SDItem, %s, %s)' % (name, repr(value)), lambda n=name, v=value: SScorer.Set(0, 'SDItem', n, v))
for v in ('Text', 'X', 'Y', 'Missing'):
	check('SScorer.Get(0, SDItem, %s)' % v, lambda v=v: SScorer.Get(0, 'SDItem', v))
check('SScorer.Set(0, NoItem, Text, x)', lambda: SScorer.Set(0, 'NoItem', 'Text', 'x'))
check('SScorer.Set(4, SDItem, X, 1)', lambda: SScorer.Set(4, 'SDItem', 'X', 1))
check('SScorer.SetCursor(0, SDItem)', lambda: SScorer.SetCursor(0, 'SDItem'))
check('SScorer.SetCursor(4, SDItem)', lambda: SScorer.SetCursor(4, 'SDItem'))
check('SScorer.SetDefault(0, SDItem)', lambda: SScorer.SetDefault(0, 'SDItem'))
check('SScorer.GetDefault(0) set', lambda: SScorer.GetDefault(0))
check('SScorer.Hide(0)', lambda: SScorer.Hide(0))
check('SScorer.Show(0)', lambda: SScorer.Show(0))
check('SScorer.Show(7)', lambda: SScorer.Show(7))
check('SScorer.SetOnCancel(4, x)', lambda: SScorer.SetOnCancel(4, 'x'))
check('SScorer.SetOnNext(4, x)', lambda: SScorer.SetOnNext(4, 'x'))
check('SScorer.SetMPFunc(4, x)', lambda: SScorer.SetMPFunc(4, 'x'))
check('SScorer.SetMarkerSprite(4, x)', lambda: SScorer.SetMarkerSprite(4, 'x'))
check('SScorer.GetTextArea(Horatio, Hello)', lambda: SScorer.GetTextArea('Horatio', 'Hello world'))
check('SScorer.GetTextArea(Horatio, empty)', lambda: SScorer.GetTextArea('Horatio', ''))
check('SScorer.Fade(5, ...)', lambda: SScorer.Fade(5, (0, 0, 0, 0), (255, 255, 255, 255), 1.0))
check('SScorer.SetCinema(5, 1, 1)', lambda: SScorer.SetCinema(5, 1, 1.0))
check('SScorer.Clear(6)', lambda: SScorer.Clear(6))
check('SScorer.AddModel(6, ...)', lambda: SScorer.AddModel(6, 'a', 'b', '', 1, 1.0))
check('SScorer.Set bad', lambda: SScorer.Set(0, 'a', 'b'))
check('SScorer.Fade bad', lambda: SScorer.Fade(0, (1, 2, 3), (1, 2, 3, 4), 1.0))
check('SScorer.AddChatMsg bad', lambda: SScorer.AddChatMsg('x', 1, 2))
check('SScorer.Get2DPos bad', lambda: SScorer.Get2DPos((1, 2)))
check('SScorer.SetSpeechText bad', lambda: SScorer.SetSpeechText('x', 1.0))

# Scrap, small functions (batch 2) in the menu. Not here: Exit, ExtractPack (writes files), Achievement
# (launcher), ClosePack, BuildFont, CreateElements, Execute, DebugInput.
for f in ('GetTimeSpeed', 'GetNetFlags', 'GetNewLevelPath', 'GetLanguage', 'GetGogID', 'IsSteam', 'GetSteamID',
		'GetSteamTicket', 'GetFreeBlocks', 'GetSavedProfilesList', 'GetVideoCurrentMode', 'DeselectProfile'):
	check('Scrap.%s()' % f, lambda f=getattr(Scrap, f): f())
for k in ('SDNoString', 'Menu_NewGame', ''):
	check('Scrap.GetLangStr(%s)' % k, lambda k=k: Scrap.GetLangStr(k))
check('Scrap.GetLangEntries(SDNo)', lambda: Scrap.GetLangEntries('SDNo'))
check('Scrap.GetLangEntries(SDNo, 1)', lambda: Scrap.GetLangEntries('SDNo', 1))
# real ids of Language/English.txt: the walk in file order while the id keeps its prefix (id minus 4 characters)
for k in ('Mission_Decontamination_RemoteMessage1', 'Mission_Decontamination_SystemMessage1',
		'Mission_Decontamination_GateKeeper_InitMsg1', 'Mission_Decontamination_Hint1'):
	for n in (1, 2, 3):
		check('Scrap.GetLangEntries(%s, %d)' % (k, n), lambda k=k, n=n: Scrap.GetLangEntries(k, n))
	check('Scrap.GetLangStr(%s)' % k, lambda k=k: Scrap.GetLangStr(k))
for v in ('FullScreen', 'VideoWidth', 'SDNoVar'):
	check('Scrap.Des(%s)' % v, lambda v=v: Scrap.Des(v))
check('Scrap.CreateSaveVar(SDVar, abc)', lambda: Scrap.CreateSaveVar('SDVar', 'abc'))
check('Scrap.GetSaveVar(SDVar)', lambda: Scrap.GetSaveVar('SDVar'))
check('Scrap.CreateSaveVar(SDNum, 7)', lambda: Scrap.CreateSaveVar('SDNum', '7'))
check('Scrap.IncSaveVar(SDNum)', lambda: Scrap.IncSaveVar('SDNum'))
check('Scrap.IncSaveVar(SDNum, 5)', lambda: Scrap.IncSaveVar('SDNum', 5))
check('Scrap.GetSaveVar(SDNum)', lambda: Scrap.GetSaveVar('SDNum'))
check('Scrap.DelSaveVars(SD)', lambda: Scrap.DelSaveVars('SD'))
check('Scrap.GetSaveVar(SDNum) after', lambda: Scrap.GetSaveVar('SDNum'))
check('Scrap.GetSaveGamesList(Save)', lambda: Scrap.GetSaveGamesList('Save'))
check('Scrap.CreateSaveProfile(x)', lambda: Scrap.CreateSaveProfile('x'))
check('Scrap.DeleteProfile(x)', lambda: Scrap.DeleteProfile('x'))
check('Scrap.LaunchDashboard(0)', lambda: Scrap.LaunchDashboard(0))
check('Scrap.ConsoleOut(x)', lambda: Scrap.ConsoleOut('sdtest out\n'))
check('Scrap.ConsoleError(x)', lambda: Scrap.ConsoleError('sdtest error\n'))
check('Scrap.AddParami(1)', lambda: Scrap.AddParami(1))
check('Scrap.AddParamf(2.5)', lambda: Scrap.AddParamf(2.5))
check('Scrap.AddParams(x)', lambda: Scrap.AddParams('x'))
check('Scrap.AddScheduledFunc(far)', lambda: Scrap.AddScheduledFunc(100000.0, check, ('x', None), 'SDSched'))
check('Scrap.DeleteScheduledFuncs(SDSched)', lambda: Scrap.DeleteScheduledFuncs('SDSched'))
check('Scrap.Achievement(-1)', lambda: Scrap.Achievement(-1))
check('Scrap.ListModels()', lambda: Scrap.ListModels())
check('Scrap.AddParamf bad', lambda: Scrap.AddParamf('x'))
check('Scrap.AddScheduledFunc bad', lambda: Scrap.AddScheduledFunc(1.0, 2))
check('Scrap.GetCam bad', lambda: Scrap.GetCam('x'))
check('Scrap.IncSaveVar bad', lambda: Scrap.IncSaveVar(1))
check('Scrap.GetTime()', lambda: Scrap.GetTime())

# Scrap, small functions (batch 3) in the menu. Not here: anything that loads or writes files or changes the
# level (LoadLevel, SaveGameVars, SaveConfig, ScreenShot, PythonCompileAll, players, Preload*, ProcessDVF,
# ModelInfo, OpenPack, SetVideoCurrentMode).
for v in (0.0, 0.49, 0.5, 0.51, 1.5, 2.5, -0.5, -0.49, -1.5, -2.7, 1e9, -1e9, 123.456):
	check('Scrap.Round(%s)' % v, lambda v=v: Scrap.Round(v))
check('Scrap.Ver()', lambda: Scrap.Ver())
check('Scrap.SetMoney(1234)', lambda: Scrap.SetMoney(1234))
check('Scrap.GetMoney() set', lambda: Scrap.GetMoney())
check('Scrap.SetSaveVar(SDSet, x)', lambda: Scrap.SetSaveVar('SDSet', 'x'))
check('Scrap.CreateSaveVar(SDSet, a)', lambda: Scrap.CreateSaveVar('SDSet', 'a'))
check('Scrap.SetSaveVar(SDSet, b)', lambda: Scrap.SetSaveVar('SDSet', 'b'))
check('Scrap.GetSaveVar(SDSet)', lambda: Scrap.GetSaveVar('SDSet'))
# Scrap.LoadGameVars loads a save file: not in the test (a missing one does not return)
check('Scrap.SetTimeSpeed(1.0)', lambda: Scrap.SetTimeSpeed(1.0))
check('Scrap.GetTimeSpeed() set', lambda: Scrap.GetTimeSpeed())
check('Scrap.SetCallFunc(SDNoFunc)', lambda: Scrap.SetCallFunc('SDNoFunc'))
# the call builder (cPy_Function): Python functions of __main__, the registry of C functions by "!name"
sdcalls = []
def sdcall(*args):
	sdcalls.append(args)
	return 7
def sdraise(*args):
	raise ValueError, 'sdraise'
sdnotcallable = 1
check('Scrap.SetCallFunc(__main__.sdcall)', lambda: Scrap.SetCallFunc('__main__.sdcall'))
check('Scrap.AddParami(1) call', lambda: Scrap.AddParami(1))
check('Scrap.AddParamf(2.5) call', lambda: Scrap.AddParamf(2.5))
check('Scrap.AddParams(x) call', lambda: Scrap.AddParams('x'))
check('Scrap.AddParamf(0.1) call', lambda: Scrap.AddParamf(0.1))
check('Scrap.Execute()', lambda: Scrap.Execute())
check('sdcall args', lambda: sdcalls[-1])
check('Scrap.Execute() again', lambda: Scrap.Execute())
check('sdcall args again', lambda: sdcalls[-1])
check('sdcall count', lambda: len(sdcalls))
check('Scrap.SetCallFunc(__main__.sdraise)', lambda: Scrap.SetCallFunc('__main__.sdraise'))
check('Scrap.Execute() raise', lambda: Scrap.Execute())
check('Scrap.SetCallFunc(Scrap.GetTimeSpeed)', lambda: Scrap.SetCallFunc('Scrap.GetTimeSpeed'))
check('Scrap.Execute() builtin', lambda: Scrap.Execute())
for f in ('SDNoDot', 'SDNoModule.f', '__main__.SDNoFunc', '__main__.sdnotcallable', '!SDNoNative', '!AAA', '!zzz',
		'!PrintMemBlockData', '!TakeLife', '', '.x', 'x.', '__main__.sdcall'):
	check('Scrap.SetCallFunc(%s)' % f, lambda f=f: Scrap.SetCallFunc(f))
# usage of the Python block pools into the log (the native function prints every pool)
check('Scrap.SetCallFunc(!PrintMemBlockData) run', lambda: Scrap.SetCallFunc('!PrintMemBlockData'))
check('Scrap.Execute() PrintMemBlockData', lambda: Scrap.Execute())
check('Scrap.SetCallFunc() clear', lambda: Scrap.SetCallFunc(''))
check('Scrap.Execute() cleared', lambda: Scrap.Execute())
check('sdcall count after', lambda: len(sdcalls))
check('Scrap.Print(x)', lambda: Scrap.Print('sdtest print\n'))
check('Scrap.Verbose(x)', lambda: Scrap.Verbose('sdtest verbose\n'))
check('Scrap.ShowGVars(SDNo)', lambda: Scrap.ShowGVars('SDNo'))
for i in (0, 1, 3, 4, -1):
	check('Scrap.UsrEntity(%d)' % i, lambda i=i: (lambda e: e and e.Name)(Scrap.UsrEntity(i)))
check('Scrap.OpenStore(-1)', lambda: Scrap.OpenStore(-1))
check('Scrap.OpenStore(1)', lambda: Scrap.OpenStore(1))
check('Scrap.SetLanguage(current)', lambda: Scrap.SetLanguage(Scrap.GetLanguage()))
check('Scrap.SetAlarm bad', lambda: Scrap.SetAlarm('x'))
check('Scrap.Round bad', lambda: Scrap.Round('x'))
check('Scrap.MusicPlayer bad', lambda: Scrap.MusicPlayer(1))
check('Scrap.UsrEntity bad', lambda: Scrap.UsrEntity('x'))

# Scrap batch 4 in the menu (CreateEntity and RunApp not here)
for v in ('FullScreen', 'VideoWidth', 'VideoGammaRGB', 'R_LastDevice', 'PythonExecute', 'ClientDelRes', 'Pad1', 'SDNoVar'):
	check('Scrap.Get(%s)' % v, lambda v=v: Scrap.Get(v))
	check('Scrap.Def(%s)' % v, lambda v=v: Scrap.Def(v))
for name, value in (('VideoGammaRGB', 1.25), ('VideoWidth', 1024.7), ('R_LastDevice', 'SDDevice'), ('FullScreen', 'x'),
		('R_LastDevice', 3), ('SDNoVar', 1), ('SDNoVar', 'x')):
	check('Scrap.Set(%s, %s)' % (name, repr(value)), lambda n=name, v=value: Scrap.Set(n, v))
	check('Scrap.Get(%s) after Set' % name, lambda n=name: Scrap.Get(n))
for a, b in ((1.0, 1.0), (2.5, 2.5), (-3.0, -3.0), (0.0, 0.0)):
	check('Scrap.Rand(%s, %s)' % (a, b), lambda a=a, b=b: Scrap.Rand(a, b))
check('Scrap.Rand(0, 10) type', lambda: type(Scrap.Rand(0.0, 10.0)).__name__)
for f in ('Scrap.cfg', 'Bin/Scrap.exe', 'Levels/FZ/Map/FZ.ini', 'Scripts/Init.py', 'SDNoFile.txt', ''):
	check('Scrap.FileExist(%s)' % f, lambda f=f: Scrap.FileExist(f))
check('Scrap.GetEntity(SDNoEntity)', lambda: Scrap.GetEntity('SDNoEntity'))
check('Scrap.GetEntity()', lambda: Scrap.GetEntity(''))
check('Scrap.IsApp(-1)', lambda: Scrap.IsApp(-1))
check('Scrap.IsApp(897610)', lambda: Scrap.IsApp(897610))
check('Scrap.Set bad', lambda: Scrap.Set('x'))
check('Scrap.Rand bad', lambda: Scrap.Rand(1))
check('Scrap.CreateEntity bad', lambda: Scrap.CreateEntity('a', 1, 2, 3))
check('Scrap.RunApp bad', lambda: Scrap.RunApp('a'))

# Scrap batch 5 in the menu
# mode numbers are not range checked: GetVideoModes(50) crashes the original (32 modes here)
for i in (-1, 0, 1, 2, 31):
	check('Scrap.GetVideoModes(%d)' % i, lambda i=i: Scrap.GetVideoModes(i))
check('Scrap.SetSaveProfile(a, b)', lambda: Scrap.SetSaveProfile('a', 'b'))
check('Scrap.EntityListGet(SDNoList, x)', lambda: Scrap.EntityListGet('SDNoList', 'x'))
check('Scrap.EntityListSet(SDNoList, x, y)', lambda: Scrap.EntityListSet('SDNoList', 'x', 'y'))
check('Scrap.EntityListGet bad', lambda: Scrap.EntityListGet('a'))
check('Scrap.AddParkingZone bad', lambda: Scrap.AddParkingZone(1, (1, 2), 'x'))
check('Scrap.GetNearestParked bad', lambda: Scrap.GetNearestParked('x'))
check('Scrap.SetGrid bad', lambda: Scrap.SetGrid(1, 2))
check('Scrap.Preload3DObject bad', lambda: Scrap.Preload3DObject())
# save game round trip in the config directory of the test prefix (SDTest.sav): write, delete all, load
check('Scrap.CreateSaveVar(SDSave, 42)', lambda: Scrap.CreateSaveVar('SDSave', '42'))
check('Scrap.SaveGameVars(Save/SDTest.sav)', lambda: Scrap.SaveGameVars('Save/SDTest.sav', 'sdtest desc'))
check('Scrap.DelSaveVars()', lambda: Scrap.DelSaveVars(''))
check('Scrap.GetSaveVar(SDSave) deleted', lambda: Scrap.GetSaveVar('SDSave'))
check('Scrap.LoadGameVars(Save/SDTest.sav)', lambda: Scrap.LoadGameVars('Save/SDTest.sav'))
check('Scrap.GetSaveVar(SDSave) loaded', lambda: Scrap.GetSaveVar('SDSave'))
check('Scrap.IncSaveVar(SDSave, 8)', lambda: Scrap.IncSaveVar('SDSave', 8))
finish()
