/*
 * CModel of the engine3d (original sources Engine/Modelo.cpp and Engine/util_escena.cpp): a model, an instance of a
 * scene (CScene, the loaded M3D or SM3 file) with its own position, animations and render state. The scenes are
 * reference counted (Refs at 0x8) and shared between the models made from the same file; util_escena.cpp keeps the
 * lists of the scenes and models it created and the array of the models to render (gModelosRender).
 */
#ifndef SCRAP_MODEL_H
#define SCRAP_MODEL_H

#include "scrap/animgroup.h"
#include "scrap/archio.h"
#include "scrap/scene.h"
#include "scrap/light.h"
#include "scrap/mathlib.h"
#include "scrap/node3d.h"
#include "scrap/groundportal.h"
#include "scrap/table.h"

#define CMODEL_VFTABLE GAME_VFTABLE(CModel_vftable, 0x3da6a0)
#define MAX_MODELS_RENDER 1024
#define g_ModelsOn GAME_GLOBAL(byte, g_ModelsOn, 0x432418)	/* RenderModels: the models are drawn */

/* creation flags of the scenes and models (crear_modelo, crear_escena) */
enum {
	SCENE_SHARED = 0x01,	/* share an equal scene that was loaded before */
	SCENE_AUTOINIT = 0x02,		/* in the model list, device objects made at once */
	SCENE_USEANIM = 0x04,		/* the animation of the scene file as the first one of the model */
	SCENE_IGNORE_SCALE = 0x08,	/* shared regardless of the scale */
	SCENE_SM3 = 0x10,		/* from the .sm3 file when there is one */
	SCENE_F20 = 0x20,
};

/* an animation slot: the animation and the one it blends out of (a NodeAnimPair for the render context) */
typedef struct CAnimSlot {
	CAnimGroup *anim;		/* 0x0 */
	CAnimGroup *prev;		/* 0x4 */
	float blend;			/* 0x8 the weight of prev, from 1 down to 0 */
	byte subset;			/* 0xc morph target */
	byte prevSubset;		/* 0xd */
	byte pad_e[2];
} CAnimSlot;
SD_SIZE(CAnimSlot, 0x10);

struct CModel;

/* a level of detail: the model of the level (Modelo.h) */
typedef struct CLod {
	struct CModel *model;		/* 0x0 */
	int f4;				/* 0x4 */
} CLod;
SD_SIZE(CLod, 8);

/* called before (after 0) and after (1) the model is drawn */
typedef void (*modelCallback_t)(struct CModel *m, int after, void *arg, void *arg2);

typedef struct CModel {
	void *vftable;			/* 0x000 [0] the deleting destructor */
	char *name;			/* 0x004 Cad */
	vec3_t pos;			/* 0x008 */
	vec3_t rot;			/* 0x014 */
	vec3_t scale;			/* 0x020 */
	vec3_t animOfs;			/* 0x02c root offset of the animations, set for each draw */
	vec3_t pivot;			/* 0x038 */
	vec3_t rot2;			/* 0x044 */
	vec3_t scaleBase;		/* 0x050 the scale the scene was made with */
	matrix_t local;			/* 0x05c applied after the transform */
	byte showBox;			/* 0x09c */
	byte animate;			/* 0x09d the animations play (and move the model) */
	byte dynLight;			/* 0x09e lit by the lights of the level (not StaticLighting) */
	byte alwaysVisible;		/* 0x09f */
	byte hasMesh;			/* 0x0a0 */
	byte hasShadow;			/* 0x0a1 */
	byte pad_a2[2];
	CTable slots;			/* 0x0a4 CAnimSlot[MAX_ANIM_CHANNEL] */
	CTable *renderNodes;			/* 0x0ac TabNodoR: CRenderNode[] */
	matrix_t mRender;		/* 0x0b0 the world matrix of the last draw */
	modelCallback_t callback;	/* 0x0f0 */
	void *cbArg;			/* 0x0f4 */
	void *cbArg2;			/* 0x0f8 */
	struct CModelList *list;	/* 0x0fc its entry in the model list */
	CTable lods;			/* 0x100 CLod[] */
	float lodDist;			/* 0x108 */
	int f10c;			/* 0x10c */
	int f110;			/* 0x110 */
	float sortZBias;		/* 0x114 */
	float sortZMult;		/* 0x118 */
	float stencilDist;		/* 0x11c */
	byte enable;			/* 0x120 in gModelosRender */
	byte visible;			/* 0x121 seen in the last frame */
	byte pad_122[2];
	CScene *scene;		/* 0x124 */
	struct CModel *mainLOD;	/* 0x128 the model this one is a LOD of */
	CAnimGroups anims;		/* 0x12c */
	SectorVis sector;		/* 0x1dc the sector lookup of the position */
} CModel;
SD_OFFSET(CModel, local, 0x5c);
SD_OFFSET(CModel, showBox, 0x9c);
SD_OFFSET(CModel, slots, 0xa4);
SD_OFFSET(CModel, mRender, 0xb0);
SD_OFFSET(CModel, lods, 0x100);
SD_OFFSET(CModel, enable, 0x120);
SD_OFFSET(CModel, scene, 0x124);
SD_OFFSET(CModel, anims, 0x12c);
SD_OFFSET(CModel, sector, 0x1dc);
SD_SIZE(CModel, 0x1f0);

/* util_escena.cpp: an entry of the scene list */
typedef struct CSceneList {
	CScene *scene;		/* 0x00 a reference */
	char *name;			/* 0x04 Cad */
	char *name2;			/* 0x08 Cad */
	vec3_t scale;			/* 0x0c */
	uint32_t flags;			/* 0x18 ESCENA_* */
	struct CSceneList *next;	/* 0x1c */
} CSceneList;
SD_SIZE(CSceneList, 0x20);

/* an entry of the model list; the models to render point to their entry from gModelosRender */
typedef struct CModelList {
	CModel *model;		/* 0x00 owned */
	char *name;			/* 0x04 Cad */
	char *name2;			/* 0x08 Cad */
	uint32_t flags;			/* 0x0c ESCENA_* */
	struct CModelList *next;	/* 0x10 */
	int renderIndex;		/* 0x14 in gModelosRender, -1 none */
	float sortKey;			/* 0x18 */
} CModelList;
SD_SIZE(CModelList, 0x1c);


typedef void(__thiscall *sceneDelete_t)(CScene *self, int flags);
typedef void(__thiscall *modelDelete_t)(CModel *self, int flags);

/* a reference to a scene dropped, the scene deleted with the last one */
static inline void Scene_Unref(CScene **slot) {
	(*slot)->Refs--;
	CScene *e = *slot;
	if (!e->Refs && e) {
		((sceneDelete_t)e->vftable[0])(e, 1);
	}
	*slot = NULL;
}

#define g_ModelList GAME_GLOBAL(CModelList *, g_ModelList, 0x4ae778)
#define g_SceneList GAME_GLOBAL(CSceneList *, g_SceneList, 0x4ae77c)
#define g_ModelsRender (GAME_ARRAY(CModelList *, g_ModelsRender, 0x4ae7c8))
#define g_NumModelsRender GAME_GLOBAL_IN(int, g_ModelsRender, 4096, 0x4af7c8)
#define g_MaxModelsRender GAME_GLOBAL_IN(int, g_ModelsRender, 4100, 0x4af7cc)	/* the most there were */

/* Engine/Modelo.cpp (src/game/engine3d/Engine/Model.c) */
CModel *__fastcall CModel_ctor(CModel *self);
void __fastcall CModel_dtor(CModel *self);
CModel *__thiscall CModel_Delete(CModel *self, byte flags);
/* table.h instances: free of an array of plain elements, delete[] of the LODs */
void __fastcall Table_DeleteLods(CTable *t);
CLod *__fastcall CLod_ctor(CLod *self);
void __fastcall CLod_dtor(CLod *self);	/* CHECK(!Model) */
/* the vector deleting destructors (flags 2: an array with the count before it, 1: free) */
void *__thiscall CLod_DeleteVec(CLod *self, uint32_t flags);
void *__thiscall CRenderNode_DeleteVec(CRenderNode *self, uint32_t flags);
void __fastcall CModel_Disable(CModel *self);
void __fastcall CModel_Enable(CModel *self);
/* the device objects of the scene (with scene) */
void __thiscall CModel_ReleaseD3D(CModel *self, void *dev, byte scene);
void __thiscall CModel_InitD3D(CModel *self, void *dev, byte scene);
CAnimSlot *__thiscall CModel_GetSlot(CModel *self, uint32_t i);
/* the speed of an animation with the global speed, 0 while the animations are frozen (the x87 product) */
double CAnim_GetSpeed(CAnimGroup *a);
/* the channels of the context from the slots, the world matrix into ctx->m; animOfs cleared */
void __thiscall CModel_SetupRender(CModel *self, NodeAnimCtx *ctx);
CRenderNode *__thiscall CModel_GetRenderNode(CModel *self, CNode3D *node);	/* CHECK(0): not used */
/* the world matrix of render node inodo into m, with the animation of the slot when anim (and its visibility and
 * fov when asked for) */
matrix_t *__thiscall CModel_GetNodeMatrix(CModel *self, matrix_t *m, uint32_t inodo, byte anim, uint32_t slot,
	byte *visible, float *fov);
void __thiscall CModel_SetNumLods(CModel *self, int n);
void __thiscall CModel_SetLod(CModel *self, uint32_t i, CModel *model);
/* the morph entry with the tag for the animation of the slot; 0 if there is none */
byte __thiscall CModel_SetMorphSubset(CModel *self, uint32_t slot, const char *tag);
int __fastcall CModel_GetSector(CModel *self);
/* the model drawn with its scene: the visibility (sector, box, portals), the LOD, the lights of the level */
void __fastcall CModel_Render(CModel *self);
/* a LOD drawn instead (then 1); 0 without LODs or the main model near enough */
byte __fastcall CModel_UpdateLOD(CModel *self);
/* 0 not seen, 2 only the stencil shadow, 3 seen; the value of the last frame in the passes that keep it */
byte __thiscall CModel_Visibility(CModel *self, const matrix_t *m);
/* a new animation of the model from the animation scene, warns about a second one with the same name */
CAnimGroup *__thiscall CModel_AddAnim(CModel *self, CScene *anim, const char *name);
/* events at the frames start and end (negative: the first and the last frame) of the named animation */
byte __thiscall CModel_AddAnimEvents(CModel *self, const char *name, float start, float end);
void __thiscall CModel_AnimPlay(CModel *self, uint32_t slot);
int __thiscall CModel_AnimState(CModel *self, uint32_t slot);	/* ANIM_END without animation */
void __thiscall CModel_AnimStop(CModel *self, uint32_t slot);
void __thiscall CModel_AnimPause(CModel *self, uint32_t slot);
void __thiscall CModel_AnimFade(CModel *self, float t, uint32_t slot);
void __thiscall CModel_MoveForward(CModel *self, float d);
/* the slots: players updated, root motion, blending and the morph subsets; iAnim must be -1 */
void __thiscall CModel_UpdateAnims(CModel *self, float dt, int iAnim);
CNode3D *__thiscall CModel_FindClass(CModel *self, CNode3D *from, uint32_t class);
CNode3D *__thiscall CModel_FindNode(CModel *self, const char *name);
CNode3D *__thiscall CModel_FindNodeWild(CModel *self, const char *pat);
/* the render node by the name of its node (wildcards with wild, case-insensitive with nocase), its index in *idx */
CRenderNode *__thiscall CModel_FindRenderNode(CModel *self, const char *name, uint32_t *idx, byte wild, byte nocase);
/* animOfs from the animation of slot 0 of the context, blended with the previous one */
void __thiscall CModel_AnimOffset(CModel *self, NodeAnimCtx *ctx);
void __thiscall CModel_GetMatrix(CModel *self, matrix_t *m);
/* animation idx of the group into the slot, blended from the old one */
void __thiscall CModel_SetAnim(CModel *self, int idx, uint32_t slot);
void __thiscall CModel_SetAnimName(CModel *self, const char *name, uint32_t slot);	/* CHECK(0): not used */
/* the scene of the model (a reference), the settings of its [Model] section */
void __thiscall CModel_SetScene(CModel *self, CScene *scn);
void __thiscall CModel_SetCallback(CModel *self, modelCallback_t fn, void *arg, void *arg2);
/* pivot = -v, or with rel the point at v (0..1) of the scene box */
void __thiscall CModel_SetPivot(CModel *self, const vec3_t *v, byte rel, byte update);
byte __thiscall CModel_SetPivotNode(CModel *self, const char *name);
void __thiscall CModel_SetSlotAnim(CModel *self, uint32_t slot, CAnimGroup *a);
/* TabNodoR: a render node for each node of the scene, the ones with NODE_NO_RENDERNODE last */
void __thiscall CModel_BuildRenderNodes(CModel *self, int unused);
void Model_Check(CModel *m);	/* CHECK(model) */
CAnimation *__thiscall CModel_GetSlotAnimM3D(CModel *self, uint32_t slot);
CAnimGroup *__thiscall CModel_GetAnim(CModel *self, int i);
int __thiscall CModel_FindAnim(CModel *self, const char *name);	/* -1 */
const char *__thiscall CModel_GetAnimName(CModel *self, int i);
CAnimGroup *__thiscall CModel_GetSlotAnim(CModel *self, uint32_t slot);
float __thiscall CModel_GetSlotFrame(CModel *self, uint32_t slot);
/* light.h: "omni", "foco", "direc" */
const char *__fastcall CLight_TypeName(const CLight *l);

/* Engine/util_escena.cpp (src/game/engine3d/Engine/UtilScene.c) */
CSceneList *__thiscall CSceneList_ctor(CSceneList *self, CScene *scn, const char *name, const char *name2,
	const vec3_t *scale, uint32_t flags, CSceneList **list);
CModelList *__thiscall CModelList_ctor(CModelList *self, CModel *m, const char *name, const char *name2,
	uint32_t flags, CModelList **list);
void __fastcall CModelList_dtor(CModelList *self);	/* deletes the model */
/* into gModelosRender while enabled, out of it while not */
void Model_UpdateRender(CModel *m);
/* the list entry of the scene freed when only it and the caller hold the scene (force: also shared ones) */
byte SceneList_Release(CScene *scn, byte force);
/* the model deleted, with its list entry and its scene entry; *pm = NULL */
void Model_Destroy(CModel **pm, byte force);
void Model_SetEnable(CModel *m, byte on);
void Models_UpdateAnims(float dt);
void UtilScene_FreeAll(void);
void UtilScene_ReleaseD3D(void);
void UtilScene_InitD3D(void);
typedef void (*utilPrintf_t)(const char *fmt, ...);
void UtilScene_Dump(utilPrintf_t fn);	/* E3D_DebugPrintf for NULL */
void Scenes_Msg(int msg, int a, int b, int c, int d);	/* to the scenes with SCENE_AUTOINIT */
CScene *Scene_Next(CScene *scn);	/* the first one for NULL */
/* the models of gModelosRender sorted by distance and drawn */
void Models_Render(void);
CSceneList *__thiscall CSceneList_Find(CSceneList *self, const char *name, const char *name2,
	const vec3_t *scale, uint32_t flags, uint32_t mask);
CModelList *__thiscall CModelList_Find(CModelList *self, const char *name, const char *name2, uint32_t flags,
	uint32_t mask);
CModel *Model_FindName(const char *name);
int String_EqualNoCase(const char *a, const char *b);	/* NULL equals "" */
void Create_Animation(CModel *m, const char *name, uint32_t flags);
CScene *Create_Scene(byte *shared, const char *name, const char *name2, const vec3_t *scale, float threshold,
	uint32_t flags);
CModel *Create_Model(const char *name, const char *name2, const vec3_t *scale, float threshold, uint32_t flags);
CModel *Create_Model3(const char *name, const char *name2, const vec3_t *scale, uint32_t flags);
/* MeshEffect.c (efec_malla.cpp): a model registered as a screen or as a model of the mesh view */
void MeshEffect_AddScreen(CModel *m);
void MeshEffect_AddModel(CModel *m);
/* the mesh view drawn (once per frame by CEngineApp_Render) */
void MeshEffect_Render(void);
CModel *Create_ModelLOD(const char *name, const char *name2, const vec3_t *scale, CModel *main, uint32_t flags);
CModel *Create_Model2(const char *name, const char *name2, uint32_t flags);
CModel *Create_Model4(const char *name, const char *name2, float threshold, uint32_t flags);
int ModelList_CompareKey(const void *a, const void *b);	/* qsort, larger sortKey first */

/* Engine/ModelFX.cpp (src/game/engine3d/Engine/ModelFX.c): the render effects of a model, f10c the effect (1..8,
 * MODEL_EFFECT_LAST), the float in f110 its parameter. Drawn in the passes 0, 3 and 13 as several draws of the scene
 * with g_MatFX1 or g_MatForced; 6..8 need the pixel shaders. */
struct CGraphic2D;
#define g_FXMat (GAME_GLOBAL(CMaterial, g_FXMat, 0x4e1c68))	/* the colors and blends of the effects */
#define g_FXGraphicNoise1 (GAME_GLOBAL(struct CGraphic2D, g_FXGraphicNoise1, 0x4e1c20))	/* bmp/noise.tga */
#define g_FXGraphicStatic (GAME_GLOBAL(struct CGraphic2D, g_FXGraphicStatic, 0x4e1d18))	/* bmp/noise_static.alpha.tga, the scan lines */
#define g_FXGraphicNoise2 (GAME_GLOBAL(struct CGraphic2D, g_FXGraphicNoise2, 0x4e1d60))	/* bmp/noise2.tga */
#define g_MatElectric GAME_GLOBAL(CMaterial *, g_MatElectric, 0x4e1c64)	/* fx_Electric (+shader:Electric) */
#define g_MatFire GAME_GLOBAL(CMaterial *, g_MatFire, 0x4e1d5c)		/* fx_Fire (+shader:Fire) */
#define g_MatNoise GAME_GLOBAL(CMaterial *, g_MatNoise, 0x4e1da4)		/* fx_Noise (+shader:Noise) */
void __thiscall ModelFX_Render(CModel *self, NodeAnimCtx *ctx);
/* 1: a TV picture with static, 2 and 3: faded out (2 over a gray pass), 4: 1 in wire frame, 5: green glow, 6:
 * electric, 7: fire, 8: noise */
void ModelFX_Static(CModel *m, NodeAnimCtx *ctx);
void ModelFX_FadeLit(CModel *m, NodeAnimCtx *ctx);
void ModelFX_Fade(CModel *m, NodeAnimCtx *ctx);
void ModelFX_Wire(CModel *m, NodeAnimCtx *ctx);
void ModelFX_Glow(CModel *m, NodeAnimCtx *ctx);
void ModelFX_Electric(CModel *m, NodeAnimCtx *ctx);
void ModelFX_Fire(CModel *m, NodeAnimCtx *ctx);
void ModelFX_Noise(CModel *m, NodeAnimCtx *ctx);
/* the gray pass of 2 and 8, and a variant of 1 no effect uses */
void ModelFX_Base(CModel *m, NodeAnimCtx *ctx);
void ModelFX_Static2(CModel *m, NodeAnimCtx *ctx);
/* the overlays and the shader materials */
void ModelFX_InitD3D(void);

/* other modules (src/imports.inc) */
float World_TimeScale(void);						/* rva 0x20aea0 */
void __fastcall CRenderNode_dtor(CRenderNode *self);				/* rva 0x23f450 */
#define CNODOR_DTOR 0x23f450
void __thiscall CRenderNode_SetNode(CRenderNode *self, CNode3D *node);		/* rva 0x23f890 */
/* rva 0x23fac0, the matrix identity, the channels cleared; the model and its render nodes at 0xa0 and 0xa4 */
void __thiscall RenderCtx_Init(NodeAnimCtx *self, CModel *m, const vec3_t *scale);
void __thiscall RenderCtx_SetChannel(NodeAnimChannel *self, CAnimSlot *slot);	/* rva 0x23fb10 */
/* the play state of an animation group */
CScene *Scene_Load(const char *name, int arg, const char *name2);	/* rva 0x2c2d60 */

#endif
