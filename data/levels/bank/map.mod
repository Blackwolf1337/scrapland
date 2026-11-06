# The module code of Map in level bank (Data.packed/levels/bank/scripts/map.pyc): its imports, variables, functions and load time calls in the order of the
# script; ScrapC makes the module from this file instead of running the .pyc. Made by tools/modules/pyc2mod.py.
# A pack in mods/ with scrapc/levels/bank/map.mod replaces this file.

import Scrap
import SInput
import SNet
import SWeap
import SScorer
import SVec
import SAct
import SSound
let Libraries = ['MapTreasure']
call Scrap.Set('BankDirectorAgressive', 1)
call Scrap.CreateSaveVar('BankMasterAtBank', '1')
let _BankDirector = 1
let _Bishop = 1
let _Functionary = 1
let _Messenger = 1
def InitStatus
def Init
