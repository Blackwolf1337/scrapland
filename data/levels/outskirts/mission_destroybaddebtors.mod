# The module code of Mission_DestroyBadDebtors in level outskirts (data01.packed/levels/outskirts/scripts/mission_destroybaddebtors.pyc): its imports, variables, functions and load time calls in the order of the
# script; ScrapC makes the module from this file instead of running the .pyc. Made by tools/modules/pyc2mod.py.
# A pack in mods/ with scrapc/levels/outskirts/mission_destroybaddebtors.mod replaces this file.

import Scrap
import SAI
import SInput
import SNet
import SLogic
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
import Menu
let TreasureModel = 'Models/Misc/Coins/TreasureA-Ext.m3d'
let TreasureRadius = 3500
let MAX_MONEY = 1000
let TREASURE_REWARD = 100
let MISSION_BANKMASTER_REWARD = 1000
let MONEY_MERCS_APPEAR = (0.1 * MAX_MONEY)
let moneyRecovered = 0
let mercsHaveAppeared = 0
let missionStartWaypoint = None
let currentMissionTarget = ''
let mercsActive = []
let bankers = []
let firstTimeDropped = 1
let treasuresActive = []
let bankMasterName = ''
let shipsMoney = {}
let MERCS_LIFE = 70
let BANK_DIRECTOR_LIFE = 70
let BANKER_LIFE = 65
def GetMercenariesProfile
def GetBankDirectorProfile
def GetBankerProfile
def InitStatus
def Init
def CreateWaypoint
def CreateShip
def OnInitMission
def ShowScorer
def EnableMissionWaypoint
def SelectNewMissionTarget
def OnMissionTargetDeath
def BankMasterAttacksMissionTarget
def OnMissionTargetDamage
def DropTreasure
def DelTreasure
def SendShipToGetTreasure
def GetPossibleMercTargets
def CreateMercs
def CreateBankMaster
def CreateBanker
def RespawnBankMaster
def OnBankMasterDeath
def OnBankMasterDamage
def CheckMercsFreezed
def OnBankerDeath
def OnBankerDamage
def OnMercDeath
def OnMercDamage
def OnTreasureReached
def OnShipReachedTreasurePos
def OnCoinTaken
def MercsAppear
def BankMasterSpeech
def ShowDeepThroatMessage
def SetNextMission
def MissionScorer
