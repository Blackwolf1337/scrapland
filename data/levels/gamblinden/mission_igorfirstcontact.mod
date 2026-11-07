# The module code of Mission_IgorFirstContact in level gamblinden (data03.packed/levels/gamblinden/scripts/mission_igorfirstcontact.pyc): its imports, variables, functions and load time calls in the order of the
# script; ScrapC makes the module from this file instead of running the .pyc. Made by tools/modules/pyc2mod.py.
# A pack in mods/ with scrapc/levels/gamblinden/mission_igorfirstcontact.mod replaces this file.

import Scrap
import SInput
import SNet
import SWeap
import SScorer
import SSound
import SVec
import SAct
import sys
import CharsNPC
import CharAct
import MissionsFuncs
import Speech
import CharConversor
import Items
import string
import SaveGame
import Init
import Scorer
let cg = None
let igor = None
let mercA = None
let mercB = None
let mercC = None
let alreadyTalked = 0
call Scrap.CreateSaveVar('IgorFirstContactMissionState', 'TalkToCrazyGambler')
def InitStatus
def Init
def TalkToCrazyGambler
def CrazyGamblerSpeech
def TalkToIgor
def IgorSpeech
def TalkToMercenaries
def MercenaryASpeech
def MercenaryCSpeech
def MercenaryBSpeech
def SetNextMission
