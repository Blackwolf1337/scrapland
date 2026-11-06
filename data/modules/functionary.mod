# The module code of Functionary (Data.packed/scripts/chars/functionary.pyc): its imports, variables, functions and load time calls in the order of the
# script; ScrapC makes the module from this file instead of running the .pyc. Made by tools/modules/pyc2mod.py.
# A pack in mods/ with scrapc/modules/functionary.mod replaces this file.

import Scrap
import SInput
import SNet
import SWeap
import SScorer
import SSound
import SVec
import SAct
import sys
let FXFunctionarySpecialActionFile = 'Models/GFX/Functionary/SpecialAction.M3D'
let MinFov = 90
def Init
def OnCreate
def OnFreeResources
let charSpeed = 4
let worldSpeed = 0.175
let speedFactor = (charSpeed * worldSpeed)
def EnterFastEffect
def ExitFastEffect
def ActionPress
def ActionRelease
