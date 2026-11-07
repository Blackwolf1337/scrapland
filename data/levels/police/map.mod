# The module code of Map in level police (data01.packed/levels/police/scripts/map.pyc): its imports, variables, functions and load time calls in the order of the
# script; ScrapC makes the module from this file instead of running the .pyc. Made by tools/modules/pyc2mod.py.
# A pack in mods/ with scrapc/levels/police/map.mod replaces this file.

import Scrap
import SInput
import SNet
import SWeap
import SScorer
import SVec
import SAct
import SSound
call Scrap.CreateSaveVar('PoliceBossAtPolice', '1')
call Scrap.Set('PoliceLogicBelligerant', 50)
call Scrap.CreateSaveVar('CostumeAtPolice', '0')
let Libraries = ['MapPlanes']
if (int(Scrap.GetSaveVar('Player.NumLives')) < 0)
	call Scrap.SetSaveVar('Player.NumLives', '0')
	if (Scrap.GetSaveVar('Mission.Library') != 'Mission_JailBreak')
		call Scrap.SetSaveVar('Mission_JailBreak.Map', Scrap.GetSaveVar('Mission.Map'))
		call Scrap.SetSaveVar('Mission_JailBreak.Library', Scrap.GetSaveVar('Mission.Library'))
		call Scrap.SetSaveVar('Mission.Map', 'Police')
		call Scrap.SetSaveVar('Mission.Library', 'Mission_JailBreak')
	end
end
let ficus = None
call Scrap.CreateSaveVar('Police.FicusDeath', '0')
def CreateFicusDeadClass
def CreateFicus
let _BankDirector = 1
let _Bishop = 1
let _Messenger = 1
def InitStatus
def Init
