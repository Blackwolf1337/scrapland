#!/bin/bash
# the AI character control family (rerunnable after a rollback)
cd "$(dirname "$0")/../.." || exit 1
python3 - <<'PY'
p='include/scrap/aicharcontrol.h'; s=open(p).read()
a="SD_SIZE(cAICharControl2DVtbl, 103 * 4);\n"
s=s.replace(a,a+"""
typedef struct cAICharControl3DVtbl {
	cAICharControlVtbl base;
	byte(*TestStep)(cAICharControl *self, const vec3_t *a, const vec3_t *b, float r);	/* 0x190 */
} cAICharControl3DVtbl;
SD_SIZE(cAICharControl3DVtbl, 101 * 4);
""",1)
open(p,'w').write(s)
PY
python3 tools/cleanup/vtgen.py emit cAICharControlVtbl cAICharControl_vftable
python3 tools/cleanup/vtgen.py emit cAICharControl2DVtbl cAICharControl2D_vftable cAIControlBankDirector_vftable cAIControlBankMaster_vftable cAIControlBishop_vftable cAIControlGear_vftable
python3 tools/cleanup/vtgen.py emit cAICharControl3DVtbl cAICharControl3D_vftable cAIControlMessenger_vftable cAIControlNurse_vftable cAIControlSentinel_vftable
python3 tools/cleanup/vtgen.py stubs src/game/AI/*.c
