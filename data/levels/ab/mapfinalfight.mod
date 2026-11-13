# The module code of MapFinalFight in level ab (data03.packed/levels/ab/scripts/mapfinalfight.pyc): its imports, variables, functions and load time calls in the order of the
# script; ScrapC makes the module from this file instead of running the .pyc. Made by tools/modules/pyc2mod.py.
# A pack in mods/ with scrapc/levels/ab/mapfinalfight.mod replaces this file.

import Scrap
import SAI
import SLogic
import SWeap
import SScorer
import SInput
import Menu
import VehiclesNPC
set Menu.HeadBoss = 1
call Scrap.CreateSaveVar('FinalBossHintsTime', '0.0')
let BOSS_LIFE = 1550
let BOSS_RegenInit = 100
let BOSS_RegenEnd = 5
let BOSS_LIFE_BULLET = 1150
let BOSS_Last_RegenInit = 100
let BOSS_Last_RegenEnd = 100
let BOSS_LIFE_FINAL = 1000
let firstHintTime = 600
let secondHintTime = 1200
let thirdHintTime = 1800
let respawnTime = 3
let bossName = ''
let numTaunt = 0
let pos = None
let time = 0
def GetBossProfile
def FakeShips
def InitStatus
def Init
def CreateBoss
def SetShieldPhase
def MissionScorer
def SetFase
def BossAttacks
def BossRelax
def OnPlayerDeath
let HINT_COUNTER = 0
def RespawnPlayer
def LaunchTaunt
let FASE = 1
def OnBossRecoversZero
def OnBossRecoversEnd
def OnBossDestroyed
def PreLoadFinal
def LoadFinal
