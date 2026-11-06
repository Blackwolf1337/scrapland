# The module code of Mission_CatchTrurl in level townhall (data02.packed/levels/townhall/scripts/mission_catchtrurl.pyc): its imports, variables, functions and load time calls in the order of the
# script; ScrapC makes the module from this file instead of running the .pyc. Made by tools/modules/pyc2mod.py.
# A pack in mods/ with scrapc/levels/townhall/mission_catchtrurl.mod replaces this file.

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
call Scrap.CreateSaveVar('CatchTrurlMissionData', 'CatchTrurl')
call Scrap.CreateSaveVar('Mission_CatchTrurl_Data', '[]')
call Scrap.CreateSaveVar('Mission_CatchTrurl_MapsPos', '[]')
call Scrap.CreateSaveVar('Mission_CatchTrurl_NumMapsDropped', '0')
call Scrap.CreateSaveVar('Mission_CatchTrurl_NumMapsTaken', '0')
def InitStatus
def Init
def CreateTrurl
def OnCatchTrurl
def OnCatchTrurlAgain
def FunctionaryMissionSpeech
def SetCallbackFuncs
def SpawnMap
def CreateMap
def TakeMapPhoto
def KillFunctionaries
def CatchTrurlAgain
def SetNextMission
def CGCalls
