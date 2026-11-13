# The module code of Init (Data.packed/scripts/init.pyc): its imports, variables, functions and load time calls in the order of the
# script; ScrapC makes the module from this file instead of running the .pyc. Made by tools/modules/pyc2mod.py.
# A pack in mods/ with scrapc/modules/init.mod replaces this file.

import Scrap
let PlaySpacelords = (not Scrap.IsApp(436180))
import SInput
import SNet
import SWeap
import SScorer
import SSound
import SAct
import sys
import SAI
import SFX
call Scrap.CreateSaveVar('CreateBertos', '0')
call Scrap.CreateSaveVar('MercsHelpDtritus', '0')
call Scrap.CreateSaveVar('RobotsControlledByBoss', '0')
call Scrap.CreateSaveVar('Player.InfiniteLives', '1')
let SUPERDEALSMAPS = ['TUCZONE', 'QF', 'OM', 'FZ', 'CA']
let OUTMAPS = (['TUCZONE', 'AB', 'COMMERCIAL', 'DOWNTOWN', 'GDB', 'OUTSKIRTS', 'SCRAPYARD'] + SUPERDEALSMAPS)
let INMAPS = ['NANO', 'ABINDOOR', 'BANK', 'GAMBLINDEN', 'ORBIT', 'POLICE', 'PRESS', 'TEMPLE', 'TOWNHALL']
let ExtraExecs = {
	'JUNKYARD': ((Scrap.PreloadLibrary, ('RustySound', 'Sound/RustySound.pyc')), (Scrap.PreloadLibrary, ('SputnikJunkyardSound', 'Sound/SputnikJunkyardSound.pyc')), (Scrap.PreloadLibrary, ('MaintenanceSound', 'Sound/MaintenanceSound.pyc')), (Scrap.PreloadLibrary, ('Rusty', 'Chars/Rusty.pyc')), (Scrap.PreloadLibrary, ('SputnikJunkyard', 'Chars/SputnikJunkyard.pyc')), (Scrap.PreloadLibrary, ('Maintenance', 'Chars/Maintenance.pyc'))),
	'GAMBLINDEN': ((Scrap.PreloadLibrary, ('MercenarySound', 'Sound/MercenarySound.pyc')), (Scrap.PreloadLibrary, ('CrazyGamblerSound', 'Sound/CrazyGamblerSound.pyc')), (Scrap.PreloadLibrary, ('MercenaryA', 'Chars/MercenaryA.pyc')), (Scrap.PreloadLibrary, ('MercenaryB', 'Chars/MercenaryB.pyc')), (Scrap.PreloadLibrary, ('MercenaryC', 'Chars/MercenaryC.pyc')), (Scrap.PreloadLibrary, ('MercenaryL', 'Chars/MercenaryL.pyc')), (Scrap.PreloadLibrary, ('CrazyGambler', 'Chars/CrazyGambler.pyc'))),
	'ABINDOOR': ((Scrap.PreloadLibrary, ('CrazyGamblerSound', 'Sound/CrazyGamblerSound.pyc')), (Scrap.PreloadLibrary, ('CrazyGambler', 'Chars/CrazyGambler.pyc'))),
	'PRESS': ((Scrap.PreloadLibrary, ('SebastianSound', 'Sound/SebastianSound.pyc')), (Scrap.PreloadLibrary, ('BossSound', 'Sound/BossSound.pyc')), (Scrap.PreloadLibrary, ('Sebastian', 'Chars/Sebastian.pyc')), (Scrap.PreloadLibrary, ('Boss', 'Chars/Boss.pyc')))
}
let OffLineGame = (Scrap.GetNetFlags() == (0, 0, 0))
let MultiPlayer = ((not OffLineGame) or (Scrap.Get('SplitScreen') != 0))
call sys.path.append('Scripts/Scorer')
call sys.path.append('Scripts/ShipEdit')
call sys.path.append('Scripts/Python')
call sys.path.append('Scripts/Weapons')
call sys.path.append('Scripts/Vehicles')
call sys.path.append('Scripts/Net')
call sys.path.append('Scripts/FX')
call sys.path.append('Scripts/Sound')
call sys.path.append('Scripts/Chars')
call sys.path.append('Scripts/Missions')
call sys.path.append('Scripts/SuperDeals')
call sys.path.append('Scripts/Split')
call Scrap.PreloadLibrary('string', 'Python/string.pyc')
import string
let UpperMap = string.upper(Scrap.GetLevelPath()[7:])
let SuperDealModule = None
let sDealType = Scrap.GetSaveVar('SuperDealType')
let HasSuperDeal = (OffLineGame and (sDealType and ((sDealType != '') and (UpperMap in SUPERDEALSMAPS))))
let OutMapSkip = (OffLineGame and (UpperMap in OUTMAPS))
let InMapSkip = (OffLineGame and (UpperMap in INMAPS))
call Scrap.PreloadLibrary('Items', 'Items.pyc')
call Scrap.PreloadLibrary('SaveGame', 'SaveGame.pyc')
if (Scrap.Get('IsXbox') == 1)
	call Scrap.PreloadLibrary('XBMenu', 'Scorer/XBMenu.pyc')
else
	call Scrap.PreloadLibrary('PCMenu', 'Scorer/PCMenu.pyc')
end
call Scrap.PreloadLibrary('Menu', 'Scorer/Menu.pyc')
call Scrap.PreloadLibrary('Scorer', 'Scorer/Scorer.pyc')
call Scrap.PreloadLibrary('CharScorer', 'Scorer/CharScorer.pyc')
call Scrap.PreloadLibrary('Net', 'Net/Net.pyc')
call Scrap.PreloadLibrary('Fx', 'FX/Fx.pyc')
call Scrap.PreloadLibrary('Sound', 'Sound/Sound.pyc')
call Scrap.PreloadLibrary('DoorSound', 'Sound/DoorSound.pyc')
call Scrap.PreloadLibrary('Chars', 'Chars/Chars.pyc')
call Scrap.PreloadLibrary('MakeChar', 'Chars/MakeChar.pyc')
call Scrap.PreloadLibrary('Speech', 'Chars/Speech.pyc')
call Scrap.PreloadLibrary('CharAct', 'Chars/CharAct.pyc')
call Scrap.PreloadLibrary('Teleport', 'Missions/Teleport.pyc')
call Scrap.PreloadLibrary('Elevator', 'Missions/Elevator.pyc')
call Scrap.PreloadLibrary('RaceMaker', 'Missions/RaceMaker.pyc')
call Scrap.PreloadLibrary('Doors', 'Missions/Doors.pyc')
call Scrap.PreloadLibrary('Alarm', 'Missions/Alarm.pyc')
call Scrap.PreloadLibrary('MisItems', 'Missions/MisItems.pyc')
call Scrap.PreloadLibrary('Traffic', 'Missions/Traffic.pyc')
call Scrap.PreloadLibrary('Scene', 'Missions/Scene.pyc')
call Scrap.PreloadLibrary('Challenge', 'Missions/Challenge.pyc')
call Scrap.PreloadLibrary('MissionsFuncs', 'Missions/MissionsFuncs.pyc')
call Scrap.PreloadLibrary('Metro', 'Missions/Metro.pyc')
def LoadCharLib
if (not InMapSkip)
	call Scrap.PreloadLibrary('Weapons', 'Weapons/Weapons.pyc')
	call Scrap.PreloadLibrary('Vulcan', 'Weapons/Vulcan.pyc')
	call Scrap.PreloadLibrary('Swarm', 'Weapons/Swarm.pyc')
	call Scrap.PreloadLibrary('Tesla', 'Weapons/Tesla.pyc')
	call Scrap.PreloadLibrary('Cloud', 'Weapons/Cloud.pyc')
	call Scrap.PreloadLibrary('Sonic', 'Weapons/Sonic.pyc')
	call Scrap.PreloadLibrary('EMI', 'Weapons/EMI.pyc')
	call Scrap.PreloadLibrary('Devastator', 'Weapons/Devastator.pyc')
	call Scrap.PreloadLibrary('ATPC', 'Weapons/ATPC.pyc')
	call Scrap.PreloadLibrary('Inferno', 'Weapons/Inferno.pyc')
	call Scrap.PreloadLibrary('Laser', 'Weapons/Laser.pyc')
	call Scrap.PreloadLibrary('Vehicles', 'Vehicles/Vehicles.pyc')
	call Scrap.PreloadLibrary('ShipMaker', 'Vehicles/ShipMaker.pyc')
	call Scrap.PreloadLibrary('OutMap', 'Vehicles/OutMap.pyc')
	call Scrap.PreloadLibrary('VehiclesNPC', 'Vehicles/VehiclesNPC.pyc')
	call Scrap.PreloadLibrary('Parking', 'Vehicles/Parking.pyc')
	call Scrap.PreloadLibrary('Accelerator', 'Vehicles/Accelerator.pyc')
	call Scrap.PreloadLibrary('RacerScorer', 'Scorer/RacerScorer.pyc')
	call LoadCharLib('Sputnik')
	call Scrap.PreloadLibrary('SputnikInterface', 'ShipEdit/SputnikInterface.pyc')
	call Scrap.PreloadLibrary('OutSound', 'Sound/OutSound.pyc')
	call Scrap.PreloadLibrary('OutPolice', 'Missions/OutPolice.pyc')
	call Scrap.PreloadLibrary('PhoneCab', 'Missions/PhoneCab.pyc')
	call Scrap.PreloadLibrary('ShipEdit', 'ShipEdit/ShipEdit.pyc')
	call Scrap.PreloadLibrary('HangarTab', 'ShipEdit/HangarTab.pyc')
	if (not OutMapSkip)
		call Scrap.PreloadLibrary('EnginesTab', 'ShipEdit/EnginesTab.pyc')
		call Scrap.PreloadLibrary('WeaponsTab', 'ShipEdit/WeaponsTab.pyc')
		call Scrap.PreloadLibrary('HullTab', 'ShipEdit/HullTab.pyc')
	end
end
if (not OutMapSkip)
	c LoadChars
	call Scrap.PreloadLibrary('CrazyWing', 'Missions/CrazyWing.pyc')
	call Scrap.PreloadLibrary('Bureaucracy', 'Chars/Bureaucracy.pyc')
	call Scrap.PreloadLibrary('CharsNPC', 'Chars/CharsNPC.pyc')
	call Scrap.PreloadLibrary('InMap', 'Chars/InMap.pyc')
	call Scrap.PreloadLibrary('InTraffic', 'Vehicles/InTraffic.pyc')
	call Scrap.PreloadLibrary('DTritusDesktop', 'Missions/DTritusDesktop.pyc')
	call Scrap.PreloadLibrary('InPolice', 'Missions/InPolice.pyc')
	call Scrap.PreloadLibrary('CharConversor', 'Missions/CharConversor.pyc')
	call Scrap.PreloadLibrary('NewsPanel', 'Missions/NewsPanel.pyc')
else
	call LoadCharLib(Scrap.GetSaveVar('Char'))
end
if ((Scrap.Get('SplitScreen') != 0) or (Scrap.Get('isXbox') and (UpperMap in ('MENU', 'SHIPEDIT'))))
	call Scrap.PreloadLibrary('Split', 'Split/Split.pyc')
	let SplitModeGameMod = Scrap.Get('ServerType')
	call Scrap.PreloadLibrary(('S' + SplitModeGameMod), (('Split/S' + SplitModeGameMod) + '.pyc'))
	import Split
	set Split.ModLib = __import__(('S' + SplitModeGameMod))
end
if (not OffLineGame)
	call Scrap.PreloadLibrary('Chat', 'Scorer/Chat.pyc')
	call Scrap.PreloadLibrary('Ffa', 'Net/Ffa.pyc')
	import Ffa
	let GameLibName = ('N' + Scrap.Get('ServerType'))
	if ((not Scrap.FileExist((('Scripts/Net/' + GameLibName) + '.pyc'))) and (not Scrap.FileExist((('Scripts/Net/' + GameLibName) + '.py'))))
		let GameLibName = 'NDeathMatch'
	end
	call Scrap.PreloadLibrary(GameLibName, (('Net/' + GameLibName) + '.pyc'))
	set Ffa.ModLib = __import__(GameLibName)
end
if ExtraExecs.has_key(UpperMap)
	c ExtraExecs
end
let isDemo = Scrap.Get('isDemo')
let Path = ''
let Outdoor = 1
let inMainMenu = 0
let isShipEdit = 0
let isJunkYard = 0
let OutArea = 'DownTown'
let isScrapYard = 0
let XboxSavingMsgTime = 3.5
if HasSuperDeal
	call Scrap.PreloadLibrary(sDealType, (('SuperDeals/' + sDealType) + '.pyc'))
	let SuperDealModule = __import__(sDealType)
end
let MapLibs = []
def AddMLib
def MLibInitStatus
def MLibInit
def isMission
def PreloadMlibs
let Arrows = (
	'Models/Misc/Action/Action',
	'Models/Misc/MMission/MMission',
	'Models/Misc/SMission/SMission',
	'Models/Misc/MMission/BMMission',
	'Models/Misc/SMission/BSMission'
)
call Scrap.Set('ArrowUseFile', Arrows[0])
call Scrap.Set('ArrowMissionFile', Arrows[1])
call Scrap.Set('ArrowTargetFile', Arrows[2])
def SetArrowSize
def Init
def SetMapLabelText
let PROGRESSBAR_PC = {
	'MENU': 200,
	'ORBIT': 1070,
	'SCRAPYARD': 1693,
	'BANK': 1641,
	'JUNKYARD': 1434,
	'SHIPEDIT': 1265,
	'DOWNTOWN': 1745,
	'OUTSKIRTS': 1775,
	'GAMBLINDEN': 1252,
	'COMMERCIAL': 1790,
	'FAKE': 1462,
	'PRESS': 1691,
	'POLICE': 1764,
	'TEMPLE': 1382,
	'TOWNHALL': 1742,
	'AB': 1366,
	'ABINDOOR': 868,
	'QF': 1474,
	'GDB': 1517,
	'FZ': 1488,
	'OM': 1537,
	'CA': 1487,
	'MCA': 1553,
	'MCB': 1553,
	'MDA': 1553,
	'MDB': 1553,
	'MOA': 1553,
	'MOB': 1553,
	'MSA': 1553,
	'MSB': 1553
}
let PROGRESSBAR_XBOX = {
	'MENU': 200,
	'ORBIT': 845,
	'SCRAPYARD': 1189,
	'BANK': 1327,
	'JUNKYARD': 990,
	'SHIPEDIT': 589,
	'DOWNTOWN': 1208,
	'OUTSKIRTS': 1182,
	'GAMBLINDEN': 995,
	'COMMERCIAL': 1214,
	'FAKE': 1208,
	'PRESS': 1448,
	'POLICE': 1533,
	'TEMPLE': 1162,
	'TOWNHALL': 1539,
	'AB': 844,
	'ABINDOOR': 690,
	'QF': 968,
	'GDB': 964,
	'FZ': 975,
	'OM': 1014,
	'CA': 953,
	'MCA': 1042,
	'MCB': 1016,
	'MDA': 1046,
	'MDB': 1048,
	'MOA': 945,
	'MOB': 985,
	'MSA': 987,
	'MSB': 857
}
def Close
