# The module code of Chars (Data.packed/scripts/chars/chars.pyc): its imports, variables, functions and load time calls in the order of the
# script; ScrapC makes the module from this file instead of running the .pyc. Made by tools/modules/pyc2mod.py.
# A pack in mods/ with scrapc/modules/chars.mod replaces this file.

import Scrap
import SInput
import SNet
import SWeap
import SScorer
import SSound
import SVec
import SAct
import SLogic
import sys
call Scrap.Set('DefInterpTime', 0.12)
call Scrap.Set('DefActionRadius', 2500)
let CharRes = []
let CanDriveList = [
	'Berto',
	'Police',
	'BankDirector',
	'Mayor',
	'Dtritus',
	'Betty',
	'Bishop',
	'Functionary',
	'PoliceBoss',
	'BankMaster',
	'CrazyGambler',
	'MercenaryA',
	'MercenaryB',
	'MercenaryC',
	'MercenaryL'
]
def CanDrive
let OnCreateMainChar = None
def CreateMainChar
def SetOnFloor
def IntSetChar
def ActSound
def StrSound
def StrSoundStop
def ActSoundLoop
def DiscardRes
def TryToDiscardThis
def LoadRes
def AddNewCharRes
def LocalLoadRes
let OnMainCharDeath = None
def MainCharDeath
call Scrap.CreateEntityList('Players')
def CreatePlayersList
def Init
def CreateStartupPlayer
