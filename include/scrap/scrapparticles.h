/*
 * ScrapEngParticles.cpp of the game: CGameParticles, the access of the entities to the particle system (CParticleSystem) of
 * a render node of their model: emitting, single particles placed by hand and a copy of the parameters of the type
 * that CGameParticles_Apply writes into the system.
 */
#ifndef SCRAP_SCRAPPARTICLES_H
#define SCRAP_SCRAPPARTICLES_H

#include "scrap/scrapmodel.h"
#include "scrap/particlesystem.h"

typedef struct CGameParticles {
	uint32_t colorIni;
	uint32_t colorEnd;
	float sizeIni;
	float sizeEnd;
	float sizeVar;
	float sizeLife;
	int birthRate;
	float lifeTime;
	vec3_t initPos;
	vec3_t initPosMin;
	vec3_t initPosMax;
	vec3_t initVel;
	vec3_t initVelMin;
	vec3_t initVelMax;
	vec3_t initAcc;
	vec3_t initAccMin;
	vec3_t initAccMax;
	float rotIni;
	float rotEnd;
	float rotVar;
	byte spriteIni;
	byte spriteEnd;
	byte pad_9a[2];
	float sparkVelScale;
	float sparkMaxSize;
	byte bounce;
	byte pad_a5[3];
	CRenderNode *Node;			/* a reference (CGameModel_RenderNodeRef) */
	CParticleSystem *ParticleSystem;		/* the object of Node */
} CGameParticles;

CGameParticles *CGameParticles_ctor(CGameParticles *self);
/* the node released (also the destructor) */
void CGameParticles_Release(CGameParticles *self);
/* the particle system of render node i of model */
void CGameParticles_Attach(CGameParticles *self, CGameModel *model, uint32_t i);
/* the parameters into the particle system */
void CGameParticles_Apply(CGameParticles *self);
/* all particles out */
void CGameParticles_KillAll(CGameParticles *self);
void CGameParticles_Kill(CGameParticles *self, int i);
/* the particles from .. to - 1 out */
void CGameParticles_KillRange(CGameParticles *self, int from, int to);
/* particle i made active at x y z; sprite is the sprite of the multi sprite texture */
void CGameParticles_SetParticle(CGameParticles *self, int i, float x, float y, float z, float size,
	uint32_t color, int sprite, float rot);
/* the members of particle i, NULL for the ones not wanted (vel and t only for active particles); its active flag */
byte CGameParticles_GetParticle(CGameParticles *self, int i, vec3_t *pos, vec3_t *vel, float *t,
	uint32_t *color, float *size);
int CGameParticles_NumActive(const CGameParticles *self);
int CGameParticles_NumParticles(const CGameParticles *self);
void CGameParticles_SetAlpha(CGameParticles *self, float alpha);
void CGameParticles_SetEnabled(CGameParticles *self, byte on);
byte CGameParticles_IsVisible(const CGameParticles *self);
void CGameParticles_SetVisible(CGameParticles *self, byte on);
void CGameParticles_SetColorRampOn(CGameParticles *self, byte on);
void CGameParticles_SetSparksOn(CGameParticles *self, byte on);
void CGameParticles_SetColorRamp(CGameParticles *self, const uint32_t *ramp);
void CGameParticles_SetEmitPos(CGameParticles *self, vec3_t *pos, byte n);
void CGameParticles_EmitN(CGameParticles *self, int n, byte force);
void CGameParticles_EmitM(CGameParticles *self, int n, byte force, const matrix_t *m);
void CGameParticles_EmitDt(CGameParticles *self, int n, byte force, float dt);
void CGameParticles_Emit(CGameParticles *self, int n, byte force, float dt, const matrix_t *m);

/* (ScrapEngMain.cpp): returns m, the matrix of the game as the matrix of the engine */

#endif
