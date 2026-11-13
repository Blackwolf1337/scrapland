# The module code of Mission_StupidMerc in level scrapyard (data02.packed/levels/scrapyard/scripts/mission_stupidmerc.pyc): its imports, variables, functions and load time calls in the order of the
# script; ScrapC makes the module from this file instead of running the .pyc. Made by tools/modules/pyc2mod.py.
# A pack in mods/ with scrapc/levels/scrapyard/mission_stupidmerc.mod replaces this file.

let NextMissionData = ('Mission_TransportKiller', 'OutSkirts')
let MercWaypoints = (
	((-157136.375), (-539.842773438), (-395505.0625)),
	((-119469.367188), 16200.2099609, (-445644.15625)),
	((-33862.3046875), 15224.4814453, (-468226.71875)),
	((-8780.24609375), 16565.2617188, (-373887.875)),
	((-57437.359375), 35557.8125, (-561570.375)),
	((-174717.265625), (-17009.4238281), (-790861.125)),
	((-328239.09375), 10843.0087891, (-718209.625)),
	((-405320.6875), (-1477.49597168), (-581958.25)),
	((-251908.359375), 33452.9023438, (-420010.9375)),
	((-255510.28125), 34730.5429688, (-647392.8125))
)
let MERC_LIFE = 150
let percStupidMercGoesForLife = 0.2
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
def InitStatus
def Init
let FirstAppereance = 1
def CreateMerc
def OnStupidMercDeath
let mercName = ''
let mercLife = 1000000
let gettingLife = 0
def OnStupidMercDamage
def CheckAITakesLife
def CheckLife
let FirstHint = 1
let rmmess = 0
def GotoNext
def CreateATPCPlans
def AddATPCPlans
def SetNextMission
