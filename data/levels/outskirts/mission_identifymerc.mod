# The module code of Mission_IdentifyMerc in level outskirts (data01.packed/levels/outskirts/scripts/mission_identifymerc.pyc): its imports, variables, functions and load time calls in the order of the
# script; ScrapC makes the module from this file instead of running the .pyc. Made by tools/modules/pyc2mod.py.
# A pack in mods/ with scrapc/levels/outskirts/mission_identifymerc.mod replaces this file.

import SAI
import Scrap
import SInput
import SNet
import SWeap
import SScorer
import SSound
import SVec
import SAct
import SLogic
import sys
import VehiclesNPC
import Scene
import MissionsFuncs
import Speech
import Menu
import CharScorer
import Vehicles
import SaveGame
import Traffic
import Elevator
call Scrap.CreateSaveVar('MissionState', 'IdentifyMerc')
let MAX_TIME_TARGET_LOST = 10.0
let MAX_SCANNING_DIST = 10000
let MAX_SCANNING_TIME = 20.0
let prevWeapList = ''
let prevAmmo = 0
let traitorius = None
let lonelyMerc = None
let pc = None
let scanningLonelyMerc = 0
let LONELY_MERC_LIFE = 250
def CreateLonelyMercProfiles
def InitStatus
def Init
def InitIdentifyMerc
def CreateLonelyMercInScene
def CheckTraitoriusDeath
def EndOfScene
def DelLonelyMerc
def CreateLonelyMerc
def PreInitMission
def InitMission
def StopMission
def ScanShip
def ResetScan
def OnFinishScan
def OnLonelyMercDeath
def OnLonelyMercDamage
def PreRespawnLonelyMerc
def RespawnLonelyMerc
def PreCallDeepThroat
def SetPhoneCab
def UsePhoneCab
def SetNextMission
def MissionScorer
