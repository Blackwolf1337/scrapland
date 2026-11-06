# The module code of Mission_Massacre in level downtown (Data.packed/levels/downtown/scripts/mission_massacre.pyc): its imports, variables, functions and load time calls in the order of the
# script; ScrapC makes the module from this file instead of running the .pyc. Made by tools/modules/pyc2mod.py.
# A pack in mods/ with scrapc/levels/downtown/mission_massacre.mod replaces this file.

let NextMissionData = ('Mission_ArrangeIgorCombat', 'GamblinDen')
import Scrap
import SInput
import SNet
import SWeap
import SScorer
import SAI
import SVec
import SAct
import MissionsFuncs
import Speech
import SaveGame
import RaceMaker
import PhoneCab
import Vehicles
import VehiclesNPC
import Menu
import Parking
import string
let MAX_POINTS = 100
call Scrap.CreateSaveVar('Mission_Massacre.Points', '0')
let betweenDeathsTime = 20
def InitStatus
def Init
def OnVehicleCallback
def MissionScorer
def TimeExpired
def SetNextMission
