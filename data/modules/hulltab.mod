# The module code of HullTab (Data.packed/scripts/shipedit/hulltab.pyc): its imports, variables, functions and load time calls in the order of the
# script; ScrapC makes the module from this file instead of running the .pyc. Made by tools/modules/pyc2mod.py.
# A pack in mods/ with scrapc/modules/hulltab.mod replaces this file.

import SScorer
import Scrap
let CircuitColorAlpha = 128
let HullMinColorRed = 122
let HullMinColorGreen = 161
let HullMinColorBlue = 176
let HullColorRed = 254
let HullColorGreen = 133
let HullColorBlue = 230
let isXbox = Scrap.Get('isXbox')
if isXbox
	let UseTxt = (Scrap.GetLangStr('XBox_Press_Button') % '\024')
else
	let UseTxt = Scrap.GetLangStr('PC_Menu_LClick')
end
def ShowHullPredictionUp
def HideHullPredictionUp
def ShowHullPredictionDown
def HideHullPredictionDown
def TestArrows
def IncrementHull
def DecrementHull
def ChangeHull
def SetHullState
def HullSliderChange
def SetHullPanel
def ShowHullPanel
def SetHullVisible
def HideHullPanel
