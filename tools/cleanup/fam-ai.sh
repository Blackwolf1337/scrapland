#!/bin/bash
# the AI family: the header changes, the vtables, the typed stubs (rerunnable after a rollback)
cd "$(dirname "$0")/../.." || exit 1
python3 - <<'PY'
p='include/scrap/ai.h'; s=open(p).read()
a=s.index('typedef struct cAIMoveVtbl {'); b=s.index('} cAIMoveVtbl;',a)
s=s[:a]+s[a:b].replace('(cAIMove *self','(cAI *self')+s[b:]
a=s.index('typedef struct cAICharVtbl {'); b=s.index('} cAICharVtbl;',a)
body=s[a:b]; k=body.index('\tvoid(*CheckParams)')
body='typedef struct cAICharVtbl {\n\tcAIMoveVtbl base;\t\t\t\t\t\t\t\t\t/* 0x00 */\n'+body[k:].replace('(cAIChar *self','(cAI *self')
s=s[:a]+body+s[b:]
for f in ('Debugf, 0x18','GetTarget, 0x38','IsPaused, 0x44','Event, 0x58'):
    s=s.replace('SD_OFFSET(cAICharVtbl, %s);\n'%f,'')
open(p,'w').write(s)
p='include/scrap/aivehicle.h'; s=open(p).read()
a=s.index('typedef struct cAIVehicleVtbl {'); b=s.index('\n\n',s.index('} cAIVehicleVtbl;',a))
s=s[:a]+s[b+2:]; s=s.replace('const cAIVehicleVtbl *vftable;','const cAIMoveVtbl *vftable;'); open(p,'w').write(s)
PY
sed -i 's/->GetTargetNode(/->GetTarget(/' src/game/Logic/Enemies.c src/game/AI/IVehicleControl.c src/game/AI/IVehicleShootControl.c include/scrap/logic.h
python3 tools/cleanup/vtgen.py emit cAIBaseVtbl cAI_vftable
python3 tools/cleanup/vtgen.py emit cAIMoveVtbl cAIMove_vftable cAIVehicle_vftable
python3 tools/cleanup/vtgen.py emit cAICharVtbl cAIBankDirector_vftable cAIBankMaster_vftable cAIBishop_vftable cAIChar_vftable cAIDesktop_vftable cAIGear_vftable cAIHuman_vftable cAIMayor_vftable cAIMessenger_vftable cAINurse_vftable cAIPolice_vftable cAISentinel_vftable
python3 tools/cleanup/vtgen.py stubs src/game/AI/*.c
# after vtloop.sh: the local function pointers of IChar.c (by hand, see git history)
