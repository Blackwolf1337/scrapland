# The module code of Mission_HumphreyRace in level downtown (Data.packed/levels/downtown/scripts/mission_humphreyrace.pyc): its imports, variables, functions and load time calls in the order of the
# script; ScrapC makes the module from this file instead of running the .pyc. Made by tools/modules/pyc2mod.py.
# A pack in mods/ with scrapc/levels/downtown/mission_humphreyrace.mod replaces this file.

import Scrap
import SInput
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
set Menu.HeadBoss = 1
call Scrap.CreateSaveVar('HumphreyRaceTutorial', '1')
call Scrap.CreateSaveVar('MissionState', 'HumphreyRace')
let RACE_PROFILE = 'Race_Rookie'
def InitStatus
def ShowSystemMessage
let prevSwitchVehicleMissionCallback = None
def Init
def ShowBoostMessage
def OnTryingToExit
def ClosePressHangar
def DelRacers
def SetMissionTarget
def OnPlayerLoosesRace
def OnPlayerWinsRace
def CallCrazyGambler
def UsePhoneCab
def PreGivePlans
def PreGivePlans2
def GivePlans
def SetNextMission
