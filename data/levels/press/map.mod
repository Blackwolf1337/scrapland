# The module code of Map in level press (data01.packed/levels/press/scripts/map.pyc): its imports, variables, functions and load time calls in the order of the
# script; ScrapC makes the module from this file instead of running the .pyc. Made by tools/modules/pyc2mod.py.
# A pack in mods/ with scrapc/levels/press/map.mod replaces this file.

import Scrap
import SInput
import SNet
import SWeap
import SScorer
import SVec
import SAct
import SSound
let Libraries = ['MapPlanes']
if (Scrap.Get('isDemo') and (Scrap.GetSaveVar('Mission.Map') != 'Press'))
	call Libraries.append('MapDemoClose')
end
call Scrap.CreateSaveVar('MapPress_HiAlarm', '1')
call Scrap.CreateSaveVar('MapPress_BureauExists', '1')
let AlarmState = 0
def RemoteMsg
def ForbiddenMsg
def StartAlarmator
def ActivateAlarm
def CreateActivators
def CreateDeactivators
def DeactivateAlarm
def BrowseAlarmLaunchers
let _BankDirector = 1
let _Bishop = 1
let _Functionary = 1
let _Messenger = 1
def InitStatus
def Init
def HideBureau
