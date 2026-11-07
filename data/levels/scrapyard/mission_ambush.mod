# The module code of Mission_Ambush in level scrapyard (data02.packed/levels/scrapyard/scripts/mission_ambush.pyc): its imports, variables, functions and load time calls in the order of the
# script; ScrapC makes the module from this file instead of running the .pyc. Made by tools/modules/pyc2mod.py.
# A pack in mods/ with scrapc/levels/scrapyard/mission_ambush.mod replaces this file.

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
import MissionsFuncs
import Speech
import Items
import string
import SaveGame
import Init
import Scorer
call Scrap.CreateSaveVar('NumProbes', '-1')
let minPlayerDist = 50000
let numProbes = (-1)
let pc = None
let MADHUNTER_LIFE = 50
let PROBE_LIFE = 250
def GetMadHuntersProfile
def InitStatus
def Init
def CallTraitorius
def UsePhoneCab
def DeepThroatCom
def PreLaunchAmbush
def LaunchAmbush
def IsValidPos
def CreateProbe
def CreateMadHunter
def CreateMadHunters
def OnMadHunterDeath
def OnProbeDeath
def PreCallDeepThroat
def CallDeepThroat
def UsePhoneCab2
def SetNextMission
