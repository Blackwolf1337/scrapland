# The module code of Mission_GetDataPart3 in level commercial (data02.packed/levels/commercial/scripts/mission_getdatapart3.pyc): its imports, variables, functions and load time calls in the order of the
# script; ScrapC makes the module from this file instead of running the .pyc. Made by tools/modules/pyc2mod.py.
# A pack in mods/ with scrapc/levels/commercial/mission_getdatapart3.mod replaces this file.

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
set Menu.HeadBoss = 1
call Scrap.CreateSaveVar('NumData', '0')
call Scrap.CreateSaveVar('MissionState', 'CallBoss')
let MAX_DATA_UNITS = 8
let funcsInWave = []
let funcWithDataName = ''
let timeToReinsertEnemy = 30
let droppedDataPos = None
let DataCubeModel = 'models/Misc/DataCube/DataCube.m3d'
let DataCubeRadius = 3500
let FUNC_LIFE = 75
def GetFunctionariesEnemiesProfile
def InitStatus
def Init
def InitMission
def StopMission
def CreateWave
def CreateFunctionary
def ReinsertFunctionaryInWave
def OnFunctionaryDeath
def OnFunctionaryDamage
def DropData
def CheckAITakesData
def CheckAI
def TakeData
let pc = None
def CallBoss
def UsePhoneCab
def PreGetData
def GetData
def PreCallDeepThroat
def PreCallDeepThroat2
def CallDeepThroat
def UsePhoneCab2
def SetNextMission
def MissionScorer
