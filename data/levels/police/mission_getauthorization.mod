# The module code of Mission_GetAuthorization in level police (data01.packed/levels/police/scripts/mission_getauthorization.pyc): its imports, variables, functions and load time calls in the order of the
# script; ScrapC makes the module from this file instead of running the .pyc. Made by tools/modules/pyc2mod.py.
# A pack in mods/ with scrapc/levels/police/mission_getauthorization.mod replaces this file.

import Scrap
import SAI
import SVec
import SAct
import SScorer
import SLogic
import Chars
import CharsNPC
import SaveGame
import Speech
import DTritusDesktop
import MissionsFuncs
import CharConversor
import Menu
import CharsNPC
import Elevator
import Scene
import MakeChar
import GateKeeper
call Scrap.CreateSaveVar('MissionState', 'DeepThroatSpeech')
let maxHumanDist = 2000
let minHumanDist = 1000
let filledByUser = 0
let scenePlayed = 0
let alarmFallDelta = None
let human = None
let conv = None
let dd = None
def InitStatus
def CreateHuman
def Init
def AfterDeepThroatMessage
def DeepThroatSpeech
def PreGetAuthorization
def GetAuthorization
def ShowNearestConversor
def ShowAfterConversionMessage
def GetAuthorization2
def OnDataTaken
def OnDataDropped
def PreFreeHuman
def FreeHuman
def TalkPepe
def StartScene
def OnTriggerActivated
def GoToPress
def GoToPress2
def OnMainCharDeath
def OnPossession
def OnExitConversor
def CheckMaxDist
def CheckHumanDist
def OnMetroUsed
def LoadScene
