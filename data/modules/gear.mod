# The module code of Gear (Data.packed/scripts/chars/gear.pyc): its imports, variables, functions and load time calls in the order of the
# script; ScrapC makes the module from this file instead of running the .pyc. Made by tools/modules/pyc2mod.py.
# A pack in mods/ with scrapc/modules/gear.mod replaces this file.

import Scrap
import SInput
import SNet
import SWeap
import SScorer
import SSound
import SVec
import SAct
import sys
let MissileFile = 'Models/GFX/Gear/Missile.M3D'
call Scrap.Set('GearShootDamage', 13)
call Scrap.Set('GearShootDistance', 20000)
call Scrap.Set('GearShootFailAlarm', 0.1)
call Scrap.Set('GearShootHitAlarm', 0.15)
call Scrap.Set('GearShootPosX', (-50.0))
call Scrap.Set('GearShootPosY', (-45.0))
call Scrap.Set('GearShootPosZ', (-180.0))
def DropMissileEvent
def StartSirenLoop
def LoadSirenActs
def Init
def OnCreate
def OnFreeResources
def AbortSiren
