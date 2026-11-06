# The module code of Mission_ProtectMayorTransport in level downtown (Data.packed/levels/downtown/scripts/mission_protectmayortransport.pyc): its imports, variables, functions and load time calls in the order of the
# script; ScrapC makes the module from this file instead of running the .pyc. Made by tools/modules/pyc2mod.py.
# A pack in mods/ with scrapc/levels/downtown/mission_protectmayortransport.mod replaces this file.

import SAI
import Scrap
import SInput
import SNet
import SWeap
import SScorer
import SSound
import SVec
import SAct
import sys
import VehiclesNPC
import MissionsFuncs
import Speech
import Menu
import CharScorer
import Vehicles
import SaveGame
import Traffic
call Scrap.CreateSaveVar('CrazyMayor', 'Transport')
call Scrap.CreateSaveVar('CrazyMayor.KillThem', '0')
call Scrap.CreateSaveVar('TransportState', 'Killed')
let RouteBegin = ((-85890.0703125), (-31833.34375), 74036.8359375)
let RouteEnd = ((-77849), (-34627), (-20712))
let RouteTarget = ((-189988), (-30177), 360645)
let MissionBegin = ((-120349), (-57360), 48512)
let ScortReward = 5000
let trans = None
let Bad1 = None
let Bad2 = None
let NumTargets = 0
let MaxTargets = 10
let MERCS_LIFE = 80
let TRANSPORT_LIFE = 500
def GetMercEnemiesProfile
def InitStatus
def Init
def MissionInit
def OnMissionStart
def OnMissionStart2
def AbortConversation
def OnPlayerDeathScorting
def TransGoto
def CreateBadGuy
def AppearsBadGuys
def AppearsBadGuys2
def NextBadGuy
def BackToTheTransport
def BackToPlayer
def MadGuyDeath
def PreLaunchTraitorius
def TraitoriusCom
def SetNextMission
def MadGuyDamage
def MercTauntMessager
def OnTravelComplete
def FailMission
def ActualizeScorer
def MissionScorer
let BornMercTaunt = (-3)
def KillThem
def InitKillThemMission
def InitKillBadGuys
def RestoreTransport
