# The module code of Mission_Win1stRace in level commercial (data02.packed/levels/commercial/scripts/mission_win1strace.pyc): its imports, variables, functions and load time calls in the order of the
# script; ScrapC makes the module from this file instead of running the .pyc. Made by tools/modules/pyc2mod.py.
# A pack in mods/ with scrapc/levels/commercial/mission_win1strace.mod replaces this file.

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
set Menu.OutHeadMessenger = 1
call Scrap.CreateSaveVar('MissionState', 'FirstTime')
let currentWaypoints = (-1)
let timeIncrement = 3.0
let maxWaypoints = 30
let waypointName = ''
let iStartPos = 0
let raceTime = 6
def InitStatus
def Init
def InitTimeRace
def OnRaceStart
def RaceGo
def OnCheckPoint
def OnAlarmOn
def OnAlarmOff
def TimeExpired
def PreSetNextMission
def SetNextMission
def MissionScorer
