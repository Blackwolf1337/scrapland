# The module code of PCMenu (Data.packed/scripts/scorer/pcmenu.pyc): its imports, variables, functions and load time calls in the order of the
# script; ScrapC makes the module from this file instead of running the .pyc. Made by tools/modules/pyc2mod.py.
# A pack in mods/ with scrapc/modules/pcmenu.mod replaces this file.

import Scrap
import SInput
import SNet
import SWeap
import SScorer
import SVec
let MOTD = ''
let Menu = None
def AbortWaitRedefine
def KetRedefineFunc
def ClearKeyEntry
def WaitForKeyPress
def RefreshControlMenu
def CreateControlMenu
def RacerMoveControlsMenu
def RacerAttackControlsMenu
def WalkCharControlsMenu
def UseCharControlsMenu
def ResetEverything
def ResetToDefault
def OptionYesNo
def AlwaysRun
def ControlsMenu
let currentActionSet = ''
let options = {}
let optionsChar = {
	'Item1': ('XBCameraAutoPad', 'ConfigurationTypeChar'),
	'Item2': ('XBInvertCharYPad', 'InvertYChar'),
	'Item3': ('XBCDigitalAsAnalog', 'DigitalCrossChar'),
	'Item4': ('XBInvertLRChar', 'InvertTriggersChar')
}
let optionsShip = {
	'Item1': ('XBVehicleAsFPS', 'ConfigurationTypeShip'),
	'Item2': ('XBInvertVehicleYPad', 'InvertYShip'),
	'Item3': ('XBVDigitalAsAnalog', 'DigitalCrossShip'),
	'Item4': ('XBInvertLRVehicle', 'InvertTriggersShip')
}
let baseYPos = 280
let buttons = {
	'Y': ('Left', 388, (baseYPos - 60)),
	'X': ('Left', 388, (baseYPos - 40)),
	'B': ('Left', 388, (baseYPos - 20)),
	'A': ('Left', 388, baseYPos),
	'BLACK': ('Left', 388, (baseYPos + 20)),
	'WHITE': ('Left', 388, (baseYPos + 40)),
	'BACK': ('Right', 252, (baseYPos + 4)),
	'START': ('Right', 252, (baseYPos + 28)),
	'L': ('Right', 280, (baseYPos - 105)),
	'R': ('Left', 360, (baseYPos - 105))
}
let buttonsSymbols = {
	'Y': '\027',
	'X': '\026',
	'B': '\025',
	'A': '\024',
	'BLACK': '\035',
	'WHITE': '\034',
	'BACK': '\033',
	'START': '\032',
	'R': '\031',
	'L': '\030'
}
def ControllerConfig
def ControllerConfigCharMenu
def ControllerConfigShipMenu
def FillControllerMenu
def SwitchOption
def PlaceMenuOption
def SetOptionValue
def DrawLine
def UpdatePadText
def ShowPadConfig
def MouseControlsMenu
def InvertMouse
def MouseSensitivitySliderFocus_Sub
def MouseSensitivitySliderFocus_Add
def MouseSensitivityFormula
def MouseSensitivityChange
let MultiplayerMenu = ['Net', '']
let BrowsingActualDomain = 'Internet'
let BrowserSlots = 13
let BrowserSort = 'Ping'
let BrowserSortOrder = 0
let SIBF_DEDICATED = 1
let SIBF_FORCE_SHIP = 2
let SIBF_PASSWORD = 4
def OnNetBrowse
def BrowserSortExecute
def BrowseLocalMenu
def MasterCommand
def BrowseINetMenu
def InetMasterBrowse
def BrowserMenuBack
def RefreshBrowseMenu
def RefreshBrowseMenuButton
def BrowseLocalMenuPrevious
def BrowseLocalMenuNext
def BrowseLocalMenuPgUp
def BrowseLocalMenuPgDown
def BrowseLocalJoin
def UnicodeParse
def CreateBrowseMenu
def BrowserColumnBack_Click
def BrowserSliderGainFocus
def BrowserSliderLooseFocus
def BrowserSliderOnRender
def UpdateBrowseSlider
def UpdateBrowseMenu
def BrowseFormatServerLine
def MultiPlayerMenu
def MenuRestartServer
def MenuInitServer
def MenuJoinGame
def TryJoin
def JoinPassword
def AskPassword
def ShowConectionStatus
def AbortJoin
def SpecifyIPMenu
def AbortMultiplayerGame
let MapSlots = 9
let MapIndex = 0
def NetDefaultContentMenu
def NetCreateGameMenu
def AdvancedServer
def BackToCreateServer
def SwitchForceVehicle
def SwitchUsePass
def RunDedicatedServer
def SetDataRate
def GetDataRateText
def ServerTypePrev
def ServerTypeNext
def UpdateServerType
def ServerTypeInfo
def MapUp
def MapDown
def MapSliderGainFocus
def MapSliderLooseFocus
def MapSliderOnRender
def UpdateMapSlider
def UpdateMapList
def MapChoose
def MapPreviewOn
def MapPreviewOff
def ServerNameGainFocus
def ServerNameLooseFocus
def ServerNameOnRender
def PlayersNumberGainFocus
def PlayersNumberLooseFocus
def PlayersNumberOnRender
def FragLimitGainFocus
def FragLimitLooseFocus
def FragLimitOnRender
def NetJoinGameMenu
def NetJoinInternetMenu
def NetJoinLocalMenu
def NetJoinIPMenu
def OptionsMenu
let YesValue = ('\002 ' + Scrap.GetLangStr('Menu_Question_Yes'))
let NoValue = ('\003 ' + Scrap.GetLangStr('Menu_Question_No'))
def VideoMenu
def ChangeVideoMode
def PrevVideoMode
def NextVideoMode
def FillVideoModes
def FillTextureFilter
def ChangeTextureFilter
def FillTextureDetail
def ChangeTextureDetail
def FillShadowsDetail
def ChangeShadowsDetail
def ChangeVSync
def ChangeLock
def ChangeFog
def ChangeBloom
def ChangeMotionBlur
def ChangeDuDv
def ChangeBoolean
def ChangeGamma
def GammaFormula
def GammaSliderFocus_Sub
def GammaSliderFocus_Add
def UpdateVideoOptions
let Last_TexFilter = 0
let Last_MipFilter = 0
let Last_VideoGamma = 0
let Last_Fog = 0
let Last_VideoLockMode = 0
let Last_VideoLockBuf = 0
def VideoGetLast
def VideoMenuBack
def VideoDefaults
def Apply_SetActive
def Apply_SetTextureFilter
def Apply_SetVideoLock
def Apply_SetFog
def ApplyChanges
