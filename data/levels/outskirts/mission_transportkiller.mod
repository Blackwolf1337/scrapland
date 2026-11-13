# The module code of Mission_TransportKiller in level outskirts (data01.packed/levels/outskirts/scripts/mission_transportkiller.pyc): its imports, variables, functions and load time calls in the order of the
# script; ScrapC makes the module from this file instead of running the .pyc. Made by tools/modules/pyc2mod.py.
# A pack in mods/ with scrapc/levels/outskirts/mission_transportkiller.mod replaces this file.

let NextMissionData = ('Mission_IgorFirstContact', 'GamblinDen')
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
import CharAct
import Traffic
let Model_Datapack = 'models/Misc/BDataPack/BDataPack.m3d'
call Scrap.CreateSaveVar('Mission_TransportKiller', 'Begin')
let TRANSPORT_LIFE = 1000
def InitStatus
def Init
def InitTransport
def isValidDamage
def TryToDeath
def TakeDataPack
let LastTimeMsg = 0.0
def TryToDamage
let pc = None
def InitCall
def UsePhoneCab
def SetNextMission
