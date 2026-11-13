# The module code of Mission_MultiplusRace in level downtown (Data.packed/levels/downtown/scripts/mission_multiplusrace.pyc): its imports, variables, functions and load time calls in the order of the
# script; ScrapC makes the module from this file instead of running the .pyc. Made by tools/modules/pyc2mod.py.
# A pack in mods/ with scrapc/levels/downtown/mission_multiplusrace.mod replaces this file.

let NextMissionData = ('Mission_FicusDead', 'Police')
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
let TOTAL_STUPIDUS_LIVES = 7
call Scrap.CreateSaveVar('MultiplusRace', 'Begin')
call Scrap.CreateSaveVar('MultiplusRace.Lives', repr(TOTAL_STUPIDUS_LIVES))
let RACE_PROFILE = 'Race_Rookie'
let FUNC_LIFE = 50
def GetFunctionaryProfile
def InitStatus
def Init
def RaceList
def GetDealType
def EndOfRace
def PreKillStupidus
def SetPhoneCab
def UsePhoneCab
let outOfRange = None
def InitRace
def AfterInitRaceTaunt
def PreInitKill
def InitKill
def HeyYouFight
def SpawnStupidus
def SpawnStupidusFriends
def StupidusFriendOnDeath
def RestoreEnemies
def StupidusOnDeath
def StupidusDamage
def MissionScorer
def SetNextMission
def CGCalls
