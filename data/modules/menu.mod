# The module code of Menu (Data.packed/scripts/scorer/menu.pyc): its imports, variables, functions and load time calls in the order of the
# script; ScrapC makes the module from this file instead of running the .pyc. Made by tools/modules/pyc2mod.py.
# A pack in mods/ with scrapc/modules/menu.mod replaces this file.

import Scrap
import SInput
import SNet
import SWeap
import SScorer
import SVec
import SSound
if (Scrap.Get('IsXbox') == 1)
	let MLib = __import__('XBMenu')
else
	let MLib = __import__('PCMenu')
end
let OptionsMenu = MLib.OptionsMenu
let LastMode = 'Menu'
let LastTimeSpeed = 0
let MissionScorerCall = None
let Border = 2.1
let saveGamesList = []
let saveGamesSlots = 6
let slotToOverwrite = (-1)
let fromSinglePlayer = 0
let fromMultiPlayer = 0
let MainMenuYStart = 244
let SubMenuYStart = 214
let OptionMenuYStart = 150
let BackButtonYStart = 430
let QuestionMenuYStart = 230
let TitleBarYOff = (-2)
let TabQuadSize = 14
let isXbox = Scrap.Get('isXbox')
if isXbox
	let UseTxt = (Scrap.GetLangStr('XBox_Press_Button') % '\024')
else
	let UseTxt = Scrap.GetLangStr('PC_Menu_LClick')
end
def LinkLR
def LinkUD
def UnLinkLR
def UnLinkUD
def DrawBackMainMenu
def DrawBackSubMenu
def DrawBackOptionMenu
def DrawCircuitMenu
def DrawMenuTitleBar
def GameOver
let currentSoundPitch = None
def StartNewMenu
def AddTabs
def DummyFunc
def isAutoSave
def ComposeSaveGameName
def VerticalMenu
def YesNoMenu
def DeathInfo
def ExitGame
def ByeMenu
def DisableMenuItem
def SinglePlayerMenu
def AudioMenu
def AudioVolumeSliderFocus_Sub
def AudioVolumeSliderFocus_Add
def AudioVolumeChange
def MainMenu
def CreateMainMenu
def PlaySpacelords
let onCreateMainMenu = CreateMainMenu
def BackToGame
def DrawScorer
def InitMenuSys
let MovieScorerCall = None
def MovieScorer
def Init
let OutHeadMessenger = 0
let HeadBoss = 0
def Initialize
def SetSkillMenu
def SetSkill
def StartNewGame
def NewGame
def AuxAbortGame
def AbortGame
def AuxLoadGame
def LoadGame
def LoadLastSavedGame
def AuxSaveGame
def SaveGame
def LoadGameMenu
def SaveGameMenu
def EnterMissionInfo
def GoPrimaryMission
def GoCrazyDeal
def GoNotes
def MenuObjetive
def CrazyDealObjetive
def MissionNotesText
def MenuMissionInfo_Left
def MenuMissionInfo_Right
def MenuMissionInfo
def ExitMissionInfo
let backFunc = ''
def ShowWarning
def ShowTextBox
def SetTabDefaults
def SliderMenu
def PreviousNextMenu
def PreviousNextMenuSimple
def TVOff
def SetCam
def TVOffEnd
def BlackTV
def CreditsMenu
def DebugMenu
def DebugSet_Level
def DebugSet_Console
def DebugSet_RenderInfo
def DebugSet_SoundInfo
def DebugSet_WorldInfo
def DebugSet_ObjectPool
def DebugSet_SceneRenderTarget
def DebugSet_StencilShadows
def DebugSet_AI
def DebugSet_MapCollision
def DebugSet_XboxPyDebug
def DebugSet_CycleVar
def DebugSet_ChangeMapMenu
def DebugSet_ChangeMapExec
