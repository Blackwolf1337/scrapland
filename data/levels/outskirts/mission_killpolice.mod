# The module code of Mission_KillPolice in level outskirts (data01.packed/levels/outskirts/scripts/mission_killpolice.pyc): its imports, variables, functions and load time calls in the order of the
# script; ScrapC makes the module from this file instead of running the .pyc. Made by tools/modules/pyc2mod.py.
# A pack in mods/ with scrapc/levels/outskirts/mission_killpolice.mod replaces this file.

import Scrap
import SScorer
import MissionsFuncs
import SaveGame
import Parking
import Speech
call Scrap.CreateSaveVar('MissionState', 'KillPolice')
call Scrap.CreateSaveVar('PlayerHasBeenKilled', '0')
let policesList = []
let policesKilled = 0
let alarmFallDelta = None
def InitStatus
def Init
def SetPos
def InitMission
def OnAlarm
def OnMainCharDeath
def ShowMessage2
def OnPoliceDeath
def BertoAppears
def BertoAppears2
def EnableMetro
def OnMetroUsed
