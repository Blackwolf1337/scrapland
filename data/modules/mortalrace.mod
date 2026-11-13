# The module code of MortalRace (Data.packed/scripts/superdeals/mortalrace.pyc): its imports, variables, functions and load time calls in the order of the
# script; ScrapC makes the module from this file instead of running the .pyc. Made by tools/modules/pyc2mod.py.
# A pack in mods/ with scrapc/modules/mortalrace.mod replaces this file.

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
import Menu
import MissionsFuncs
import Speech
import Items
import string
import SaveGame
import Init
import Scorer
let CrazyGamblerStartMortalRaceFirst = 1
let BayScorer = {}
let BestScorer = 0
let YourScore = 0
let iStartPos = 0
let waypointName = ''
let userWithBaton = 0
let shipWithBaton = ''
let messageKillShipThrown = 0
let numWaypoints = 0
let profileType = ''
let respawnTime = 0
let numRacers = 0
let racers = []
let nameProfileDict = {}
let baton = None
let MeetPointModel = 'models/misc/MeetingPoint-Race/MeetingPoint-Race.m3d'
let WayPointModel = 'models/Misc/Wayp/Wayp.m3d'
let BatonModel = 'models/Misc/BDataPack/BDataPack.m3d'
let BatonRadius = 1800
let WaypRadius = 1000
let MIN_CHASE_DIST = 50000.0
let MAX_FIGHTING_FREEZE_DIST = 30000.0
let NUM_WAYPOINTS_AHEAD = 7
let FightingFreezeCadenceShoot = 0
let FightingFreezeControlAim = 4
let prevCadenceShoot = None
let prevControlAim = None
let prof = None
def GetMenuDescription
def InitStatus
def Init
def ReleaseBaton
def LostBatonMessage
def SetInvulnerableStatus
def AbortRemoteMessages
def CrazyGamblerStartMortalRace
def SetVulnerable
def CreateBaton
def TakeBaton
def InitRace
def CreateFirstWaypoint
def OnRaceStart
def RaceGo
def CheckShipsState
def OnPositionReached
def OnCheckPoint
def Restart
def RestartRace
def MoveWaypoint
def EndOfMortalRace
def MoveRacersToTraffic
def MoveRacersToPos
def RacersAttackShip
def OnPlayerDeath
def RacerKilled
def RacerDamaged
def RespawnRacer
def RespawnPlayer
def PreOnVictory
