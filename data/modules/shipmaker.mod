# The module code of ShipMaker (Data.packed/scripts/vehicles/shipmaker.pyc): its imports, variables, functions and load time calls in the order of the
# script; ScrapC makes the module from this file instead of running the .pyc. Made by tools/modules/pyc2mod.py.
# A pack in mods/ with scrapc/modules/shipmaker.mod replaces this file.

import Scrap
import SInput
import SNet
import SWeap
import SScorer
import SAct
import sys
let ShipTon = {}
let ShipValues = {}
let ShipBalance = {}
let ShipCallback = {}
let MotorValues = {}
let WeaponValues = {}
let StdShips = {}
let Chars = {}
let PilotsList = [
	'P_Dtritus',
	'P_CrazyGambler',
	'P_BankDirector',
	'P_BankMaster',
	'P_Berto',
	'P_Betty',
	'P_Humphrey',
	'P_Functionary',
	'P_Mayor',
	'P_Police',
	'P_PoliceBoss',
	'P_MercenaryB',
	'P_MercenaryC',
	'P_Bishop',
	'P_Boss'
]
if (Scrap.GetSaveVar('LonelyMercDataActive') == '1')
	call PilotsList.append('P_MercenaryL')
else
	call PilotsList.append('P_MercenaryA')
end
let WeaponCosts = {}
let shipsCosts = {}
let enginesCosts = {}
let FLAPS_ANM = 1
let MOTOR_ANM = 2
let PILOTAMN = ('Left', 'Right', 'Go', 'Back', 'Relax')
let armorPieceCost = 10
call Scrap.Set('AmorPieceWeight', 10)
def SetShipPrice
def GetShipPrice
def SetEnginePrice
def GetEnginePrice
def AddShipTon
def GetShipTon
def AddShipBalance
def GetShipBalance
def AddShip
def GetShip
def AddMotor
def GetMotor
def IncludeMotor
def AddWeapon
def GetWeapon
def CalculateVerticalBalance
def SetUpChars
def AddChar
def GetBoostFactor
def SetShipParams
def CreateStdShips
def Init
def SetAnmShip
def InitAnmShip
