# The module code of Vehicles (Data.packed/scripts/vehicles/vehicles.pyc): its imports, variables, functions and load time calls in the order of the
# script; ScrapC makes the module from this file instead of running the .pyc. Made by tools/modules/pyc2mod.py.
# A pack in mods/ with scrapc/modules/vehicles.mod replaces this file.

import Scrap
import SInput
import SNet
import SWeap
import SScorer
import SSound
import SVec
import SAI
import SFX
call Scrap.Set('MotorDownFactor', 0.25)
call Scrap.Set('VehicleCrashAccLo', (6000 * 28))
call Scrap.Set('VehicleCrashAccMed', (12000 * 28))
call Scrap.Set('VehicleCrashAccHi', (18000 * 28))
call Scrap.Set('VehicleCrashAccHev', (24000 * 28))
let MotorExplosionSoundVol = 0.4
let MotorExplosionSoundAttIni = 0
let MotorExplosionSoundAttEnd = 25000
let ShipExplosionSoundVol = 0.6
let ShipExplosionSoundAttIni = 0
let ShipExplosionSoundAttEnd = 27000
let ShipExplosionSoundAttIniRadiusFactor = 0.0
let ShipExplosionSoundAttEndRadiusFactor = 25.0
let ShipExplosionForceInplace = 0
let ShipExplosionScaleFactor = 1.0
let MotorExplosionScaleFactor = 1.0
let LastClientCreate = None
def ClientCreate
let LastClientAfterCreate = None
def ClientAfterCreate
def ServerMotorHit
def MotorHit
def CreateDumbMotor
def SetUpDumbMotor
def DumbShipHit
let HullLifeTimeDeath = (0.6, 1.2)
def OnClientDeath
def ActivateHitType
def GetCoin
def GetRandomMercenaryHead
let OnDeathCallback = None
let ForceDeathTaunt = 0
let DelPlayerShipOnDeath = 1
def OnDeath
def SwitchView
def SetActiveCam
def ReSpawnShip
def SetProperties
def SetStdProperties
def Init
