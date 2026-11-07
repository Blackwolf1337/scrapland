# The module code of MapTriggers in level scrapyard (data02.packed/levels/scrapyard/scripts/maptriggers.pyc): its imports, variables, functions and load time calls in the order of the
# script; ScrapC makes the module from this file instead of running the .pyc. Made by tools/modules/pyc2mod.py.
# A pack in mods/ with scrapc/levels/scrapyard/maptriggers.mod replaces this file.

import Scrap
let mosquitosLairs = {}
let madHuntersLairs = {}
let WithScrapyardInhabitants = 1
let shipsAllowedToLaunchMosquitos = []
let shipsAllowedToLaunchMadHunters = []
let numMosquitosKilled = None
call Scrap.CreateSaveVar('Stats.Mosquito', '0')
def GetMadHunterProfile
def GetMosquitoProfile
def InitStatus
def Init
def CheckMosquitoCadenceShoot
def CreateMosquitosTriggers
def OnMosquitoTriggerTaken
def CreateMosquito
def CreateMadHuntersTriggers
def OnMadHunterTriggerTaken
def CreateMadHunter
