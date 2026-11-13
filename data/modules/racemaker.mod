# The module code of RaceMaker (Data.packed/scripts/missions/racemaker.pyc): its imports, variables, functions and load time calls in the order of the
# script; ScrapC makes the module from this file instead of running the .pyc. Made by tools/modules/pyc2mod.py.
# A pack in mods/ with scrapc/modules/racemaker.mod replaces this file.

import Scrap
import SInput
import SNet
import SWeap
import SScorer
import SVec
import SAI
import SSound
let RaceCountDownSound1 = 'foco-aviso1'
let RaceCountDownSound1Vol = 0.3
let RaceCountDownSound2 = 'foco-aviso2'
let RaceCountDownSound2Vol = 0.3
let StartPos = []
let iStartPos = 0
let nextStartPos = iStartPos
let Players = []
let BayScorer = {}
let FoesScorer = 0
let YourScore = 0
let WayPointlimit = 20
let SkipTimer = 0
let NumOponNotGoNextItem = 0
let ChallengeFoeType = ''
let prevCamera = None
let cameraItemName = None
let cameraVehicleName = None
call Scrap.CreateSaveVar('Race.Num', '0')
call Scrap.CreateSaveVar('Race.FirstTime', '1')
call Scrap.CreateSaveVar('Race.Profile', 'Rookie')
call Scrap.CreateSaveVar('Combat.FirstTime', '1')
call Scrap.CreateSaveVar('Combat.Profile', 'Rookie')
call Scrap.CreateEntityList('Race')
let challenged_NPCs_Names = []
let challengePartners = {
	'PoliceBoss': ['Police'],
	'Police': ['Police'],
	'BankDirector': ['BankDirector'],
	'BankMaster': ['BankDirector'],
	'CrazyGambler': ['BankDirector'],
	'Bishop': ['Bishop'],
	'Mayor': ['Functionary'],
	'Functionary': ['Functionary'],
	'Berto': ['Betty', 'Humphrey'],
	'Betty': ['Berto', 'Humphrey'],
	'Humphrey': ['Betty', 'Berto'],
	'MercenaryA': ['MercenaryB', 'MercenaryC'],
	'MercenaryB': ['MercenaryA', 'MercenaryC'],
	'MercenaryC': ['MercenaryA', 'MercenaryB']
}
let RaceSpeachers = {
	'Humphrey': 'Berto',
	'Betty': 'Berto',
	'MercenaryB': 'MercenaryA',
	'MercenaryC': 'MercenaryA',
	'BankMaster': 'BankDirector',
	'BankMaster': 'BankDirector',
	'PoliceBoss': 'Police',
	'CrazyGambler': 'BankDirector'
}
def ShowSystemMessage
def ShowSystemMessageDelay
def NPCCombatKilled
def DeleteCurrentWaypoint
def MovePlayersToTraffic
let EndOfRaceCallback = None
let OnPlayerWinsRace = None
let OnPlayerLoosesRace = None
def EndOfRace
def RaceStart
def OnCounterMeasure
def RacerKilled
def RacerInterruptedByAlarm
def RacerDamaged
def RacerLeft
def AbortCameraTravelling
def StartCameraTravelling
def OnRaceStart
let OnRaceGoCallback = None
def RaceGo
def CreateRaceCountDown
def DestroyRaceCountDown
def DestroyRaceCountDownEnd
def Counter
let OnCheckPointCallback = None
def OnCheckPoint
def OnNearCheckPoint
def NameOponGoItem
def LocalGetDealType
let GetDealType = LocalGetDealType
def StopTimer
def StartTimer
def FirstTimeRace
def FirstTimeCombat
def PointObjectTo
let OnInitRace = None
let OnPreInitRace = None
let OnInitialTaunt = None
def PreloadCnDnResources
def InitRace
def InitCombat
def MoveCombatNPCsToPos
def PlaceNPCAtRacePos
def SetScorerValues
def MissionScorer2
def MissionScorer
let AISHIPS = {
	'PoliceBoss': ['SPOLIBOSS1'],
	'Police': ['SPOLI1', 'SPOLI2', 'SPOLI3', 'SPOLI4', 'SPOLI5'],
	'Bishop': ['SBISHOP1', 'SARCHBISHOP1'],
	'ArchBishop': ['SARCHBISHOP1'],
	'Functionary': ['SFUNC1'],
	'Mayor': ['SMAYOR1'],
	'BankDirector': ['SBANKER1'],
	'BankMaster': ['SBANKMASTER1'],
	'CrazyGambler': ['SCRAZYGAMBLER1'],
	'Humphrey': ['SHUMP1'],
	'Berto': ['SBERTO1'],
	'Betty': ['SBETTY1'],
	'Boss': ['SBOSS1'],
	'MercenaryA': ['SMERC3'],
	'MercenaryB': ['SMERC1'],
	'MercenaryC': ['SMERC2'],
	'Mercs': ['SMERC1', 'SMERC2', 'SMERC3'],
	'Press': ['SBERTO1', 'SBETTY1', 'SHUMP1']
}
def InitShipIAProfile
let nextIAProfile = (-1)
def GetRandomShipIAProfile
