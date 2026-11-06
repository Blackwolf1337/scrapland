# The module code of WeaponsTab (Data.packed/scripts/shipedit/weaponstab.pyc): its imports, variables, functions and load time calls in the order of the
# script; ScrapC makes the module from this file instead of running the .pyc. Made by tools/modules/pyc2mod.py.
# A pack in mods/ with scrapc/modules/weaponstab.mod replaces this file.

import SScorer
import Scrap
import SSound
let NormalColorAlpha = 255
let NormalColorRed = 115
let NormalColorGreen = 150
let NormalColorBlue = 255
let SelectedColorAlpha = 255
let SelectedColorRed = 250
let SelectedColorGreen = 230
let SelectedColorBlue = 90
let UpgSelectedColorRed = 250
let UpgSelectedColorGreen = 140
let UpgSelectedColorBlue = 60
let NotAvailableColorAlpha = 222
let NotAvailableColorRed = 8
let NotAvailableColorGreen = 8
let NotAvailableColorBlue = 8
let HighLightColorRed = 255
let HighLightColorGreen = 255
let HighLightColorBlue = 190
let TabQuadSize = 14
let controlsToLinkList = []
let prevWeapon = ''
let slotId = ('A', 'B', 'C', 'D')
let weapId = {'Vulcan': '00', 'Devastator': '03', 'Tesla': '02', 'ATPC': '04', 'Swarm': '01', 'Inferno': '05'}
let upgradeId = {
	"'Rate'": '1',
	"'Accuracy'": '2',
	"'Range'": '7',
	"'Dispersion'": '8',
	"'TeslaUpgrade1'": '5',
	"'TeslaUpgrade2'": '6',
	"'ATPCUpgrade1'": '9',
	"'ATPCUpgrade2'": '10',
	"'InfernoUpgrade1'": '11',
	"'InfernoUpgrade2'": '12',
	"'Speed'": '3',
	"'Swerve'": '4'
}
let auxUpgradeId = {'Vulcan': '1', 'Devastator': '7', 'Tesla': '5', 'ATPC': '9', 'Inferno': '11', 'Swarm': '3'}
let auxUpgradeId2 = {
	'VulcanUpgrade1': "'Rate'",
	'VulcanUpgrade2': "'Accuracy'",
	'DevastatorUpgrade1': "'Range'",
	'DevastatorUpgrade2': "'Dispersion'",
	'TeslaUpgrade1': "'TeslaUpgrade1'",
	'TeslaUpgrade2': "'TeslaUpgrade2'",
	'ATPCUpgrade1': "'ATPCUpgrade1'",
	'ATPCUpgrade2': "'ATPCUpgrade2'",
	'InfernoUpgrade1': "'InfernoUpgrade1'",
	'InfernoUpgrade2': "'InfernoUpgrade2'",
	'SwarmUpgrade1': "'Speed'",
	'SwarmUpgrade2': "'Swerve'"
}
let isXbox = Scrap.Get('isXbox')
if isXbox
	let UseTxt = (Scrap.GetLangStr('XBox_Press_Button') % '\024')
else
	let UseTxt = Scrap.GetLangStr('PC_Menu_LClick')
end
def DoNothing
def ShowWeightPrediction
def HideWeightPrediction
def ShowWeaponDesc
def HideWeaponDesc
def ShowWeaponUpgradeDesc
def HideWeaponUpgradeDesc
def ShowSlotMarker
def HideSlotMarker
def LoadWeapon
def HideSlots
def FillSlots
def UpdateUpgrades
def SelectWeapon
def SelectUpgrade
def SetWeaponsPanel
def ShowWeaponsPanel
def HideWeaponsPanel
