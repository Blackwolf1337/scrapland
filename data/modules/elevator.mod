# The module code of Elevator (Data.packed/scripts/missions/elevator.pyc): its imports, variables, functions and load time calls in the order of the
# script; ScrapC makes the module from this file instead of running the .pyc. Made by tools/modules/pyc2mod.py.
# A pack in mods/ with scrapc/modules/elevator.mod replaces this file.

import Scrap
import SInput
import SNet
import SWeap
import SScorer
import SVec
import SAct
import SLogic
let OutMapList = ['GDB', 'DOWNTOWN', 'SCRAPYARD', 'COMMERCIAL', 'OUTSKIRTS']
def isOutMap
let ELEVATORS = {}
let ElevatorStart = {}
let TranspHeight = 500
let TranspCamRadius = 1000
let TrapspCamAngle = 0
def ElevatorCamera
def ElevatorBackCamera
let MOVE_UP = 'MoveUp '
let MOVE_DOWN = 'MoveDown '
let MOVE_LEVEL = 'MoveLevel '
let MOVE_TELE = 'MoveTele '
def ConnectElevators
def DoorInitializator
def ObjInitializator
def ObjInitializatorNang
let LinkedElevator = {}
let ELEVATOR_UP = 1
let ELEVATOR_UP_REACH = (-1)
let ELEVATOR_DOWN = 2
let ELEVATOR_DOWN_REACH = (-2)
let TELEPORTER_UP = 3
let TELEPORTER_UP_REACH = (-3)
let ELEVATOR_NEXTLEVEL = 4
def UseElevator
def ElevatorActiveToGet
def ElevatorCloseToGo
def ElevatorHaveClose
let OnElevatorReachDown = None
let OnElevatorReachUp = None
def ElevatorReachUp
def ElevatorReachDown
def ElevatorHaveOpen
def LinkToElevator
def UnlinkFromElevator
def Init
def SetDef
def TryToElevatorLaunchFirst
def PlayerCreatedElevator
