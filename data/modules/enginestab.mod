# The module code of EnginesTab (Data.packed/scripts/shipedit/enginestab.pyc): its imports, variables, functions and load time calls in the order of the
# script; ScrapC makes the module from this file instead of running the .pyc. Made by tools/modules/pyc2mod.py.
# A pack in mods/ with scrapc/modules/enginestab.mod replaces this file.

import SScorer
import Scrap
import SSound
let NormalColorAlpha = 255
let NormalColorRed = 115
let NormalColorGreen = 150
let NormalColorBlue = 255
let SelectedColorAlpha = 255
let SelectedColorRed = 250
let SelectedColorGreen = 230
let SelectedColorBlue = 90
let HighLightColorRed = 255
let HighLightColorGreen = 255
let HighLightColorBlue = 190
let ForbiddenColorRed = 200
let ForbiddenColorGreen = 30
let ForbiddenColorBlue = 30
let CircuitColorAlpha = 128
let TabQuadSize = 14
let EnginesYOffset = 125
let enginesList = []
let canBeMounted = []
let engineIndex = 0
let currentEngineGroup = ''
let isXbox = Scrap.Get('isXbox')
if isXbox
	let UseTxt = (Scrap.GetLangStr('XBox_Press_Button') % '\024')
else
	let UseTxt = Scrap.GetLangStr('PC_Menu_LClick')
end
def HighlightCurrentMountedEngine
def ShowEngineDesc
def HideEngineDesc
def ChangeRecycleButtonState
def UpdateEnginesList
def PutEngine
def RecycleEngine
def ShowMoneySellingEngine
def SelectEngineGroup
def SetEnginesPanel
def SetPrevNextEngineVisible
def SetRecycleVisible
def ShowEnginesPanel
def CreateLayouts
let nEngSlots = [0, 0, 0, 0]
def UpdateLayouts
def HideLayouts
def UpdateLayoutsState
def UpdateLayoutsGlow
def LayoutsGainFocus
def LayoutsLostFocus
def HideEnginesPanel
def ScrollEngineListUp
def ScrollEngineListDown
