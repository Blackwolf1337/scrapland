# The module code of GateKeeper (Data.packed/scripts/chars/gatekeeper.pyc): its imports, variables, functions and load time calls in the order of the
# script; ScrapC makes the module from this file instead of running the .pyc. Made by tools/modules/pyc2mod.py.
# A pack in mods/ with scrapc/modules/gatekeeper.mod replaces this file.

import Scrap
import SInput
import SNet
import SWeap
import SScorer
import SSound
import SVec
import SAct
import sys
let PoliceGroupGateKeeper = ('Police',)
let PoliceBossGroupGateKeeper = ('PoliceBoss',)
let BankMasterGroupGateKeeper = ('BankMaster',)
let BankDirectorGroupGateKeeper = ('BankDirector', 'BankMaster')
let BishopGroupGateKeeper = ('Bishop',)
let MayorGroupGateKeeper = ('Mayor',)
let DtritusGroupGateKeeper = ('Dtritus',)
let NoOneGroupGateKeeper = ('NonOne',)
let HashGroup = {
	PoliceGroupGateKeeper: '3',
	PoliceBossGroupGateKeeper: '4',
	BankMasterGroupGateKeeper: '5',
	BankDirectorGroupGateKeeper: '6',
	BishopGroupGateKeeper: '7',
	MayorGroupGateKeeper: '8',
	DtritusGroupGateKeeper: '9',
	NoOneGroupGateKeeper: '10'
}
def Init
class GateKeeperType
	let FoeType = ()
	let MPass = 0
	let Door = 'DM_Door_OrbitDoorA_00'
	let CanPassFunc = None
	let ClosedFunc = None
end
let GateKeepers = {}
let GKDoors = {}
def UseDoor
def OnCreate
def DoNothingFunc
def SetDoor
def DoorFoe
def GetGateKeeperId
def RestoreState
let LocalCallFunc = None
let LocalParams = None
def EndOfSpeech
let AfterGateKeeperSpeech = None
def GateKeeperSpeech
