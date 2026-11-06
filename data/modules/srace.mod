# The module code of srace (Data.packed/scripts/split/srace.pyc): its imports, variables, functions and load time calls in the order of the
# script; ScrapC makes the module from this file instead of running the .pyc. Made by tools/modules/pyc2mod.py.
# A pack in mods/ with scrapc/modules/srace.mod replaces this file.

import Scrap
import SInput
import SNet
import SWeap
import SScorer
import SSound
import SAct
import sys
import SAI
import SFX
import SVec
let FlagModel = 'models/Misc/Wayp/Wayp.m3d'
let race = []
let Flag = None
let AcFlag = 0
let RaceCountDownSound1 = 'foco-aviso1'
let RaceCountDownSound1Vol = 0.3
let RaceCountDownSound2 = 'foco-aviso2'
let RaceCountDownSound2Vol = 0.3
def BrowseAreas
def GetRndFlag
def TakeFlag
def CreateFlag
def InitRacePos
def GetReSpawnPos
def InitStatus
def Init
def ControlToRacers
def OnDeathFunc
