# The module code of EMI (Data.packed/scripts/weapons/emi.pyc): its imports, variables, functions and load time calls in the order of the
# script; ScrapC makes the module from this file instead of running the .pyc. Made by tools/modules/pyc2mod.py.
# A pack in mods/ with scrapc/modules/emi.mod replaces this file.

import SWeap
import Scrap
import SNet
import math
let ModelFile = 'Models/Weapons/EMI/EMI.M3D'
let NetResName = 'EMI'
let IDAmmo = '00'
let AmmoNeed = 5
let RechargeTime = 1.5
let LifeTime = 15.0
let LowRadiusFactor = 0.4
let Radius = (600 / LowRadiusFactor)
let LastClientCreate = None
def ClientCreate
let LastClientAfterCreate = None
def ClientAfterCreate
def ActivateHitType
def CreateEMI
def SetUpEMI
def LoadResources
def Init
