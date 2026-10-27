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
# cPyEntity attributes (tp_getattr/tp_setattr): script variables of int, float, vector and string type, then the
# methods. Several handles of one entity share its link ring; they go again right away.
for v in ('Name', 'Life', 'MaxLife', 'MinLife', 'Pos', 'Vel', 'Mass', 'Energy', 'Invulnerable', 'TimeSpeed',
	'WeapList', 'Bogus', '__methods__'):
	check('Player0.%s' % v, lambda v=v, p=p: getattr(p, v))
check('Player0.IsType method', lambda p=p: callable(p.IsType))
check('Player0 handles', lambda: len(map(lambda i: Scrap.UsrEntity(0), range(20))))
check('Player0.Name after handles', lambda p=p: p.Name)
check('Player0.Life = Life', lambda p=p: setattr(p, 'Life', p.Life))
check('Player0.Mass = 2.5', lambda p=p: setattr(p, 'Mass', 2.5))
check('Player0.Mass set', lambda p=p: p.Mass)
check('Player0.Pos = Pos', lambda p=p: setattr(p, 'Pos', p.Pos))
check('Player0.Pos set', lambda p=p: p.Pos)
check('Player0.Life = x', lambda p=p: setattr(p, 'Life', 'x'))
check('Player0.Life = []', lambda p=p: setattr(p, 'Life', []))
check('Player0.Bogus = 1', lambda p=p: setattr(p, 'Bogus', 1))
check('Player0.Bogus = (1, 2, 3)', lambda p=p: setattr(p, 'Bogus', (1, 2, 3)))
check('Player0.Bogus = x', lambda p=p: setattr(p, 'Bogus', 'x'))
check('Player0.Bogus = None', lambda p=p: setattr(p, 'Bogus', None))
check('first.Name after Del', lambda f=f: f.Name)
# tp_print through the print statement into the output file
sys.stdout = out
print p
print f
sys.stdout = sys.__stdout__
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

# Scrap batch 4 in the level
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
check('Scrap.GetEntity(Player0).Name', lambda: Scrap.GetEntity('Player0').Name)
# Scrap batch 5 in the level: entity lists, camera distance, parking zones
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
for l in ('Traffic', 'Enemies', 'SDList'):
	for v in ('Num', 'Count', 'Name', 'Missing'):
		check('Scrap.EntityListGet(%s, %s)' % (l, v), lambda l=l, v=v: Scrap.EntityListGet(l, v))
check('Scrap.EntityListSet(SDList, Name, x)', lambda: Scrap.EntityListSet('SDList', 'Name', 'x'))
for p in ((0, 0, 0), (1000, 2000, 3000), (-16762.8, 14713.5, 25705.8)):
	check('Scrap.GetMinCamDist%s' % (p,), lambda p=p: Scrap.GetMinCamDist(p[0], p[1], p[2]))
check('Scrap.GetNearestParked()', lambda: Scrap.GetNearestParked())
check('Scrap.GetFarestParked()', lambda: Scrap.GetFarestParked())
check('Scrap.GetNearestParked(0, (0,0,0))', lambda: Scrap.GetNearestParked(0, (0, 0, 0)))
check('Scrap.AddParkingZone(1, player, SDPark)', lambda p=pp: Scrap.AddParkingZone(1, p, 'SDPark'))
check('Scrap.GetNearestParked(1, player)', lambda p=pp: Scrap.GetNearestParked(1, p))
# Scrap batch 6 in the level: collision tests, sphere calls, dummy search, debris, items
sdcalls = []
def SDSphereCB(*args):
	sdcalls.append(args)
for a, b in ((pp, (pp[0], pp[1] - 5000.0, pp[2])), (pp, (pp[0] + 3000.0, pp[1], pp[2])), ((0, 0, 0), (0, 1, 0))):
	for l in ('Traffic', 'Enemies'):
		check('Scrap.TestLine(%s, %s, %s)' % (a, b, l), lambda a=a, b=b, l=l: Scrap.TestLine(a, b, l))
for r in (10.0, 500.0, 5000.0):
	for l in ('Traffic', 'Enemies'):
		check('Scrap.TestSphere(player, %s, %s)' % (r, l), lambda r=r, l=l: Scrap.TestSphere(pp[0], pp[1], pp[2], r, l))
		check('Scrap.TestSphere(player, %s, %s, Player0)' % (r, l),
			lambda r=r, l=l: Scrap.TestSphere(pp[0], pp[1], pp[2], r, l, 'Player0'))
check('Scrap.TestSphere(player, 500, Traffic, SDNoEntity)',
	lambda: Scrap.TestSphere(pp[0], pp[1], pp[2], 500.0, 'Traffic', 'SDNoEntity'))
for r in (1.0, 5000.0):
	check('Scrap.SphereCall(player, %s, Traffic)' % r, lambda r=r: Scrap.SphereCall(pp[0], pp[1], pp[2], r, 'Traffic', 'SDSphereCB'))
	check('Scrap.SphereCall((player), %s, Traffic, 0)' % r, lambda r=r: Scrap.SphereCall(pp, r, 'Traffic', 'SDSphereCB', 0))
check('SphereCall callbacks', lambda: len(sdcalls))
check('Scrap.TestLine bad', lambda: Scrap.TestLine((1, 2, 3), 'x'))
check('Scrap.TestSphere bad', lambda: Scrap.TestSphere(1, 2, 3))
check('Scrap.SphereCall bad', lambda: Scrap.SphereCall(1, 2, 3, 4))
check('Scrap.NextDummySearch bad', lambda: Scrap.NextDummySearch(1))
for d in ('', 'Park', 'SDNoDummy'):
	check('Scrap.StartDummySearch(%s)' % d, lambda d=d: Scrap.StartDummySearch(d))
	for i in range(4):
		check('Scrap.NextDummySearch() %s %d' % (d, i), lambda: Scrap.NextDummySearch())
for n, v in ((0, (1.5, 2.5, 3.5)), (1, (90.0, 45.0, 10.0)), (2, (0.1, 0.2, 0.3)), (3, (360.0, -180.0, 1.0)), (4, (1, 2, 3)), (-1, (1, 2, 3))):
	check('Scrap.SetDebrisValue(%d, %s)' % (n, v), lambda n=n, v=v: Scrap.SetDebrisValue(n, v[0], v[1], v[2]))
check('Scrap.SetDebrisValue bad', lambda: Scrap.SetDebrisValue(1, 2))
check('Scrap.SetDebrisSys bad', lambda: Scrap.SetDebrisSys(1))
check('Scrap.DropDebris bad', lambda: Scrap.DropDebris('x'))
check('Scrap.SetDebrisSys(SDDebris, 0)', lambda: Scrap.SetDebrisSys('SDDebris', 0))
# the name is a model file: a missing one ("File not found") never returns, in the original too. Bang.M3D of the
# scripts is not in the archives (only commented out there), FiredDebris is (models/gfx/fireddebris.sm3)
sddebris = 'Models/GFX/FiredDebris.M3D'
check('Scrap.SetDebrisSys(FiredDebris, 2, ...)', lambda: Scrap.SetDebrisSys(sddebris, 2, 10000.0, 1.0, 0.7, 9800.0, 1))
check('Scrap.SetDebrisSys(FiredDebris)', lambda: Scrap.SetDebrisSys(sddebris))
check('Scrap.DropDebris(FiredDebris, 1)', lambda: Scrap.DropDebris(sddebris, 1.0))
check('Scrap.DropDebris(FiredDebris, 10)', lambda: Scrap.DropDebris(sddebris, 10.0))
check('Scrap.SetDebrisSys(FiredDebris, -1)', lambda: Scrap.SetDebrisSys(sddebris, -1))
check('Scrap.AddItem(Other)', lambda: Scrap.AddItem('Other', 1))
check('Scrap.AddItem(Life) bad', lambda: Scrap.AddItem('Life', 1))
check('Scrap.AddItem(Ammo) bad', lambda: Scrap.AddItem('Ammo', 'x'))
check('Scrap.AddItem(Life, SDLife, 5)', lambda: Scrap.AddItem('Life', 'SDLife', 5))
check('Scrap.AddItem(Ammo, SDAmmo, Part, 5)', lambda: Scrap.AddItem('Ammo', 'SDAmmo', 'Part', 5))
check('Scrap.AddItem(Ammo, SDAmmo2, Ener, 7)', lambda: Scrap.AddItem('Ammo', 'SDAmmo2', 'Ener', 7))
# SAI.SetStateVehicle/SetStateChar handlers: no entity of FZ has an AI, so only the error paths (report in the
# log): bad arguments, missing or wrong entity, missing target, target Player0/Parked (the target check comes
# first). Last the entities themselves: an active one without AI reports ENTIDAD SIN IA.
def sdvargs(n, t):
	return {0: (n,), 1: (n, 1, 2, 3, 4), 2: (n, t), 3: (n,), 4: (n,), 5: (n, t, 1.5), 6: (n, 1, 2, 3, 4),
		7: (n, t), 8: (n, t), 9: (n,)}
def sdcargs(n, t):
	return {0: (n,), 1: (n, 1.5), 2: (n, t, 1.5), 3: (n, t, 1.5), 4: (n, 1, 2, 3, 4, 5, 6, 7, 8), 5: (n, t, 1.5),
		6: (n, 1, 2, 3, 4, 5), 7: (n, t, 1.5), 8: (n, t, 1), 9: (n, t, 1)}
# bad arguments only after the entity name: the original reports an uninitialized name otherwise (and hangs)
sdvbad = {1: ('SDNo', 'x', 2, 3, 4), 2: ('SDNo', 5), 3: ('SDNo', 'x'), 5: ('SDNo', 'SDNo', 'x'),
	6: ('SDNo', 'x', 2, 3, 4), 7: ('SDNo', 5), 8: ('SDNo', 5)}
sdcbad = {1: ('SDNo', 'x'), 2: ('SDNo', 'SDNo', 'x'), 3: ('SDNo', 'SDNo', 'x'), 4: ('SDNo', 'x', 2, 3, 4, 5, 6, 7, 8),
	5: ('SDNo', 'SDNo', 'x'), 6: ('SDNo', 'x', 2, 3, 4, 5), 7: ('SDNo', 'SDNo', 'x'), 8: ('SDNo', 'SDNo', 'x'),
	9: ('SDNo', 'SDNo', 'x')}
for st in range(10):
	if sdvbad.has_key(st):
		check('SAI.SetStateVehicle(%d) bad' % st, lambda st=st: apply(SAI.SetStateVehicle, (st,) + sdvbad[st]))
	if sdcbad.has_key(st):
		check('SAI.SetStateChar(%d) bad' % st, lambda st=st: apply(SAI.SetStateChar, (st,) + sdcbad[st]))
	for n, t in (('SDNo', 'SDNo'), ('SDNo', 'Player0'), ('SDNo', 'Parked'), ('Player0', 'SDNo'),
			('Player0', 'Parked'), ('', '')):
		check('SAI.SetStateVehicle(%d, %s, %s)' % (st, n, t), lambda st=st, a=sdvargs(n, t)[st]:
			apply(SAI.SetStateVehicle, (st,) + a))
	for n, t in (('SDNo', 'SDNo'), ('SDNo', 'Player0'), ('Parked', 'SDNo'), ('Parked', 'Player0')):
		check('SAI.SetStateChar(%d, %s, %s)' % (st, n, t), lambda st=st, a=sdcargs(n, t)[st]:
			apply(SAI.SetStateChar, (st,) + a))
check('SAI.SetStateVehicle(0, Parked)', lambda: SAI.SetStateVehicle(0, 'Parked'))
check('SAI.SetStateChar(0, Player0)', lambda: SAI.SetStateChar(0, 'Player0'))

# the sectors of the level (Engine/SueloPortal.c): their names and the sector walks between their first nodes
import SDTest
nz = SDTest.Peek(0x4b00d8, 'i')
check('sectors', lambda: (nz, SDTest.Peek(0x4e1e00, 'i')))
# what the portal culling of the last frame saw (Zona_Pintar): the sectors, how often, their screen rectangles
def sdpintar():
	r = []
	for i in range(SDTest.Peek(0x4afed0, 'i')):
		e = 0x4b0168 + 0x20 * i
		v = []
		for k in range(8):
			v.append(SDTest.Peek(e + 4 * k, 'i'))
		r.append(v)
	return r
check('Zona_Pintar', sdpintar)
# a cull from each of the first sectors, on a frame number of its own (the real render of this frame checks that the
# sectors were not seen yet)
frame = SDTest.Peek(0x4a8ec8, 'i')
for sec in range(min(nz, 6)):
	SDTest.Poke(0x4a8ec8, frame + 1000 + sec)
	check('Portales_Cull' + fmt((sec,)), lambda sec=sec: (SDTest.Call(0x26d350, 'vi', sec), sdpintar()))
SDTest.Poke(0x4a8ec8, frame)
for i in range(min(nz, 12)):
	check('Sector_GetName' + fmt((i,)), lambda i=i: SDTest.Call(0x26c340, 'si', i)[0])
	check('Sector_IsOutside' + fmt((i,)), lambda i=i: SDTest.Call(0x26c390, 'bi', i)[0])
	z = SDTest.Peek(0x4afed8 + 4 * i, 'i')
	check('CZona' + fmt((i,)), lambda z=z: (SDTest.Peek(z, '@i'), SDTest.Peek(z + 0x30, '@i'), SDTest.Peek(z + 0x134, '@i')))
for a in range(min(nz, 6)):
	for b in range(min(nz, 6)):
		pa = SDTest.Peek(SDTest.Peek(0x4afed8 + 4 * a, 'i') + 0x24, '@i')
		pb = SDTest.Peek(SDTest.Peek(0x4afed8 + 4 * b, 'i') + 0x24, '@i')
		check('Sector_Recorrer' + fmt((a, b)), lambda a=a, pa=pa, pb=pb: SDTest.Call(0x26cba0, 'iiiii', a, pa + 0x8c,
			pb + 0x8c, 0)[0])

# the world (Engine/Mundo.c) after the level load: gMundo, its dummies, the .dum file written and read back, the debug
# points. Floats as their bits, pointers only compared.
mundo = SDTest.Peek(0x4afde8, 'i')
def mwi(a):
	return SDTest.Peek(a, '@i')
def mwints(a, n):
	r = []
	for i in range(n):
		r.append(mwi(a + 4 * i))
	return r
def mwdummies(d):
	r = []
	while d:
		r.append((SDTest.Peek(d + 4, '@p'), mwints(d + 8, 6), mwi(d + 0x20) != 0))
		d = mwi(d)
	return r
def mwnodo(n):
	if n:
		return SDTest.Peek(n + 4, '@p')
	return None
dummies = mwi(mundo + 0xc4)
check('CMundo', lambda: (mwints(mundo + 0x24, 2), mwi(mundo + 0xa4) == mundo + 0x2c, mwints(mundo + 0x2c + 0x14, 9),
	mwi(mundo + 0xa8) != 0, mwi(mundo + 0xac) != 0, mwi(mundo + 0xb0) != 0, mwi(mundo + 0xb4) != 0,
	mwi(mundo + 0xb8) != 0, mwi(mundo + 0xbc) != 0, mwi(mundo + 0xc0) & 0xff))
check('CMundo dummies', lambda: mwdummies(dummies))
check('MundoDummy_Count', lambda: SDTest.Call(0x2697d0, 'tii', dummies)[0])
def mwfind(name, own, case):
	d = SDTest.Call(0x26a710, 'tiisii', dummies, name, own, case)[0]
	i = 0
	e = dummies
	while e and e != d:
		e = mwi(e)
		i = i + 1
	return (d != 0, i)
if dummies:
	first = SDTest.Peek(dummies + 4, '@p')
	for name, own, case in ((first, 1, 1), (first, 0, 1), (strop.upper(first), 1, 0), (strop.upper(first), 1, 1),
		(SDTest.Peek(mwi(dummies) + 4, '@p') or 'x', 0, 0), ('DM_sdnone', 1, 0)):
		check('MundoDummy_Find' + fmt((name, own, case)), lambda n=name, o=own, c=case: mwfind(n, o, c))
for name in ('DM_sdnone', 'sd*'):
	check('CMundo_FindNodo' + fmt((name,)), lambda n=name: mwnodo(SDTest.Call(0x26b270, 'tiis', mundo, n)[0]))
	check('CMundo_FindNodoWild' + fmt((name,)), lambda n=name: mwnodo(SDTest.Call(0x26b290, 'tiis', mundo, n)[0]))
check('CMundo_FindClase(luz)', lambda: mwnodo(SDTest.Call(0x26b250, 'tiiii', mundo, 0, 0xa3000008)[0]))
check('CMundo_GetCamaraSuelo', lambda: SDTest.Call(0x26b210, 'tiii', mundo, 0)[0] != 0)
saved = mwints(mundo + 0x24, 2)
check('CMundo_SetCieloPos', lambda: (SDTest.Call(0x26a790, 'tvif', mundo, 37.5), mwi(mundo + 0x24)))
check('CMundo_SetCieloRot', lambda: (SDTest.Call(0x26a7c0, 'tvif', mundo, -30.0), mwi(mundo + 0x28)))
SDTest.Poke(mundo + 0x24 - 0x400000, saved[0])
SDTest.Poke(mundo + 0x28 - 0x400000, saved[1])
# the .dum file of the floor scene read again (files cannot be written here), a dummy made here
escpath = SDTest.Peek(mwi(mundo + 0xac) + 4, '@p')
loaded = SDTest.Call(0x2699d0, 'isp', escpath, 0)
check('Mundo_LoadDUM', lambda: (loaded[0], mwdummies(loaded[1])))
cell = SDTest.Call(0x1334d0, 'ii', 4)[0]
SDTest.Poke(cell - 0x400000, loaded[1])
check('MundoDummy_FreeList', lambda: (SDTest.Call(0x2697e0, 'vi', cell), mwi(cell)))
for name in ('DM_sdnuevo', 'sdnuevo'):
	d = SDTest.Call(0x269480, 'tiisFF', SDTest.Call(0x1334d0, 'ii', 0x24)[0], name, (1.5, -2.0, 3.0), (0.1, 0.2, -0.3))[0]
	check('MundoDummy_ctor' + fmt((name,)), lambda d=d: mwdummies(d))
	check('MundoDummy_dtor' + fmt((name,)), lambda d=d: (SDTest.Call(0x269650, 'tvi', d), mwi(d + 4)))
# the debug points: a point, a line, a box, then cleared; the limit with a count of 3
sdon = SDTest.Peek(0x4a8efa, 'i')
sdmax = SDTest.Peek(0x432584, 'i')
def mwpuntos():
	n = SDTest.Peek(0x4afecc, 'i')
	p = SDTest.Peek(0x4afec8, 'i')
	if p:
		return (n, SDTest.Peek(0x4afec4, 'i'), mwints(p, 5 * n))
	return (n, SDTest.Peek(0x4afec4, 'i'))
SDTest.Call(0x26b450, 'v')
check('Debug_Punto', lambda: (SDTest.Call(0x26b300, 'iFif', (1.0, 2.0, 3.0), 0xff0000, 0.5)[0], mwpuntos()))
check('Debug_PuntoLinea', lambda: (SDTest.Call(0x26b2c0, 'iFFii', (0.0, 0.0, 0.0), (4.0, 5.0, 6.0), 0xff, 0xff00)[0],
	mwpuntos()))
for i in range(5):
	check('Debug_Punto box' + fmt((i,)), lambda i=i: (SDTest.Call(0x26b300, 'iFif', (float(i), 1.0, -1.0), i, -2.0)[0],
		mwpuntos()))
check('Debug_PuntosClear', lambda: (SDTest.Call(0x26b450, 'v'), mwpuntos()))
SDTest.Poke(0x432584, 3)
for i in range(5):
	check('Debug_Punto max' + fmt((i,)), lambda i=i: (SDTest.Call(0x26b300, 'iFif', (0.0, float(i), 0.0), 7, 1.0)[0],
		mwpuntos()))
SDTest.Call(0x26b450, 'v')
SDTest.Poke(0x432584, sdmax)
SDTest.Poke(0x4a8efa, sdon)

# the fog of the world (Engine/Niebla.c) through the D3D log: CNiebla_Apply with its flags and the fog variables
# changed, all put back after
def nblog(fn):
	SDTest.Out()
	SDTest.D3DLog(1)
	r = fn()
	SDTest.D3DLog(0)
	return (r[0], SDTest.Out(), SDTest.Peek(0x4ad744, 'i'))
nbsaved = mwints(mundo, 9)
nbvars = (0x4b1cb8, 0x4b1ce8, 0x4b1cd0, 0x4b1ca0)
nbvarsaved = []
for a in nbvars:
	nbvarsaved.append(SDTest.Peek(a, 'i'))
check('CNiebla', lambda: nbsaved)
check('CNiebla_Apply', lambda: nblog(lambda: SDTest.Call(0x278420, 'tvi', mundo)))
# activa, tabla, capsW, capsTabla, fuente, modo; then R_FogEnable, R_FogColor, R_FogAbsDist, R_FogDensity
for flags in ((1, 0, 0, 0, 0, 3), (1, 1, 0, 1, 0, 3), (1, 1, 1, 1, 1, 1), (1, 1, 0, 1, 1, 2), (0, 1, 0, 1, 1, 3),
	(1, 0, 1, 0, 1, 3)):
	for fv in ((1, -1, 0, 0), (1, 0x204080, 0, 0), (1, -1, 1, 0), (1, -1, 0, 1061158912), (0, -1, 0, 0)):
		SDTest.Poke(mundo - 0x400000, (nbsaved[0] & ~0xff) | flags[0])
		SDTest.Poke(mundo + 0x10 - 0x400000, flags[4])
		SDTest.Poke(mundo + 0x1c - 0x400000, flags[1] | flags[2] << 8 | flags[3] << 16 | (nbsaved[7] & 0xff000000))
		SDTest.Poke(mundo + 0x20 - 0x400000, flags[5])
		for i in range(4):
			SDTest.Poke(nbvars[i], fv[i])
		check('CNiebla_Apply' + fmt((flags, fv)), lambda: (nblog(lambda: SDTest.Call(0x278420, 'tvi', mundo)),
			mwints(mundo, 9)))
		for i in range(9):
			SDTest.Poke(mundo + 4 * i - 0x400000, nbsaved[i])
for i in range(4):
	SDTest.Poke(nbvars[i], nbvarsaved[i])
nb = SDTest.Call(0x1334d0, 'ii', 0x24)[0]
# 0x14 and 0x18 (the own color) are not set by the constructor
def nbset(nb):
	return mwints(nb, 5) + mwints(nb + 0x1c, 2)
check('CNiebla_ctor', lambda: (SDTest.Call(0x278360, 'tii', nb)[0] == nb, nbset(nb)))
check('CNiebla_InitD3D', lambda: (SDTest.Call(0x2783a0, 'tvi', nb), nbset(nb)))
check('CNiebla_SetColor', lambda: (SDTest.Call(0x278680, 'tviif', nb, 0x11223344, 0.75), mwints(nb, 9)))
check('CNiebla_SetRango', lambda: (SDTest.Call(0x2786a0, 'tviff', nb, 0.25, 0.9), mwints(nb, 9)))
# the tests of single modules in the level (tools/test/py/sdlevel_*.py); a missing one is skipped
for part in ('sdlevel_collision.py', 'sdlevel_scrapworld.py', 'sdlevel_world.py', 'sdlevel_fxcounter.py', 'sdlevel_grid.py', 'sdlevel_fxmisc.py'):
	try:
		f = open(part)
	except IOError:
		continue
	f.close()
	execfile(part)
finish()
