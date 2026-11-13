# The module code of Speech (Data.packed/scripts/chars/speech.pyc): its imports, variables, functions and load time calls in the order of the
# script; ScrapC makes the module from this file instead of running the .pyc. Made by tools/modules/pyc2mod.py.
# A pack in mods/ with scrapc/modules/speech.mod replaces this file.

import Scrap
import SInput
import SNet
import SWeap
import SScorer
import SSound
import SVec
import SAct
import SAI
import sys
let PlayerName = ''
let NPCName = ''
let TextColor = (174, 229, 251)
let DefTextColor = (174, 229, 251)
let UsrTextColor = (154, 251, 209)
let NPCAnswer = ''
let NPCAnswerTime = 6
let OnEndOfSpeech = None
let OnEndOfSpeechParams = ()
let AnsAnima = 'TalkNPC'
let LockAlarm = 0
let ArrowCanMission = 0
let TalkListen = 'TalkListen'
let LaunchedTauntEntityName = ''
let CanLaunchRemote = 1
let VoiceShTime = 0.25
def ScheduledVoiceCallback
def PlayVoiceScheduled
def NISpeech
def AutoAddNI
def AddNI
def RunNI
def ObjNIEnd
def PerformAnswer
def NormalAbortSpeech
def CallEndOfSpeech
def AbortSpeech
def RestoreState
def AbortNPCSpeech
def AutoAnswer
def StopMenuEvents
def RestartMenuEvents
def RestoreMoveTime
def StdAnsLst
def StdOnSpch
def CalculateSpeechTime
def OnSpch
def OnChallengeSpch
def OnSpeech
def PlayerAnsEnd
def PlayerAns
def SetPlayerToTalk
let prevTaunt = ''
def LaunchTaunt
def RestoreSpeech
def DoRemote
def OffVoice
def RemoteComunication
def ForceRemoteComInternal
def ForceRemoteCom
let HeadDict = {
	'Ficus': 'Police',
	'Multiplus': 'Police',
	'LoneMerc': 'MercenaryA',
	'Traitorius': 'Functionary',
	'Pickus': 'Functionary',
	'Stupidus': 'Functionary',
	'Rudolph': 'Functionary',
	'Mickus': 'Functionary',
	'Corruptus': 'Functionary',
	'Trurl': 'Messenger',
	'Igor': 'Bishop',
	'Gus': 'Bishop',
	'Porkins': 'Bishop',
	'Scrapus': 'Banker'
}
def ForceRemoteComunication
let RemoteCom = []
let CurrentRemoteCom = ''
def AddRemoteCom
def ProcessRemoteCom
def CancelRemoteCom
let SpeechRoutines = {}
def InspectChar
def AutoSpeech
let ObjEntity = None
let ObjAnswer = None
let ObjCharType = None
let ObjAutoAnswerAction = None
let ObjAutoAnswerDefLoop = None
let OnOutOfRange = None
def TestInRange
def StartObjSpch
def StartObjectSpeech
def StdObjSpch
def OnObjSpch
def OnChallengeObjSpch
def OnObjectSpeech
def ObjectAutoAnswer
def ObjectAutoAnswer2
def EndObjectAnswer
