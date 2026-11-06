# The module code of Mission_ArchbishopDead in level temple (data02.packed/levels/temple/scripts/mission_archbishopdead.pyc): its imports, variables, functions and load time calls in the order of the
# script; ScrapC makes the module from this file instead of running the .pyc. Made by tools/modules/pyc2mod.py.
# A pack in mods/ with scrapc/levels/temple/mission_archbishopdead.mod replaces this file.

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
import Menu
let NumProofsWithPhoto = 0
let NextMissionMarked = 0
let ProofsNames = []
call Scrap.CreateSaveVar('Mission_ArchbishopDead.Status', 'Photo')
def InitStatus
def Init
def ForbiddenMsg
def CreateViscProofs
def ViewProof
def TakeProofPhoto
def ShowDeepThroatMessage
def AfterDeepThroatMessageShown
