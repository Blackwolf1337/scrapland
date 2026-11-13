# The module code of Mission_StealMayorShip in level downtown (Data.packed/levels/downtown/scripts/mission_stealmayorship.pyc): its imports, variables, functions and load time calls in the order of the
# script; ScrapC makes the module from this file instead of running the .pyc. Made by tools/modules/pyc2mod.py.
# A pack in mods/ with scrapc/levels/downtown/mission_stealmayorship.mod replaces this file.

import Scrap
import SInput
import SNet
import SWeap
import SScorer
import SSound
import SLogic
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
set Menu.OutHeadMessenger = 1
let COPS_TO_KILL = 20
call Scrap.CreateSaveVar('NumCopsKilled', '0')
let numCopsKilled = 0
let missionShipName = ''
let pc = None
def InitStatus
def Init
def CreateMissionShip
def TestForMissionShip
def SetPoliceTargets
def SetPlatformTarget
def TestForCorrectPlatform
def UseParkedMayorShip
def UsePhoneCab
def SetNextMission
def CGCalls
def StartMission
def ContinueMission
def SetPoliceListTargetStatus
def SetPoliceShipTargetStatus
def ComeBack
def MissionScorer
def OnPoliceDeath
def ParkMayorShip
