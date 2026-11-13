# The module code of CharConversor (Data.packed/scripts/missions/charconversor.pyc): its imports, variables, functions and load time calls in the order of the
# script; ScrapC makes the module from this file instead of running the .pyc. Made by tools/modules/pyc2mod.py.
# A pack in mods/ with scrapc/modules/charconversor.mod replaces this file.

import Scrap
import SInput
import SNet
import SWeap
import SScorer
import SVec
import SAct
import SLogic
import SFX
let _usrName = ''
let _charName = ''
let _optId = ''
let _fullOption = ''
let ConversionChars = (
	'Dtritus',
	'Police',
	'Nurse',
	'BankDirector',
	'Desktop',
	'Sentinel',
	'Mayor',
	'Gear',
	'Bishop',
	'Messenger',
	'Functionary',
	'Betty',
	'Berto',
	'PoliceBoss',
	'BankMaster'
)
call Scrap.CreateSaveVar('Conversor.ActiveConversors', '1')
call Scrap.CreateSaveVar('Conversor.FirstConversion', '1')
call Scrap.CreateSaveVar('Conversor.FirstPossession', '1')
let AvailableChars = eval(Scrap.GetSaveVar('Conversor.AvailableChars'))
let ConversionPoints = []
let prevChar = None
let newChar = None
let PrevConv = None
def GetNearestConversionPoint
def CreateConversorClass
def Init
def ConvertChar
def CPPos
def CPAng
def DrawBackground
def AddInterfaceContent
def SpawnChar
def MarkSpawnWaypoint
let CheckConversor = None
def EnterCharConversor
let afterExitConversor = None
def InitOfScene
def EndOfScene
def InitInterfaceEvent
def ExitCharConversor
def ShowCharDesc
def HideCharDesc
def Convert
def ConvertExecute
let OnPossession = None
let PossessionTime = 0.2
def Possession
def NewPlayer
