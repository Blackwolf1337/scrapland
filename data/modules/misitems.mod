# The module code of MisItems (Data.packed/scripts/missions/misitems.pyc): its imports, variables, functions and load time calls in the order of the
# script; ScrapC makes the module from this file instead of running the .pyc. Made by tools/modules/pyc2mod.py.
# A pack in mods/ with scrapc/modules/misitems.mod replaces this file.

import Scrap
import SInput
import SNet
import SWeap
import SScorer
import SVec
import SAct
import SSound
import sys
import math
let MONEY_TIME = 60
let MONEY_APPEARS_TIME = 1.1
let COIN_RADIUS_OUTDOOR = 1000
let COIN_RADIUS_INDOOR = 100
let MoneyQuantos = [150, 1000]
def Init
def OnVehicleDeat
def GetTargetReward
def GiveMoney
def GetMoneyModel
def GetObjModel
def CreateItem
def CreateCoin
def ActivateCoin
def CreateBattery
def ActivateBattery
def TakeBattery
let DazedReward = (4 * 8)
def DazedCoin
