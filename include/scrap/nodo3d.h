/*
 * CNodo3D of the engine3d (original source Engine/Nodo3D.cpp): a node of the scene graph. The nodes of a scene are a
 * doubly linked list (sig, ant) with a parent each, reference counted like CObjRef (objref.h): the links of the list
 * and the parent only count, the last release by an owner deletes the node through its virtual destructor. A node
 * owns its 3D object (IObjeto3D: mesh, camera, light, particle system ...), takes its position and rotation from the
 * animation channels of the scene (track indice) and keeps the world matrix in m.
 */
#ifndef SCRAP_NODO3D_H
#define SCRAP_NODO3D_H

#include "scrap/animacion.h"
#include "scrap/archio.h"
#include "scrap/mathlib.h"

/* the classes of the 3D objects (IObjeto3D clase, objeto3d.h), Obj3D_ClaseName gives their names */
#define CLASE_OBJ3D_MALLATRI 0xa1000001
#define CLASE_OBJ3D_MALLAD3D 0xa1000002
#define CLASE_OBJ3D_CAMARA 0xa2000004
#define CLASE_OBJ3D_LUZ 0xa3000008
#define CLASE_OBJ3D_SUELO 0xa4000010
#define CLASE_OBJ3D_SISTPART 0xa5000020
#define CLASE_OBJ3D_GRAFICO3D 0xa6000040
#define CLASE_OBJ3D_FLARE 0xa6000080	/* the animated visibility sets its float at 0x28 (alpha 0..1) */
#define CLASE_OBJ3D_PORTAL 0xa7000100
#define MAX_ANIM_CHANNEL 4

/* flags */
#define NODO_COLISION (1u << 12)	/* a mesh of the static collision of a game model */
#define NODO_POSFIJA (1u << 16)		/* the position of the node instead of the animated one */
#define NODO_NOUPDATE (1u << 17)
#define NODO_SLERP2 (1u << 18)		/* R_NodeSlerp2 1: the blend of two channels relative to rot */
#define NODO_FX (1u << 19)		/* CNodoR_ApplyFX on the world matrix */
#define NODO_BIP (1u << 21)		/* a node of a Biped or bone hierarchy ("Bip ", "Bone" names) */
#define NODO_NO_TABNODOR (1u << 22)	/* no render node of its own; the render nodes of a model end at the first */
#define NODO_SOMBRA (1u << 26)		/* lights of this node cast shadows */
#define NODO_PASS100 (1u << 30)		/* drawn only in the render pass 100, and only those */
#define NODO_HIDE 0x88
#define NODO_PASS3 0x40			/* drawn in the passes 3 and 13 also without the class bits 0xe0 */

typedef struct IObjeto3D {
	void **vftable;			/* 0x0 [0] deleting destructor, [1] Update(CNodoR *), [2] Render(arg, CNodoR *) */
	uint32_t clase;			/* 0x4 */
} IObjeto3D;
SD_SIZE(IObjeto3D, 8);

/* the slots of the vftable of the 3D objects (objeto3d.h) */
enum {
	OBJ3D_VF_DELETE = 0,		/* (int flags) */
	OBJ3D_VF_UPDATE = 1,		/* (CNodoR *) */
	OBJ3D_VF_RENDER = 2,		/* (void *arg, CNodoR *) */
	OBJ3D_VF_INITD3D = 3,		/* (void *dev) */
	OBJ3D_VF_RELEASED3D = 4,	/* (void *dev) */
	OBJ3D_VF_WRITE = 5,		/* byte (CArchIO *f, MallaD3D_FileCtx *ctx) */
	OBJ3D_VF_READ = 6		/* byte (CArchIO *f, MallaD3D_FileCtx *ctx) */
};
typedef void *(__thiscall *objDelete_t)(void *self, int flags);	/* the deleting destructor of any object */
typedef void(__thiscall *objDev_t)(IObjeto3D *self, void *dev);
typedef byte(__thiscall *objFile_t)(IObjeto3D *self, CArchIO *f, void *ctx);
#define OBJ3D_CALL(o, slot, type, ...) ((type)(o)->vftable[slot])((o), ##__VA_ARGS__)

/* a counted reference (objref.h): the deleting destructor first, the count at 0x8 */
typedef struct ObjRef {
	void **vftable;			/* 0x0 */
	int i4;
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
			((objDelete_t)o->vftable[0])(o, 1);
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
			((objDelete_t)o->vftable[0])(o, 1);
		}
		*dst = NULL;
	}
}

/* o as a class: CHECK on another class and NULL (objeto3d.h) */
static inline void *Obj3D_Cast(IObjeto3D *o, uint32_t clase) {
	CHECK(o->clase == clase);
	return o->clase == clase ? o : NULL;
}

/* a rotation given by angles, its quaternion made on demand */
typedef struct CRotacion {
	vec3_t ang;			/* 0x00 radians */
	quat_t q;			/* 0x0c */
	uint32_t flags;			/* 0x1c ROT_DIRTY, ROT_ZERO */
	int modo;			/* 0x20 0: Quat_FromEuler of -ang, else the product of the axis rotations */
} CRotacion;
SD_SIZE(CRotacion, 0x24);
#define ROT_DIRTY 1
#define ROT_ZERO 2			/* all angles 0: the identity */

/* one animation channel of a scene: one animation or the blend of two (weight blend of the second) */
typedef struct NodoAnimPair {
	void *anim[2];			/* each: +0x94 an object whose +0x3c is the track set (CAnimacion_GetNodoTabla) */
} NodoAnimPair;

typedef struct NodoAnimChannel {
	NodoAnimPair *anim;		/* 0x00 */
	int frame;			/* 0x04 */
	float t;			/* 0x08 */
	int frame2;			/* 0x0c of anim[1] */
	float t2;			/* 0x10 */
	float blend;			/* 0x14 */
} NodoAnimChannel;
SD_SIZE(NodoAnimChannel, 0x18);

/* the render state of a scene (RenderCtx.h) */
typedef struct NodoAnimCtx {
	NodoAnimChannel chan[MAX_ANIM_CHANNEL];	/* 0x00 */
	matrix_t m;			/* 0x60 the parent of the nodes without one */
	struct CModelo *modelo;		/* 0xa0 */
	void *nodosR;			/* 0xa4 set: the render nodes of modelo are drawn instead of the scene's */
} NodoAnimCtx;
SD_OFFSET(NodoAnimCtx, m, 0x60);

/* a scene (Engine/escena.cpp): a list of nodes with their materials, an animation and the user properties */
/* a color (r in the low byte, then g, b, a) and the intensity it is multiplied with (Color_ToD3D) */
typedef struct ColorIntens {
	uint32_t rgba;			/* 0x0 */
	float intens;			/* 0x4 */
} ColorIntens;
SD_SIZE(ColorIntens, 8);

typedef struct CEscena {
	void **vftable;			/* 0x00 [0] the deleting destructor */
	char *name;			/* 0x04 */
	int Refs;			/* 0x08 */
	struct CNodo3D *Nodo3D;		/* 0x0c the first node */
	NodoAnimCtx *anim;		/* 0x10 the render context during CEscena_Render and CEscena_Update */
	ColorIntens ambiente;		/* 0x14 0x0b0b0b, 1 */
	ColorIntens niebla;		/* 0x1c the fog color, 0, 1 */
	box_t box;			/* 0x24 CEscena_UpdateBox */
	struct CAnimacion *AnimM3D;	/* 0x3c */
	char *nodo;			/* 0x40 the name of the node whose animation node a group starts at */
	char *mainLOD;			/* 0x44 the scene name of the main LOD */
	float umbralUnirVert;		/* 0x48 the weld threshold of the vertices, -1 */
	CArchINI *ini;			/* 0x4c FileINI */
	CArchINI *userProp;		/* 0x50 */
	IObjeto3D *CollideMesh;		/* 0x54 deleted through vftable[0] */
	CTabla mats;			/* 0x58 TabMaterial: CMaterial * */
	byte fromAmc;			/* 0x60 loaded from the .sm3 and the .amc */
} CEscena;
SD_SIZE(CEscena, 0x64);
SD_OFFSET(CEscena, anim, 0x10);
SD_OFFSET(CEscena, box, 0x24);
SD_OFFSET(CEscena, AnimM3D, 0x3c);
SD_OFFSET(CEscena, ini, 0x4c);
SD_OFFSET(CEscena, mats, 0x58);
SD_OFFSET(CEscena, fromAmc, 0x60);

/* how a render node orients its node (CNodoR look) */
typedef struct NodoLook {
	int tipo;			/* 0x00 1: rot of r1 and r2 around the animated one, 2..5: billboards */
	CRotacion r1;			/* 0x04 */
	CRotacion r2;			/* 0x28 */
} NodoLook;
SD_OFFSET(NodoLook, r2, 0x28);

/* the render instance of a node (Engine/NodoR.cpp, 0xa8 bytes) */
typedef struct CNodoR {
	byte pad_0[4];
	byte flags;			/* 0x04 2: a render node of a model */
	byte pad_5[3];
	byte *owner;			/* 0x08 the CModelo; look 2 turns to the negated angles at 0x14 */
	NodoLook *look;			/* 0x0c */
	matrix_t m;			/* 0x10 world */
	matrix_t mObj;			/* 0x50 mObj of the node times m */
	int frame;			/* 0x90 g_FrameCount of the last update */
	void *skin;			/* 0x94 the CEVA (vertex animation) of the meshes of this node (RenderEVA.cpp) */
	uint16_t slot;			/* 0x98 the animation slot of the owner model for the vertex animation */
	byte pad_9a[2];
	IObjeto3D *obj3D;		/* 0x9c instead of the node's, the same class */
	IObjeto3D *obj3D2;		/* 0xa0 owned */
	struct CNodo3D *nodo;		/* 0xa4 a reference */
} CNodoR;
SD_OFFSET(CNodoR, flags, 0x04);
SD_OFFSET(CNodoR, m, 0x10);
SD_OFFSET(CNodoR, frame, 0x90);
SD_OFFSET(CNodoR, obj3D, 0x9c);
SD_OFFSET(CNodoR, nodo, 0xa4);
SD_SIZE(CNodoR, 0xa8);

typedef struct CNodo3D {
	void **vftable;			/* 0x00 [0] the deleting destructor */
	char *name;			/* 0x04 */
	int Refs;			/* 0x08 */
	struct CNodo3D *sig;		/* 0x0c */
	struct CNodo3D *ant;		/* 0x10 */
	struct CNodo3D *padre;		/* 0x14 */
	uint32_t flags;			/* 0x18 NODO_* */
	vec3_t pos;			/* 0x1c */
	quat_t rot;			/* 0x28 */
	int pad_38;
	IObjeto3D *obj3D;		/* 0x3c owned */
	CEscena *escena;		/* 0x40 */
	int indice;			/* 0x44 the track of the node in the animations, -1: none */
	int i48;			/* 0x48 -1 */
	int nodoR;			/* 0x4c -1, else Update goes to CNodoR_UpdateNodo */
	int i50;			/* 0x50 */
	int i54;			/* 0x54 CEscena_SetNodos54 */
	IObjeto3D *obj58;		/* 0x58 owned, deleted through its vftable[0] */
	matrix_t m;			/* 0x5c world, the position in m[12..14] */
	matrix_t mObj;			/* 0x9c the object in the node */
	quat_t qLook;			/* 0xdc the orientation for the looks 3 and 4 */
	vec3_t escala;			/* 0xec */
	CArchINI *ini;			/* 0xf8 */
} CNodo3D;
SD_OFFSET(CNodo3D, flags, 0x18);
SD_OFFSET(CNodo3D, obj3D, 0x3c);
SD_OFFSET(CNodo3D, m, 0x5c);
SD_OFFSET(CNodo3D, qLook, 0xdc);
SD_OFFSET(CNodo3D, ini, 0xf8);
SD_SIZE(CNodo3D, 0xfc);

CNodo3D *__fastcall CNodo3D_ctor(CNodo3D *self);
void __fastcall CNodo3D_dtor(CNodo3D *self);
CNodo3D *__thiscall CNodo3D_Delete(CNodo3D *self, byte flags);
/* the object of the render node if it has one, else the node's */
IObjeto3D *__thiscall CNodo3D_GetObj3D(CNodo3D *self, CNodoR *r);
/* the box of the object (and its matrix into m if not NULL); 0 for classes without one */
byte __thiscall CNodo3D_GetBBox(CNodo3D *self, matrix_t *m, box_t *bbox, byte recalc);
/* position and rotation of channel c (pos and rot blended with the second animation); 0 without a track */
byte __thiscall CNodo3D_AnimChannel(CNodo3D *self, NodoAnimChannel *c, vec3_t *pos, quat_t *rot);
void __thiscall CNodo3D_Render(CNodo3D *self, void *arg, CNodoR *r);
/* the world matrix from the animation, the parent and the look of r; the object updated */
void __thiscall CNodo3D_Update(CNodo3D *self, CNodoR *r);
/* the first node from this one on with a camera or light object named name (NULL: any) */
IObjeto3D *__thiscall CNodo3D_FindCamara(CNodo3D *self, const char *name);
IObjeto3D *__thiscall CNodo3D_FindLuz(CNodo3D *self, const char *name);
/* along sig, from this node (incl) or the next one */
CNodo3D *__thiscall CNodo3D_Find(CNodo3D *self, const char *name, byte incl);
/* along ant from the previous node */
CNodo3D *__thiscall CNodo3D_FindPrev(CNodo3D *self, const char *name);
CNodo3D *__thiscall CNodo3D_FindWild(CNodo3D *self, const char *pattern, byte incl);
uint32_t __fastcall CNodo3D_Clase(CNodo3D *self);
/* the device mesh of a CMallaTri, the nodes of a CSuelo; 0 for other classes */
byte __thiscall CNodo3D_BuildD3D(CNodo3D *self, uint32_t flags);
/* out of the list of the scene */
void __fastcall CNodo3D_Unlink(CNodo3D *self);
void __fastcall CNodo3D_FreeObj3D(CNodo3D *self);
void __thiscall CNodo3D_SetAnt(CNodo3D *self, CNodo3D *n);
void __thiscall CNodo3D_SetObj3D(CNodo3D *self, IObjeto3D *obj);
void __thiscall CNodo3D_SetPadre(CNodo3D *self, CNodo3D *n);
void __thiscall CNodo3D_SetPadreName(CNodo3D *self, const char *name);
void __thiscall CNodo3D_SetSig(CNodo3D *self, CNodo3D *n);
/* the owned objects and the parent released */
void __fastcall CNodo3D_Free(CNodo3D *self);
/* a, q and b combined (b around the result); the quaternions of a and b are made into one buffer, so with both
 * not zero b's is lost and a's is used twice, as in the original */
quat_t *Rot_Combine(quat_t *out, CRotacion *a, const quat_t *q, CRotacion *b);
quat_t *Rot_Combine2(quat_t *out, CRotacion *a, CRotacion *b);
/* the track of the node in animation 0 or 1 of channel ch; NULL without */
void *__thiscall CNodo3D_Track(CNodo3D *self, uint32_t ch);
void *__thiscall CNodo3D_Track2(CNodo3D *self, uint32_t ch);
/* the color of the node's animated light (color times *mult) from channel 0; 0 without one */
byte __thiscall CNodo_LightColor(CNodo3D *self, vec3_t *color, float *mult);
/* the member at 0x20 of the track of channel ch */
CEVA *__thiscall CNodo3D_TrackData(CNodo3D *self, uint32_t ch);
/* an animated float (the camera's fov) from channel 0 */
byte __thiscall CNodo3D_AnimFloat(CNodo3D *self, float *out);
/* position and rotation from the first channel (3 down to 0) that has the node */
byte __thiscall CNodo3D_SampleAnim(CNodo3D *self, vec3_t *pos, quat_t *rot);
/* the animated visibility (0..255) from channel 0 */
byte __thiscall CNodo3D_AnimAlpha(CNodo3D *self, byte *out);
/* the box of a CSuelo (objeto3d.h) */
box_t *__thiscall CSuelo_GetBox(void *self, box_t *out);
NodoAnimChannel *__thiscall CNodo3D_Channel(CNodo3D *self, uint32_t ch);
quat_t *__thiscall CRotacion_GetQuat(CRotacion *self, quat_t *out);

/* other modules, called from Nodo3D.cpp (src/imports.inc) */
void __thiscall CRotacion_SetEuler(CRotacion *self, const vec3_t *ang);	/* rva 0x212510 */
void *CSistPart_Cast(IObjeto3D *obj);	/* rva 0x23f310, CHECK of the class */
/* Engine/NodoR.cpp (src/game/engine3d/Engine/NodoR.c, the rest in modelo.h and scrapmodel.h) */
NodoLook *__fastcall NodoLook_ctor(NodoLook *self);
CNodoR *__fastcall CNodoR_ctor(CNodoR *self);
byte __thiscall CNodo3D_DebugDraw(CNodo3D *self, CNodoR *r);	/* rva 0x23d9d0, R_ShowLocalBox; 0: not drawn */
void __thiscall CNodoR_UpdateNodo(CNodoR *self);	/* rva 0x23f5d0 */
int __thiscall CNodoR_ApplyFX(CNodoR *self, matrix_t *m);	/* rva 0x23f540 */

#endif
