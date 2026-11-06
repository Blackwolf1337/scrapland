# The module code of Map in level gamblinden (data03.packed/levels/gamblinden/scripts/map.pyc): its imports, variables, functions and load time calls in the order of the
# script; ScrapC makes the module from this file instead of running the .pyc. Made by tools/modules/pyc2mod.py.
# A pack in mods/ with scrapc/levels/gamblinden/map.mod replaces this file.

import Scrap
import SInput
import SNet
import SWeap
import SScorer
import SVec
import SAct
import SSound
call Scrap.CreateSaveVar('MayorAtGambinDen', '1')
call Scrap.CreateSaveVar('MayorAtTownHall', '1')
call Scrap.CreateSaveVar('PoliceBossAtPolice', '1')
call Scrap.CreateSaveVar('CrazyGamblerAtGambinDen', '1')
call Scrap.CreateSaveVar('FunctionaryTwinsAtGambinDen', '1')
call Scrap.CreateSaveVar('BankersAtGambinDen', '1')
call Scrap.CreateSaveVar('BishopsAtGambinDen', '1')
call Scrap.CreateSaveVar('PolicesAtGambinDen', '1')
call Scrap.Set('EnergyBarActive', 0)
call Scrap.Set('SpecialActionActive', 0)
call Scrap.SetAlarm(0)
call Scrap.SetLockAlarm(1)
if Scrap.Get('isDemo')
	call Scrap.SetSaveVar('GamblinMusic', '0')
else
	call Scrap.CreateSaveVar('GamblinMusic', '0')
end
let Libraries = []
def InitStatus
def AfterBrowseNPC
def CreateFixedInit
def CreateFixedMercInit
def CreateLonelyMerc
def Init
def LonelyMercSpeech
def CharUsrAct
let Borrachera = 0
let Cheer = []
def CharUsrDance
def MercDrink
def ChatNPCs
def CharDance
def MercenarySpeech
def StartContract
