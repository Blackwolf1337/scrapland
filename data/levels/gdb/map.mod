# The module code of Map in level gdb (data03.packed/levels/gdb/scripts/map.pyc): its imports, variables, functions and load time calls in the order of the
# script; ScrapC makes the module from this file instead of running the .pyc. Made by tools/modules/pyc2mod.py.
# A pack in mods/ with scrapc/levels/gdb/map.mod replaces this file.

import Scrap
import SInput
import SNet
import SWeap
import SScorer
import SLogic
import string
let Libraries = []
if (string.upper(Scrap.GetSaveVar('Mission.Map')) == 'GDB')
	call Libraries.append('MapMatrix')
end
call Scrap.CreateSaveVar('GDB.BishopsMsg', '3')
let CreateBishops = 1
def InitStatus
def Init
def InitPoliceBishops
def EndOfAppears
def GiveLife
def NextWave
