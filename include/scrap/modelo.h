/*
 * CModelo of the engine3d (original sources Engine/Modelo.cpp and Engine/util_escena.cpp): a model, an instance of a
 * scene (CEscena, the loaded M3D or SM3 file) with its own position, animations and render state. The scenes are
 * reference counted (Refs at 0x8) and shared between the models made from the same file; util_escena.cpp keeps the
 * lists of the scenes and models it created and the array of the models to render (gModelosRender).
 */
#ifndef SCRAP_MODELO_H
#define SCRAP_MODELO_H

#include "scrap/animgrupo.h"
#include "scrap/archio.h"
#include "scrap/escena.h"
#include "scrap/luz.h"
#include "scrap/mathlib.h"
#include "scrap/nodo3d.h"
#include "scrap/sueloportal.h"
#include "scrap/tabla.h"

#define CMODELO_VFTABLE 0x3da6a0
#define MAX_MODELOS_RENDER 1024
#define g_ModelosOn GAME_DATA(byte, 0x432418)	/* RenderModels: the models are drawn */

/* creation flags of the scenes and models (crear_modelo, crear_escena) */
enum {
	ESCENA_COMPARTIR = 0x01,	/* share an equal scene that was loaded before */
	ESCENA_AUTOINIC = 0x02,		/* in the model list, device objects made at once */
	ESCENA_USARANIM = 0x04,		/* the animation of the scene file as the first one of the model */
	ESCENA_IGN_ESCALA = 0x08,	/* shared regardless of the scale */
	ESCENA_SM3 = 0x10,		/* from the .sm3 file when there is one */
	ESCENA_F20 = 0x20,
};

/* classes of the scene objects (IObjeto3D clase) */
#define CLASE_OBJ3D_MALLA 0xa1000001

/* an animation slot: the animation and the one it blends out of (a NodoAnimPair for the render context) */
typedef struct CAnimSlot {
	CAnimGrupo *anim;		/* 0x0 */
	CAnimGrupo *prev;		/* 0x4 */
	float blend;			/* 0x8 the weight of prev, from 1 down to 0 */
	byte subset;			/* 0xc morph target */
	byte prevSubset;		/* 0xd */
	byte pad_e[2];
} CAnimSlot;
SD_SIZE(CAnimSlot, 0x10);

struct CModelo;

/* a level of detail: the model of the level (Modelo.h) */
typedef struct CLod {
	struct CModelo *model;		/* 0x0 */
	int f4;				/* 0x4 */
} CLod;
SD_SIZE(CLod, 8);

/* called before (after 0) and after (1) the model is drawn */
typedef void (*modeloCallback_t)(struct CModelo *m, int after, void *arg, void *arg2);

typedef struct CModelo {
	void *vftable;			/* 0x000 [0] the deleting destructor */
	char *name;			/* 0x004 Cad */
	vec3_t pos;			/* 0x008 */
	vec3_t rot;			/* 0x014 */
	vec3_t scale;			/* 0x020 */
	vec3_t animOfs;			/* 0x02c root offset of the animations, set for each draw */
	vec3_t pivot;			/* 0x038 */
	vec3_t rot2;			/* 0x044 */
	vec3_t escala;			/* 0x050 the scale the scene was made with */
	matrix_t local;			/* 0x05c applied after the transform */
	byte showBox;			/* 0x09c */
	byte animate;			/* 0x09d the animations play (and move the model) */
	byte dynLight;			/* 0x09e lit by the lights of the level (not StaticLighting) */
	byte alwaysVisible;		/* 0x09f */
	byte hasMesh;			/* 0x0a0 */
	byte hasShadow;			/* 0x0a1 */
	byte pad_a2[2];
	CTabla slots;			/* 0x0a4 CAnimSlot[MAX_ANIM_CHANNEL] */
	CTabla *nodosR;			/* 0x0ac TabNodoR: CNodoR[] */
	matrix_t mRender;		/* 0x0b0 the world matrix of the last draw */
	modeloCallback_t callback;	/* 0x0f0 */
	void *cbArg;			/* 0x0f4 */
	void *cbArg2;			/* 0x0f8 */
	struct CListaModelo *lista;	/* 0x0fc its entry in the model list */
	CTabla lods;			/* 0x100 CLod[] */
	float lodDist;			/* 0x108 */
	int f10c;			/* 0x10c */
	int f110;			/* 0x110 */
	float sortZBias;		/* 0x114 */
	float sortZMult;		/* 0x118 */
	float stencilDist;		/* 0x11c */
	byte enable;			/* 0x120 in gModelosRender */
	byte visible;			/* 0x121 seen in the last frame */
	byte pad_122[2];
	CEscena *escena;		/* 0x124 */
	struct CModelo *mainLOD;	/* 0x128 the model this one is a LOD of */
	CAnimGrupos anims;		/* 0x12c */
	SectorVis sector;		/* 0x1dc the sector lookup of the position */
} CModelo;
SD_OFFSET(CModelo, local, 0x5c);
SD_OFFSET(CModelo, showBox, 0x9c);
SD_OFFSET(CModelo, slots, 0xa4);
SD_OFFSET(CModelo, mRender, 0xb0);
SD_OFFSET(CModelo, lods, 0x100);
SD_OFFSET(CModelo, enable, 0x120);
SD_OFFSET(CModelo, escena, 0x124);
SD_OFFSET(CModelo, anims, 0x12c);
SD_OFFSET(CModelo, sector, 0x1dc);
SD_SIZE(CModelo, 0x1f0);

/* util_escena.cpp: an entry of the scene list */
typedef struct CListaEscena {
	CEscena *escena;		/* 0x00 a reference */
	char *name;			/* 0x04 Cad */
	char *name2;			/* 0x08 Cad */
	vec3_t escala;			/* 0x0c */
	uint32_t flags;			/* 0x18 ESCENA_* */
	struct CListaEscena *next;	/* 0x1c */
} CListaEscena;
SD_SIZE(CListaEscena, 0x20);

/* an entry of the model list; the models to render point to their entry from gModelosRender */
typedef struct CListaModelo {
	CModelo *modelo;		/* 0x00 owned */
	char *name;			/* 0x04 Cad */
	char *name2;			/* 0x08 Cad */
	uint32_t flags;			/* 0x0c ESCENA_* */
	struct CListaModelo *next;	/* 0x10 */
	int indiceRender;		/* 0x14 in gModelosRender, -1 none */
	float sortKey;			/* 0x18 */
} CListaModelo;
SD_SIZE(CListaModelo, 0x1c);


typedef void(__thiscall *escenaDelete_t)(CEscena *self, int flags);
typedef void(__thiscall *modeloDelete_t)(CModelo *self, int flags);

/* a reference to a scene dropped, the scene deleted with the last one */
static inline void Escena_Unref(CEscena **slot) {
	(*slot)->Refs--;
	CEscena *e = *slot;
	if (!e->Refs && e) {
		((escenaDelete_t)e->vftable[0])(e, 1);
	}
	*slot = NULL;
}

#define g_ListaModelos GAME_DATA(CListaModelo *, 0x4ae778)
#define g_ListaEscenas GAME_DATA(CListaEscena *, 0x4ae77c)
#define g_ModelosRender ((CListaModelo **)Game_Ptr(0x4ae7c8))
#define g_NumModelosRender GAME_DATA(int, 0x4af7c8)
#define g_MaxModelosRender GAME_DATA(int, 0x4af7cc)	/* the most there were */

/* Engine/Modelo.cpp (src/game/engine3d/Engine/Modelo.c) */
CModelo *__fastcall CModelo_ctor(CModelo *self);
void __fastcall CModelo_dtor(CModelo *self);
CModelo *__thiscall CModelo_Delete(CModelo *self, byte flags);
/* tabla.h instances: free of an array of plain elements, delete[] of the LODs */
void __fastcall Tabla_DeleteLods(CTabla *t);
void __fastcall CLod_dtor(CLod *self);	/* CHECK(!Model) */
/* the vector deleting destructors (flags 2: an array with the count before it, 1: free) */
void *__thiscall CLod_DeleteVec(CLod *self, uint32_t flags);
void *__thiscall CNodoR_DeleteVec(CNodoR *self, uint32_t flags);
void __fastcall CModelo_Disable(CModelo *self);
void __fastcall CModelo_Enable(CModelo *self);
/* the device objects of the scene (with escena) */
void __thiscall CModelo_ReleaseD3D(CModelo *self, void *dev, byte escena);
void __thiscall CModelo_InitD3D(CModelo *self, void *dev, byte escena);
CAnimSlot *__thiscall CModelo_GetSlot(CModelo *self, uint32_t i);
/* the speed of an animation with the global speed, 0 while the animations are frozen (the x87 product) */
double CAnim_GetSpeed(CAnimGrupo *a);
/* the channels of the context from the slots, the world matrix into ctx->m; animOfs cleared */
void __thiscall CModelo_SetupRender(CModelo *self, NodoAnimCtx *ctx);
CNodoR *__thiscall CModelo_GetNodoR(CModelo *self, CNodo3D *nodo);	/* CHECK(0): not used */
/* the world matrix of render node inodo into m, with the animation of the slot when anim (and its visibility and
 * fov when asked for) */
matrix_t *__thiscall CModelo_GetNodeMatrix(CModelo *self, matrix_t *m, uint32_t inodo, byte anim, uint32_t slot,
	byte *visible, float *fov);
void __thiscall CModelo_SetNumLods(CModelo *self, int n);
void __thiscall CModelo_SetLod(CModelo *self, uint32_t i, CModelo *model);
/* the morph entry with the tag for the animation of the slot; 0 if there is none */
byte __thiscall CModelo_SetMorphSubset(CModelo *self, uint32_t slot, const char *tag);
int __fastcall CModelo_GetSector(CModelo *self);
/* a new animation of the model from the animation scene, warns about a second one with the same name */
CAnimGrupo *__thiscall CModelo_AddAnim(CModelo *self, CEscena *anim, const char *nombre);
/* events at the frames start and end (negative: the first and the last frame) of the named animation */
byte __thiscall CModelo_AddAnimEvents(CModelo *self, const char *name, float start, float end);
void __thiscall CModelo_AnimPlay(CModelo *self, uint32_t slot);
int __thiscall CModelo_AnimState(CModelo *self, uint32_t slot);	/* ANIM_FIN without animation */
void __thiscall CModelo_AnimStop(CModelo *self, uint32_t slot);
void __thiscall CModelo_AnimPause(CModelo *self, uint32_t slot);
void __thiscall CModelo_AnimFade(CModelo *self, float t, uint32_t slot);
void __thiscall CModelo_MoveForward(CModelo *self, float d);
/* the slots: players updated, root motion, blending and the morph subsets; iAnim must be -1 */
void __thiscall CModelo_UpdateAnims(CModelo *self, float dt, int iAnim);
CNodo3D *__thiscall CModelo_FindClase(CModelo *self, CNodo3D *from, uint32_t clase);
CNodo3D *__thiscall CModelo_FindNodo(CModelo *self, const char *name);
CNodo3D *__thiscall CModelo_FindNodoWild(CModelo *self, const char *pat);
/* the render node by the name of its node (wildcards with wild, case-insensitive with nocase), its index in *idx */
CNodoR *__thiscall CModelo_FindNodoR(CModelo *self, const char *name, uint32_t *idx, byte wild, byte nocase);
/* animOfs from the animation of slot 0 of the context, blended with the previous one */
void __thiscall CModelo_AnimOffset(CModelo *self, NodoAnimCtx *ctx);
void __thiscall CModelo_GetMatrix(CModelo *self, matrix_t *m);
/* animation idx of the group into the slot, blended from the old one */
void __thiscall CModelo_SetAnim(CModelo *self, int idx, uint32_t slot);
void __thiscall CModelo_SetAnimName(CModelo *self, const char *name, uint32_t slot);	/* CHECK(0): not used */
/* the scene of the model (a reference), the settings of its [Model] section */
void __thiscall CModelo_SetEscena(CModelo *self, CEscena *esc);
void __thiscall CModelo_SetCallback(CModelo *self, modeloCallback_t fn, void *arg, void *arg2);
/* pivot = -v, or with rel the point at v (0..1) of the scene box */
void __thiscall CModelo_SetPivot(CModelo *self, const vec3_t *v, byte rel, byte update);
byte __thiscall CModelo_SetPivotNodo(CModelo *self, const char *name);
void __thiscall CModelo_SetSlotAnim(CModelo *self, uint32_t slot, CAnimGrupo *a);
/* TabNodoR: a render node for each node of the scene, the ones with NODO_NO_TABNODOR last */
void __thiscall CModelo_BuildNodosR(CModelo *self, int unused);
void Modelo_Check(CModelo *m);	/* CHECK(modelo) */
CAnimacion *__thiscall CModelo_GetSlotAnimM3D(CModelo *self, uint32_t slot);
CAnimGrupo *__thiscall CModelo_GetAnim(CModelo *self, int i);
int __thiscall CModelo_FindAnim(CModelo *self, const char *name);	/* -1 */
const char *__thiscall CModelo_GetAnimName(CModelo *self, int i);
CAnimGrupo *__thiscall CModelo_GetSlotAnim(CModelo *self, uint32_t slot);
float __thiscall CModelo_GetSlotFrame(CModelo *self, uint32_t slot);
/* luz.h: "omni", "foco", "direc" */
const char *__fastcall CLuz_TipoNombre(const CLuz *l);

/* Engine/util_escena.cpp (src/game/engine3d/Engine/UtilEscena.c) */
CListaEscena *__thiscall CListaEscena_ctor(CListaEscena *self, CEscena *esc, const char *name, const char *name2,
	const vec3_t *escala, uint32_t flags, CListaEscena **lista);
CListaModelo *__thiscall CListaModelo_ctor(CListaModelo *self, CModelo *m, const char *name, const char *name2,
	uint32_t flags, CListaModelo **lista);
void __fastcall CListaModelo_dtor(CListaModelo *self);	/* deletes the model */
/* into gModelosRender while enabled, out of it while not */
void Modelo_UpdateRender(CModelo *m);
/* the list entry of the scene freed when only it and the caller hold the scene (force: also shared ones) */
byte Escena_Release(CEscena *esc, byte force);
/* the model deleted, with its list entry and its scene entry; *pm = NULL */
void Modelo_Destroy(CModelo **pm, byte force);
void Modelo_SetEnable(CModelo *m, byte on);
void Modelos_UpdateAnims(float dt);
void UtilEscena_FreeAll(void);
void UtilEscena_ReleaseD3D(void);
void UtilEscena_InitD3D(void);
typedef void (*utilPrintf_t)(const char *fmt, ...);
void UtilEscena_Dump(utilPrintf_t fn);	/* E3D_DebugPrintf for NULL */
void Escenas_Msg(int msg, int a, int b, int c, int d);	/* to the scenes with ESCENA_AUTOINIC */
CEscena *Escena_Next(CEscena *esc);	/* the first one for NULL */
/* the models of gModelosRender sorted by distance and drawn */
void Modelos_Render(void);
CListaEscena *__thiscall CListaEscena_Find(CListaEscena *self, const char *name, const char *name2,
	const vec3_t *escala, uint32_t flags, uint32_t mask);
CListaModelo *__thiscall CListaModelo_Find(CListaModelo *self, const char *name, const char *name2, uint32_t flags,
	uint32_t mask);
CModelo *Modelo_FindName(const char *name);
int Cad_EqualNoCase(const char *a, const char *b);	/* NULL equals "" */
void Crear_Animacion(CModelo *m, const char *name, uint32_t flags);
CEscena *Crear_Escena(byte *shared, const char *name, const char *name2, const vec3_t *escala, float umbral,
	uint32_t flags);
CModelo *Crear_Modelo(const char *name, const char *name2, const vec3_t *escala, float umbral, uint32_t flags);
CModelo *Crear_Modelo3(const char *name, const char *name2, const vec3_t *escala, uint32_t flags);
CModelo *Crear_ModeloLOD(const char *name, const char *name2, const vec3_t *escala, CModelo *main, uint32_t flags);
CModelo *Crear_Modelo2(const char *name, const char *name2, uint32_t flags);
CModelo *Crear_Modelo4(const char *name, const char *name2, float umbral, uint32_t flags);
int ListaModelo_CompareKey(const void *a, const void *b);	/* qsort, larger sortKey first */

/* other modules (src/imports.inc) */
float World_TimeScale(void);						/* rva 0x20aea0 */
void Matrix_PostScale(matrix_t *m, const vec3_t *s);			/* rva 0x239210 */
void Matrix_PostRotate(matrix_t *m, const vec3_t *rot);			/* rva 0x23a100, yaw y pitch x roll z */
void __fastcall CNodoR_dtor(CNodoR *self);				/* rva 0x23f450 */
#define CNODOR_DTOR 0x23f450
void __thiscall CNodoR_SetNodo(CNodoR *self, CNodo3D *nodo);		/* rva 0x23f890 */
/* rva 0x23fac0, the matrix identity, the channels cleared; the model and its render nodes at 0xa0 and 0xa4 */
void __thiscall RenderCtx_Init(NodoAnimCtx *self, CModelo *m, const vec3_t *scale);
void __thiscall RenderCtx_SetCanal(NodoAnimChannel *self, CAnimSlot *slot);	/* rva 0x23fb10 */
/* the play state of an animation group */
void __fastcall CModelo_Render(CModelo *self);				/* rva 0x253610 */
void __thiscall Font_SetColor(void *font, uint32_t color);		/* rva 0x299980 */
void Font_Printf(void *font, float x, float y, int arg, const char *fmt, ...);	/* rva 0x29b850 */
void Font_PrintfXY(void *font, float x, float y, const char *fmt, ...);		/* rva 0x299ce0 */
CEscena *SM3_Load(const char *name);					/* rva 0x2bc5c0 */
void Escena_PostLoad(const char *name, CEscena *esc);			/* rva 0x2c0e40 */
CEscena *Escena_Load(const char *name, int arg, const char *name2);	/* rva 0x2c2d60 */

#endif
