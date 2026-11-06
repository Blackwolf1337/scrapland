# The module code of CTF (Data.packed/scripts/superdeals/ctf.pyc): its imports, variables, functions and load time calls in the order of the
# script; ScrapC makes the module from this file instead of running the .pyc. Made by tools/modules/pyc2mod.py.
# A pack in mods/ with scrapc/modules/ctf.mod replaces this file.

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
let CrazyGamblerStartCTFFirst = 1
let enemies = []
let friends = []
let flagsActive = []
let flagsSpecial = []
let flags = 0
let maxFlags = 0
let onDeathSpawnTime = 5.0
let flagsEnemies = 0
let flagsFriends = 0
def GetMenuDescription
def InitStatus
def Init
def SetInvulnerableStatus
def AbortRemoteMessages
def SetProfile
def CreateEnemies
def CreateFriends
def CrazyGamblerStartCTF
def SetVulnerable
def StartCTF
def OnVehicleDeathCallback
def OnPlayerDeath
def RemoveFlag
def EndOfCTF
def RespawnEnemy
def RespawnFriend
def CreateFlag
def ResetFlag
def TakeFlag
def SpawnFlags
def RespawnPlayer
def PreRestartMenu
def RestartCTF
def PreOnVictory
