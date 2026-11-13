# The module code of Mission_BeatPickus in level police (data01.packed/levels/police/scripts/mission_beatpickus.pyc): its imports, variables, functions and load time calls in the order of the
# script; ScrapC makes the module from this file instead of running the .pyc. Made by tools/modules/pyc2mod.py.
# A pack in mods/ with scrapc/levels/police/mission_beatpickus.mod replaces this file.

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
import Alarm
call Scrap.CreateSaveVar('MissionState', 'TalkToTraitorius')
let followPlayerVel = 0.8
let thugsStopped = 0
let launcher = None
let plansNames = []
let numPlansWithPhoto = 0
let alreadyTalked = 0
let FXSceneBeatPickusFile = 'Models/GFX/Scenes/BeatPickus.M3D'
def InitStatus
def Init
def DisableIAFunctionary
def OnTraitoriusRespawn
def OnThugRespawn
def SetThugsState
def SetState
def TraitoriusSpeech
def PreHireThugs
def HireThugs
def ThugLeaderSpeech
def ThugNormalSpeech
def PreBeatPickus
def BeatPickus
def OnAlarmActivated
def OnAlarmDeactivated
def CheckMaxDist
def CheckThugsDist
def CreateLauncher
def LaunchPickusFightScene
def CreatePickusBeatenClass
def InitPickusBeaten
def DelThug
def AskPickus
def OnTalkToPickus
def PreGetFuncShipPlans
def GetFuncShipPlans
def TakePlanPhoto
def PreTalkToTraitoriusAgain
def TalkToTraitoriusAgain
def TraitoriusAgainSpeech
def SetNextMission
def LoadScene
def LightingActivate
