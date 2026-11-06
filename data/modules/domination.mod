# The module code of domination (Data.packed/scripts/superdeals/domination.pyc): its imports, variables, functions and load time calls in the order of the
# script; ScrapC makes the module from this file instead of running the .pyc. Made by tools/modules/pyc2mod.py.
# A pack in mods/ with scrapc/modules/domination.mod replaces this file.

import Scrap
import SLogic
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
import Scorer
call Scrap.CreateSaveVar('PlayerRounds', '0')
call Scrap.CreateSaveVar('EnemyRounds', '0')
let zonesCanBeTaken = 1
let playerZones = 0
let enemyZones = 0
let zones = []
let enemies = []
let friends = []
let zonesStates = {}
let ZoneModelNeutral = 'models/Misc/Flag/Flag.m3d'
let ZoneModelFriends = 'models/Misc/Flag/FlagGreen.m3d'
let ZoneModelEnemies = 'models/Misc/Flag/FlagRed.m3d'
let BatonModel = 'models/Misc/BDataPack/BDataPack.m3d'
let BatonRadius = 1000
let userWithBaton = 0
let shipWithBaton = ''
let onDeathSpawnTime = None
let CrazyGamblerStartDominationFirst = 1
def GetMenuDescription
def InitStatus
def Init
def ReleaseBaton
def LostBatonMessage
def SetInvulnerableStatus
def AbortRemoteMessages
def SetProfile
def CreateEnemies
def CreateFriends
def CreateBaton
def CrazyGamblerStartDomination
def OnVehicle
def StartDomination
def TakeBaton
def CreateZone
let msgTime = 0
def TakeZone
def ActivateZone
def RestartDomination
def OnPlayerDeath
def OnEnemyDeath
def OnFriendDeath
def RespawnPlayer
def RespawnEnemy
def RespawnFriend
def EndOfDomination
def PreRestartMenu
def RestartDomin
def PreOnVictory
def MissionScorer
