# The module code of Mission_TalkToBoss in level press (data01.packed/levels/press/scripts/mission_talktoboss.pyc): its imports, variables, functions and load time calls in the order of the
# script; ScrapC makes the module from this file instead of running the .pyc. Made by tools/modules/pyc2mod.py.
# A pack in mods/ with scrapc/levels/press/mission_talktoboss.mod replaces this file.

import Scrap
import SInput
import SNet
import SWeap
import SScorer
import SSound
import SVec
import SAct
import SAI
import sys
import Chars
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
import Scene
import MakeChar
import Elevator
import SaveGame
set Menu.HeadBoss = 1
let human = None
let bossName = 'Boss_00'
let alarmFallDelta = None
let maxHumanDist = 2000
let minHumanDist = 1000
call Scrap.CreateSaveVar('MissionState', 'FreeHuman2')
def InitStatus
def AfterBrowseNPC
def SetBossSpeech
def CreateHuman
def Init
def HideHumanIntoBureau
def CreateSceneLaunchers
def LaunchScene
def EndOfScene
def LoadScene
def OnPossession
def OnExitConversor
def CheckMaxDist
def CheckHumanDist
def PreGoTalkToBoss
def GoTalkToBoss
def BossSpeech
def SetNextMission
