# The module code of Mission_DefeatCrazyGambler1 in level downtown (Data.packed/levels/downtown/scripts/mission_defeatcrazygambler1.pyc): its imports, variables, functions and load time calls in the order of the
# script; ScrapC makes the module from this file instead of running the .pyc. Made by tools/modules/pyc2mod.py.
# A pack in mods/ with scrapc/levels/downtown/mission_defeatcrazygambler1.mod replaces this file.

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
call Scrap.CreateSaveVar('NumCrazyGamblerLifes', '12')
let CrazyGamblerLifesNextMission = 9
let crazyGamblerName = ''
let numTaunt = 0
let pilotName = 'P_CrazyGambler'
let CRAZY_GAMBLER_LIFE = 350
def GetCrazyGamblerProfile
def InitStatus
def Init
def LaunchTaunts
def CreateCrazyGambler
def OnCrazyGamblerDeath
def PreSetNextMission
def SetNextMission
def MissionScorer
