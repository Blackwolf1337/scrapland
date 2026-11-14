/*
 * ScrapEngModel.cpp of the game: CGameModel, the model of an entity: a CModel of the engine3d made with its LOD
 * models (&U&D&N.LODn), the collision mesh of its meshes in the floor of the world and the access of the entities to
 * its animations, render nodes and vertices. Animation channels ("chan") are the slots of the CModel.
 */
#ifndef SCRAP_SCRAPMODEL_H
#define SCRAP_SCRAPMODEL_H

#include "scrap/gvar.h"
#include "scrap/material.h"
#include "scrap/model.h"
#include "scrap/ground.h"

typedef struct CGameModel {
	CModel *Model;
	CCollideMesh *CollideMesh;
	int CollisionMeshID;		/* -1: the model has none */
	int CollisionIni;		/* the first triangle in the floor */
	byte collide;			/* on */
	byte pad_11[3];
	float animBaseFPS;		/* AnimBaseFPS of the ini, the frames of the animations scaled to it */
	int frame;			/* g_FrameCount of the last transform of the render nodes */
} CGameModel;

#define MAX_RENDERFX_PARAM 1
#define MAX_VALID 256
#define MAT_SINVERTICES 0x4000		/* the meshes of the material give no vertices to the effects */

extern cGVar GVar_R_SplitScreen;
#define g_SplitScreen (GVar_R_SplitScreen.value.i)		/* R_SplitScreen: every model visible */

CGameModel *CGameModel_ctor(CGameModel *self, const char *name);
/* the default constructor for the arrays: without a model */
CGameModel *CGameModel_ctorDefault(CGameModel *self);
void CGameModel_LoadName(CGameModel *self, const char *name);
void CGameModel_LoadScaled(CGameModel *self, const char *name, const vec3_t *scale);
/* the model freed (Load with NULL) */
void CGameModel_Reset(CGameModel *self);
/* free: the scenes of its animations and LODs released with it */
void CGameModel_Release(CGameModel *self, byte free);
byte CGameModel_IsLoaded(const CGameModel *self);
const char *CGameModel_Name(const CGameModel *self);
void CGameModel_Enable(CGameModel *self);
void CGameModel_Disable(CGameModel *self);
byte CGameModel_IsEnabled(const CGameModel *self);
byte CGameModel_IsVisible(const CGameModel *self);
int CGameModel_GetSector(CGameModel *self);
float CGameModel_Radius(const CGameModel *self);
vec3_t *CGameModel_GetPos(const CGameModel *self, vec3_t *out);
void CGameModel_SetPos(CGameModel *self, const vec3_t *v);
vec3_t *CGameModel_GetRot(const CGameModel *self, vec3_t *out);
void CGameModel_SetRot(CGameModel *self, const vec3_t *v);
void CGameModel_SetRot2(CGameModel *self, const vec3_t *v);
void CGameModel_SetPivot(CGameModel *self, const vec3_t *v, byte rel, byte recalcBB);
void CGameModel_SetPivotNode(CGameModel *self, const char *name);
void CGameModel_SetSortZ(CGameModel *self, float bias, float mult);
uint32_t CGameModel_GetEffect(const CGameModel *self, uint32_t *out);
void CGameModel_SetEffect(CGameModel *self, uint32_t fx);
void CGameModel_GetEffectParam(const CGameModel *self, int i, uint32_t *out);
void CGameModel_SetEffectParam(CGameModel *self, int i, uint32_t v);
/* the model drawn now, flags 2 with the default light, 3 also lit */
void CGameModel_RenderNow(CGameModel *self, byte flags);

/* the animations */
int CGameModel_AddAnim(CGameModel *self, const char *name);
int CGameModel_FindAnim(CGameModel *self, const char *name);
const char *CGameModel_AnimName(CGameModel *self, int i);
byte CGameModel_AnimInfo(CGameModel *self, int i, float *dur, float *fps);
/* the model has animation i (not in the original) */
byte CGameModel_HasAnim(CGameModel *self, int i);
byte CGameModel_SetAnimFactor(CGameModel *self, int i, float factor);
byte CGameModel_SetAnimSpeed(CGameModel *self, int i, float speed);
byte CGameModel_SetAnimLoop(CGameModel *self, int i, byte loop);
/* "Morph,n,tag" */
void CGameModel_AnimCmd(CGameModel *self, const char *cmd);
/* the time of frame of animation i (scaled from animBaseFPS); 0 out of its frames */
byte CGameModel_AnimTime(CGameModel *self, int i, float frame, float *time);
void CGameModel_SetAnim(CGameModel *self, int i, uint32_t chan, int unused);
void CGameModel_StopAnim(CGameModel *self, uint32_t chan);
byte CGameModel_ChanEnd(CGameModel *self, uint32_t chan);
byte CGameModel_ChanInfo(CGameModel *self, uint32_t chan, int *mode, int *anim);
byte CGameModel_ChanTime(CGameModel *self, uint32_t chan, float *t);
/* the root motion of the channels over t, of the first one into move and jumps */
void CGameModel_RootMotion(CGameModel *self, float t, vec3_t *move, int *jumps);
/* the channel moved t seconds on, its root motion since the last frame */
byte CGameModel_AnimDelta(CGameModel *self, uint32_t chan, float t, vec3_t *move);

/* the render nodes */
int CGameModel_FindRenderNode(CGameModel *self, const char *name);
int CGameModel_FindRenderNodeWild(CGameModel *self, const char *name);
CRenderNode *CGameModel_RenderNodeRef(CGameModel *self, uint32_t i);
/* a reference of CGameModel_RenderNodeRef dropped (the count at 0x0, nothing is freed), *slot cleared */
static inline void RenderNode_Unref(CRenderNode **slot) {
	if (*slot) {
		(*(int *)*slot)--;
		*slot = NULL;
	}
}
vec3_t *CGameModel_NodePos(CGameModel *self, vec3_t *out, uint32_t i, byte animated);
matrix_t *CGameModel_NodeMatrix(CGameModel *self, matrix_t *out, uint32_t i, byte anim, byte *visible,
	uint32_t slot);
/* the transform mode of the look of node i (0 none, 1..6) with the angles v */
byte CGameModel_NodeLook(CGameModel *self, uint32_t i, int mode, const vec3_t *v);
float CGameModel_NodeIniFloat(CGameModel *self, uint32_t i, const char *sect, const char *key, float def);
int CGameModel_NodeIniInt(CGameModel *self, uint32_t i, const char *sect, const char *key, int def);
uint32_t CGameModel_NodeIniColor(CGameModel *self, uint32_t i, const char *sect, const char *key,
	uint32_t def);
vec3_t *CGameModel_NodeIniVec3(CGameModel *self, vec3_t *out, uint32_t i, const char *sect, const char *key,
	const vec3_t *def);

void CGameModel_RandomVertices(CGameModel *self, vec3_t *pos, void *normal, void *area, int count,
	uint32_t node);
void CGameModel_Vertices(CGameModel *self, vec3_t *pos, void *normal, void *area, int count, int first,
	int *index, uint32_t node, int *indices);
void CGameModel_ClosestVertex(CGameModel *self, float x, float y, float z, vec3_t *pos, int *index,
	uint32_t node);

void CGameModel_EnableCollision(CGameModel *self, byte on, byte create);
byte CGameModel_CollideOn(const CGameModel *self);

/* the sector of the floor at pos, -2 for none (ScrapEngWorld.c) */
int Sector_At(const vec3_t *pos);
/* (NodoR.cpp): the look type of the node */
void CRenderNode_SetLook(CRenderNode *self, int type);

#endif
