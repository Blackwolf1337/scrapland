# The module code of Mission_GetDataPart1 in level downtown (Data.packed/levels/downtown/scripts/mission_getdatapart1.pyc): its imports, variables, functions and load time calls in the order of the
# script; ScrapC makes the module from this file instead of running the .pyc. Made by tools/modules/pyc2mod.py.
# A pack in mods/ with scrapc/levels/downtown/mission_getdatapart1.mod replaces this file.

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
set Menu.HeadBoss = 1
call Scrap.CreateSaveVar('MissionState', 'FirstTime')
call Scrap.CreateSaveVar('NumData', '0')
let MAX_TRANSPORTS = 5
let MAX_TRANSPORTS_DATA_UNITS = 5
let MAX_DATA_UNITS = 25
let TRANSPORT_LIFE = 100
let FUNCS_LIFE = 40
let NumDataFuncsAttackTransport = int((float(MAX_DATA_UNITS) * 0.5))
let TimeBetweenData = 1
let TimeDataDisappears = 10
let transports = []
let transportsTaunts = []
let funcsAttackingTransport = []
let currentTransport = ''
let currentTransportSpawnedData = 0
let funcsSpawned = 0
let funcsAttackTransportMessage = 0
let droppedData = 0
let DataCubeModel = 'models/Misc/DataCube/DataCube.m3d'
def GetFuncEnemiesProfile
def InitStatus
def Init
def InitMission
def StopMission
def CreateTransport
def CreateFunctionary
def AddTransportToList
def SpawnFunctionaries
def OnFunctionaryDeath
def OnTransportsDeath
def OnTransportsDamage
def SpawnData
def DropData
def DelData
def TakeData
def SetNextMission
def MissionScorer
