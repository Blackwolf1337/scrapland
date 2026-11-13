# The module code of Challenge (Data.packed/scripts/missions/challenge.pyc): its imports, variables, functions and load time calls in the order of the
# script; ScrapC makes the module from this file instead of running the .pyc. Made by tools/modules/pyc2mod.py.
# A pack in mods/ with scrapc/modules/challenge.mod replaces this file.

import Scrap
import SInput
import SNet
import SWeap
import SScorer
import SSound
import SAct
import SAI
import sys
let numChallengeEnemies = 1
let challengeType = ''
let foeType = ''
let playerWinsRevenge = 0
let groups = {
	'Functionary': 'Bureaucracy',
	'Mayor': 'Bureaucracy',
	'Berto': 'Press',
	'Betty': 'Press',
	'Humphrey': 'Press',
	'BankDirector': 'Bankers',
	'BankMaster': 'Bankers',
	'CrazyGambler': 'Bankers',
	'PoliceBoss': 'Police',
	'Police': 'Police',
	'Gear': 'Police',
	'Bishop': 'Bishops',
	'MercenaryA': 'Mercs',
	'MercenaryB': 'Mercs',
	'MercenaryC': 'Mercs'
}
let combatRewards = {'Bureaucracy': 1000, 'Bankers': 2000, 'Press': 3000, 'Police': 4000, 'Bishops': 5000}
let raceRewards = {'Bureaucracy': 5000, 'Bankers': 4000, 'Press': 3000, 'Police': 2000, 'Bishops': 1000}
def ComputeReward
def Init
def SetChallengerSkills
def OnChallengeOpt
def SetRevengeParams
def SetRevenge
def LaunchRevenge
def LaunchChallenge
def CleanChallenge
def CleanRevenge
def CancelChallenge
def SetChallengeOptions
def AddChallenge
