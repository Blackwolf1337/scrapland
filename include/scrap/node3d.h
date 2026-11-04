/*
 * CNode3D of the engine3d (original source Engine/Nodo3D.cpp): a node of the scene graph. The nodes of a scene are a
 * doubly linked list (next, ant) with a parent each, reference counted like CObjRef (objref.h): the links of the list
 * and the parent only count, the last release by an owner deletes the node through its virtual destructor. A node
 * owns its 3D object (IObject3D: mesh, camera, light, particle system ...), takes its position and rotation from the
 * animation channels of the scene (track index) and keeps the world matrix in m.
 */
#ifndef SCRAP_NODE3D_H
#define SCRAP_NODE3D_H

#include "scrap/animation.h"
#include "scrap/archio.h"
#include "scrap/mathlib.h"

/* the classes of the 3D objects (IObject3D class, objeto3d.h), Obj3D_ClassName gives their names */
#define CLASS_OBJ3D_TRIMESH 0xa1000001
#define CLASS_OBJ3D_MESHD3D 0xa1000002
#define CLASS_OBJ3D_CAMERA 0xa2000004
#define CLASS_OBJ3D_LIGHT 0xa3000008
#define CLASS_OBJ3D_GROUND 0xa4000010
#define CLASS_OBJ3D_PARTICLESYSTEM 0xa5000020
#define CLASS_OBJ3D_GRAPHIC3D 0xa6000040
#define CLASS_OBJ3D_FLARE 0xa6000080	/* the animated visibility sets its float at 0x28 (alpha 0..1) */
#define CLASS_OBJ3D_PORTAL 0xa7000100
#define MAX_ANIM_CHANNEL 4

/* flags */
#define NODE_COLLISION (1u << 12)	/* a mesh of the static collision of a game model */
#define NODE_POSTFIX (1u << 16)		/* the position of the node instead of the animated one */
#define NODE_NOUPDATE (1u << 17)
#define NODE_SLERP2 (1u << 18)		/* R_NodeSlerp2 1: the blend of two channels relative to rot */
#define NODE_FX (1u << 19)		/* CRenderNode_ApplyFX on the world matrix */
#define NODE_BIP (1u << 21)		/* a node of a Biped or bone hierarchy ("Bip ", "Bone" names) */
#define NODE_NO_RENDERNODE (1u << 22)	/* no render node of its own; the render nodes of a model end at the first */
#define NODE_SHADOW (1u << 26)		/* lights of this node cast shadows */
#define NODE_PASS100 (1u << 30)		/* drawn only in the render pass 100, and only those */
#define NODE_HIDE 0x88
#define NODE_PASS3 0x40			/* drawn in the passes 3 and 13 also without the class bits 0xe0 */

typedef struct IObject3D IObject3D;
typedef struct CRenderNode CRenderNode;
typedef struct MeshD3D_FileCtx MeshD3D_FileCtx;

/* the slots of the 3D objects (objeto3d.h) */
typedef struct IObject3DVtbl {
	void (*Free)(IObject3D *self);				/* 0x00 */
	void(*Update)(IObject3D *self, CRenderNode *r);				/* 0x04 */
	void(*Render)(IObject3D *self, void *dev, CRenderNode *r);			/* 0x08 */
	void(*InitD3D)(IObject3D *self, void *dev);					/* 0x0c */
	void(*ReleaseD3D)(IObject3D *self, void *dev);				/* 0x10 */
	byte(*Write)(IObject3D *self, CFileIO *f, MeshD3D_FileCtx *ctx);		/* 0x14 */
	byte(*Read)(IObject3D *self, CFileIO *f, MeshD3D_FileCtx *ctx);		/* 0x18 */
} IObject3DVtbl;

/* calls through the slots of IObject3DVtbl */
static inline void Object3D_Free(void *self) {
	VT(IObject3DVtbl, self)->Free(self);
}

static inline void Object3D_InitD3D(void *self, void *dev) {
	VT(IObject3DVtbl, self)->InitD3D(self, dev);
}

static inline byte Object3D_Read(void *self, CFileIO *f, MeshD3D_FileCtx *ctx) {
	return VT(IObject3DVtbl, self)->Read(self, f, ctx);
}

static inline void Object3D_ReleaseD3D(void *self, void *dev) {
	VT(IObject3DVtbl, self)->ReleaseD3D(self, dev);
}

static inline void Object3D_Render(void *self, void *dev, CRenderNode *r) {
	VT(IObject3DVtbl, self)->Render(self, dev, r);
}

static inline void Object3D_Update(void *self, CRenderNode *r) {
	VT(IObject3DVtbl, self)->Update(self, r);
}

static inline byte Object3D_Write(void *self, CFileIO *f, MeshD3D_FileCtx *ctx) {
	return VT(IObject3DVtbl, self)->Write(self, f, ctx);
}

struct IObject3D {
	const IObject3DVtbl *vftable;	/* 0x0 */
	uint32_t class;			/* 0x4 */
};
extern const IObject3DVtbl IObject3D_vftable;
#define IOBJECT3D_VFTABLE (&IObject3D_vftable)

/* a counted reference (objref.h): the head of CNode3D and CScene, the deleting destructor first */
typedef struct ObjRef ObjRef;
typedef struct ObjRefVtbl {
	void (*Free)(ObjRef *self);	/* 0x0 */
} ObjRefVtbl;

/* calls through the slots of ObjRefVtbl */
static inline void ObjRef_Free(void *self) {
	VT(ObjRefVtbl, self)->Free(self);
}

typedef struct ObjRef {
	const ObjRefVtbl *vftable;	/* 0x0 */
	char *name;			/* 0x4 */
	int Refs;			/* 0x8 */
} ObjRef;

/* *slot released (objref.h): counted down, deleted when nobody holds it any more */
static inline void ObjRef_Release(void *slot) {
	ObjRef **dst = slot;
	CHECK(!*dst || (*dst)->Refs > 0);
	if (*dst) {
		(*dst)->Refs--;
		ObjRef *o = *dst;
		if (!o->Refs && o) {
			ObjRef_Free(o);
		}
		*dst = NULL;
	}
}

/* the same without the CHECK */
static inline void ObjRef_Drop(void *slot) {
	ObjRef **dst = slot;
	if (*dst) {
		(*dst)->Refs--;
		ObjRef *o = *dst;
		if (!o->Refs && o) {
			ObjRef_Free(o);
		}
		*dst = NULL;
	}
}

/* o as a class: CHECK on another class and NULL (objeto3d.h) */
static inline void *Obj3D_Cast(IObject3D *o, uint32_t class) {
	CHECK(o->class == class);
	return o->class == class ? o : NULL;
}

/* a rotation given by angles, its quaternion made on demand */
typedef struct CRotation {
	vec3_t ang;			/* 0x00 radians */
	quat_t q;			/* 0x0c */
	uint32_t flags;			/* 0x1c ROT_DIRTY, ROT_ZERO */
	int mode;			/* 0x20 0: Quat_FromEuler of -ang, else the product of the axis rotations */
} CRotation;
#define ROT_DIRTY 1
#define ROT_ZERO 2			/* all angles 0: the identity */

/* one animation channel of a scene: one animation or the blend of two (weight blend of the second) */
typedef struct NodeAnimPair {
	void *anim[2];			/* each: +0x94 an object whose +0x3c is the track set (CAnimation_GetNodeTable) */
} NodeAnimPair;

typedef struct NodeAnimChannel {
	NodeAnimPair *anim;		/* 0x00 */
	int frame;			/* 0x04 */
	float t;			/* 0x08 */
	int frame2;			/* 0x0c of anim[1] */
	float t2;			/* 0x10 */
	float blend;			/* 0x14 */
} NodeAnimChannel;

/* the render state of a scene (RenderCtx.h) */
typedef struct NodeAnimCtx {
	NodeAnimChannel chan[MAX_ANIM_CHANNEL];	/* 0x00 */
	matrix_t m;			/* 0x60 the parent of the nodes without one */
	struct CModel *model;		/* 0xa0 */
	void *renderNodes;			/* 0xa4 set: the render nodes of model are drawn instead of the scene's */
} NodeAnimCtx;

/* a scene (Engine/escena.cpp): a list of nodes with their materials, an animation and the user properties */
/* a color (r in the low byte, then g, b, a) and the intensity it is multiplied with (Color_ToD3D) */
typedef struct ColorIntens {
	uint32_t rgba;			/* 0x0 */
	float intens;			/* 0x4 */
} ColorIntens;

typedef struct CScene {
	union {
		ObjRef ref;
		struct {
			const ObjRefVtbl *vftable;	/* 0x00 */
			char *name;			/* 0x04 */
			int Refs;			/* 0x08 */
		};
	};
	struct CNode3D *Node3D;		/* 0x0c the first node */
	NodeAnimCtx *anim;		/* 0x10 the render context during CScene_Render and CScene_Update */
	ColorIntens ambient;		/* 0x14 0x0b0b0b, 1 */
	ColorIntens fog;		/* 0x1c the fog color, 0, 1 */
	box_t box;			/* 0x24 CScene_UpdateBox */
	struct CAnimation *AnimM3D;	/* 0x3c */
	char *node;			/* 0x40 the name of the node whose animation node a group starts at */
	char *mainLOD;			/* 0x44 the scene name of the main LOD */
	float vertWeldThreshold;		/* 0x48 the weld threshold of the vertices, -1 */
	CIniFile *ini;			/* 0x4c FileINI */
	CIniFile *userProp;		/* 0x50 */
	IObject3D *CollideMesh;		/* 0x54 deleted through vftable[0] */
	CTable mats;			/* 0x58 TabMaterial: CMaterial * */
	byte fromAmc;			/* 0x60 loaded from the .sm3 and the .amc */
} CScene;

/* how a render node orients its node (CRenderNode look) */
typedef struct NodeLook {
	int type;			/* 0x00 1: rot of r1 and r2 around the animated one, 2..5: billboards */
	CRotation r1;			/* 0x04 */
	CRotation r2;			/* 0x28 */
} NodeLook;

/* the render instance of a node (Engine/NodoR.cpp, 0xa8 bytes) */
typedef struct CRenderNode {
	int unused0;			/* 0x00 kept 0, checked at the destruction */
	byte flags;			/* 0x04 2: a render node of a model */
	byte unused5;			/* 0x05 cleared by the constructor */
	byte pad_6[2];
	struct CModel *owner;		/* 0x08 look 2 turns to the negated angles of its rot */
	NodeLook *look;			/* 0x0c */
	matrix_t m;			/* 0x10 world */
	matrix_t mObj;			/* 0x50 mObj of the node times m */
	int frame;			/* 0x90 g_FrameCount of the last update */
	void *skin;			/* 0x94 the CEVA (vertex animation) of the meshes of this node (RenderEVA.cpp) */
	uint16_t slot;			/* 0x98 the animation slot of the owner model for the vertex animation */
	int16_t skinFrame;		/* 0x9a the frame of that channel when CMeshD3D_SetRender set skin */
	IObject3D *obj3D;		/* 0x9c instead of the node's, the same class */
	struct INodeFX *obj3D2;		/* 0xa0 owned, the effect of the node (a copy of obj58) */
	struct CNode3D *node;		/* 0xa4 a reference */
} CRenderNode;

typedef struct CNode3D {
	union {
		ObjRef ref;
		struct {
			const ObjRefVtbl *vftable;	/* 0x00 */
			char *name;			/* 0x04 */
			int Refs;			/* 0x08 */
		};
	};
	struct CNode3D *next;		/* 0x0c */
	struct CNode3D *prev;		/* 0x10 */
	struct CNode3D *parent;		/* 0x14 */
	uint32_t flags;			/* 0x18 NODO_* */
	vec3_t pos;			/* 0x1c */
	quat_t rot;			/* 0x28 */
	float visible;			/* 0x38 "Visible:" of the M3D, kept in the caches */
	IObject3D *obj3D;		/* 0x3c owned */
	CScene *scene;		/* 0x40 */
	int index;			/* 0x44 the track of the node in the animations, -1: none */
	int slot;			/* 0x48 in the node table of the scene (Scene_NodeTable), render nodes first; -1 */
	int renderNode;			/* 0x4c -1, else Update goes to CRenderNode_UpdateNode */
	int aniLocked;		/* 0x50 ANIM_* channels whose animation is discarded (~AniMask) */
	int mark;			/* 0x54 scratch: sort order, tree item of the debug view (CScene_SetMarks) */
	IObject3D *obj58;		/* 0x58 owned, deleted through its vftable[0] */
	matrix_t m;			/* 0x5c world, the position in m[12..14] */
	matrix_t mObj;			/* 0x9c the object in the node */
	quat_t qLook;			/* 0xdc the orientation for the looks 3 and 4 */
	vec3_t scale;			/* 0xec */
	CIniFile *ini;			/* 0xf8 */
} CNode3D;

CNode3D *CNode3D_ctor(CNode3D *self);
void CNode3D_dtor(CNode3D *self);
/* the box of the object (and its matrix into m if not NULL); 0 for classes without one */
byte CNode3D_GetBBox(CNode3D *self, matrix_t *m, box_t *bbox, byte recalc);
void CNode3D_Render(CNode3D *self, void *arg, CRenderNode *r);
/* the world matrix from the animation, the parent and the look of r; the object updated */
void CNode3D_Update(CNode3D *self, CRenderNode *r);
/* the first node from this one on with a camera or light object named name (NULL: any) */
IObject3D *CNode3D_FindCamera(CNode3D *self, const char *name);
/* along next, from this node (incl) or the next one */
CNode3D *CNode3D_Find(CNode3D *self, const char *name, byte incl);
/* along ant from the previous node */
CNode3D *CNode3D_FindPrev(CNode3D *self, const char *name);
CNode3D *CNode3D_FindWild(CNode3D *self, const char *pattern, byte incl);
uint32_t CNode3D_Class(CNode3D *self);
/* the device mesh of a CTriMesh, the nodes of a CGround; 0 for other classes */
byte CNode3D_BuildD3D(CNode3D *self, uint32_t flags);
/* out of the list of the scene */
void CNode3D_Unlink(CNode3D *self);
void CNode3D_FreeObj3D(CNode3D *self);
void CNode3D_SetPrev(CNode3D *self, CNode3D *n);
void CNode3D_SetObj3D(CNode3D *self, IObject3D *obj);
void CNode3D_SetParent(CNode3D *self, CNode3D *n);
void CNode3D_SetParentName(CNode3D *self, const char *name);
void CNode3D_SetNext(CNode3D *self, CNode3D *n);
/* the owned objects and the parent released */
void CNode3D_Release(CNode3D *self);
/* the track of the node in animation 0 or 1 of channel ch; NULL without */
void *CNode3D_Track(CNode3D *self, uint32_t ch);
/* the color of the node's animated light (color times *mult) from channel 0; 0 without one */
byte CNode_LightColor(CNode3D *self, vec3_t *color, float *mult);
/* the member at 0x20 of the track of channel ch */
CEVA *CNode3D_TrackData(CNode3D *self, uint32_t ch);
/* an animated float (the camera's fov) from channel 0 */
byte CNode3D_AnimFloat(CNode3D *self, float *out);
NodeAnimChannel *CNode3D_Channel(CNode3D *self, uint32_t ch);

/* other modules, called from Nodo3D.cpp (src/imports.inc) */
void CRotation_SetEuler(CRotation *self, const vec3_t *ang);
void *CParticleSystem_Cast(IObject3D *obj);	/* rva 0x23f310, CHECK of the class */
CRenderNode *CRenderNode_ctor(CRenderNode *self);
void CRenderNode_UpdateNode(CRenderNode *self);
int CRenderNode_ApplyFX(CRenderNode *self, matrix_t *m);

#endif
