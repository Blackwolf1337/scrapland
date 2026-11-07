# The module code of Mission_GetDataPart2 in level bank (Data.packed/levels/bank/scripts/mission_getdatapart2.pyc): its imports, variables, functions and load time calls in the order of the
# script; ScrapC makes the module from this file instead of running the .pyc. Made by tools/modules/pyc2mod.py.
# A pack in mods/ with scrapc/levels/bank/mission_getdatapart2.mod replaces this file.

import Scrap
import SAI
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
import Menu
set Menu.HeadBoss = 1
call Scrap.CreateSaveVar('MissionState', 'TalkToBoss')
let funcNames = ['Functionary_00', 'Functionary_01', 'Functionary_02']
let activatedDesktops = []
let dataDownloaded = 0
let maxDataDownloaded = 3
let speedFactor = 5.0
let dd = None
let firstTime = 1
def ComputeAutoUpdateStopValue
def InitStatus
def Init
def TalkToBoss
def BossSpeech
def PreInitMission
def InitMission
def SetDesktopState
def CreateFuncInit
def OnFunctionarySpeech
def OnFuncDefState
def SetStopDownloadingState
def SetNormalState
def AbortDownload
def OnDownloadAborted
def OnBeginDownloadData
def OnFuncStartsWorking
def OnDownloadComplete
def SetNextMission
def MissionScorer
