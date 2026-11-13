# The module code of Mission_GoToOutskirts in level outskirts (data01.packed/levels/outskirts/scripts/mission_gotooutskirts.pyc): its imports, variables, functions and load time calls in the order of the
# script; ScrapC makes the module from this file instead of running the .pyc. Made by tools/modules/pyc2mod.py.
# A pack in mods/ with scrapc/levels/outskirts/mission_gotooutskirts.mod replaces this file.

import Scrap
import SaveGame
import MissionsFuncs
import Chars
import Speech
import Elevator
call Scrap.CreateSaveVar('PlayerHasBeenKilled', '0')
let messageInterval = 60
let numCrazyGamblerComs = 0
def InitStatus
def Init
def LaunchCrazyGamblerComunication
def PlayCrazyGamblerComunication
def OnMainCharDeath
def UseElevator
