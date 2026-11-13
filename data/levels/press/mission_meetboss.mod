# The module code of Mission_MeetBoss in level press (data01.packed/levels/press/scripts/mission_meetboss.pyc): its imports, variables, functions and load time calls in the order of the
# script; ScrapC makes the module from this file instead of running the .pyc. Made by tools/modules/pyc2mod.py.
# A pack in mods/ with scrapc/levels/press/mission_meetboss.mod replaces this file.

import Scrap
import SInput
import SNet
import SWeap
import SScorer
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
import Scene
import MakeChar
import Elevator
call Scrap.CreateSaveVar('MissionState', 'MeetBoss')
call Scrap.CreateSaveVar('PlayerHasBeenKilled', '0')
let alarmFallDelta = None
let sebastian = None
def InitStatus
def AfterBrowseNPC
def CreateHumphreyInit
def CreateBertoInit
let FirstBertoSpeech = 1
def BertoSpeech
def BertoSpeech2
def DeactivateEmero
def ActivateEmero
def EndOfMetroAnim
def Init
def OnMainCharDeath
def OnTryingToExit
def OnTryingToExit2
def OnTryingToExit3
def PlayBettyTheme
def StopBettyTheme
def PlayProlog0
def PlayProlog1
def PlayProlog2
def AssignFirstMission
def CreateBattery
def AssingOnTakeBattery
def OnTakeBattery
def PreFirstCrazyDeal
def FirstCrazyDeal
def CrazyGamblerSpeech
def PreBertoPossession
def BertoPossession
def OnWarning
def OnActionPressed
def OnPossession
def ShowMessage
def OnAlarmEnd
def UseConversor
def OnConversorUsed
def CheckConversion
def PreChallengeHumphrey
def ChallengeHumphrey
def HumphreySpeech
def SetNextMission
def LoadProlog0Scene
def LoadProlog1Scene
def LoadProlog2Scene
def FXMeetBossProlog2Flash
