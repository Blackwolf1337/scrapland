# The module code of MakeChar (Data.packed/scripts/chars/makechar.pyc): its imports, variables, functions and load time calls in the order of the
# script; ScrapC makes the module from this file instead of running the .pyc. Made by tools/modules/pyc2mod.py.
# A pack in mods/ with scrapc/modules/makechar.mod replaces this file.

import Scrap
import SInput
import SNet
import SWeap
import SScorer
import SSound
import SAct
import sys
let MaxSpeed = 1.5
let MinSpeed = 0.8
let DazedChars = []
let RelaxRotLimit = 90
def CreateRelaxAction
def AddRelaxActions
def AddDiscover
def AddMove
def AddWalkActions
def AddInitWalkActions
def AddEndWalkActions
def AddFallAction
def AddTalkAction
def AddTalkMorph
def AddMorphAction
let MorphEyesList = (
	'Betty',
	'Bishop',
	'Berto',
	'Dtritus',
	'Sebastian',
	'CrazyGambler',
	'GateKeeper',
	'Mayor',
	'Rusty',
	'MercenaryA'
)
let MorphLipsList = ('Betty', 'Bishop')
def AddDefMorphActions
def EventAnimCommand
def AddTalkAct
def AddFlashedAction
def AddCrazyAction
def AddDazeAction
def ListenAction
def AddBreakLoopAction
def SetSitActions
def InitChar
def SetNodeRotation
def TurretBasicActions
def AddTurretTalkAction
def AddTurretTalkAct
