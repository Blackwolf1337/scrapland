# The module code of Mission_ReinsertOwnMatrix2 in level gdb (data03.packed/levels/gdb/scripts/mission_reinsertownmatrix2.pyc): its imports, variables, functions and load time calls in the order of the
# script; ScrapC makes the module from this file instead of running the .pyc. Made by tools/modules/pyc2mod.py.
# A pack in mods/ with scrapc/levels/gdb/mission_reinsertownmatrix2.mod replaces this file.

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
import Menu
let MAX_BISHOP_RESPAWN_TIME = 80.0
let MIN_BISHOP_RESPAWN_TIME = 10.0
let bishops = []
let rm = None
let BISHOP_LIFE = 85
def GetBishopProfile
def InitStatus
def Init
def InitMission
def BishopsAttackPlayer
def CreateBishop
def CreateBishops
def OnBishopsDeath
def PreSetNextMission
def ShowDTMessage
def SetNextMission
