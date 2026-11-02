#!/bin/bash
# the classes whose vtable holds only the deleting destructor (rerunnable after a rollback)
cd "$(dirname "$0")/../.." || exit 1
python3 - <<'PY'
import re
def ed(p, a, b):
    s = open(p).read(); assert a in s, (p, a); s = s.replace(a, b, 1); open(p, 'w').write(s)

def own(p, cls, head_re, member_re, vt=None, tag=None):
    """XVtbl before the struct of cls in p, its vftable member typed"""
    vt = vt or cls + 'Vtbl'
    tag = tag or cls
    s = open(p).read()
    m = re.search(head_re, s, re.M)
    assert m, (p, cls)
    block = 'typedef struct %s {\n\tstruct %s *(*Delete)(struct %s *self, byte flags);\t/* 0x00 */\n} %s;\n\n' % (vt, tag, tag, vt)
    s = s[:m.start()] + block + s[m.start():]
    i = s.index(m.group(0), m.start() + len(block))
    j = s.index('\n', i)
    k = re.compile(member_re).search(s, j)
    assert k, (p, cls, 'member')
    s = s[:k.start()] + k.group(1) + 'const %s *vftable;' % vt + s[k.end():]
    open(p, 'w').write(s)

own('include/scrap/actions.h', 'cActClass', r'^/\* an action class \(0x20c bytes', r'(\t)const cActVtbl \*vftable;')
own('include/scrap/actions.h', 'cActClassList', r'^struct cActClassList \{', r'(\t)const cActVtbl \*vftable;')
own('include/scrap/actions.h', 'cAction', r'^struct cAction \{', r'(\t)const cActVtbl \*vftable;')
own('include/scrap/aivehiclecontrol.h', 'cAIVehicleControl', r'^typedef struct cAIVehicleControl \{', r'(\t)const cAIObjVtbl \*vftable;')
own('include/scrap/aivehiclecontrol.h', 'cAIVehicleShoot', r'^typedef struct cAIVehicleShoot \{', r'(\t)const cAIObjVtbl \*vftable;')
own('include/scrap/worldlogic.h', 'cLogicControl', r'^typedef struct cLogicControl \{', r'(\t)void \*vftable;')
own('include/scrap/worldparts.h', 'cScheduledFuncStack', r'^typedef struct cScheduledFuncStack \{', r'(\t)void \*vftable;')
own('include/scrap/scorer.h', 'cScorer', r'^typedef struct cScorer \{', r'(\t)void \*vftable;')
own('include/scrap/model.h', 'CModel', r'^typedef struct CModel \{', r'(\t)void \*vftable;')
own('include/scrap/material.h', 'CMatList', r'^struct CMatList \{', r'(\t)void \*\*vftable;')
own('include/scrap/material.h', 'CMaterial', r'^typedef struct CMaterial \{', r'(\t)void \*\*vftable;')
own('include/scrap/game.h', 'cWorld', r'^typedef struct cWorld \{', r'(\t)void \*vftable;')
own('include/scrap/fxcountermeasures.h', 'cTargetLock', r'^typedef struct cTargetLock \{', r'(\t)const void \*vftable;')
# the counted references: ObjRef the root of CNode3D and CScene
ed('include/scrap/node3d.h', """typedef struct ObjRef {
	void **vftable;			/* 0x0 */""", """typedef struct ObjRef ObjRef;
typedef struct ObjRefVtbl {
	ObjRef *(*Delete)(ObjRef *self, byte flags);	/* 0x0 */
} ObjRefVtbl;

typedef struct ObjRef {
	const ObjRefVtbl *vftable;	/* 0x0 */""")
s = open('include/scrap/node3d.h').read()
s = s.replace('\t\t\t((objDelete_t)o->vftable[0])(o, 1);', '\t\t\to->vftable->Delete(o, 1);')
for c in ('CScene', 'CNode3D'):
    m = re.search(r'(typedef struct %s \{\n)\tvoid \*\*vftable;([^\n]*)\n\tchar \*name;([^\n]*)\n\tint Refs;([^\n]*)\n' % c, s)
    assert m, c
    s = s.replace(m.group(0), m.group(1) + '\tunion {\n\t\tObjRef ref;\n\t\tstruct {\n\t\t\tconst ObjRefVtbl *vftable;\t/* 0x00 */\n\t\t\tchar *name;' + m.group(3) + '\n\t\t\tint Refs;' + m.group(4) + '\n\t\t};\n\t};\n', 1)
open('include/scrap/node3d.h', 'w').write(s)
# the index calls of the deleting destructor
E = [
 ('src/game/World/FX/FXScene.c', '((FXSUpdate_t *)el->vftable)[1](el, d, 0.0f);', 'el->vftable->Update(el, d, 0.0f);'),
 ('src/game/AI/NodesTraffic.c', '\ttypedef void(* getBox_t)(CNodesWorld * self, vec3_t * min, vec3_t * max);\n', ''),
 ('src/game/AI/NodesTraffic.c', '((getBox_t)((void *const *)w->sup.vftable)[1])(w, min, max);', 'w->sup.vftable->GetBox(w, min, max);'),
 ('src/game/engine3d/Engine/Scene.c', '((objDelete_t)n->vftable[0])(n, 1);', 'n->vftable->Delete(n, 1);'),
 ('src/game/engine3d/Engine/Scene.c', '((objDelete_t)n->vftable[0])(n, 1);', 'n->vftable->Delete(n, 1);'),
 ('src/game/engine3d/Engine/Material.c', '((deleteObj_t)self->first->vftable[0])(self->first, 1);', 'self->first->vftable->Delete(self->first, 1);'),
 ('src/game/engine3d/Engine/Material.c', '((deleteObj_t)mat->vftable[0])(mat, 1);', 'mat->vftable->Delete(mat, 1);'),
 ('src/game/engine3d/Engine/Material.c', '((deleteObj_t)self->sub->vftable[0])(self->sub, 1);', 'self->sub->vftable->Delete(self->sub, 1);'),
 ('src/game/engine3d/Engine/MeshD3D.c', '((nodeDelete_t)old->vftable[0])(old, 1);', 'old->vftable->Delete(old, 1);'),
 ('src/game/engine3d/Engine/Light.c', '((nodeDelete_t)old->vftable[0])(old, 1);', 'old->vftable->Delete(old, 1);'),
 ('src/game/engine3d/Engine/Camera.c', '((nodeDelete_t)old->vftable[0])(old, 1);', 'old->vftable->Delete(old, 1);'),
 ('src/game/engine3d/Engine/ReadM3D.c', '((objDelete_t)g_M3DLMat->vftable[0])(g_M3DLMat, 1);', 'g_M3DLMat->vftable->Delete(g_M3DLMat, 1);'),
 ('src/game/engine3d/Engine/ReadM3D.c', '((objDelete_t)((void **)all->vftable)[0])(all, 1);', 'all->vftable->Delete(all, 1);'),
 ('src/game/engine3d/Engine/ReadM3D.c', '((objDelete_t)mat->vftable[0])(mat, 1);', 'mat->vftable->Delete(mat, 1);'),
 ('src/game/engine3d/Engine/ReadM3D.c', '((CMatList *(*)(CMatList *, byte))g_M3DLMat->vftable[0])(g_M3DLMat, 1);', 'g_M3DLMat->vftable->Delete(g_M3DLMat, 1);'),
 ('src/game/engine3d/Engine/UtilScene.c', '((modelDelete_t)((void **)self->model->vftable)[0])(self->model, 1);', 'self->model->vftable->Delete(self->model, 1);'),
 ('src/game/engine3d/Engine/UtilScene.c', '((modelDelete_t)((void **)(*pm)->vftable)[0])(*pm, 1);', '(*pm)->vftable->Delete(*pm, 1);'),
 ('src/game/engine3d/Engine/Engine.c', '((void *(*)(CScene *, int))e->vftable[0])(e, 1);', 'e->vftable->Delete(&e->ref, 1);'),
 ('include/scrap/model.h', '((sceneDelete_t)e->vftable[0])(e, 1);', 'e->vftable->Delete(&e->ref, 1);'),
 ('src/game/engine3d/Engine/World.c', '((objDelete_t)n->vftable[0])(n, 1);', 'n->vftable->Delete(n, 1);'),
]
for f, a, b in E:
    ed(f, a, b)
PY
e() { python3 tools/cleanup/vtgen.py emit "$@" || exit 1; }
e cActClassVtbl cActClass_vftable
e cActClassListVtbl cActClassList_vftable
e cActionVtbl cActionMove_vftable
e cAIVehicleControlVtbl cAIVehicleControl_vftable
e cAIVehicleShootVtbl cAIVehicleShootControl_vftable
e cLogicControlVtbl cLogicControl_vftable
e cScheduledFuncStackVtbl cScheduledFuncStack_vftable
e cScorerVtbl cScorer_vftable
e CModelVtbl CModel_vftable
e CMatListVtbl CMatList_vftable
e CMaterialVtbl CMaterial_vftable
e cWorldVtbl cWorld_vftable
e cTargetLockVtbl cWeaponManager_vftable
e ObjRefVtbl CNode3D_vftable CScene_vftable
