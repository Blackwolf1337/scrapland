# The module code of SaveGame (Data.packed/scripts/savegame.pyc): its imports, variables, functions and load time calls in the order of the
# script; ScrapC makes the module from this file instead of running the .pyc. Made by tools/modules/pyc2mod.py.
# A pack in mods/ with scrapc/modules/savegame.mod replaces this file.

import Scrap
import SInput
import SNet
import SWeap
import SScorer
import SSound
import SAct
import sys
call Scrap.CreateSaveVar('Map', 'Levels/Orbit')
call Scrap.CreateSaveVar('PrevMap', 'Levels/Orbit')
call Scrap.CreateSaveVar('Spawn', 'DM_Player_Spawn_Beginning')
call Scrap.CreateSaveVar('Char', 'Dtritus')
call Scrap.CreateSaveVar('ComeFrom', '')
call Scrap.CreateSaveVar('JackInTheBox.Active', '1')
call Scrap.CreateSaveVar('AlarmActive', '0')
call Scrap.CreateSaveVar('AlarmStatus', '0.0')
call Scrap.CreateSaveVar('Money', '0')
call Scrap.CreateSaveVar('Challenge', '1')
call Scrap.CreateSaveVar('Challenge.Type', '')
call Scrap.CreateSaveVar('Challenge.Foe', '')
call Scrap.CreateSaveVar('Challenge.NumEnemies', '')
call Scrap.CreateSaveVar('Challenge.Money', '')
call Scrap.CreateSaveVar('Revenge.Type', '')
call Scrap.CreateSaveVar('Revenge.Foe', '')
call Scrap.CreateSaveVar('Revenge.NumEnemies', '')
call Scrap.CreateSaveVar('Revenge.LastWinner', '')
call Scrap.CreateSaveVar('Mission.Map', 'Orbit')
call Scrap.CreateSaveVar('Mission.Library', 'MapDecont')
call Scrap.CreateSaveVar('BadReputation', '0')
call Scrap.CreateSaveVar('GameplayTime', '0')
call Scrap.CreateSaveVar('GameAct', 'Prologue')
call Scrap.CreateSaveVar('BankDebt', '0')
call Scrap.CreateSaveVar('LonelyMercInGamblinDen', '0')
call Scrap.CreateSaveVar('LonelyMercLairActive', '0')
call Scrap.CreateSaveVar('LonelyMercDataActive', '0')
call Scrap.CreateSaveVar('ComSatsMissionsMapsFinished', '[]')
call Scrap.CreateSaveVar('Conversor.AvailableChars', "['Dtritus','Police','Nurse','BankDirector','Desktop','Sentinel','Mayor','Gear','Bishop','Messenger','Functionary','Betty','Berto','PoliceBoss','BankMaster']")
call Scrap.CreateSaveVar('Batteries', '5')
call Scrap.CreateSaveVar('AcBatteries', '2')
call Scrap.CreateSaveVar('PrimaryMissionDesc', '')
call Scrap.Set('PrimaryMissionDesc', Scrap.GetSaveVar('PrimaryMissionDesc'))
call Scrap.CreateSaveVar('SecondaryMissionDesc', '')
call Scrap.Set('SecondaryMissionDesc', Scrap.GetSaveVar('SecondaryMissionDesc'))
call Scrap.CreateSaveVar('EnergyBarActive', '1')
call Scrap.Set('EnergyBarActive', int(Scrap.GetSaveVar('EnergyBarActive')))
call Scrap.CreateSaveVar('SpecialActionActive', '2')
call Scrap.Set('SpecialActionActive', int(Scrap.GetSaveVar('SpecialActionActive')))
call Scrap.CreateSaveVar('TakePhotoMsg', '1')
if int(Scrap.GetSaveVar('TakePhotoMsg'))
	call Scrap.Set('FirstPhotoMessage', (Scrap.GetLangStr('TakePhotoMsg') % SInput.GetDefinedEntry(0, 'Walk', 'Use', 'Mouse')))
else
	call Scrap.Set('FirstPhotoMessage', '')
end
def MMDesc
def SMDesc
def SaveGame
def InstantSaveGame
def LoadGame
def NewGame
def CheckAllBlueprints
def MarkWayPoint
def QWayPoint
def SaveAlarm
def LoadAlarm
def SaveMoney
def LoadMoney
def QLoad
def StartGame
def Restart
def QSaveReg
def SaveReg
def GetReg
def InitVars
