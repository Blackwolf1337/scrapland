# The module code of Sonic (Data.packed/scripts/weapons/sonic.pyc): its imports, variables, functions and load time calls in the order of the
# script; ScrapC makes the module from this file instead of running the .pyc. Made by tools/modules/pyc2mod.py.
# A pack in mods/ with scrapc/modules/sonic.mod replaces this file.

import SWeap
import SSound
import Scrap
import SNet
import SVec
import math
let MineFile = 'Models/Weapons/Sonic/Sonic.M3D'
let SonicXpldeFile = 'Models/Weapons/Sonic/Xplde.M3D'
let NetResName = 'Sonic'
let SwarmPow = 80000
let XP_Radius = (15000 / 2)
let FXSonicRadius = 15000
let LifeTime = 5.0
let LastClientCreate = None
def ClientCreate
let LastClientAfterCreate = None
def ClientAfterCreate
def SonicHit
def OnHit
def OnServerHit
def OnDeath
def SetUpToDetonate
def CreateSonic
def SetUpSonic
def LoadResources
def Init
