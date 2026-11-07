# The module code of Mission_KillRudolph in level outskirts (data01.packed/levels/outskirts/scripts/mission_killrudolph.pyc): its imports, variables, functions and load time calls in the order of the
# script; ScrapC makes the module from this file instead of running the .pyc. Made by tools/modules/pyc2mod.py.
# A pack in mods/ with scrapc/levels/outskirts/mission_killrudolph.mod replaces this file.

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
import OutMap
call Scrap.CreateSaveVar('KillRudolphMissionData', 'RudolphRace')
let rudolphEntityName = 'BadBoy0'
let DataPackModel = 'models/Misc/BDataPack/BDataPack.m3d'
let MONEY_REWARD = 10000
let RUDOLPH_LIFE = 300
def GetRudolphProfile
def RaceList
def InitStatus
def Init
def GetDealType
def InitRace
def initCreateRudolph
def SetRudolphDef
def OnDamage
def EndOfRace
def SetRudolphAsNone
def RudolphKilled
def CreateClue
def ActivateClue
def OnClueTaken
def SetNextMission
let pc = None
def InitTalkToPoliceBoss
def UsePhoneCab
def SetNextMission2
