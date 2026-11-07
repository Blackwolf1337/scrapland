# The module code of Mission_DistractHumphrey in level downtown (Data.packed/levels/downtown/scripts/mission_distracthumphrey.pyc): its imports, variables, functions and load time calls in the order of the
# script; ScrapC makes the module from this file instead of running the .pyc. Made by tools/modules/pyc2mod.py.
# A pack in mods/ with scrapc/levels/downtown/mission_distracthumphrey.mod replaces this file.

import Scrap
import SAI
import SInput
import SLogic
import SNet
import SWeap
import SScorer
import SSound
import SVec
import SAct
import sys
import MissionsFuncs
import Speech
import Items
import string
import SaveGame
import Init
import Scorer
import Menu
import Parking
call Scrap.CreateSaveVar('MissionState', 'CallHumphrey')
call Scrap.CreateSaveVar('DistractionStart', '0')
let missionStartWaypoint = None
let humphreyName = ''
let probHumpreyAttacksPlayerTarget = 1.0
let missionTime = (5 * 60)
let maxShips = 10
let shipsKilledPlayer = 0
let shipsKilledHumphrey = 0
let missionComplete = 0
let shipNameForTaunt = ''
let numTaunt = 0
let MeetingPoint_ModelFile = 'models/Misc/MeetingPoint/MeetingPoint.m3d'
let HUMPHREY_LIFE = 90
def GetHumphreyProfile
def InitStatus
def Init
def CallHumphrey
def UsePhoneCab
def PreDistractHumphrey
def CreateWaypoint
def OnStartDistraction
def EnableMissionWaypoint
def StartCameraTravelling
def AbortCameraTravelling
def CreateHumphrey
def LaunchMission
def AssignNewHumphreyTarget
def OnHumphreyDeath
def OnHumphreyDamage
def OnGeneralDamage
def OnDeath
def StopMission
def TimeExpired
def PreCallDeepThroat
def CallDeepThroat
def UsePhoneCab2
def SetNextMission
def MissionScorer
