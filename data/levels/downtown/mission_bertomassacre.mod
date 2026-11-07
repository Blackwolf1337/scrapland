# The module code of Mission_BertoMassacre in level downtown (Data.packed/levels/downtown/scripts/mission_bertomassacre.pyc): its imports, variables, functions and load time calls in the order of the
# script; ScrapC makes the module from this file instead of running the .pyc. Made by tools/modules/pyc2mod.py.
# A pack in mods/ with scrapc/levels/downtown/mission_bertomassacre.mod replaces this file.

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
let DTritusMatrixModel = 'models/Misc/MatrixExt/MatrixExt.m3d'
let DTritusMatrixRadius = 3500
let bertoFound = 0
let bertoName = ''
let firstDeath = 1
let bertoDeath = 0
let initBar = 1
let MaxBarValue = 6
def InitStatus
def Init
def StartMission
def DeepThroatMessage
def CheckBertoDeath
def OnBertoFound
def DropMatrix
def TakeMatrix
def SetNextMission
def GetBerto
def MissionScorer
