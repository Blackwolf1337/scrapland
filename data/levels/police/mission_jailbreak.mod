# The module code of Mission_JailBreak in level police (data01.packed/levels/police/scripts/mission_jailbreak.pyc): its imports, variables, functions and load time calls in the order of the
# script; ScrapC makes the module from this file instead of running the .pyc. Made by tools/modules/pyc2mod.py.
# A pack in mods/ with scrapc/levels/police/mission_jailbreak.mod replaces this file.

import Scrap
import SInput
import SNet
import SWeap
import SScorer
import SVec
import SAct
import CharsNPC
import SaveGame
import CharConversor
import Menu
import Speech
if Scrap.Get('IsXbox')
	let PuffTurn = (-0.19)
	let PuffWalk = 0.25
else
	let PuffTurn = (-0.175)
	let PuffWalk = 1
end
def InitStatus
def AfterBrowseNPC
def CreateMCList
def CreateGearStay
def CreatePuff
def InitPuffReborn
def StaplerSpeech
let ActPress = 0
def CheckForJump
def OnPossession
def FirstTimeJail
def Init
def NextMission
def LoadPolice
