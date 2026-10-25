# Level checks: run in the multiplayer level FZ (difftest.sh starts the game with -imap:Levels/FZ, PythonExecute
# runs after the level is loaded). Real weapon tables, action classes and entities; Player0 is the local player
# (a WalkChar with life). Python 1.5.2 syntax.
OUT = 'sdlevel.out'
execfile('sdlib.py')
import Scrap, SWeap, SVec, SAct, SFX, SLogic, SAI, SNet, SScorer

check('Scrap.GetLevelPath()', lambda: Scrap.GetLevelPath())
check('Scrap.GetMoney()', lambda: Scrap.GetMoney())

# SWeap with the tables the weapon scripts filled
check('SWeap.GetNWeap()', lambda: SWeap.GetNWeap())
check('SWeap.GetNAmmo()', lambda: SWeap.GetNAmmo())
check('SWeap.GetFirstiWeap()', lambda: SWeap.GetFirstiWeap())
for i in range(SWeap.GetNWeap()):
	for v in ('MinAmmo', 'iPrev', 'iNext', 'iAmmo', 'Upgrd00', 'Upgrd01', 'Upgrd02', 'Upgrd03', 'Bogus'):
		check('SWeap.GetFWeap(%d, %s)' % (i, v), lambda i=i, v=v: SWeap.GetFWeap(i, v))
	for v in ('Name', 'OnPress', 'OnRelease', 'OnFire', 'OnHit', 'OnStart', 'OnEnd', 'SetFx', 'Bogus'):
		check('SWeap.GetSWeap(%d, %s)' % (i, v), lambda i=i, v=v: SWeap.GetSWeap(i, v))
for i in range(SWeap.GetNAmmo()):
	for v in ('Max', 'Charger', 'Bogus'):
		check('SWeap.GetFAmmo(%d, %s)' % (i, v), lambda i=i, v=v: SWeap.GetFAmmo(i, v))
	for v in ('Name', 'OnPress', 'OnRelease', 'OnFire', 'OnHit', 'Bogus'):
		check('SWeap.GetSAmmo(%d, %s)' % (i, v), lambda i=i, v=v: SWeap.GetSAmmo(i, v))
check('SWeap.FillPriority()', lambda: SWeap.FillPriority())
check('SWeap.GetFirstiWeap() after', lambda: SWeap.GetFirstiWeap())
check('SWeap.SetFWeap(1, MinAmmo, 3)', lambda: SWeap.SetFWeap(1, 'MinAmmo', 3))
check('SWeap.GetFWeap(1, MinAmmo) set', lambda: SWeap.GetFWeap(1, 'MinAmmo'))
check('SWeap.SetFAmmo(1, Max, 500)', lambda: SWeap.SetFAmmo(1, 'Max', 500))
check('SWeap.GetFAmmo(1, Max) set', lambda: SWeap.GetFAmmo(1, 'Max'))

# SAct with a class of the game
classvars = ('Name', 'ModelFilePath', 'DefAction', 'SoundDispX', 'SoundDispY', 'SoundDispZ', 'FallSound',
	'FallSoundVol', 'CanFly', 'CanJump', 'FlySpeed', 'FlyAccel', 'SRadius', 'VRadius', 'WRadius', 'TurnAngLimit',
	'OnCreate', 'ActionPress', 'Missing')
actvars = ('Name', 'AnmName', 'AnmFilePath', 'AnmFPS', 'AnmTotalTime', 'isLoop', 'MaxSpeed', 'MinSpeed',
	'RotSpeed', 'InterpTime', 'PriorityLevel', 'UseDisp', 'Missing')
check('SAct.GetClass(Dtritus)', lambda: SAct.GetClass('Dtritus'))
for v in classvars:
	check('SAct.GetCls(%s)' % v, lambda v=v: SAct.GetCls(v))
defaction = SAct.GetCls('DefAction')
if type(defaction) == type(''):
	check('SAct.GetAction(DefAction)', lambda d=defaction: SAct.GetAction(d))
	for v in actvars:
		check('SAct.GetAct(%s)' % v, lambda v=v: SAct.GetAct(v))

# SFX on the player; BishopSellLife takes an empty attacker
pos = Scrap.UsrEntity(0).Pos
check('SFX.ShipExplosion at player', lambda p=pos: SFX.ShipExplosion(p, (0, 0, 1), 1.0, 0.5, 2, 0))
check('SFX.MoneyTransfer(Player0, Player0)', lambda: SFX.MoneyTransfer('Player0', 'Player0', 1.0))
check('SFX.BishopSellLife(, Player0)', lambda: SFX.BishopSellLife('', 'Player0'))
check('SFX.BishopSellLife(Player0, Player0)', lambda: SFX.BishopSellLife('Player0', 'Player0'))
check('SFX.CharacterConversion(Player0, Player0)', lambda: SFX.CharacterConversion('Player0', 'Player0', 0.5))
check('SFX.CharacterConversor(Player0, Player0)', lambda: SFX.CharacterConversor('Player0', 'Player0', 1))
check('Scrap.GetMoney() after', lambda: Scrap.GetMoney())
check('player life', lambda: Scrap.UsrEntity(0).Life)
# cPyEntity methods on the player and on the first entity of the world. Del only marks the entity, it stays
# until later, so the handle still works after it. SetModel/SetFX not here: loading a missing model hangs the
# original.
p = Scrap.UsrEntity(0)
for t in ('WalkChar', 'WithLife', 'walkchar', 'Car', ''):
	check('Player0.IsType(%s)' % t, lambda t=t, p=p: p.IsType(t))
# only directions without the hovering vehicle "Parked" as target: its position depends on the time since the
# level started, so those results differ from run to run even with the same code
for args in ((90, 45, 1), (0, 180, 100000)):
	check('Player0.GetTgAng%s' % (args,), lambda a=args, p=p: apply(p.GetTgAng, a))
for args in ((0, 0, 1000), (10, -20, 5000), (90, 45, 1)):
	check('Player0.DrawWeap%s' % (args,), lambda a=args, p=p: apply(p.DrawWeap, a))
check('Player0.IsType bad', lambda p=p: p.IsType(1))
check('Player0.GetTgAng bad', lambda p=p: p.GetTgAng(1, 2))
f = Scrap.GetFirst()
check('first.Name', lambda f=f: f.Name)
check('first.IsType(WithLife)', lambda f=f: f.IsType('WithLife'))
check('first.Del()', lambda f=f: f.Del())
check('first.Del() again', lambda f=f: f.Del())
check('first.IsType(WithLife) after Del', lambda f=f: f.IsType('WithLife'))
check('first.GetTgAng after Del', lambda f=f: f.GetTgAng(0, 0, 10))
check('first.DrawWeap after Del', lambda f=f: f.DrawWeap(0, 0, 10))
# SLogic in the level
pp = Scrap.UsrEntity(0).Pos
check('SLogic.IsEnemyActive()', lambda: SLogic.IsEnemyActive())
check('SLogic.GetNearestShip(player)', lambda p=pp: SLogic.GetNearestShip(p))
check('SLogic.GetNearestShip(player, Parked)', lambda p=pp: SLogic.GetNearestShip(p, 'Parked'))
check('SLogic.GetNearestShip(0,0,0)', lambda: SLogic.GetNearestShip((0, 0, 0)))
check('SLogic.SetOnFloor(Player0)', lambda: SLogic.SetOnFloor('Player0'))
check('SLogic.Flash(player, 100)', lambda p=pp: SLogic.Flash(p, 100))
check('SLogic.SetCharState(Nobody)', lambda: SLogic.SetCharState('Nobody', 0, ''))
check('SLogic.SetShipToRegenerate(Parked)', lambda: SLogic.SetShipToRegenerate('Parked', 1.0, 2.0))
check('SLogic.SetShipToRegenerate(Player0)', lambda: SLogic.SetShipToRegenerate('Player0', 1.0, 2.0))
check('SLogic.SendSentinelToWatch(player)', lambda p=pp: SLogic.SendSentinelToWatch(p))
check('SLogic.GearAttack(Player0)', lambda: SLogic.GearAttack('Player0'))
check('SLogic.UpdateTauntEndTime()', lambda: SLogic.UpdateTauntEndTime())
check('SLogic.ChangeZoneState(0, 1)', lambda: SLogic.ChangeZoneState(0, 1))
check('SLogic.ChangeBatonState(player, 1)', lambda p=pp: SLogic.ChangeBatonState(p, 1))
check('SLogic.SetDominationZones([Player0])', lambda: SLogic.SetDominationZones(['Player0']))
# SAI in the outdoor level FZ (IsMapOutDoor 1: no character group and no 2D graph, so no GetReposCharPos and no
# BuildGraph2D). IniAI/AnalizeMap/BuildGraph rebuild the graphs, not here. GetRandomVisibilityPoint uses the CRT
# rand, whose sequence differs between runs, so only the form of its result is compared.
def shape(v):
	if type(v) == type(()):
		r = ()
		for x in v:
			r = r + (type(x).__name__,)
		return r
	return type(v).__name__
check('SAI.GetRandomVisibilityPoint() shape', lambda: shape(SAI.GetRandomVisibilityPoint()))
check('SAI.GetStateVehicle(Player0)', lambda: SAI.GetStateVehicle('Player0'))
check('SAI.GetStateChar(Player0)', lambda: SAI.GetStateChar('Player0'))
check('SAI.GetStateVehicle(Parked)', lambda: SAI.GetStateVehicle('Parked'))
check('SAI.GetNearestItemLife(Player0)', lambda: SAI.GetNearestItemLife('Player0'))
check('SAI.GetNearestItemLife(Parked)', lambda: SAI.GetNearestItemLife('Parked'))
check('SAI.GetNextRacePoint(player)', lambda p=pp: SAI.GetNextRacePoint(p, 100, 1000))
check('SAI.SetStateVehicle(99)', lambda: SAI.SetStateVehicle(99))
check('SAI.SetStateChar(-1)', lambda: SAI.SetStateChar(-1))
check('SAI.EnableAIChar(Player0)', lambda: SAI.EnableAIChar('Player0', 1, 0))
check('SAI.EnableAIVehicle(Player0)', lambda: SAI.EnableAIVehicle('Player0', 1, 1.0, 1, 1, 1, 1, 1, 1.0, 1, 1, 1.0, 1.0))
check('SAI.EnableAIVehicle(Nobody)', lambda: SAI.EnableAIVehicle('Nobody', 0, 1.0, 1, 1, 1, 1, 1, 1.0, 1, 1, 1.0, 1.0))
check('SAI.AddVehicleRace(Player0)', lambda: SAI.AddVehicleRace('Player0'))
check('SAI.SetRotStaticObj(Player0)', lambda: SAI.SetRotStaticObj('Player0', 1.0, 2.0))
check('SAI.SetInertia(Player0)', lambda: SAI.SetInertia('Player0', 1))
check('SAI.SetRotStaticObj(Parked)', lambda: SAI.SetRotStaticObj('Parked', 1.0, 2.0))
check('SAI.SetInertia(Parked)', lambda: SAI.SetInertia('Parked', 0))
check('SAI.InitVehicleRace()', lambda: SAI.InitVehicleRace())
# SAI.AnalizeTraffic() on the running level does not return in the original either
check('SAI.GetOD()', lambda: SAI.GetOD())
# SNet in the level (FZ runs as a local game); nothing that changes the level or sends over the network
for f in ('IsServer', 'IsClient', 'IsDedicated', 'IsMaster', 'GetServerData', 'GetClientData', 'GetMyClientShip',
		'GetBotName', 'GetObjName'):
	check('SNet.%s()' % f, lambda f=getattr(SNet, f): f())
check('SNet.GetPing(Player0)', lambda: SNet.GetPing('Player0'))
check('SNet.GetPing(Nobody)', lambda: SNet.GetPing('Nobody'))
check('SNet.Kick(Nobody)', lambda: SNet.Kick('Nobody'))
check('SNet.SendChatString(0, hi)', lambda: SNet.SendChatString(0, 'hi'))
check('SNet.SendCtrlString(x)', lambda: SNet.SendCtrlString('x'))
check('SNet.SendUsrString(0, x)', lambda: SNet.SendUsrString(0, 'x'))
# SScorer in the level: HUD functions; Clear(0) last, it removes the HUD
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
check('SScorer.AddChatMsg(hello)', lambda: SScorer.AddChatMsg('hello', 255, 128, 0))
check('SScorer.SetMsgText(msg)', lambda: SScorer.SetMsgText('message', 2.0))
check('SScorer.SetLabelText(label)', lambda: SScorer.SetLabelText('label', 2.0))
check('SScorer.Fade(0, ...)', lambda: SScorer.Fade(0, (0, 0, 0, 0), (10, 20, 30, 40), 0.5))
check('SScorer.SetCinema(0, 1, 0.5)', lambda: SScorer.SetCinema(0, 1, 0.5))
check('SScorer.SetCinema(0, 0, 0.5)', lambda: SScorer.SetCinema(0, 0, 0.5))
check('SScorer.Get2DPos(player)', lambda p=pp: SScorer.Get2DPos(p))
check('SScorer.SetOnCancel(0, x)', lambda: SScorer.SetOnCancel(0, 'SDNoFunc'))
check('SScorer.SetOnSpecialHint(0, x, y)', lambda: SScorer.SetOnSpecialHint(0, 'SDNoFunc', 'hint'))
check('SScorer.SetMarkerSprite(0, )', lambda: SScorer.SetMarkerSprite(0, ''))
check('SScorer.CancelSpeech()', lambda: SScorer.CancelSpeech())
check('SScorer.SetConsole(0)', lambda: SScorer.SetConsole(0))
check('SScorer.Clear(0)', lambda: SScorer.Clear(0))
check('SScorer.GetActual(0) after Clear', lambda: SScorer.GetActual(0))
# Scrap, small functions (batch 2) in the level
for f in ('GetTimeSpeed', 'GetNetFlags', 'GetNewLevelPath', 'GetLanguage', 'GetGogID', 'IsSteam', 'GetSteamID',
		'GetSteamTicket', 'GetFreeBlocks', 'GetSavedProfilesList', 'GetVideoCurrentMode', 'DeselectProfile'):
	check('Scrap.%s()' % f, lambda f=getattr(Scrap, f): f())
for k in ('SDNoString', 'Menu_NewGame', ''):
	check('Scrap.GetLangStr(%s)' % k, lambda k=k: Scrap.GetLangStr(k))
check('Scrap.GetLangEntries(SDNo)', lambda: Scrap.GetLangEntries('SDNo'))
check('Scrap.GetLangEntries(SDNo, 1)', lambda: Scrap.GetLangEntries('SDNo', 1))
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
check('Scrap.GetAlarm()', lambda: Scrap.GetAlarm())
check('Scrap.GetAlarmChars()', lambda: Scrap.GetAlarmChars())
check('Scrap.GetLockAlarm()', lambda: Scrap.GetLockAlarm())
for i in (0, 1, 2):
	check('Scrap.GetCam(%d)' % i, lambda i=i: Scrap.GetCam(i))
check('Scrap.GetFirst().Name', lambda: Scrap.GetFirst().Name)
for y in (0.0, 25000.0, -1000.0):
	check('Scrap.GetFloor(%s)' % y, lambda y=y: Scrap.GetFloor(y))
check('Scrap.CreateEntityList(SDList)', lambda: Scrap.CreateEntityList('SDList'))
check('Scrap.CallElements(SDNoElem)', lambda: Scrap.CallElements('SDNoElem'))
# Scrap, small functions (batch 3) in the level
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
check('Scrap.SetAlarm(0.5)', lambda: Scrap.SetAlarm(0.5))
check('Scrap.SetAlarmGrow(1)', lambda: Scrap.SetAlarmGrow(1))
check('Scrap.SetAlarmChar(Player0)', lambda: Scrap.SetAlarmChar('Player0'))
check('Scrap.SetLockAlarm(1)', lambda: Scrap.SetLockAlarm(1))
check('Scrap.GetAlarm() set', lambda: Scrap.GetAlarm())
check('Scrap.GetLockAlarm() set', lambda: Scrap.GetLockAlarm())
check('Scrap.GetAlarmChars() set', lambda: Scrap.GetAlarmChars())
check('Scrap.SetCam(0, MainCam)', lambda: Scrap.SetCam(0, 'MainCam'))
check('Scrap.SetCam(1, SDNoCam)', lambda: Scrap.SetCam(1, 'SDNoCam'))
check('Scrap.SetTime(5.0)', lambda: Scrap.SetTime(5.0))
check('Scrap.GetTime() set', lambda: Scrap.GetTime())
finish()
