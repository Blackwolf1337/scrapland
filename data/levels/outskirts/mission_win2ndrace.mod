# The module code of Mission_Win2ndRace in level outskirts (data01.packed/levels/outskirts/scripts/mission_win2ndrace.pyc): its imports, variables, functions and load time calls in the order of the
# script; ScrapC makes the module from this file instead of running the .pyc. Made by tools/modules/pyc2mod.py.
# A pack in mods/ with scrapc/levels/outskirts/mission_win2ndrace.mod replaces this file.

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
let WayPointlimit = 20
let racersList = ['BankDirector', 'Bishop', 'Police', 'Functionary']
let iStartPos = 0
let nextStartPos = iStartPos
let BayScorer = {}
let YourScore = (-1)
let waypointName = ''
let racers = []
let racersDict = {}
let NumOponNotGoNextItem = 0
let RACE_PROFILE = 'Race_Elite'
def InitStatus
def Init
def InitRace
def OnCounterMeasure
def GetRaceWaypointIndex
def RacerKilled
def RacerDamaged
def OnRaceStart
def RaceGo
def OnCheckPoint
def OnNearCheckPoint
def NameOponGoItem
def OnAlarmOn
def OnAlarmOff
def CancelMission
def PreSetNextMission
def SetNextMission
