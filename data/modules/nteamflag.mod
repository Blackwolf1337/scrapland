# The module code of nteamflag (Data.packed/scripts/net/nteamflag.pyc): its imports, variables, functions and load time calls in the order of the
# script; ScrapC makes the module from this file instead of running the .pyc. Made by tools/modules/pyc2mod.py.
# A pack in mods/ with scrapc/modules/nteamflag.mod replaces this file.

import Scrap
import SInput
import SNet
import SWeap
import SScorer
import SSound
import SAct
import sys
import SAI
import SFX
let MultiplayerMenuOnGame = [Scrap.GetLangStr('Net_Menu_Game'), 'Ffa.TeamMenu']
let MultiplayerMenuSpectator = [Scrap.GetLangStr('Net_Menu_Join_Team'), 'Ffa.TeamMenu']
let TeamGame = 1
def DeathFunc
def CheckEndOfGame
def GetGameDescriptor
def InitServerMod
def InitClientMod
def ShowHelp
def ServerHidePlayer
let FlagModel = 'models/Misc/Flag/Flag.m3d'
let ZoneList = {}
let Flag = None
let AcFlag = None
def GetRndFlag
def BrowseAreas
def CreateFlag
def TakeFlag
def OnUsrClient
def ClientJoin
def ServerTakeFlag
