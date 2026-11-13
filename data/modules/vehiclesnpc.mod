# The module code of VehiclesNPC (Data.packed/scripts/vehicles/vehiclesnpc.pyc): its imports, variables, functions and load time calls in the order of the
# script; ScrapC makes the module from this file instead of running the .pyc. Made by tools/modules/pyc2mod.py.
# A pack in mods/ with scrapc/modules/vehiclesnpc.mod replaces this file.

import Scrap
import SInput
import SNet
import SWeap
import SScorer
import SAct
import SAI
import sys
call Scrap.CreateSaveVar('StdShipAIProfile', 'Rookie')
let lmigd = (Scrap.GetSaveVar('LonelyMercDataActive') == '1')
class cShipAI
	let Ship = 'Spoli1'
	let ControlStrafe = 2
	let ControlBrake = 0
	let ControlBoost = 1
	let ControlBoostRace = 1
	let ControlBoostItemLife = 1
	let ControlBoostItemAmmo = 1
	let StupidPathFinding = 0
	let CadenceShoot = 15
	let FactorDamageGetAway = 0.1
	let FactorDamageItemLife = 0.25
	let FactorItemAmmo = 0.25
	let TimeMaxStrafe = 2
	let VelMaxStrafe = 0.25
	let AngAttack = 0.785
	let ControlAim = 2
	let CounterAttack = 1.0
	let LifeMult = 1.0
	let GoNextItemRace = 1
	let ControlHookRace = 0
	let FactorHookMap = 0
	let ControlStrafeEvasion = 0
	let CadenceShootCM = 3
	let UsePercSonic = 50
	let UsePercCloud = 35
	let UsePercElectro = 15
	let UsePercHook = 0
	let UsePercHookGetAway = 0
end
let StdShips = {}
let AIProf = {}
def InitStdShips
def MakeAI
def GetIAProf
def ModAIProff
def SetIAProf
def SetAIProfile
def SetStdProperties
let UseMercs = 1
let Mercs = {
	'SMERC2': 'MercFriends.MercenaryA_Smartie',
	'SMERC3': 'MercFriends.MercenaryC_Brutus',
	'SMERC1': 'MercFriends.MercenaryB_Dumber'
}
def MercOnDeath
def InitMercs
def Init
