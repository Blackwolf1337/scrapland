# The module code of Mission_DestroyHumphrey in level outskirts (data01.packed/levels/outskirts/scripts/mission_destroyhumphrey.pyc): its imports, variables, functions and load time calls in the order of the
# script; ScrapC makes the module from this file instead of running the .pyc. Made by tools/modules/pyc2mod.py.
# A pack in mods/ with scrapc/levels/outskirts/mission_destroyhumphrey.mod replaces this file.

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
import OutMap
import VehiclesNPC
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
call Scrap.CreateSaveVar('NumHumphreyLifes', '5')
let humphreyName = ''
let numTaunt = (-1)
let HUMPHREY_LIFE = 300
def GetHumphreyProfile
def InitStatus
def Init
def StartMission
def CreateHumphrey
def HumphreyAttacks
def LaunchTaunt
def OnHumphreyDeath
def BossAppears
def SetNextMission
def MissionScorer
