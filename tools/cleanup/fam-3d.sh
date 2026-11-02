#!/bin/bash
# the 3D objects (IObject3D): the vtable struct, the heads of the classes, the vtables (rerunnable after a rollback)
cd "$(dirname "$0")/../.." || exit 1
python3 - <<'PY'
import re
def ed(p, a, b):
    s = open(p).read(); assert a in s, (p, a); s = s.replace(a, b, 1); open(p, 'w').write(s)
ed('include/scrap/node3d.h', """typedef struct IObject3D {
	void **vftable;			/* 0x0 [0] deleting destructor, [1] Update(CRenderNode *), [2] Render(arg, CRenderNode *) */
	uint32_t class;			/* 0x4 */
} IObject3D;""", """typedef struct IObject3D IObject3D;
typedef struct CRenderNode CRenderNode;
typedef struct MeshD3D_FileCtx MeshD3D_FileCtx;

/* the slots of the 3D objects (objeto3d.h) */
typedef struct IObject3DVtbl {
	IObject3D *(*Delete)(IObject3D *self, byte flags);				/* 0x00 */
	void(*Update)(IObject3D *self, CRenderNode *r);				/* 0x04 */
	void(*Render)(IObject3D *self, void *dev, CRenderNode *r);			/* 0x08 */
	void(*InitD3D)(IObject3D *self, void *dev);					/* 0x0c */
	void(*ReleaseD3D)(IObject3D *self, void *dev);				/* 0x10 */
	byte(*Write)(IObject3D *self, CFileIO *f, MeshD3D_FileCtx *ctx);		/* 0x14 */
	byte(*Read)(IObject3D *self, CFileIO *f, MeshD3D_FileCtx *ctx);		/* 0x18 */
} IObject3DVtbl;

struct IObject3D {
	const IObject3DVtbl *vftable;	/* 0x0 */
	uint32_t class;			/* 0x4 */
};""")
for c, h in (('CTriMesh', 'trimesh'), ('CMeshD3D', 'meshd3d'), ('CGround', 'ground'), ('CLight', 'light'), ('CCamera', 'camera'),
             ('CFlare', 'flare'), ('CGraphic3D', 'graphic3d'), ('CPortal', 'portal'), ('CParticleSystem', 'particlesystem')):
    p = 'include/scrap/%s.h' % h
    s = open(p).read()
    m = re.search(r'(struct %s \{\n)\tvoid \*\*?vftable;([^\n]*)\n\tuint32_t class;([^\n]*)\n' % c, s)
    assert m, c
    s = s.replace(m.group(0), m.group(1) + '\tunion {\n\t\tIObject3D base;\n\t\tstruct {\n\t\t\tconst IObject3DVtbl *vftable;\t/* 0x00 */\n\t\t\tuint32_t class;' + m.group(3) + '\n\t\t};\n\t};\n', 1)
    open(p, 'w').write(s)
PY
python3 tools/cleanup/vtgen.py emit IObject3DVtbl IObject3D_vftable CObjBase3D_vftable CTriMesh_vftable CMeshD3D_vftable CGround_vftable CLight_vftable CCamera_vftable CFlare_vftable CGraphic3D_vftable CPortal_vftable CParticleSystem_vftable
python3 tools/cleanup/vtgen.py stubs $(grep -rl "const IObject3DVtbl \w*_vftable = {" src)
