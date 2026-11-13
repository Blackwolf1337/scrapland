# The module code of Split (Data.packed/scripts/split/split.pyc): its imports, variables, functions and load time calls in the order of the
# script; ScrapC makes the module from this file instead of running the .pyc. Made by tools/modules/pyc2mod.py.
# A pack in mods/ with scrapc/modules/split.mod replaces this file.

import Scrap
import SInput
import SNet
import SWeap
import SScorer
import SSound
import SAct
import sys
import SAI
import SFX
if Scrap.Get('isDemo')
	let SplitMaps = ['FZ', 'GDB']
	let ModesMap = {'DeathMatch': ['FZ', 'GDB'], 'FlagHunt': ['FZ', 'GDB'], 'Race': ['FZ', 'GDB']}
else
	let SplitMaps = ['QF', 'FZ', 'OM', 'CA', 'GDB', 'MCA', 'MCB', 'MDA', 'MDB', 'MOA', 'MOB', 'MSA', 'MSB']
	let ModesMap = {
		'DeathMatch': ['QF', 'FZ', 'OM', 'CA', 'GDB', 'MCA', 'MCB', 'MDA', 'MDB', 'MOA', 'MOB', 'MSA', 'MSB'],
		'FlagHunt': ['QF', 'FZ', 'OM', 'CA', 'GDB', 'MCA', 'MCB', 'MDA', 'MDB', 'MOA', 'MOB', 'MSA', 'MSB'],
		'Postman': ['FZ', 'OM'],
		'Race': ['FZ', 'CA', 'GDB', 'MCA', 'MCB', 'MDA', 'MDB', 'MOA', 'MOB', 'MSA', 'MSB']
	}
end
let Modes = ModesMap.keys()
call Modes.sort()
let MortalProfiles = [1, 1]
let PlayersReady = [0, 0]
def PurgeServerMapList
def GetSpawnPos
let FuncGetSpawnPos = GetSpawnPos
def CreateUserShip
def Init
let RespawnTime = 5
def GetReSpawnPos
let FuncGetReSpawnPos = GetReSpawnPos
def ReSpawnShip
let OnDeathFunc = None
def OnDeathCallback
def ChooseShips
def ChangeShip
def NextShip
def PrevShip
let FirstTime = 1
def StartGame
def DefaultSplitScorer
let Scorer = [0, 0]
def EndOfGame
def PurgeServerMapList
def ReloadNext
def ActualizeScorer
def MissionScorer
def SeleShip
def InitChooseScorer
def ReadyToStartScorer
def ConfirmAbortMain
def ConfirmAbortPause
def ConfirmRestartPause
def ConfirmAbort
def Abort
def Restart
def PauseGame
def PauseGameMenu
def LittleText
def BlackBack
