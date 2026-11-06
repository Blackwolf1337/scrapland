# The module code of Parking (Data.packed/scripts/vehicles/parking.pyc): its imports, variables, functions and load time calls in the order of the
# script; ScrapC makes the module from this file instead of running the .pyc. Made by tools/modules/pyc2mod.py.
# A pack in mods/ with scrapc/modules/parking.mod replaces this file.

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
let AlarmProbOnParkedShipDeath = 0.15
let ParkingDist = 800
let MinParkingVel = 1000
let ParkingDepth = 300
let P_None = 'P_None'
let PL_Junk = 'DM_Player_Spawn_Junkyard'
let PR_Junk = 'DM_Parking_Junkyard'
let PR_Lst = 'ParkingBox'
let PR_ComeFrom = 'UseShip:'
let PR_Spawn = 'DM_Ship_Spawn_'
call Scrap.CreateSaveVar('usrShip.Ammo00', '100')
call Scrap.CreateSaveVar('usrShip.Ammo01', '100')
call Scrap.CreateSaveVar('usrShip.Ammo02', '100')
call Scrap.CreateSaveVar('usrShip.AcWeap', '6')
let SwitchCharMissionCallback = None
let SwitchVehicleMissionCallback = None
let ParkedOutCallback = None
let SwitchCharMissionCallbackAux = None
let SwitchVehicleMissionCallbackAux = None
call Scrap.CreateSaveVar('Parked.Police', "['SPOLI1', 'SPOLI2', 'SPOLI3', 'SPOLI4', 'SPOLI5', 'SPOLIBOSS1']")
call Scrap.CreateSaveVar('Parked.Mercs', "['SMERC1', 'SMERC2', 'SMERC3']")
call Scrap.CreateSaveVar('Parked.TownHall', "['SMAYOR1','SFUNC1']")
call Scrap.CreateSaveVar('Parked.Bank', "['SBANKER1', 'SBANKMASTER1']")
call Scrap.CreateSaveVar('Parked.Press', "['SBERTO1', 'SBETTY1','SHUMP1','SBOSS1']")
call Scrap.CreateSaveVar('Parked.Temple', "['SARCHBISHOP1', 'SBISHOP1']")
let ShipTypes = {
	'Police': eval(Scrap.GetSaveVar('Parked.Police')),
	'Mercs': eval(Scrap.GetSaveVar('Parked.Mercs')),
	'TownHall': eval(Scrap.GetSaveVar('Parked.TownHall')),
	'Bank': eval(Scrap.GetSaveVar('Parked.Bank')),
	'Press': eval(Scrap.GetSaveVar('Parked.Press')),
	'Temple': eval(Scrap.GetSaveVar('Parked.Temple')),
	'Bishop': ('SBISHOP1',),
	'Mayor': ('SMAYOR1',),
	'LonelyMerc': ('SMERC3',),
	'Boss': ('SBOSS1',)
}
let AllShips = (
	((((ShipTypes['Police'] + ShipTypes['Mercs']) + ShipTypes['TownHall']) + ShipTypes['Bank']) + ShipTypes['Press']) + ShipTypes['Temple']
)
def RestoreDeathPlayerShip
def SetInvunerable
let SetWeaponOnShip = 0
def CharOnShip
def UseShip
def PlayVoiceChannel
def ExecSwitchVehicleMissionCallback
def EndPathCam
def SetCameraStart
def isValidToPark
def OnPark
let PlayerShip = None
let DtritusPlate = None
let DtritusPlateModel = 'models/Misc/DtritusPlate/DtritusPlate.m3d'
let DtritusPlateNode = 'DC_Plate'
def SetDtritusPlate
def CreateDtritusPlate
let OnCreatePlayerShip = None
def CreatePlayerShip
def ShowChar
let LastCamera = None
let LastPark = None
def EndJunkerPathCam
def JunkerPark
let ParkingZones = {}
let LastParkingZone = None
def OnParkingOccupied
def OnParkedOut
def CreateBox
def SetShipRadius
def SetOnFloor
def SetShipAsParked
def ParkedDeat
def BrowseItemPos
def CreateLonelyMercParkings
def CreateParkedShip
def CheckWeapons
def BrowseParkedPos
def ChangeLevel
def BrowseParkingAreas
def BrowseSwitchLevel
def ShowNextMap
def DoorInitializator
call Scrap.CreateSaveVar('Parking.Desolate', '0')
def Init
call Scrap.Set('OnParkingOccupied', 'Parking.OnParkingOccupied')
call Scrap.Set('OnParkedOut', 'Parking.OnParkedOut')
call Scrap.Set('ParkingCheckRadius', 500.0)
call Scrap.Set('ParkingCharCheckDistance', 15000.0)
call Scrap.Set('ParkingVehiCheckDistance', 50000.0)
