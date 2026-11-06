# The module code of nteampost (Data.packed/scripts/net/nteampost.pyc): its imports, variables, functions and load time calls in the order of the
# script; ScrapC makes the module from this file instead of running the .pyc. Made by tools/modules/pyc2mod.py.
# A pack in mods/ with scrapc/modules/nteampost.mod replaces this file.

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
let AUTO_RESPAWN_FLAG_TIME = 60.0
def DeathFunc
def CheckEndOfGame
def GetGameDescriptor
def InitServerMod
def InitClientMod
def ShowHelp
def ServerHidePlayer
let FlagModel = 'models/Misc/Flag/Flag.m3d'
let FlagPilotModel = 'models/Misc/Flag/FlagPl.m3d'
let FlagNoUsrModel = 'models/Misc/Flag/FlagUs.m3d'
let Flag = Scrap.CreateEntity('FlagToBeHunted', 0, 0, 0, 'Item')
let FlagSpawnPos = None
let ZoneList = {}
let AcZone = ''
def GetRndZone
def BrowseAreas
def CreateFlag
def LinkFlagTo
def TryLinkFlagTo
def TakeFlag
def DropFlag
def AbortZoneKeys
def OnUsrClient
def ClientJoin
def ServerTakeBase
def ServerDropFlag
def ServerTakeFlag
def RespawnFlag
