# The module code of CrazyWing (Data.packed/scripts/missions/crazywing.pyc): its imports, variables, functions and load time calls in the order of the
# script; ScrapC makes the module from this file instead of running the .pyc. Made by tools/modules/pyc2mod.py.
# A pack in mods/ with scrapc/modules/crazywing.mod replaces this file.

import Scrap
import SInput
import SNet
import SWeap
import SScorer
import SSound
import SAct
import SVec
import sys
call Scrap.CreateSaveVar('CrazyWing.List', '[]')
call Scrap.CreateSaveVar('CrazyWing.Status', '1')
let CrazyChars = [
	'Desktop',
	'Sentinel',
	'Berto',
	'Police',
	'Nurse',
	'Gear',
	'BankDirector',
	'Mayor',
	'Functionary',
	'Betty',
	'Bishop',
	'PoliceBoss',
	'CrazyGambler',
	'BankMaster'
]
let CrazyWingLoTime = 60
let CrazyWingHiTime = 180
let CrazyWingTime = 10
let CrazyWingList = eval(Scrap.GetSaveVar('CrazyWing.List'))
let FirstPrize = 2000
let IncPrize = 1000
let SecondPrize = 500
let BigPrize = 100000
def InitCrazyWing
def CrazyWing
def SetCrazy
def DeCrazy
def RestoreSpeech
def AllInList
def CrazyPhoto
