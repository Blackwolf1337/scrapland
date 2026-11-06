# The module code of Mission_FindDtritusMatrix in level scrapyard (data02.packed/levels/scrapyard/scripts/mission_finddtritusmatrix.pyc): its imports, variables, functions and load time calls in the order of the
# script; ScrapC makes the module from this file instead of running the .pyc. Made by tools/modules/pyc2mod.py.
# A pack in mods/ with scrapc/levels/scrapyard/mission_finddtritusmatrix.mod replaces this file.

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
let dtritusMatrix = None
let dtritusMatrixArrow = None
let prevLonelyMercShipCameraPos = None
let lonelyMercShipDestroyed = 0
let lonelyMercShipName = ''
let lonelyMercParkings = []
let plansDictHelper = {}
let lastShipUsed = ''
let plans = []
let MatrixModel = 'models/Misc/MatrixInt/MatrixInt.m3d'
let LONELY_MERC_LIFE = 250
def SetLonelyMercProfile
def InitStatus
def Init
def CreateLonelyMercShip
def OnLonelyMercShipDeath
def CheckLonelyMercShipExist
def TestForLonelyMercShip
def CheckCreateBadGuy
def BadGuyAppears
def OutOfLonelyMercShip
def ActivateMissionObjects
def CreateBadGuy
def OnBadGuyDeath
def CreatePlan
def CreateMissionPlans
def TakePlanPhoto
def CreateDtritusMatrix
def TakeMatrix
def CheckEndOfMission
def SetNextMission
