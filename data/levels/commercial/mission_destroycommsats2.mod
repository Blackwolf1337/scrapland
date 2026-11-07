# The module code of Mission_DestroyCommSats2 in level commercial (data02.packed/levels/commercial/scripts/mission_destroycommsats2.pyc): its imports, variables, functions and load time calls in the order of the
# script; ScrapC makes the module from this file instead of running the .pyc. Made by tools/modules/pyc2mod.py.
# A pack in mods/ with scrapc/levels/commercial/mission_destroycommsats2.mod replaces this file.

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
import Menu
import MissionsFuncs
import Speech
import Items
import string
import SaveGame
import Init
import Scorer
call Scrap.CreateSaveVar('NumProbes', '6')
let minPlayerDist = 50000
let minDistBetweenProbes = 10000
let PROBE_LIFE = 250
def InitStatus
def Init
def IsValidPoint
def CreateProbes
def OnProbeDeath
def DestroyShipsInList
def DestroyEnemyShips
def SetNextMission
def MissionScorer
