# The module code of Mission_DestroyMickus in level commercial (data02.packed/levels/commercial/scripts/mission_destroymickus.pyc): its imports, variables, functions and load time calls in the order of the
# script; ScrapC makes the module from this file instead of running the .pyc. Made by tools/modules/pyc2mod.py.
# A pack in mods/ with scrapc/levels/commercial/mission_destroymickus.mod replaces this file.

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
import Parking
call Scrap.CreateSaveVar('MissionState', 'FirstTime')
call Scrap.CreateSaveVar('NumTransports', '10')
let MAX_SCANNING_DIST = 5000
let MAX_SCANNING_TIME = 5.0
let MAX_TIME_TARGET_LOST = 3
let MAX_MICKUS_LIFE = 1000
let MAX_MICKUS_DIST = 150000
let currentScannedShip = ''
let transports = []
let mickus = ''
let TreasureModel = 'Models/Misc/Coins/TreasureA-Ext.m3d'
let FUNCS_LIFE = 40
def GetFuncEnemiesProfile
def InitStatus
def Init
def CreateTransports
def InitMission
def StopMission
def OnMickusDamage
def ScanShip
def CheckContinueScanning
def ResetScan
def ResetScan2
def OnFinishScan
def MickusIdentified
def ReassignTarget
def TraitoriusFuncEnemiesCom
def CheckMickusVisibility
def CreateFuncEnemies
def OnFuncEnemiesDeath
def OnTransportsDeath
def DropMoney
def TakeMoney
def SetNextMission
def MissionScorer
