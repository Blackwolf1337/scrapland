# Level checks: run in the multiplayer level FZ (difftest.sh starts the game with -imap:Levels/FZ, PythonExecute
# runs after the level is loaded). Real weapon tables, action classes and entities; Player0 is the local player
# (a WalkChar with life). Python 1.5.2 syntax.
OUT = 'sdlevel.out'
execfile('sdlib.py')
import Scrap, SWeap, SVec, SAct, SFX

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
finish()
