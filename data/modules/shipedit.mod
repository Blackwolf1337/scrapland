# The module code of ShipEdit (Data.packed/scripts/shipedit/shipedit.pyc): its imports, variables, functions and load time calls in the order of the
# script; ScrapC makes the module from this file instead of running the .pyc. Made by tools/modules/pyc2mod.py.
# A pack in mods/ with scrapc/modules/shipedit.mod replaces this file.

import SSound
import SScorer
import Scrap
import SVec
import SAct
import SAI
let sizeX = 546
let sizeY = 406
let offsetX = (320 - (sizeX / 2))
let offsetY = (240 - (sizeY / 2))
let TabQuadSize = 14
let NormalColorAlpha = 128
let NormalColorRed = 83
let NormalColorGreen = 114
let NormalColorBlue = 193
let SelectedColorAlpha = 192
let SelectedColorRed = 212
let SelectedColorGreen = 200
let SelectedColorBlue = 80
let HighLightColorRed = 255
let HighLightColorGreen = 255
let HighLightColorBlue = 190
call Scrap.CreateSaveVar('Hangar.HangarShip1', '')
call Scrap.CreateSaveVar('Hangar.HangarShip2', '')
call Scrap.CreateSaveVar('Hangar.HangarShip3', '')
call Scrap.CreateSaveVar('Hangar.HangarShip4', '')
call Scrap.CreateSaveVar('Hangar.HangarShip5', '')
call Scrap.CreateSaveVar('Hangar.HangarShip6', '')
call Scrap.CreateSaveVar('Hangar.HangarShip7', '')
call Scrap.CreateSaveVar('Hangar.HangarShip8', '')
call Scrap.CreateSaveVar('Hangar.HangarShip9', '')
call Scrap.CreateSaveVar('Hangar.HangarShipAux', 'SLifeBoat<-<-<-<-<-0,0,0,0,0,0,1<-50<-0,0,0,0,0,0')
call Scrap.CreateSaveVar('Hangar.DestroyedShips', '[0,0,0,0,0,0,0,0,0,0]')
let prevTab = None
let camera = None
let lastCam = None
let editShipPosDummy = None
let editPlatform = None
let EditCameraPitch = 15
let CameraDist = 1500
let editedShips = []
let destroyedShips = []
let fromSputnik = 0
if Scrap.Get('isDemo')
	let StdShipsToEdit = '["SPoli1","SPoli2", "SMerc1","SBanker1","SBetty1"]'
	let StdEnginesToEdit = "['MPOLI1','MPOLI2', 'MMERC1','MBANKER1','MBETTY1']"
	let StdWeapList = '["Vulcan", "Devastator", "Swarm","ATPC"]'
else
	let StdShipsToEdit = '["SPoli1", "SPoli2", "SPoli3", "SPoli4", "SPoli5", "SPoliBoss1", "SMerc1","SMerc2","SMerc3","SMayor1","SBanker1","SBankMaster1","SBishop1","SArchbishop1","SFunc1","SBerto1","SBetty1","SHump1","SBoss1"]'
	let StdEnginesToEdit = "['MPOLI4', 'MPOLI5', 'MPOLIBOSS1', 'MPOLI2', 'MBERTO1', 'MBETTY1', 'MPOLI1', 'MMERC1','MMERC2', 'MMERC3', 'MPOLI3', 'MMAYOR1', 'MFUNC1', 'MBANKER1','MBANKMASTER1', 'MBISHOP1','MARCHBISHOP1', 'MHUMP1', 'MBOSS1']"
	let StdWeapList = '["Vulcan", "Devastator", "Swarm","Inferno", "Tesla", "ATPC"]'
end
call Scrap.CreateSaveVar('Hangar.shipsToEditList', StdShipsToEdit)
let shipsToEditList = eval(Scrap.GetSaveVar('Hangar.shipsToEditList'))
call Scrap.CreateSaveVar('Hangar.availableWeaponsList', StdWeapList)
let availableWeaponsList = eval(Scrap.GetSaveVar('Hangar.availableWeaponsList'))
call Scrap.CreateSaveVar('Hangar.availableUpgradesList', '["VulcanUpgrade1", "VulcanUpgrade2", "DevastatorUpgrade1", "DevastatorUpgrade2", "SwarmUpgrade1", "SwarmUpgrade2", "InfernoUpgrade1", "InfernoUpgrade2", "TeslaUpgrade1", "TeslaUpgrade2", "ATPCUpgrade1", "ATPCUpgrade2"]')
let availableUpgradesList = eval(Scrap.GetSaveVar('Hangar.availableUpgradesList'))
call Scrap.CreateSaveVar('Hangar.availableEnginesList', StdEnginesToEdit)
let isXbox = Scrap.Get('isXbox')
if isXbox
	let UseTxt = (Scrap.GetLangStr('XBox_Press_Button') % '\024')
else
	let UseTxt = Scrap.GetLangStr('PC_Menu_LClick')
end
let auxShipsToEditList = []
let shipsEnginesLayouts = {
	'SPoli1': [(307, 247, 333, 268, 1)],
	'SPoli2': [(262, 244, 378, 267, 2)],
	'SPoli3': [(244, 242, 396, 266, 2), (309, 270, 332, 293, 1)],
	'SPoli4': [(224, 192, 416, 216, 2), (224, 252, 416, 275, 2)],
	'SPoli5': [(200, 196, 440, 220, 2), (200, 292, 440, 315, 2)],
	'SPoliBoss1': [(160, 260, 480, 284, 2), (230, 290, 410, 312, 2)],
	'SMerc1': [(262, 242, 378, 265, 2)],
	'SMerc2': [(425, 220, 445, 240, 1), (220, 250, 240, 270, 1), (422, 270, 442, 290, 1)],
	'SMerc3': [(200, 152, 440, 190, 2), (220, 272, 420, 320, 2)],
	'SMayor1': [(242, 234, 398, 257, 2)],
	'SBanker1': [(192, 244, 446, 270, 2)],
	'SBetty1': [(240, 244, 400, 267, 2)],
	'SBankMaster1': [(220, 112, 420, 136, 2), (180, 272, 460, 296, 2)],
	'SBishop1': [(360, 168, 386, 190, 1), (230, 232, 444, 256, 2), (360, 290, 386, 312, 1)],
	'SArchbishop1': [(180, 134, 410, 166, 2), (230, 292, 420, 331, 2)],
	'SFunc1': [(307, 280, 333, 302, 1)],
	'SBerto1': [(254, 208, 278, 230, 1), (378, 246, 402, 270, 1), (254, 280, 278, 306, 1)],
	'SHump1': [(220, 162, 420, 186, 2), (220, 308, 420, 331, 2)],
	'SBoss1': [(180, 216, 460, 238, 2), (180, 276, 460, 298, 2)],
	'SLifeBoat': []
}
let tabs = None
let tabPixelX = {}
let armorPieceWeight = Scrap.Get('AmorPieceWeight')
def IsEditingMultiplayer
def RustyOnPlatform
def PointObjectTo
def LaunchPlatformAnim
def ChangeShip
def VerifyChangeShip
def DoorInitializator
def ObjInitializator
let ctnRotating = 0
let isRotating = 0
def EndMoveEngine
def RotateShip
def LoopRotateShip
def StartRotateLoop
def StopRotateLoop
def DrawBackground
def AddShipEditTabs
def AddDefaultPanelContent
def ChangeTabLeft
def ChangeTabRight
def ChangeTab
def CleanEditedShips
def CreatePlayerShip
def SetupForEdit
def SaveShipInfoInSaveVar
def SaveShipInfoInGVar
def CreateShipFromSaveVar
def AssignShip
def SetupEdit
def FreeEdit
def EnterEditShipMenu
let EndShipEditCallBack = None
def ExitEditShipMenu
def SaveShip
def ShowSpecialHint
