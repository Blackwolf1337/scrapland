# The module code of NDeathMatchBots (Data.packed/scripts/net/ndeathmatchbots.py), written by hand from the source:
# the packs ship no .pyc of it, so the original game cannot load the module; ScrapC makes it from this file (its
# functions are C, src/game/Script/NetModes.c) when PreloadLibrary asks for it.
# A pack in mods/ with scrapc/modules/ndeathmatchbots.mod replaces this file.

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
let MultiplayerMenuOnGame = [Scrap.GetLangStr('Net_Menu_Spectate'), 'Ffa.Spectate']
let MultiplayerMenuSpectator = [Scrap.GetLangStr('Net_Menu_Join'), 'Ffa.Spectate']
let TeamGame = 0
let BotsList = [('[Rouco]', 'SArchbishop1', 'MegaAce'), ('[PedroJ]', 'SHump1', 'MegaAce'), ('[Botin]', 'SBankMaster1', 'MegaAce')]
def DeathFunc
def Reborn
def CheckEndOfGame
def GetGameDescriptor
def TryGarage
def SetBotState
def CheckBotState
def InitServerMod
def InitClientMod
def ShowHelp
def ServerHidePlayer
