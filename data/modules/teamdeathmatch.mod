# The module code of TeamDeathMatch (Data.packed/scripts/superdeals/teamdeathmatch.pyc): its imports, variables, functions and load time calls in the order of the
# script; ScrapC makes the module from this file instead of running the .pyc. Made by tools/modules/pyc2mod.py.
# A pack in mods/ with scrapc/modules/teamdeathmatch.mod replaces this file.

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
let CharLangTrans = {'MercenaryA': 'Mercs', 'MercenaryB': 'Mercs', 'MercenaryC': 'Mercs'}
let enemiesFrags = 0
let friendsFrags = 0
let enemies = []
let friends = []
let KeepPlaying = None
def GetMenuDescription
def InitStatus
def Init
def SetInvulnerableStatus
def SetEnemyProfile
def SetFriendProfile
def CreateFriends
def CreateEnemies
def AbortRemoteMessages
def CrazyGamblerStartCombat
def OnVehicle
def StartCombat
def OnEnemyDeath
def OnEnemyDeath2
def RespawnEnemy
def OnFriendDeath
def OnFriendDeath2
def RespawnFriend
def RespawnPlayer
def OnPlayerDeath
def OnPlayerDeath2
def RestartCombat
