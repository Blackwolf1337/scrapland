# The module code of Ffa (Data.packed/scripts/net/ffa.pyc): its imports, variables, functions and load time calls in the order of the
# script; ScrapC makes the module from this file instead of running the .pyc. Made by tools/modules/pyc2mod.py.
# A pack in mods/ with scrapc/modules/ffa.mod replaces this file.

import Scrap
import SInput
import SNet
import SWeap
import SScorer
import SVec
let Scorer = {}
let isScorerVisible = 0
let TotalRed = 0
let TotalGreen = 0
let ModLib = None
let SpectatorMode = 0
def InitFfa
def SetupUserShip
def RestartOnPress
def DelayedRestartOnPress
def OnDeath
let GameFinished = 0
def ShowPresFireMessage
def DelayedRestartOnTime
def EndOfGame
def ServerSendPlayerStatus
def ServerSendTeamData
def ServerEndOfGame
def OnDeathServer
def OnDeathClient
def Scorer2Str
def Str2Scorer
def ClientJoin
def BuildCfgString
def ModiTry
def ModifyOnReyoin
def ClientModify
def TryToModify
def PlayerMsgShow
def PlayerDisconected
def PlayerSpectator
def PlayerJoinGame
def ClientExit
def ReSpawnClient
def ServerRemoveSpectatorState
def SetSpawnPos
def InitServer
def ClientSetConfig
def ClientAfterCreate
def ClientCreate
def OnUsrTest
def SvrShowMsg
def OnUsrClient
def OnUsrServer
def Spectate
def JoinRed
def JoinGreen
def ReJoinGame
def RejoinMsg
def JoinGame
def TeamMenu
def GetGameTeamDescriptor
def CameraSpectator
def AbortSpectateMode
def ServerSpectate
def InitClient
def StdInitFfa
let USRCOLOR = (255, 255, 0)
let STDCOLOR = (255, 255, 255)
let REDCOLOR = (255, 0, 0, 128)
let GREENCOLOR = (0, 128, 0, 128)
let MasterFlag = None
def GetScorerInfo
let GameScorer = []
let YourPosition = 1
let YourScore = 0
let Winner = 0
let ScorerFraglimit = Scrap.Get('FragLimit')
def OnPositionChange
def isTied
let GameStart = 0
def CheckGameStart
def UsrPos
def ActualizeGameScorer
def ShowScorer
def BackMenuFunc
def ShowHelp
def SetupGameRules
def SendGameRules
def SetScorerValues
def FfaScorer
