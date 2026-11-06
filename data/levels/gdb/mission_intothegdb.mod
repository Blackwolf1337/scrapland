# The module code of Mission_IntoTheGDB in level gdb (data03.packed/levels/gdb/scripts/mission_intothegdb.pyc): its imports, variables, functions and load time calls in the order of the
# script; ScrapC makes the module from this file instead of running the .pyc. Made by tools/modules/pyc2mod.py.
# A pack in mods/ with scrapc/levels/gdb/mission_intothegdb.mod replaces this file.

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
call Scrap.CreateSaveVar('MissionState', 'KillTraitorBishop')
let MAX_BISHOP_RESPAWN_TIME = 80.0
let MIN_BISHOP_RESPAWN_TIME = 10.0
let MAX_SCANNING_DIST = 7500
let MAX_SCANNING_TIME = 5.0
let MAX_TIME_TARGET_LOST = 3
let currentScannedShip = ''
let traitor = ''
let bishops = []
let rm = None
let traitorFound = 0
let MatrixModel = 'models/Misc/MatrixExt/MatrixExt.m3d'
let BISHOP_LIFE = 85
def GetBishopProfile
def InitStatus
def Init
def CreateBishop
def CreateBishops
def InitMission
def StopMission
def ScanShip
def CheckContinueScanning
def ResetScan
def ResetScan2
def OnFinishScan
def OnBishopsDeath
def OnBishopsDamage
def DropMatrix
def ActivateMatrix
def ReinsertMatrix
def TakeMatrix
def RestoreMatrix
def LaunchRestoreMatrixAnim
def RelaxBishops
def PreSetNextMission
def SetNextMission
def MissionScorer
