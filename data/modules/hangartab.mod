# The module code of HangarTab (Data.packed/scripts/shipedit/hangartab.pyc): its imports, variables, functions and load time calls in the order of the
# script; ScrapC makes the module from this file instead of running the .pyc. Made by tools/modules/pyc2mod.py.
# A pack in mods/ with scrapc/modules/hangartab.mod replaces this file.

import SSound
import SScorer
import Scrap
let NormalColorAlpha = 255
let NormalColorRed = 83
let NormalColorGreen = 114
let NormalColorBlue = 193
let SelectedColorAlpha = 255
let SelectedColorRed = 212
let SelectedColorGreen = 200
let SelectedColorBlue = 80
let HighLightColorRed = 255
let HighLightColorGreen = 255
let HighLightColorBlue = 190
let DestroyedColorRed = 255
let DestroyedColorGreen = 32
let DestroyedColorBlue = 32
let CircuitColorAlpha = 128
let TabQuadSize = 14
call Scrap.CreateSaveVar('Hangar.iHangarShip', '9')
let prevItem = ''
let prevShip = None
let miniShip = None
let prevHangar = 0
let currentHangar = eval(Scrap.GetSaveVar('Hangar.iHangarShip'))
let currentShipToEdit = 0
let currentPilot = 0
let holograms = 0
let shipTotalCost = 0
let hangarBaseX = 52
let hangarBaseY = 204
let baseShipInfoX = 194
let baseShipInfoY = 349
let baseMiniShipInfoX = (hangarBaseX + 12)
let baseMiniShipInfoY = (hangarBaseY - 60)
let speedRampX = 25
let speedRampY = 20
let isXbox = Scrap.Get('isXbox')
if isXbox
	let UseTxt = (Scrap.GetLangStr('XBox_Press_Button') % '\024')
else
	let UseTxt = Scrap.GetLangStr('PC_Menu_LClick')
end
def ShowBoostPrediction
def DisableChangePlatformButtons
def EnableChangePlatformButtons
def PrevPilot
def NextPilot
def DrawBackground
def SetSellVisible
def SetSellActive
def SetExitVisible
def SetShipTypeVisible
def SetPilotVisible
def HideHangarPanel
def UpdateHangars
def ShowHangarPanel
def IsShipValid
def ComputeShipTotalCost
def ShowRebuildShipMoney
def HideRebuildShipMoney
def AuxShowMoney
def ShowMoneyBuyingShip
def ShowMoneySellingShip
def HideMoney
def ScaleSpeedRamp
def GetSpeedRampOffsets
def GetMaxSpeedTime
def SetHangarPanel
def CmSec2Value
def ShowShipInfo
def HideShipInfo
def PrevShipToEdit
def NextShipToEdit
def SetShipTypeButtonsState
def SelectShipToEdit
def SelectHangar
def SelectItem
def CreateEmptyShip
def BuyShip
def SellShip
def AddReferenceShipControls
def ShowShipMiniInfo
def HideShipMiniInfo
