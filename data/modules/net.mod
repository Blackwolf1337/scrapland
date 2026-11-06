# The module code of Net (Data.packed/scripts/net/net.pyc): its imports, variables, functions and load time calls in the order of the
# script; ScrapC makes the module from this file instead of running the .pyc. Made by tools/modules/pyc2mod.py.
# A pack in mods/ with scrapc/modules/net.mod replaces this file.

import Scrap
import SNet
import SScorer
import SInput
let TeamGame = 0
if Scrap.Get('isDemo')
	let NetMaps = ['FZ', 'GDB']
	let ModesMap = {
		'DeathMatch': ['FZ', 'GDB'],
		'TeamDM': ['FZ', 'GDB'],
		'FlagHunt': ['FZ', 'GDB'],
		'TeamFlag': ['FZ', 'GDB']
	}
else
	let NetMaps = ['QF', 'FZ', 'OM', 'CA', 'GDB', 'MCA', 'MCB', 'MDA', 'MDB', 'MOA', 'MOB', 'MSA', 'MSB']
	let ModesMap = {
		'DeathMatch': ['QF', 'FZ', 'OM', 'CA', 'GDB', 'MCA', 'MCB', 'MDA', 'MDB', 'MOA', 'MOB', 'MSA', 'MSB'],
		'TeamDM': ['QF', 'FZ', 'OM', 'CA', 'GDB', 'MCA', 'MCB', 'MDA', 'MDB', 'MOA', 'MOB', 'MSA', 'MSB'],
		'FlagHunt': ['QF', 'FZ', 'OM', 'CA', 'GDB', 'MCA', 'MCB', 'MDA', 'MDB', 'MOA', 'MOB', 'MSA', 'MSB'],
		'TeamFlag': ['QF', 'FZ', 'OM', 'CA', 'GDB', 'MCA', 'MCB', 'MDA', 'MDB', 'MOA', 'MOB', 'MSA', 'MSB'],
		'Postman': ['FZ', 'OM'],
		'TeamPost': ['FZ', 'OM']
	}
end
let Modes = ModesMap.keys()
call Modes.sort()
def PurgeServerMapList
def Spectate
def ClientCreateDef
let ClientCreate = ClientCreateDef
def ClientAfterCreateDef
let ClientAfterCreate = ClientAfterCreateDef
def ClientJoinDef
let ClientJoin = ClientJoinDef
def LoadSinglePlayer
