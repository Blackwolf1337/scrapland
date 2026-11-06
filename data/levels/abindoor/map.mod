# The module code of Map in level abindoor (data03.packed/levels/abindoor/scripts/map.pyc): its imports, variables, functions and load time calls in the order of the
# script; ScrapC makes the module from this file instead of running the .pyc. Made by tools/modules/pyc2mod.py.
# A pack in mods/ with scrapc/levels/abindoor/map.mod replaces this file.

import Scrap
import SAI
import SInput
import SNet
import SWeap
import SScorer
import SSound
import SVec
import SAct
import sys
import Chars
import CharsNPC
import CharAct
import MissionsFuncs
import Speech
import CharConversor
import Items
import string
import SaveGame
import Init
import Scorer
import Scene
import MakeChar
import Elevator
let Libraries = []
call Scrap.Set('EnergyBarActive', 0)
call Scrap.Set('SpecialActionActive', 0)
call Scrap.SetAlarm(0)
call Scrap.SetLockAlarm(1)
call Scrap.CreateSaveVar('ABIndoor.Scene', '0')
let FXSceneTheEndBFile = 'Models/GFX/Scenes/TheEndB.M3D'
def InitStatus
def CreateHuman
def CreateBerto
def CreateBetty
def CreateCrazyGambler
def OnHumanSpeech
def OnBettySpeech
def OnBertoSpeech
def OnCrazyGamblerSpeech
def Init
def PlaySceneA
def Goodbye
def LaunchCinematic
def ShowEndTitle
def FadeEndTitle
def Credits
def LoadSceneA
def LoadSceneB
