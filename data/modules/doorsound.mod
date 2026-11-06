# The module code of DoorSound (Data.packed/scripts/sound/doorsound.pyc): its imports, variables, functions and load time calls in the order of the
# script; ScrapC makes the module from this file instead of running the .pyc. Made by tools/modules/pyc2mod.py.
# A pack in mods/ with scrapc/modules/doorsound.mod replaces this file.

import Scrap
import SInput
import SNet
import SWeap
import SScorer
import SSound
import SAct
import sys
let SoundList = {
	'GAMBLINDEN': ['CloseMechDoor', 'OpenMechDoor', 'apertura2', 'placas-metal22', 'electricidad333', 'desaparicion', 'desaparicion4', 'desaparicion3', 'desaparicion5', 'electricidad344', 'transporter3', 'transporter4'],
	'BANK': ['puerta3', 'llegada-metroB', 'llegada-metro22', 'cierre-viscoso', 'puerta11', 'cierre2', 'cajon-open', 'cajon-close', 'puerta1', 'electricidad333', 'desaparicion', 'desaparicion4', 'desaparicion3', 'desaparicion5', 'electricidad344', 'CloseMechDoor', 'OpenMechDoor', 'placas-metal22', 'apertura2', 'transporter3', 'transporter4', 'metro-open', 'cierre-metro1', 'clik-data', 'clik-data2'],
	'ORBIT': ['cuchilla1', 'up1', 'ascensor-orbit2', 'cuchilla2', 'clik-mecanismo1', 'subida-ascensor2', 'bajada-ascensor2', 'cierre1', 'placas-metal22', 'apertura2', 'transporter3', 'transporter4', 'CloseMechDoor', 'OpenMechDoor'],
	'TOWNHALL': ['apertura2', 'llegada-metroB', 'puerta1', 'llegada-metro22', 'OpenMechDoor', 'CloseMechDoor', 'apertura4', 'apertura5', 'clik-mecanismo2', 'clik-mecanismo3', 'clik-mecanismo5', 'clik-mecanismo7', 'cierre-10', 'apertura2', 'CloseMechDoor', 'OpenMechDoor', 'placas-metal22', 'transporter3', 'transporter4', 'electricidad333', 'desaparicion', 'desaparicion4', 'desaparicion3', 'desaparicion5', 'electricidad344', 'clik-data', 'clik-data2', 'metro-open', 'cierre-metro1'],
	'PRESS': ['puerta-emero-open', 'puerta-emero-close', 'cierre-emero', 'puerta-security-open', 'puerta-security-close', 'metro-open', 'apertura2', 'cierre-metro1', 'llegada-metro22', 'puerta1', 'llegada-metroB', 'cuchilla1', 'cuchilla2', 'abre-stairs', 'cierre-stairs', 'ascensor-orbit1', 'ascensor-orbit2', 'apertura4', 'apertura5', 'clik-data', 'clik-data2', 'CloseMechDoor', 'OpenMechDoor', 'electricidad333', 'desaparicion', 'desaparicion4', 'desaparicion3', 'desaparicion5', 'electricidad344', 'placas-metal22', 'clik-data', 'clik-data2', 'transporter3', 'transporter4'],
	'TEMPLE': ['baldosa7', 'metro-open', 'apertura2', 'cierre-metro2', 'placas-metal22', 'puerta1', 'llegada-metro22', 'transporter3', 'transporter4', 'apertura2', 'electricidad333', 'llegada-metroB', 'desaparicion', 'desaparicion4', 'desaparicion3', 'desaparicion5', 'electricidad344', 'OpenMechDoor', 'CloseMechDoor', 'clik-data', 'clik-data2', 'transporter3', 'transporter4'],
	'POLICE': ['ascensor-orbit1', 'ascensor-orbit2', 'apertura4', 'gas2', 'placas-metal22', 'puerta1', 'transporter3', 'transporter4', 'apertura2', 'apertura5', 'llegada-metro22', 'cierre-10', 'cierre-100', 'puerta3', 'llegada-metroB', 'cierre-emero', 'puerta-jail', 'cierre2', 'cierre-viscoso', 'clik-mecanismo2', 'viscoso-open', 'puerta4', 'tritura2', 'baldosa7', 'clik-data', 'clik-data2', 'electricidad333', 'desaparicion', 'desaparicion4', 'desaparicion3', 'desaparicion5', 'electricidad344', 'OpenMechDoor', 'CloseMechDoor', 'metro-open', 'cierre-metro1'],
	'JUNKYARD': ['cuchilla1111D', 'cuchilla2222D1', 'gas2', 'up11', 'up22', 'up33', 'cierre-100'],
	'MENU': [],
	'FAKE': [],
	'AB': [],
	'CA': ['cuchilla1111D', 'cuchilla2222D1'],
	'FZ': ['cuchilla1111D', 'cuchilla2222D1'],
	'MCA': ['cuchilla1111D', 'cuchilla2222D1'],
	'MCB': ['cuchilla1111D', 'cuchilla2222D1'],
	'MDA': ['cuchilla1111D', 'cuchilla2222D1'],
	'MDB': ['cuchilla1111D', 'cuchilla2222D1'],
	'MOA': ['cuchilla1111D', 'cuchilla2222D1'],
	'MOB': ['cuchilla1111D', 'cuchilla2222D1'],
	'MSA': ['cuchilla1111D', 'cuchilla2222D1'],
	'MSB': ['cuchilla1111D', 'cuchilla2222D1'],
	'OM': ['cuchilla1111D', 'cuchilla2222D1', 'OpenMechDoor', 'CloseMechDoor'],
	'QF': ['cuchilla1111D', 'cuchilla2222D1', 'OpenMechDoor', 'CloseMechDoor', 'puerta4', 'puerta3'],
	'OUTSKIRTS': ['OpenMechDoor', 'CloseMechDoor', 'puerta4', 'puerta3'],
	'DOWNTOWN': ['OpenMechDoor', 'CloseMechDoor', 'puerta4', 'puerta3'],
	'COMMERCIAL': ['OpenMechDoor', 'CloseMechDoor', 'puerta4', 'puerta3'],
	'GDB': ['cuchilla1111D', 'cuchilla2222D1', 'placas-metal22', 'apertura2'],
	'SCRAPYARD': ['OpenMechDoor', 'CloseMechDoor', 'puerta4', 'puerta3'],
	'ABINDOOR': ['placas-metal22', 'apertura2'],
	'SHIPEDIT': ['cuchilla1111D', 'cuchilla2222D1']
}
let AllSounds = [
	'CloseMechDoor',
	'OpenMechDoor',
	'puerta4',
	'puerta3',
	'up1',
	'up11',
	'up22',
	'up33',
	'cuchilla1',
	'cuchilla2',
	'cuchilla1111D',
	'cuchilla2222D1',
	'gas2',
	'cierre-100',
	'cierre1',
	'cierre-10',
	'apertura2',
	'apertura4',
	'apertura5',
	'clik-mecanismo1',
	'clik-mecanismo2',
	'clik-mecanismo3',
	'clik-mecanismo5',
	'clik-mecanismo7',
	'ascensor-orbit1',
	'ascensor-orbit2',
	'subida-ascensor2',
	'bajada-ascensor2',
	'transporter3',
	'transporter4',
	'placas-metal22',
	'desaparicion',
	'desaparicion3',
	'desaparicion4',
	'desaparicion5',
	'electricidad333',
	'electricidad344',
	'puerta-emero-open',
	'puerta-emero-close',
	'cierre-emero',
	'puerta-security-open',
	'puerta-security-close',
	'metro-open',
	'cierre-metro1',
	'cierre-metro2',
	'llegada-metro22',
	'llegada-metroB',
	'puerta1',
	'puerta11',
	'abre-stairs',
	'cierre-stairs',
	'baldosa7',
	'cajon-open',
	'cajon-close',
	'puerta-jail',
	'cierre2',
	'cierre-viscoso',
	'viscoso-open',
	'cierre2',
	'clik-data',
	'clik-data2',
	'tritura2'
]
def LoadSounds
def LoadSndList
def SetSlide
def SetInitDoorSound
def ConversorSetSounds
