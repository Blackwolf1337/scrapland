# The module code of Mission_TalkToMayor in level townhall (data02.packed/levels/townhall/scripts/mission_talktomayor.pyc): its imports, variables, functions and load time calls in the order of the
# script; ScrapC makes the module from this file instead of running the .pyc. Made by tools/modules/pyc2mod.py.
# A pack in mods/ with scrapc/levels/townhall/mission_talktomayor.mod replaces this file.

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
import Menu
let MAX_TAMAYO_MONEY = 1000.0
let MIN_TAMAYO_MONEY = 400.0
call Scrap.CreateSaveVar('MissionState', 'TalkToMayor1')
let functionaryTamayoName = ''
let porkinsName = ''
let porkinsMoney = (MAX_TAMAYO_MONEY * 7)
let tamayoMoney = MAX_TAMAYO_MONEY
let porkinsHasMoney = 1
let tamayoStartOrigin = None
def InitStatus
def AfterBrowseNPC
def CreateMayorInit
def CreateBishopPorkins
def CreateFunctionaryTamayo
def Init
def MayorSpeech
def BishopSpeech
def FunctionarySpeech
def PreStealFunctionary
def StealFunctionary
def CheckTamayoMoney
def RestoreTamayoState
def OnBankerSteal
def EndOfStealFunctionary
def SetNextMission
def MissionScorer
