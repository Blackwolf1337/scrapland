# The module code of MapPlanes in level press (data01.packed/levels/press/scripts/mapplanes.pyc): its imports, variables, functions and load time calls in the order of the
# script; ScrapC makes the module from this file instead of running the .pyc. Made by tools/modules/pyc2mod.py.
# A pack in mods/ with scrapc/levels/press/mapplanes.mod replaces this file.

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
call Scrap.CreateSaveVar('PressBlueprints.Ships', '["SBerto1","SBetty1","SHump1"]')
call Scrap.CreateSaveVar('PressBlueprints.Engines', '["MBerto1","MBetty1","MHump1"]')
let Naves = eval(Scrap.GetSaveVar('PressBlueprints.Ships'))
let Motores = eval(Scrap.GetSaveVar('PressBlueprints.Engines'))
def InitStatus
def Init
def TakenPhoto
