# The module code of Mission_GoWithHuman in level press (data01.packed/levels/press/scripts/mission_gowithhuman.pyc): its imports, variables, functions and load time calls in the order of the
# script; ScrapC makes the module from this file instead of running the .pyc. Made by tools/modules/pyc2mod.py.
# A pack in mods/ with scrapc/levels/press/mission_gowithhuman.mod replaces this file.

import Scrap
import SAI
import SVec
import SAct
import Chars
import CharsNPC
import SaveGame
import Speech
import DTritusDesktop
import MissionsFuncs
import CharConversor
import Menu
import CharsNPC
import Elevator
import Scene
import MakeChar
set Menu.HeadBoss = 1
let libname = 'Mission_GoWithHuman'
let maxHumanDist = 1000
let minDistFromSwitch = 500
let switch = None
let human = None
call Scrap.CreateSaveVar('Press.MentalControllerStatus', '0')
let FXSceneHumanCDFile = 'Models/GFX/Scenes/HumanCD.M3D'
def InitStatus
def CreateHuman
def Init
def OnPossession
def OnExitConversor
def CheckMaxDist
def CheckHumanDist
def LaunchScene
def PlayHumanCD2
def LoadNextMission
def LoadScene
def LoadScene2
def CreateFakeSpeech
def EyeCamera
def NearCamera
def NormCamera
def SetTalk
