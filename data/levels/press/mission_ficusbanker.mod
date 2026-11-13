# The module code of Mission_FicusBanker in level press (data01.packed/levels/press/scripts/mission_ficusbanker.pyc): its imports, variables, functions and load time calls in the order of the
# script; ScrapC makes the module from this file instead of running the .pyc. Made by tools/modules/pyc2mod.py.
# A pack in mods/ with scrapc/levels/press/mission_ficusbanker.mod replaces this file.

let NextMissionData = ('Mission_MultiplusRace', 'DownTown')
import Scrap
import SInput
import SNet
import SWeap
import SScorer
import SAI
import SVec
import SAct
import SSound
import CharsNPC
import MissionsFuncs
import Speech
import CharAct
import CharConversor
import SaveGame
call Scrap.CreateSaveVar('Mission_Police_FicusBanker', 'TalkFicus')
call Scrap.CreateSaveVar('CheckPos', 'None')
def InitStatus
let Model_Check = 'models/Misc/Check/Check.m3d'
def Init
def CreatePoliceFicusInit
let Firstone = 0
def FicusMissionSpeech
def CreateBankDirectorScrapusInit
def ScrapusMissionSpeech
def InitKillScrapus
def SpawnCheck
def TakeCheck
def InitTalkFicus2
def SetNextMission
