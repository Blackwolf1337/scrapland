/*
 * ScrapEngModel.cpp of the game: CGameModel, the model of an entity: a CModelo of the engine3d made with its LOD
 * models (&U&D&N.LODn), the collision mesh of its meshes in the floor of the world and the access of the entities to
 * its animations, render nodes and vertices. Animation channels ("chan") are the slots of the CModelo.
 */
#ifndef SCRAP_SCRAPMODEL_H
#define SCRAP_SCRAPMODEL_H

#include "scrap/modelo.h"
#include "scrap/suelo.h"

typedef struct CGameModel {
	CModelo *Model;			/* 0x00 */
	CCollideMesh *CollideMesh;	/* 0x04 */
	int CollisionMeshID;		/* 0x08 -1: the model has none */
	int CollisionIni;		/* 0x0c the first triangle in the floor */
	byte collide;			/* 0x10 on */
	byte pad_11[3];
	float animBaseFPS;		/* 0x14 AnimBaseFPS of the ini, the frames of the animations scaled to it */
	int frame;			/* 0x18 g_FrameCount of the last transform of the render nodes */
} CGameModel;
SD_SIZE(CGameModel, 0x1c);

#define MODEL_EFFECT_LAST 8
#define MAX_RENDERFX_PARAM 1
#define MAX_VALIDOS 256
#define MAT_SINVERTICES 0x4000		/* the meshes of the material give no vertices to the effects */

/* the engine variables (Gvars) */
#define g_GetModelVertex GAME_DATA(int, 0x4a80c4)		/* R_GetModelVertex, else the model position */
#define g_ForcePreRenderTransform GAME_DATA(int, 0x4a8034)	/* R_ForcePreRenderTransform */
#define g_SplitScreen GAME_DATA(int, 0x4a8c8c)		/* R_SplitScreen: every model visible */
#define g_CoefMLODShips GAME_DATA(float, 0x4a80f4)
#define g_CoefMLODPilots GAME_DATA(float, 0x4a80dc)
#define g_CoefMLODEngines GAME_DATA(float, 0x4a8064)
#define g_CoefMLODTraffic GAME_DATA(float, 0x4a804c)
#define g_CoefMLODCharacters GAME_DATA(float, 0x4a80ac)
#define g_CoefMLODOutElement GAME_DATA(float, 0x4a807c)
#define g_CoefMLODMenuElement GAME_DATA(float, 0x4a8094)

CGameModel *__thiscall CGameModel_ctor(CGameModel *self, const char *name);
/* the model of name (the .sm3 if the name is .m3d and the other way with the app flag 4) at the scale */
void __thiscall CGameModel_Load(CGameModel *self, const char *name, const char *name2, const vec3_t *scale);
void __thiscall CGameModel_LoadName(CGameModel *self, const char *name);
void __thiscall CGameModel_LoadScaled(CGameModel *self, const char *name, const vec3_t *scale);
/* the model freed (Load with NULL) */
void __fastcall CGameModel_Reset(CGameModel *self);
/* free: the scenes of its animations and LODs released with it */
void __thiscall CGameModel_Release(CGameModel *self, byte free);
byte __fastcall CGameModel_IsLoaded(const CGameModel *self);
const char *__fastcall CGameModel_Name(const CGameModel *self);
const char *__fastcall CGameModel_SceneName(const CGameModel *self);
void __fastcall CGameModel_Enable(CGameModel *self);
void __fastcall CGameModel_Disable(CGameModel *self);
byte __fastcall CGameModel_IsEnabled(const CGameModel *self);
byte __fastcall CGameModel_IsVisible(const CGameModel *self);
int __fastcall CGameModel_GetSector(CGameModel *self);
float __fastcall CGameModel_Radius(const CGameModel *self);
vec3_t *__thiscall CGameModel_GetPos(const CGameModel *self, vec3_t *out);
void __thiscall CGameModel_SetPos(CGameModel *self, const vec3_t *v);
vec3_t *__thiscall CGameModel_GetRot(const CGameModel *self, vec3_t *out);
void __thiscall CGameModel_SetRot(CGameModel *self, const vec3_t *v);
void __thiscall CGameModel_SetRot2(CGameModel *self, const vec3_t *v);
/* d along z turned by rot2 and rot */
void __thiscall CGameModel_MoveForward(CGameModel *self, float d);
void __thiscall CGameModel_SetPivot(CGameModel *self, const vec3_t *v, byte rel, byte recalcBB);
void __thiscall CGameModel_SetPivotNodo(CGameModel *self, const char *name);
void __thiscall CGameModel_SetSortZ(CGameModel *self, float bias, float mult);
uint32_t __thiscall CGameModel_GetEffect(const CGameModel *self, uint32_t *out);
void __thiscall CGameModel_SetEffect(CGameModel *self, uint32_t fx);
void __thiscall CGameModel_GetEffectParam(const CGameModel *self, int i, uint32_t *out);
void __thiscall CGameModel_SetEffectParam(CGameModel *self, int i, uint32_t v);
/* the model drawn now, flags 2 with the default light, 3 also lit */
void __thiscall CGameModel_RenderNow(CGameModel *self, byte flags);

/* the animations */
int __thiscall CGameModel_AddAnim(CGameModel *self, const char *name);
int __thiscall CGameModel_FindAnim(CGameModel *self, const char *name);
const char *__thiscall CGameModel_AnimName(CGameModel *self, int i);
byte __thiscall CGameModel_AnimInfo(CGameModel *self, int i, float *dur, float *fps);
byte __thiscall CGameModel_SetAnimVel(CGameModel *self, int i, float vel);
byte __thiscall CGameModel_SetAnimFactor(CGameModel *self, int i, float factor);
byte __thiscall CGameModel_SetAnimSpeed(CGameModel *self, int i, float speed);
byte __thiscall CGameModel_SetAnimLoop(CGameModel *self, int i, byte loop);
/* "Morph,n,tag" */
void __thiscall CGameModel_AnimCmd(CGameModel *self, const char *cmd);
/* the time of frame of animation i (scaled from animBaseFPS); 0 out of its frames */
byte __thiscall CGameModel_AnimTime(CGameModel *self, int i, float frame, float *time);
void __thiscall CGameModel_SetAnim(CGameModel *self, int i, uint32_t chan, int unused);
void __thiscall CGameModel_SetAnimName(CGameModel *self, const char *name, uint32_t chan, int unused);
void __fastcall CGameModel_StopAnims(CGameModel *self);
void __thiscall CGameModel_StopAnim(CGameModel *self, uint32_t chan);
CAnimGrupo *__thiscall CGameModel_ChanAnim(CGameModel *self, uint32_t chan);
byte __thiscall CGameModel_ChanEnd(CGameModel *self, uint32_t chan);
byte __thiscall CGameModel_ChanInfo(CGameModel *self, uint32_t chan, int *mode, int *anim);
byte __thiscall CGameModel_ChanTime(CGameModel *self, uint32_t chan, float *t);
/* the root motion of the channels over t, of the first one into move and saltos */
void __thiscall CGameModel_RootMotion(CGameModel *self, float t, vec3_t *move, int *saltos);
/* the channel moved t seconds on, its root motion since the last frame */
byte __thiscall CGameModel_AnimDelta(CGameModel *self, uint32_t chan, float t, vec3_t *move);

/* the render nodes */
int __thiscall CGameModel_FindNodoR(CGameModel *self, const char *name);
int __thiscall CGameModel_FindNodoRWild(CGameModel *self, const char *name);
CNodoR *__thiscall CGameModel_NodoRRef(CGameModel *self, uint32_t i);
vec3_t *__thiscall CGameModel_NodoPos(CGameModel *self, vec3_t *out, uint32_t i, byte animated);
matrix_t *__thiscall CGameModel_NodoMatrix(CGameModel *self, matrix_t *out, uint32_t i, byte anim, byte *visible,
	uint32_t slot);
/* the transform mode of the look of node i (0 none, 1..6) with the angles v */
byte __thiscall CGameModel_NodoLook(CGameModel *self, uint32_t i, int mode, const vec3_t *v);
float __thiscall CGameModel_NodoIniFloat(CGameModel *self, uint32_t i, const char *sect, const char *key, float def);
int __thiscall CGameModel_NodoIniInt(CGameModel *self, uint32_t i, const char *sect, const char *key, int def);
uint32_t __thiscall CGameModel_NodoIniColor(CGameModel *self, uint32_t i, const char *sect, const char *key,
	uint32_t def);
vec3_t *__thiscall CGameModel_NodoIniVec3(CGameModel *self, vec3_t *out, uint32_t i, const char *sect, const char *key,
	const vec3_t *def);
/* the frame of the animated texture named name in the meshes */
byte __thiscall CGameModel_SetTexFrame(CGameModel *self, const char *name, int frame);

/* vertices for the effects (R_GetModelVertex, else the position of the model) */
int __thiscall CGameModel_VertexCounts(CGameModel *self, int *out, int max);
void __thiscall CGameModel_RandomVertices(CGameModel *self, vec3_t *pos, void *normal, void *area, int count,
	uint32_t nodo);
void __thiscall CGameModel_Vertices(CGameModel *self, vec3_t *pos, void *normal, void *area, int count, int first,
	int *index, uint32_t nodo, int *indices);
void __thiscall CGameModel_ClosestVertex(CGameModel *self, float x, float y, float z, vec3_t *pos, int *index,
	uint32_t nodo);

/* the collision mesh */
void __fastcall CGameModel_FreeCollision(CGameModel *self);
void __thiscall CGameModel_EnableCollision(CGameModel *self, byte on, byte create);
byte __thiscall CGameModel_CreateCollision(CGameModel *self, byte all);
byte __fastcall CGameModel_CollideOn(const CGameModel *self);

/* inline methods of the engine3d instantiated here */
void __thiscall CAnimMat_SetFrame(CAnimMat *self, int frame);
/* rva 0x2146e0 (ScrapEngMain.cpp): the sector of the floor at pos, -2 for none */
int Sector_At(const vec3_t *pos);
/* rva 0x23f7f0 (NodoR.cpp): the look type of the node */
void __thiscall CNodoR_SetLook(CNodoR *self, int tipo);

#endif
