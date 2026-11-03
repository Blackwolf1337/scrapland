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

#define MAX_MODELS_RENDER 1024
extern byte g_ModelsOn;	/* RenderModels: the models are drawn */

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

struct CModel;

/* a level of detail: the model of the level (Modelo.h) */
typedef struct CLod {
	struct CModel *model;		/* 0x0 */
	int unused_04;				/* 0x4 */
} CLod;

/* called before (after 0) and after (1) the model is drawn */
typedef void (*modelCallback_t)(struct CModel *m, int after, void *arg, void *arg2);

typedef struct CModel {
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
	int renderEffect;		/* 0x10c MODEL_EFFECT_*, ModelFX_Render */
	int effectParam;		/* 0x110 the float parameter of the effect (0..1) as bits */
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

/* util_escena.cpp: an entry of the scene list */
typedef struct CSceneList {
	CScene *scene;		/* 0x00 a reference */
	char *name;			/* 0x04 Cad */
	char *name2;			/* 0x08 Cad */
	vec3_t scale;			/* 0x0c */
	uint32_t flags;			/* 0x18 ESCENA_* */
	struct CSceneList *next;	/* 0x1c */
} CSceneList;

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

/* a reference to a scene dropped, the scene deleted with the last one */
static inline void Scene_Unref(CScene **slot) {
	(*slot)->Refs--;
	CScene *e = *slot;
	if (!e->Refs && e) {
		ObjRef_Delete(e, 1);
	}
	*slot = NULL;
}

/* Engine/Modelo.cpp (src/game/engine3d/Engine/Model.c) */
CModel *CModel_ctor(CModel *self);
CLod *CLod_ctor(CLod *self);
void CLod_dtor(CLod *self);	/* CHECK(!Model) */
void CModel_Disable(CModel *self);
void CModel_Enable(CModel *self);
/* the device objects of the scene (with scene) */
void CModel_ReleaseD3D(CModel *self, void *dev, byte scene);
void CModel_InitD3D(CModel *self, void *dev, byte scene);
CAnimSlot *CModel_GetSlot(CModel *self, uint32_t i);
/* the channels of the context from the slots, the world matrix into ctx->m; animOfs cleared */
void CModel_SetupRender(CModel *self, NodeAnimCtx *ctx);
/* the world matrix of render node inodo into m, with the animation of the slot when anim (and its visibility and
 * fov when asked for) */
matrix_t *CModel_GetNodeMatrix(CModel *self, matrix_t *m, uint32_t inodo, byte anim, uint32_t slot,
	byte *visible, float *fov);
void CModel_SetNumLods(CModel *self, int n);
void CModel_SetLod(CModel *self, uint32_t i, CModel *model);
/* the morph entry with the tag for the animation of the slot; 0 if there is none */
byte CModel_SetMorphSubset(CModel *self, uint32_t slot, const char *tag);
int CModel_GetSector(CModel *self);
/* the model drawn with its scene: the visibility (sector, box, portals), the LOD, the lights of the level */
void CModel_Render(CModel *self);
/* a new animation of the model from the animation scene, warns about a second one with the same name */
CAnimGroup *CModel_AddAnim(CModel *self, CScene *anim, const char *name);
/* events at the frames start and end (negative: the first and the last frame) of the named animation */
byte CModel_AddAnimEvents(CModel *self, const char *name, float start, float end);
/* the slots: players updated, root motion, blending and the morph subsets; iAnim must be -1 */
void CModel_UpdateAnims(CModel *self, float dt, int iAnim);
CNode3D *CModel_FindNode(CModel *self, const char *name);
CNode3D *CModel_FindNodeWild(CModel *self, const char *pat);
/* the render node by the name of its node (wildcards with wild, case-insensitive with nocase), its index in *idx */
CRenderNode *CModel_FindRenderNode(CModel *self, const char *name, uint32_t *idx, byte wild, byte nocase);
void CModel_GetMatrix(CModel *self, matrix_t *m);
/* animation idx of the group into the slot, blended from the old one */
void CModel_SetAnim(CModel *self, int idx, uint32_t slot);
/* the scene of the model (a reference), the settings of its [Model] section */
void CModel_SetScene(CModel *self, CScene *scn);
/* pivot = -v, or with rel the point at v (0..1) of the scene box */
void CModel_SetPivot(CModel *self, const vec3_t *v, byte rel, byte update);
byte CModel_SetPivotNode(CModel *self, const char *name);
/* TabNodoR: a render node for each node of the scene, the ones with NODE_NO_RENDERNODE last */
void CModel_BuildRenderNodes(CModel *self, int unused);
CAnimGroup *CModel_GetAnim(CModel *self, int i);
int CModel_FindAnim(CModel *self, const char *name);	/* -1 */
const char *CModel_GetAnimName(CModel *self, int i);

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
CModel *Model_FindName(const char *name);
void Create_Animation(CModel *m, const char *name, uint32_t flags);
CModel *Create_Model3(const char *name, const char *name2, const vec3_t *scale, uint32_t flags);
/* MeshEffect.c (efec_malla.cpp): a model registered as a screen or as a model of the mesh view */
void MeshEffect_AddScreen(CModel *m);
void MeshEffect_AddModel(CModel *m);
/* the mesh view drawn (once per frame by CEngineApp_Render) */
void MeshEffect_Render(void);
CModel *Create_ModelLOD(const char *name, const char *name2, const vec3_t *scale, CModel *main, uint32_t flags);
CModel *Create_Model2(const char *name, const char *name2, uint32_t flags);

/* CModel.renderEffect (SetEffect of the entities and the scripts) */
enum {
	MODEL_EFFECT_NONE,
	MODEL_EFFECT_STATIC,
	MODEL_EFFECT_FADE_LIT,
	MODEL_EFFECT_FADE,
	MODEL_EFFECT_WIRE,
	MODEL_EFFECT_GLOW,
	MODEL_EFFECT_ELECTRIC,
	MODEL_EFFECT_FIRE,
	MODEL_EFFECT_NOISE,
	MODEL_EFFECT_LAST = MODEL_EFFECT_NOISE
};

/* Engine/ModelFX.cpp (src/game/engine3d/Engine/ModelFX.c): the render effects of a model, renderEffect the effect, the
 * float in effectParam its parameter. Drawn in the passes 0, 3 and 13 as several draws of the scene with g_MatFX1 or
 * g_MatForced; ELECTRIC, FIRE and NOISE need the pixel shaders. */
struct CGraphic2D;
void ModelFX_Render(CModel *self, NodeAnimCtx *ctx);
/* the overlays and the shader materials */
void ModelFX_InitD3D(void);

/* other modules (src/imports.inc) */
float World_TimeScale(void);
void CRenderNode_dtor(CRenderNode *self);
void CRenderNode_SetNode(CRenderNode *self, CNode3D *node);
/* rva 0x23fac0, the matrix identity, the channels cleared; the model and its render nodes at 0xa0 and 0xa4 */
void RenderCtx_Init(NodeAnimCtx *self, CModel *m, const vec3_t *scale);
void RenderCtx_SetChannel(NodeAnimChannel *self, CAnimSlot *slot);
/* the play state of an animation group */
CScene *Scene_Load(const char *name, int arg, const char *name2);
CModel *CModel_Delete(CModel *self, byte flags);

#endif
