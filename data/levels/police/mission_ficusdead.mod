# The module code of Mission_FicusDead in level police (data01.packed/levels/police/scripts/mission_ficusdead.pyc): its imports, variables, functions and load time calls in the order of the
# script; ScrapC makes the module from this file instead of running the .pyc. Made by tools/modules/pyc2mod.py.
# A pack in mods/ with scrapc/levels/police/mission_ficusdead.mod replaces this file.

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
call Scrap.CreateSaveVar('MissionState', 'TalkToFicus')
let ficus = None
let numCrazyGamblerComs = 0
def InitFicusDead
def InitStatus
def Init
def OnTalkToFicus
def AfterDtritusSpeechShown
def PreAfterDeepThroatMessageShown
def AfterDeepThroatMessageShown
def DeepThroatSpeech
def SetNextMission
