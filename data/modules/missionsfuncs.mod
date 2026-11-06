# The module code of MissionsFuncs (Data.packed/scripts/missions/missionsfuncs.pyc): its imports, variables, functions and load time calls in the order of the
# script; ScrapC makes the module from this file instead of running the .pyc. Made by tools/modules/pyc2mod.py.
# A pack in mods/ with scrapc/modules/missionsfuncs.mod replaces this file.

import SAI
import Scrap
import SInput
import SScorer
call Scrap.CreateSaveVar('IsSecondMission', '0')
call Scrap.Set('IsSecondMission', int(Scrap.GetSaveVar('IsSecondMission')))
call Scrap.CreateSaveVar('AutoSaveGameOnLoad', '0')
let WorldMap = {
	'JUNKYARD': {'OUTSKIRTS': 'DM_Door_EditShip_00', 'BANK': 'DM_Door_EditShip_00', 'POLICE': 'DM_Door_EditShip_00', 'GAMBLINDEN': 'DM_Door_EditShip_00', 'DOWNTOWN': 'DM_Door_EditShip_00', 'COMMERCIAL': 'DM_Door_EditShip_00', 'SCRAPYARD': 'DM_Door_EditShip_00', 'TOWNHALL': 'DM_Door_EditShip_00', 'ORBIT': 'DM_Door_EditShip_00', 'TEMPLE': 'DM_Door_EditShip_00', 'GDB': 'DM_Door_EditShip_00', 'PRESS': 'DM_Door_EditShip_00'},
	'TOWNHALL': {'DOWNTOWN': 'DM_Door_Elevator_DownTown', 'JUNKYARD': 'DM_Door_Elevator_DownTown', 'ORBIT': 'DM_Door_ElevatorOrbit_Orbit', 'PRESS': 'DM_Door_Metro_Press', 'BANK': 'DM_Door_Metro_Press', 'TEMPLE': 'DM_Door_Metro_Press', 'GDB': 'DM_Door_Metro_Press', 'POLICE': 'DM_Door_Metro_Press', 'GAMBLINDEN': 'DM_Door_Metro_Press', 'OUTSKIRTS': 'DM_Door_Metro_Press', 'SCRAPYARD': 'DM_Door_Metro_Press', 'COMMERCIAL': 'DM_Door_Metro_Press'},
	'DOWNTOWN': {'OUTSKIRTS': 'DM_OutLink_Outskirts', 'SCRAPYARD': 'DM_OutLink_Scrapyard', 'COMMERCIAL': 'DM_OutLink_Commercial', 'JUNKYARD': 'ParkParking_Junkyard', 'BANK': 'DM_Door_MetroExt_Downtown', 'TEMPLE': 'DM_Door_MetroExt_Downtown', 'GDB': 'DM_Door_MetroExt_Downtown', 'POLICE': 'DM_Door_MetroExt_Downtown', 'GAMBLINDEN': 'DM_OutLink_Outskirts', 'ORBIT': 'DM_Door_Elevator_TownHall', 'TOWNHALL': 'DM_Door_Elevator_TownHall', 'PRESS': 'DM_Door_Elevator_Press'},
	'COMMERCIAL': {'JUNKYARD': 'ParkParking_Junkyard', 'TOWNHALL': 'DM_Door_MetroExt_Comercial', 'ORBIT': 'DM_Door_MetroExt_Comercial', 'BANK': 'DM_Door_MetroExt_Comercial', 'POLICE': 'DM_Door_MetroExt_Comercial', 'PRESS': 'DM_Door_MetroExt_Comercial', 'TEMPLE': 'DM_Door_Elevator_Temple', 'GDB': 'DM_Door_Elevator_Temple', 'GAMBLINDEN': 'DM_OutLink_Outskirts', 'SCRAPYARD': 'DM_OutLink_Scrapyard', 'OUTSKIRTS': 'DM_OutLink_Outskirts', 'DOWNTOWN': 'DM_OutLink_DownTown'},
	'ORBIT': {'COMMERCIAL': 'DM_Door_ElevatorOrbitC_TownHall', 'JUNKYARD': 'DM_Door_ElevatorOrbitC_TownHall', 'TOWNHALL': 'DM_Door_ElevatorOrbitC_TownHall', 'BANK': 'DM_Door_ElevatorOrbitC_TownHall', 'POLICE': 'DM_Door_ElevatorOrbitC_TownHall', 'PRESS': 'DM_Door_ElevatorOrbitC_TownHall', 'TEMPLE': 'DM_Door_ElevatorOrbitC_TownHall', 'GDB': 'DM_Door_ElevatorOrbitC_TownHall', 'GAMBLINDEN': 'DM_Door_ElevatorOrbitC_TownHall', 'SCRAPYARD': 'DM_Door_ElevatorOrbitC_TownHall', 'OUTSKIRTS': 'DM_Door_ElevatorOrbitC_TownHall', 'DOWNTOWN': 'DM_Door_ElevatorOrbitC_TownHall'},
	'BANK': {'SCRAPYARD': 'DM_Door_Metro_Outskirts', 'TOWNHALL': 'DM_Door_Metro_Outskirts', 'DOWNTOWN': 'DM_Door_Metro_Outskirts', 'ORBIT': 'DM_Door_Metro_Outskirts', 'TEMPLE': 'DM_Door_Metro_Outskirts', 'GDB': 'DM_Door_Metro_Outskirts', 'POLICE': 'DM_Door_Metro_Outskirts', 'GAMBLINDEN': 'DM_Door_Metro_Outskirts', 'PRESS': 'DM_Door_Metro_Outskirts', 'COMMERCIAL': 'DM_Door_Metro_Outskirts', 'JUNKYARD': 'DM_Door_Elevator_Outskirts', 'OUTSKIRTS': 'DM_Door_Elevator_Outskirts'},
	'TEMPLE': {'TOWNHALL': 'DM_Door_Metro_Templo00', 'ORBIT': 'DM_Door_Metro_Templo00', 'BANK': 'DM_Door_Metro_Templo00', 'POLICE': 'DM_Door_Metro_Templo00', 'PRESS': 'DM_Door_Metro_Templo00', 'GAMBLINDEN': 'DM_Door_Metro_Templo00', 'SCRAPYARD': 'DM_Door_Metro_Templo00', 'OUTSKIRTS': 'DM_Door_Metro_Templo00', 'DOWNTOWN': 'DM_Door_Metro_Templo00', 'GDB': 'DM_Door_TempleTeleporter_GDB', 'COMMERCIAL': 'DM_Door_Elevator_Commercial', 'JUNKYARD': 'DM_Door_Elevator_Commercial'},
	'GDB': {'JUNKYARD': 'DM_Door_TempleTeleporter_Temple', 'TOWNHALL': 'DM_Door_TempleTeleporter_Temple', 'ORBIT': 'DM_Door_TempleTeleporter_Temple', 'BANK': 'DM_Door_TempleTeleporter_Temple', 'POLICE': 'DM_Door_TempleTeleporter_Temple', 'PRESS': 'DM_Door_TempleTeleporter_Temple', 'GAMBLINDEN': 'DM_Door_TempleTeleporter_Temple', 'SCRAPYARD': 'DM_Door_TempleTeleporter_Temple', 'OUTSKIRTS': 'DM_Door_TempleTeleporter_Temple', 'DOWNTOWN': 'DM_Door_TempleTeleporter_Temple', 'TEMPLE': 'DM_Door_TempleTeleporter_Temple', 'COMMERCIAL': 'DM_Door_TempleTeleporter_Temple'},
	'POLICE': {'DOWNTOWN': 'DM_Door_Metro_Press', 'TOWNHALL': 'DM_Door_Metro_Press', 'ORBIT': 'DM_Door_Metro_Press', 'BANK': 'DM_Door_Metro_Press', 'TEMPLE': 'DM_Door_Metro_Press', 'GDB': 'DM_Door_Metro_Press', 'PRESS': 'DM_Door_Metro_Press', 'COMMERCIAL': 'DM_Door_Metro_Press', 'SCRAPYARD': 'DM_Door_Metro_Press', 'GAMBLINDEN': 'DM_Door_Elevator_Outskirts', 'OUTSKIRTS': 'DM_Door_Elevator_Outskirts', 'JUNKYARD': 'DM_Door_Elevator_Outskirts'},
	'GAMBLINDEN': {'DOWNTOWN': 'DM_Door_Elevator_Outskirts', 'JUNKYARD': 'DM_Door_Elevator_Outskirts', 'TOWNHALL': 'DM_Door_Elevator_Outskirts', 'ORBIT': 'DM_Door_Elevator_Outskirts', 'BANK': 'DM_Door_Elevator_Outskirts', 'TEMPLE': 'DM_Door_Elevator_Outskirts', 'GDB': 'DM_Door_Elevator_Outskirts', 'POLICE': 'DM_Door_Elevator_Outskirts', 'PRESS': 'DM_Door_Elevator_Outskirts', 'OUTSKIRTS': 'DM_Door_Elevator_Outskirts', 'COMMERCIAL': 'DM_Door_Elevator_Outskirts', 'SCRAPYARD': 'DM_Door_Elevator_Scrapyard'},
	'PRESS': {'DOWNTOWN': 'DM_Door_Elevator_Press', 'JUNKYARD': 'DM_Door_Elevator_Press', 'TOWNHALL': 'DM_Door_Metro_Press', 'ORBIT': 'DM_Door_Metro_Press', 'BANK': 'DM_Door_Metro_Press', 'TEMPLE': 'DM_Door_Metro_Press', 'GDB': 'DM_Door_Metro_Press', 'POLICE': 'DM_Door_Metro_Press', 'GAMBLINDEN': 'DM_Door_Metro_Press', 'OUTSKIRTS': 'DM_Door_Metro_Press', 'SCRAPYARD': 'DM_Door_Metro_Press', 'COMMERCIAL': 'DM_Door_Metro_Press'},
	'OUTSKIRTS': {'JUNKYARD': 'ParkParking_Junkyard', 'BANK': 'DM_Door_Elevator_Bank', 'POLICE': 'DM_Door_Elevator_Police', 'GAMBLINDEN': 'DM_Door_Elevator_GamblinDen', 'DOWNTOWN': 'DM_OutLink_DownTown', 'COMMERCIAL': 'DM_OutLink_Commercial', 'SCRAPYARD': 'DM_OutLink_Scrapyard', 'TOWNHALL': 'DM_Door_MetroExt_', 'ORBIT': 'DM_Door_MetroExt_', 'TEMPLE': 'DM_Door_MetroExt_', 'GDB': 'DM_Door_MetroExt_', 'PRESS': 'DM_Door_MetroExt_'},
	'SCRAPYARD': {'JUNKYARD': 'ParkParking_Junkyard', 'GAMBLINDEN': 'DM_Door_Elevator_GamblinDen', 'DOWNTOWN': 'DM_OutLink_DownTown', 'COMMERCIAL': 'DM_OutLink_Commercial', 'OUTSKIRTS': 'DM_OutLink_Outskirts', 'TOWNHALL': 'DM_Door_MetroExt_ScrapYard', 'ORBIT': 'DM_Door_MetroExt_ScrapYard', 'TEMPLE': 'DM_Door_MetroExt_ScrapYard', 'GDB': 'DM_Door_MetroExt_ScrapYard', 'PRESS': 'DM_Door_MetroExt_ScrapYard', 'BANK': 'DM_Door_MetroExt_ScrapYard', 'POLICE': 'DM_Door_MetroExt_ScrapYard'}
}
def Init
def TryMsg
def SetNextMission
def SetFirstDemoMission
def SetOutMissionArrow
def ActivateTarget
def GetSecondary
def SetSecondary
def FinishGame
def ShowMissionInfo
let currentTarget = None
let targetsList = []
def SetMissionTarget
def SetMultipleMissionTargets
def CleanAllTargets
def DelMissionTarget
def IsCharMissionObjective
def CharMissionDialog
def CancelSecondaryMission
def SinglePlayerDeath
def CreatePhotoObject
def UnsetPhotoObject
let maxBertos = 10
let currentBertos = []
let launchNewWave = 1
let timeBetweenWaves = 15
let OnNewWaveCallback = None
let OnBertoDeathCallback = None
def ReassignBertosPlayerTarget
def CreateBerto
def CreateBertos
def OnBertoDeath
call Scrap.CreateSaveVar('CrazyDeal.1.Var', 'Stats.BankDirector')
call Scrap.CreateSaveVar('CrazyDeal.1.Tgt', '10000')
call Scrap.CreateSaveVar('CrazyDeal.2.Var', 'Stats.Messenger')
call Scrap.CreateSaveVar('CrazyDeal.2.Tgt', '8')
call Scrap.CreateSaveVar('CrazyDeal.3.Var', 'Stats.Betty')
call Scrap.CreateSaveVar('CrazyDeal.3.Tgt', '20')
call Scrap.CreateSaveVar('SuperDeal.Map', 'CA')
call Scrap.CreateSaveVar('SuperDeal.Library', 'SuperDeal_First')
call Scrap.CreateSaveVar('SuperDeal.Type', 'Versus')
def CrazyDealStatus
def GetNumCrazyDealCompleted
def OnCrazyDealFinishedMessage
def OnCrazyDealTarget
def NewCrazyDeal
def SetNextSuperDeal
def StartActualSuperDeal
def EndOfSuperDeal
def StartSuperDeal
def RelaxShipsInList
def SetInvulnerableShipsInList
let cameraData = None
let mapsCameraData = {
	'CA': ((20091.45, (-23309.88), (-26126.76)), (1.83, (-26.84), 0.0)),
	'QF': ((43390.7, (-53931.0), 2267.0), ((-16.53), 305.8, 0.0)),
	'FZ': (((-38383.24), 35365.96, 53203.84), ((-5.09), 498.06, 0.0)),
	'OM': ((31228.03, 15214.36, 155090.03), (7.41, 337.22, 0.0))
}
let friendsNames = ['Betty', 'Berto', 'Trurl', 'CrazyGambler']
let enemiesNames = ['Scrapus', 'Corruptus', 'Porkins', 'Igor', 'Multiplus']
let textColor = (255, 255, 255)
let bgColorRed = (255, 0, 0, 128)
let bgColorGreen = (0, 255, 0, 128)
def OnSuperDealFinish
def BuildMultipleEnemiesData
def BuildTeamDeathMatchData
def BuildVersusData
def BuildDominationData
def BuildCTFData
def SuperDealOnDefeat
def SuperDealOnVictory
