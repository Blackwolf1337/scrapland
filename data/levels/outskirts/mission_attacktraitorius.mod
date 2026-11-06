# The module code of Mission_AttackTraitorius in level outskirts (data01.packed/levels/outskirts/scripts/mission_attacktraitorius.pyc): its imports, variables, functions and load time calls in the order of the
# script; ScrapC makes the module from this file instead of running the .pyc. Made by tools/modules/pyc2mod.py.
# A pack in mods/ with scrapc/levels/outskirts/mission_attacktraitorius.mod replaces this file.

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
import VehiclesNPC
call Scrap.CreateSaveVar('TraitoriusFrightened', '0')
let styleOffset = 1000
let minLifePercentage = 0.3
let frightenedTime = 60.0
let timeCallsPolicePercentage = 0.58
let traitorius = None
let pc = None
let pd = None
let TRAITORIUS_LIFE = 100
def GetTraitoriusProfile
def InitStatus
def Init
def ReassignTarget
def CreateTraitorius
let firstTimeDamage = 0
def OnTraitoriusDamage
def OnTraitoriusDeath
def TraitoriusTakesLife
def TimeExpired
def PreCallTraitorius
def CallTraitorius
def UsePhoneCab
def SetNextMission
def MissionScorer
