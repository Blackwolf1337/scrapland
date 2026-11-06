# The module code of Mission_ProtectGus in level commercial (data02.packed/levels/commercial/scripts/mission_protectgus.pyc): its imports, variables, functions and load time calls in the order of the
# script; ScrapC makes the module from this file instead of running the .pyc. Made by tools/modules/pyc2mod.py.
# A pack in mods/ with scrapc/levels/commercial/mission_protectgus.mod replaces this file.

import SAI
import Scrap
import SInput
import SNet
import SWeap
import SScorer
import SSound
import SVec
import SAct
import VehiclesNPC
import sys
import Scene
import MissionsFuncs
import Speech
import Menu
import CharScorer
import Vehicles
import SaveGame
import Traffic
call Scrap.CreateSaveVar('MissionState', 'ProtectGus')
let pc = None
let lonelyMerc = None
let bishopGus = None
let lonelyMercLifes = 10
let bishopGusLifes = 10
let LONELY_MERC_LIFE = 250
let BISHOP_GUS_LIFE = 200
def CreateLonelyMercProfile
def CreateBishopGusProfile
def InitStatus
def Init
def InitMission
def AttackEachOther
def CreateLonelyMerc
def CreateBishopGus
def OnLonelyMercDeath
def OnBishopGusDeath
def PreCallDeepThroat
def SetPhoneCab
def UsePhoneCab
def SetNextMission
def MissionScorer
