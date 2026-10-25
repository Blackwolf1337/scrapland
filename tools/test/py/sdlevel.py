# Level checks: run in the multiplayer level FZ (difftest.sh starts the game with -imap:Levels/FZ, PythonExecute
# runs after the level is loaded). Real weapon tables, action classes and entities; Player0 is the local player
# (a WalkChar with life). Python 1.5.2 syntax.
OUT = 'sdlevel.out'
execfile('sdlib.py')
import Scrap, SWeap, SVec, SAct, SFX, SLogic, SAI, SNet

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
finish()
