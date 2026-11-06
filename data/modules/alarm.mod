# The module code of Alarm (Data.packed/scripts/missions/alarm.pyc): its imports, variables, functions and load time calls in the order of the
# script; ScrapC makes the module from this file instead of running the .pyc. Made by tools/modules/pyc2mod.py.
# A pack in mods/ with scrapc/modules/alarm.mod replaces this file.

import Scrap
import SInput
import SNet
import SWeap
import SScorer
import SSound
import SVec
import SAct
import sys
let SetAlarmCallback = []
let AbortMissionCallback = None
let RestoreMissionCallback = None
let TempAbortMissionCallback = None
let TempRestoreMissionCallback = None
def SetAlarm
def ActivateAlarm
def DeactivateAlarm
def Init
call Scrap.Set('AlarmCallback', 'Alarm.SetAlarm')
