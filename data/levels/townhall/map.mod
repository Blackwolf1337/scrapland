# The module code of Map in level townhall (data02.packed/levels/townhall/scripts/map.pyc): its imports, variables, functions and load time calls in the order of the
# script; ScrapC makes the module from this file instead of running the .pyc. Made by tools/modules/pyc2mod.py.
# A pack in mods/ with scrapc/levels/townhall/map.mod replaces this file.

import Scrap
import SInput
import SNet
import SWeap
import SScorer
import SVec
import SAct
import SSound
let Libraries = []
call Scrap.CreateSaveVar('MayorAtTownHall', '1')
call Scrap.CreateSaveVar('PoliceBossAtTownHall', '0')
let _BankDirector = 1
let _Bishop = 1
let _Functionary = 1
def InitStatus
def CreateGearMayor
def Init
def OrbitLockMsg
