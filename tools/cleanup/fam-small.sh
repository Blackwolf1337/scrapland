#!/bin/bash
# the small families with typed structs already (rerunnable after a rollback)
cd "$(dirname "$0")/../.." || exit 1
python3 - <<'PY'
import re
def ed(p, a, b):
    s = open(p).read(); assert a in s, (p, a); s = s.replace(a, b, 1); open(p, 'w').write(s)
# cGVarListVtbl: the root cFuncGvar
ed('include/scrap/scrap.h', """	void(*Save)(void *self, void *f);	/* 0x0 */
	void(*Load)(void *self, void *f);	/* 0x4 */
} cGVarListVtbl;""", """	void(*Save)(struct cFuncGvar *self, void *f);	/* 0x0 */
	void(*Load)(struct cFuncGvar *self, void *f);	/* 0x4 */
} cGVarListVtbl;""")
# cInfItemVtbl slot 0
ed('include/scrap/worldlogic.h', "\tvoid *vf00;\n\tvoid(*AddLife)(cInfItem *self", "\tcInfItem *(*Delete)(cInfItem *self, byte flags);\t\t/* 0x00 */\n\tvoid(*AddLife)(cInfItem *self")
# cVehicleMotorsVtbl next to its class, typed
s = open('include/scrap/vehicleentity.h').read()
m = re.search(r'typedef struct cVehicleMotorsVtbl \{.*?\} cVehicleMotorsVtbl;\n', s, re.S)
vt = m.group(0).replace('void *self', 'struct cVehicleMotors *self')
s = s.replace(m.group(0), ''); open('include/scrap/vehicleentity.h', 'w').write(s)
ed('include/scrap/vehiclemotors.h', 'struct cVehicleMotors {\n\tconst void *vftable;\t\t/* 0x00 */', vt + '\nstruct cVehicleMotors {\n\tconst cVehicleMotorsVtbl *vftable;\t/* 0x00 */')
# cUserNetManagerVtbl: SendHello only in the server
ed('include/scrap/net.h', """	void(*SendHello)(cUserNetManager *self, byte type);				/* 0x10 cServerNetManager */
} cUserNetManagerVtbl;
SD_OFFSET(cUserNetManagerVtbl, SendHello, 0x10);
""", """} cUserNetManagerVtbl;

typedef struct cServerNetManagerVtbl {
	cUserNetManagerVtbl base;
	void(*SendHello)(cUserNetManager *self, byte type);				/* 0x10 */
} cServerNetManagerVtbl;
SD_OFFSET(cServerNetManagerVtbl, SendHello, 0x10);
""")
PY
python3 - <<'PY'
# the cast vtable macros to plain GAME_VFTABLE ones the emitter knows; a macro named like its symbol gets a capital name
import re, glob
files = [f for f in glob.glob('src/**/*.[ch]', recursive=True) + glob.glob('include/scrap/*.h')
         if not f.endswith('Vtables.c') and '/core/test' not in f]
ren = {}
for f in files:
    s = open(f).read()
    def rep(m):
        mac, sym = m.group(1), m.group(2)
        if mac == sym:
            ren[mac] = sym.upper()
            mac = sym.upper()
        return '#define %s GAME_VFTABLE(%s)' % (mac, sym)
    t = re.sub(r'#define (\w+) \(\(const \w+ \*\)GAME_VFTABLE\((\w+)\)\)', rep, s)
    if t != s:
        open(f, 'w').write(t)
for f in files:
    s = open(f).read()
    t = s
    for a, b in ren.items():
        t = re.sub(r'(?<!GAME_VFTABLE\()\b%s\b' % a, b, t)
    if t != s:
        open(f, 'w').write(t)
print(ren)
PY
e() { python3 tools/cleanup/vtgen.py emit "$@" || exit 1; }
e cMapElementVtbl cFXElementsLighting_vftable cFXElementsFireDrop_vftable cFXElementsLensFlare_vftable cFXElementsABFakeMeteors_vftable cFXElementsABFakeLimit_vftable cFXElementsOrbitBuoy_vftable cFXElementsOrbitBuoyController_vftable cFXElementsMentalController_vftable cFXElementsMentalControllerDamageFX_vftable cFXElementsMentalControllerMainFX_vftable cMapElement_vftable
e cAbstractFileVtbl cAbstractFile_vftable cFile_vftable cFileMem_vftable cFile2_vftable cFilePack_vftable
e cAIEntityGroupVtbl cAICharGroup_vftable cAIEntityGroup_vftable cAIVehicleGroup_vftable
e cAbstractIDeviceVtbl cAbstractIDevice_vftable cPcKeyboard_vftable cPcMouse_vftable cPcPad_vftable
e CAStarVtbl CAStar_vftable CAStar2D_vftable CAStarSup_vftable
e CNodesWorldVtbl CNodesWorld_vftable CNodesWorld2D_vftable CNodesWorldSup_vftable
e CNodesTrafficVtbl CNodesTraffic_vftable CNodesTrafficInDoor_vftable
e cGridElementsVtbl cGridElements_InfoTrafficNode_vftable cGridElements_InfPointVis_vftable
e cNetControllerVtbl cNetController_vftable cNetVController_vftable
e cSlotControllerVtbl cSlotController_vftable cRndSlotController_vftable@src/game/Global/SlotController.c
e cUserNetManagerVtbl cUserNetManager_vftable@src/game/Net/MsgMan.c cClientNetManager_vftable
e cServerNetManagerVtbl cServerNetManager_vftable
e CAppVtbl CApp_vftable CWinApp_vftable
e CD3DApplicationVtbl CEngineApp_vftable CD3DApplication_vftable
e cActCtrlVarVtbl cActCtrl_vftable
e cVehicleMotorsVtbl cVehicleMotors_vftable
e cGVarListVtbl cFuncGvarDevices_vftable
e cInfItemVtbl cInfItem_vftable
e CSoundSysVtbl CSoundSys_vftable CSoundXBox_vftable
python3 tools/cleanup/vtgen.py stubs $(grep -rl "_vftable = {" src --include=*.c)
