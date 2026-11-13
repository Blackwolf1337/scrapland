# The module code of Traffic (Data.packed/scripts/missions/traffic.pyc): its imports, variables, functions and load time calls in the order of the
# script; ScrapC makes the module from this file instead of running the .pyc. Made by tools/modules/pyc2mod.py.
# A pack in mods/ with scrapc/modules/traffic.mod replaces this file.

import Scrap
import SInput
import SNet
import SWeap
import SScorer
import SAct
import SAI
import SVec
import sys
call Scrap.CreateSaveVar('Traffic.AcShips', '["SPoliBoss1", "SMerc1","SMerc2","SMerc3","SLMerc","SMayor1","SBanker1","SBishop1","SFunc1","SBerto1","SHump1","SBetty1","SBoss1","SBankMaster1","SArchbishop1"]')
let AcShips = eval(Scrap.GetSaveVar('Traffic.AcShips'))
let AcNormalShips = ['TLight1', 'TLight2', 'TLight3']
let AcMediumShips = ['TMed1', 'TMed2', 'TMed3']
let AcHeavyShips = ['THeavy1', 'THeavy2', 'THeavy3']
call Scrap.CreateEntityList('HeavyTraffic')
call Scrap.CreateEntityList('NormalTraffic')
call Scrap.CreateEntityList('MediumTraffic')
let NumNormal = 0
let NumMedium = 0
let NumHeavy = 0
call Scrap.Set('iTrafficNumber', 10)
call Scrap.Set('TrafficDamageTolerance', 10)
call Scrap.Set('MaxTrafficEnemyTime', 30)
call Scrap.Set('MinTrafficEnemyTime', 1)
call Scrap.Set('MaxTrafficEnemy', 4)
let Path = 'Models/Vehicles/OutTraffic/'
let PrevVal = None
def SetPath
def ResetPath
def InitAIProfiles
def EmptyFunc
let onTrackCreate = EmptyFunc
def MakeTrafic
def InitTraffic
def OnNeedMoreTraffic
def OnNeedMoreChasingTraffic
def OnPlayerHelpedPolice
def RailTrafficDeath
def HeavyTrafficDeath
def NormalTrafficDeath
def MediumTrafficDeath
def InitCivilTraffic
def Init
