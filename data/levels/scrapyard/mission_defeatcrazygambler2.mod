# The module code of Mission_DefeatCrazyGambler2 in level scrapyard (data02.packed/levels/scrapyard/scripts/mission_defeatcrazygambler2.pyc): its imports, variables, functions and load time calls in the order of the
# script; ScrapC makes the module from this file instead of running the .pyc. Made by tools/modules/pyc2mod.py.
# A pack in mods/ with scrapc/levels/scrapyard/mission_defeatcrazygambler2.mod replaces this file.

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
let CrazyGamblerLifesNextMission = 6
let timeMosquitosNewWave = 30
let crazyGamblerName = ''
let maxMosquitos = 10
let mosquitos = []
let numTaunt = 0
let pilotName = 'P_CrazyGambler'
let CRAZY_GAMBLER_LIFE = 250
let MOSQUITO_LIFE = 20
def GetCrazyGamblerProfile
def GetMosquitoProfile
def InitStatus
def Init
def LaunchTaunts
let givesSomething = 1
def CreateMosquito
def CreateMosquitos
def CreateCrazyGambler
def OnCrazyGamblerDeath
def PreSetNextMission
def SetNextMission
def MissionScorer
