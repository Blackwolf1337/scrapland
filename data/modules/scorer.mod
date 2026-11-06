# The module code of Scorer (Data.packed/scripts/scorer/scorer.pyc): its imports, variables, functions and load time calls in the order of the
# script; ScrapC makes the module from this file instead of running the .pyc. Made by tools/modules/pyc2mod.py.
# A pack in mods/ with scrapc/modules/scorer.mod replaces this file.

import Scrap
import SInput
import SNet
import SWeap
import SScorer
if Scrap.Get('IsXbox')
	call Scrap.Set('ScorerMarginH', 48)
	call Scrap.Set('ScorerMarginV', 36)
else
	call Scrap.Set('ScorerMarginH', 12)
	call Scrap.Set('ScorerMarginV', 10)
end
let MarginH = Scrap.Get('ScorerMarginH')
let MarginV = Scrap.Get('ScorerMarginV')
let SplitScreen = Scrap.Get('SplitScreen')
let MissionScorerName = 'Mission'
def CreateStatusScorer
def CreateCopMeterScorer
def CreateMonitorScorer
def CreateMsgScorer
def CreateMissionScorer
def ShowMissionScorer
def CreateRadarScorer
def Blink
let MissionScorerText_X = MarginH
let MissionScorerText_Y = (MarginV + 110)
let MissionScorerText_Color = (200, 200, 255, 255)
let MissionScorerBar_X = (MarginH + 6)
let MissionScorerBar_Y = ((MarginV + 136) + 6)
let MissionScorerBar_ColorBack = (200, 200, 255, 255)
let MissionScorerBar_ColorMain = (255, 16, 16, 192)
let MissionScorerBar_ColorBlack = (0, 0, 0, 128)
def MissionScorerBar_SetDefaults
