# The module code of CharsNPC (Data.packed/scripts/chars/charsnpc.pyc): its imports, variables, functions and load time calls in the order of the
# script; ScrapC makes the module from this file instead of running the .pyc. Made by tools/modules/pyc2mod.py.
# A pack in mods/ with scrapc/modules/charsnpc.mod replaces this file.

import Scrap
import SInput
import SNet
import SWeap
import SScorer
import SSound
import SVec
import SAct
import SLogic
import SAI
import sys
let CharRes = {}
class CharSpawnPos
	let Pos = (0, 0, 0)
	let Ang = (0, 0, 0)
	let Name = 'Character NPC'
	let ActCtrl = 'Police'
	let Entity = None
	let ExtraInfo = None
	let afterInitSpeechFunc = None
	let repos = 0
	c SpawnPosInit
end
c SpawnPosClass
let AfterBrowseNPC = []
call Scrap.Set('GenCharStdFar', 800)
def AddNPCPos
def CreateMissionChar
def DeleteMissionChar
let ReposChars = {}
def SetReposChars
def CreateReposChars
def BrowseNPC
let NPCAttribs = {
	'BankDirector': (1500, 30, 70),
	'BankMaster': (3000, 60, 90),
	'Berto': (240, 24, 70),
	'Betty': (450, 39, 70),
	'Bishop': (900, 21, 75),
	'Boss': (0, 3, 100),
	'Desktop': (60, 18, 20),
	'Functionary': (600, 45, 55),
	'Gear': (750, 60, 95),
	'Humphrey': (450, 39, 70),
	'Mayor': (1800, 36, 75),
	'Messenger': (1800, 900, 45),
	'Nurse': (180, 15, 55),
	'Police': (750, 60, 60),
	'PoliceBoss': (1500, 60, 80),
	'Sentinel': (150, 30, 45)
}
def AssignAttrib
def CallCreateChar
def CreateChar
def CharNPCInclude
let RespawnChars = ['Police', 'Gear', 'Sentinel', 'Nurse']
def Respawn
def ActiveSpawned
let Repos = 2
def Init
let optionsList = []
let subOptionsList = []
let ActualEntityName = ''
def CheckActualEntity
def MercsHired
def IncludeSubOptions
def Classify
def ClassifyOptions
def PlayOption
def GenericNPCSpeech
