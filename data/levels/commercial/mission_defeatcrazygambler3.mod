# The module code of Mission_DefeatCrazyGambler3 in level commercial (data02.packed/levels/commercial/scripts/mission_defeatcrazygambler3.pyc): its imports, variables, functions and load time calls in the order of the
# script; ScrapC makes the module from this file instead of running the .pyc. Made by tools/modules/pyc2mod.py.
# A pack in mods/ with scrapc/levels/commercial/mission_defeatcrazygambler3.mod replaces this file.

import Scrap
import SAI
import SInput
import SLogic
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
import Parking
call Scrap.CreateSaveVar('BossMatrixPos', 'None')
let crazyGamblers = []
let numTaunt = 0
let BossMatrixModel = 'models/Misc/MatrixExt/MatrixExt.m3d'
let BossMatrixRadius = 3500
let pilotName = 'P_CrazyGambler'
let GAMBLER_LIFE = 250
def GetCrazyGamblerProfile
def InitStatus
def Init
def LaunchFirstTaunt
def LaunchTaunts
def RefreshScorer
def CreateCrazyGambler
def CreateCrazyGamblers
def OnCrazyGamblerDeath
def AfterDeepThroatMessage
def DropBossMatrix
def TakeMatrix
def SetNextMission
def MissionScorer
