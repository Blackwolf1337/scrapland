# super deals and mission SetVars as data (main session, round 7): recorders for the calls that load levels
import Scrap, sys
_SD12V = ('CTFFlags', 'CTFNumEnemies', 'CTFFriendProfile', 'CTFEnemyProfile', 'CTFOnDeathSpawnTime', 'OnAbort', 'OnVictory',
	'Hangar.availableUpgradesList', 'NewBluePrintAvaliable', 'MortalRaceRace', 'DominationZones', 'PlayerRounds', 'EnemyRounds',
	'PoliceBossAtPolice', 'Hangar.shipsToEditList', 'Hangar.availableEnginesList')
def _sd12on():
	import MissionsFuncs
	for n in ('NewCrazyDeal', 'SetNextSuperDeal', 'EndOfSuperDeal', 'SetNextMission'):
		setattr(MissionsFuncs, n, _sdrec(n))
	Scrap.Achievement = _sdrec('Achievement')
	return 1
def _sd12v():
	return _r6sv(_SD12V)
# a module of the level pack, imported (and so given its C or data functions) on demand
def _sd12m(m):
	try:
		mod = __import__(m)
	except:
		return (m, 'import', sys.exc_type, sys.exc_value)
	return (m, _r6fn(m))
def _sd12try(f, *a):
	try:
		return ('ok', apply(_sd, (f,) + a))
	except:
		return ('raise', sys.exc_type, str(sys.exc_value))
