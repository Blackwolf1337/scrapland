# The module code of Mission_MosquitoClue in level scrapyard (data02.packed/levels/scrapyard/scripts/mission_mosquitoclue.pyc): its imports, variables, functions and load time calls in the order of the
# script; ScrapC makes the module from this file instead of running the .pyc. Made by tools/modules/pyc2mod.py.
# A pack in mods/ with scrapc/levels/scrapyard/mission_mosquitoclue.mod replaces this file.

let NextMissionData = ('Mission_CatchRudolph', 'Bank')
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
import PhoneCab
import Vehicles
import VehiclesNPC
import Menu
import Parking
import Traffic
import OutPolice
call Scrap.CreateSaveVar('Mission_MosquitoClue', 'Swarm')
call Scrap.CreateSaveVar('Mission_MosquitoClue.NumLairsExplored', '0')
let numMosquitosInLair = 20
let numMosquitosKilled = 0
let checkArrow = None
let lairsList = []
let Model_Datapack = 'models/Misc/BDataPack/BDataPack.m3d'
let MOSQUITO_LIFE = 25
def GetMosquitoProfile
def InitStatus
def Init
def InitSwarm
def InitMosquito
let WithAmmo = 0
def CreateMosquito
def MosquitoDeath
def TakeDataPack
def InitPoliChat
def UsePhoneCab
def SetNextMission
