# The module code of DeathMatch (Data.packed/scripts/superdeals/deathmatch.pyc): its imports, variables, functions and load time calls in the order of the
# script; ScrapC makes the module from this file instead of running the .pyc. Made by tools/modules/pyc2mod.py.
# A pack in mods/ with scrapc/modules/deathmatch.mod replaces this file.

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
let enemiesFrags = {}
let playerFrags = 0
let KeepPlaying = None
def GetMenuDescription
def InitStatus
def Init
def SetInvulnerableStatus
def SetProfile
def CreateEnemies
def AbortRemoteMessages
def CrazyGamblerStartCombat
def SetVulnerable
def StartCombat
def GetMaxFragsEnemies
def OnEnemyDeath
def RespawnEnemy
def RespawnPlayer
def OnPlayerDeath
def RestartCombat
