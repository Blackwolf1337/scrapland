# The module code of Inferno (Data.packed/scripts/weapons/inferno.pyc): its imports, variables, functions and load time calls in the order of the
# script; ScrapC makes the module from this file instead of running the .pyc. Made by tools/modules/pyc2mod.py.
# A pack in mods/ with scrapc/modules/inferno.mod replaces this file.

import math
import Scrap
import SWeap
import SNet
import SVec
import SSound
let IDWeap = '01'
let IDAmmo = '01'
let NetResName = 'Inferno'
let MissileFile = 'Models/Weapons/Inferno/Inferno.M3D'
let ExplosionFile = 'Models/Weapons/Inferno/Explosion.M3D'
let LastClientCreate = None
def ClientCreate
let LastClientAfterCreate = None
def ClientAfterCreate
def LoadResources
def Init
