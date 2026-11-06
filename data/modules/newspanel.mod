# The module code of NewsPanel (Data.packed/scripts/missions/newspanel.pyc): its imports, variables, functions and load time calls in the order of the
# script; ScrapC makes the module from this file instead of running the .pyc. Made by tools/modules/pyc2mod.py.
# A pack in mods/ with scrapc/modules/newspanel.mod replaces this file.

import Scrap
import SInput
import SNet
import SWeap
import SScorer
import SVec
import SAct
import SLogic
import SFX
let NewsPanelModel = 'Models/Misc/NewsPanel/NewsPanel-'
let Panels = []
let ShowingNews = 0
c Settings
let MissionUse = 0
call Scrap.CreateSaveVar('WindowsError', '0')
let act = Scrap.GetSaveVar('GameAct')
let NewsOnMem = (act in ('Prologue', '3rdMurder', '2ndMurder', '1stMurder'))
if (Scrap.GetSaveVar('WindowsError') == '1')
	call Scrap.Set('NewsLogoBmp', '2D/News/WinError.bmp')
end
def GetNews
def FillNews
def Init
def UseNewsPanel
def EndOfNews
def SwitchAlarm
