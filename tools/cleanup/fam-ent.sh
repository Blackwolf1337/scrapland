#!/bin/bash
# the entity family (rerunnable after a rollback)
cd "$(dirname "$0")/../.." || exit 1
python3 - <<'PY'
def ed(p, a, b, n=1):
    s = open(p).read(); assert s.count(a) >= 1, (p, a); s = s.replace(a, b) if n == 0 else s.replace(a, b, n); open(p, 'w').write(s)
ed('include/scrap/entity.h', '\tvoid *vf20_21[2];\n', '\tvoid(*Vf20)(cEntity *self);\t\t\t\t/* 0x50 vf20 */\n\tint(*Vf21)(cEntity *self, int a);\t\t\t/* 0x54 vf21 */\n')
ed('include/scrap/camcontrol.h', """	void(*GetAng2)(cEntity *self, float *ang);				/* 0x74 vf29 FCam only */
	void(*SetAng2)(cEntity *self, const float *ang);			/* 0x78 vf30 FCam only */
} cCamControlVtbl;
SD_OFFSET(cCamControlVtbl, GetCamPos, 0x5c);
SD_OFFSET(cCamControlVtbl, SetTarget, 0x6c);
SD_OFFSET(cCamControlVtbl, SetAng2, 0x78);
""", """} cCamControlVtbl;
SD_OFFSET(cCamControlVtbl, GetCamPos, 0x5c);
SD_OFFSET(cCamControlVtbl, SetTarget, 0x6c);

/* the follow camera adds two */
typedef struct cFollowCamVtbl {
	cCamControlVtbl base;
	void(*GetAng2)(cEntity *self, float *ang);				/* 0x74 vf29 */
	void(*SetAng2)(cEntity *self, const float *ang);			/* 0x78 vf30 */
} cFollowCamVtbl;
SD_OFFSET(cFollowCamVtbl, SetAng2, 0x78);
""")
import re
for p, t in (('include/scrap/vehicleentity.h', 'cVehicleEntity'), ('include/scrap/missileentity.h', 'cMissileEntity')):
    s = open(p).read()
    a = s.index('typedef struct %sVtbl {' % t); b = s.index('} %sVtbl;' % t, a)
    s = s[:a] + s[a:b].replace('(%s *self' % t, '(cEntity *self') + s[b:]
    open(p, 'w').write(s)
PY
python3 tools/cleanup/vtgen.py emit cEntityVtbl cEntity_vftable
python3 tools/cleanup/vtgen.py emit cObjEntityVtbl cObjEntity_vftable cItemEntity_vftable
python3 tools/cleanup/vtgen.py emit cWithLifeEntityVtbl cWithLifeEntity_vftable
python3 tools/cleanup/vtgen.py emit cWalkCharVtbl cWalkCharEntity_vftable
python3 tools/cleanup/vtgen.py emit cVehicleEntityVtbl cVehicleEntity_vftable
python3 tools/cleanup/vtgen.py emit cMissileEntityVtbl cMissileEntity_vftable
python3 tools/cleanup/vtgen.py emit cCamControlVtbl cCamControl_vftable cCamPathEntity_vftable
python3 tools/cleanup/vtgen.py emit cFollowCamVtbl cFollowCamEntity_vftable
python3 tools/cleanup/vtgen.py stubs $(grep -l "_vftable = {" src/game/Entities/*.c src/game/Entities/*/*.c src/game/World/*.c 2>/dev/null)
