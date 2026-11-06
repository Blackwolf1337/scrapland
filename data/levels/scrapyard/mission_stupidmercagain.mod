# The module code of Mission_StupidMercAgain in level scrapyard (data02.packed/levels/scrapyard/scripts/mission_stupidmercagain.pyc): its imports, variables, functions and load time calls in the order of the
# script; ScrapC makes the module from this file instead of running the .pyc. Made by tools/modules/pyc2mod.py.
# A pack in mods/ with scrapc/levels/scrapyard/mission_stupidmercagain.mod replaces this file.

let NextMissionData = ('Mission_Massacre', 'DownTown')
let MercWaypoints = (
	((-157136.375), (-539.842773438), (-395505.0625)),
	((-278538.40625), 22176.4023438, (-398509.25)),
	((-315413.6875), 11769.3935547, (-541715.1875)),
	((-267888.40625), 31416.2636719, (-634259.625)),
	((-253330.859375), (-890.915588379), (-559655.625)),
	((-226284.828125), 12095.4130859, (-518057.6875)),
	((-128692.234375), 37402.125, (-580989.25)),
	((-84952.2265625), 36921.1835938, (-606845.1875)),
	((-32656.0664063), 34780.4335938, (-679031.75))
)
let EVASION_DISTANCE = 40000.0
let EVASION_TIME = 20.0
import Scrap
import SInput
import SNet
import SWeap
import SScorer
import SAI
import SVec
import SAct
import MissionsFuncs
import Speech
import SaveGame
import RaceMaker
import PhoneCab
import Vehicles
import VehiclesNPC
import Menu
import Parking
import CharAct
call Scrap.CreateSaveVar('Mission_StupidMercAgain', 'Follow')
let MERCS_LIFE = 150
def GetMercsEnemiesProfile
def InitStatus
def Init
def InitFollow
let FirstAppereance = 1
def CreateMerc
let SmartMercName = ''
let MedMercName = ''
def CreateMercs
def SmartMerDeath
let FirstHint = 1
let rmmess = 0
def GotoNext
def CreateMercPlans
def AddMercPlans
def InitEvasion
def StartEvasion
let EvasionTime = 0.0
def CheckEvasion
let pc = None
def InitTalkBishop
def UsePhoneCab
def SetNextMission
