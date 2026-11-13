# The module code of Versus (Data.packed/scripts/superdeals/versus.pyc): its imports, variables, functions and load time calls in the order of the
# script; ScrapC makes the module from this file instead of running the .pyc. Made by tools/modules/pyc2mod.py.
# A pack in mods/ with scrapc/modules/versus.mod replaces this file.

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
let combatDeaths = 0
let playerDeaths = 0
let enemyName = ''
call Scrap.CreateSaveVar('NumEnemyDeaths', '0')
call Scrap.CreateSaveVar('NumPlayerDeaths', '0')
def GetMenuDescription
def InitStatus
def Init
def CreateEnemy
def AbortRemoteMessages
def CrazyGamblerStartCombat
def StartCombat
def OnEnemyDeath
def RespawnEnemy
def OnPlayerDeath
def EnemyWins
def RespawnPlayer
def RestartCombat
