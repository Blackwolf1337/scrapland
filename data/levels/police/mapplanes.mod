# The module code of MapPlanes in level police (data01.packed/levels/police/scripts/mapplanes.pyc): its imports, variables, functions and load time calls in the order of the
# script; ScrapC makes the module from this file instead of running the .pyc. Made by tools/modules/pyc2mod.py.
# A pack in mods/ with scrapc/levels/police/mapplanes.mod replaces this file.

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
call Scrap.CreateSaveVar('PoliceBlueprints.Ships', '["SPoli2","SPoli3","SPoli4","SPoli5","SPoliBoss1"]')
call Scrap.CreateSaveVar('PoliceBlueprints.Engines', '["MPoli2","MPoli3","MPoli4","MPoli5","MPoliBoss1"]')
let Naves = eval(Scrap.GetSaveVar('PoliceBlueprints.Ships'))
let Motores = eval(Scrap.GetSaveVar('PoliceBlueprints.Engines'))
def InitStatus
def Init
def TakenPhoto
