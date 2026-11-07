# The module code of Mission_GetCrazyData in level press (data01.packed/levels/press/scripts/mission_getcrazydata.pyc): its imports, variables, functions and load time calls in the order of the
# script; ScrapC makes the module from this file instead of running the .pyc. Made by tools/modules/pyc2mod.py.
# A pack in mods/ with scrapc/levels/press/mission_getcrazydata.mod replaces this file.

import Scrap
import SAI
import SInput
import SNet
import SWeap
import SScorer
import SLogic
import SSound
import SVec
import SAct
import sys
import CharsNPC
import CharAct
import MissionsFuncs
import Speech
import CharConversor
import Items
import string
import SaveGame
import Init
import Scorer
import Menu
import Alarm
call Scrap.CreateSaveVar('MissionState', 'GetCrazyData')
let switches = ['DM_Mission_Switch1', 'DM_Mission_Switch2', 'DM_Mission_Switch3', 'DM_Mission_Switch4']
let switchesRemaining = 4
let missionTime = 31
let TimerCanBeSeen = 0
let bureauPos = None
def InitStatus
def Init
def GetCrazyData
def OnAlarmActivated
def OnSwitchPressed
def StartTimer
def TimeExpired
def PreGoToPressRoom
def GoToPressRoom
def OnFakeBureauTaken
def PreFindHumphrey
def FindHumphrey
def HumphreySpeech
def PreFindTrurl
def PreFindTrurl2
def FindTrurl
def TrurlSpeech
def SetNextMission
def MissionScorer
