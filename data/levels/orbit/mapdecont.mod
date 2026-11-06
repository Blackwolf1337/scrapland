# The module code of MapDecont in level orbit (data01.packed/levels/orbit/scripts/mapdecont.pyc): its imports, variables, functions and load time calls in the order of the
# script; ScrapC makes the module from this file instead of running the .pyc. Made by tools/modules/pyc2mod.py.
# A pack in mods/ with scrapc/levels/orbit/mapdecont.mod replaces this file.

import Scrap
import SInput
import SNet
import SWeap
import SScorer
import SVec
import SAct
import SLogic
let FXSceneDecontaminationFile = 'Models/GFX/Scenes/Decontamination.M3D'
let FXSceneBishopsFile = 'Models/GFX/Scenes/Bishops.M3D'
call Scrap.CreateSaveVar('Orbit.Decontaminated', 'no')
def InitStatus
def Decontamination
def EndOfScene
def DecontaminatedWaypoint
def SetWaypoint
def LoadDecontamination
def Init
def MsgOffVoice
def SetFirstInfoPointAsMission
def CantUseElevator
def CloseGateKeeperDoor
def OpenGateKeeperDoor
def AfterBrowseNPC
def CreateFunctionaryInit
def CreateBishopInit
def CreateGateKeeperInit
def CreateSentinelInit
def ShowSkipConvsMsg
def GateKeeperSpeech
def SentinelSpeech
def GaugeBlink
def CheckShowNextHowto
def CreateBishopsLauncher
def EndOfSceneBishops
def LoadBishopsScene
def FXBishopsPray
def PlayBishops
def LoadFunctionaryScene
def AddDF
def SetDFAnm
def EndOfSceneFunctionary
def CreateDownKeyPad
def PlayFalling
def JumpNextMission
def PlayFunctionary
def LoadFallingScene
