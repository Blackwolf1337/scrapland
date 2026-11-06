# The module code of Mission_FunctionaryHunting in level outskirts (data01.packed/levels/outskirts/scripts/mission_functionaryhunting.pyc): its imports, variables, functions and load time calls in the order of the
# script; ScrapC makes the module from this file instead of running the .pyc. Made by tools/modules/pyc2mod.py.
# A pack in mods/ with scrapc/levels/outskirts/mission_functionaryhunting.mod replaces this file.

import Scrap
import SInput
import SNet
import SWeap
import SScorer
import SSound
import SLogic
import SVec
import SAI
import SAct
import sys
import MissionsFuncs
import Speech
import Items
import string
import SaveGame
import Init
import Scorer
let FUNCTIONARIES_TO_KILL = 15
call Scrap.CreateSaveVar('NumFunctionariesKilled', '0')
let numFunctionariesKilled = 0
let missionStartWaypoint = None
let initialFriendsList = []
let FUNCS_LIFE = 75
def GetFuncEnemiesProfile
def GetPoliceFriendsProfile
def InitStatus
def Init
def CreateWaypoint
def EnableMissionWaypoint
def MissionScorer
def ComeBack
def OnChangeVehicle
def CreateNewPoliShip
def CreateNewFuncShip
def CreateFriendsShips
def CreateEnemiesShips
def SetNextMission
def CGCalls
def OnFuncDeath
def isPoliceShip
def OnMissionStart
def LaunchMission
def StartCameraTravelling
def AbortCameraTravelling
