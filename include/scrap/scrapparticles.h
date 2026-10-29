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
	uint32_t colorIni;		/* 0x00 */
	uint32_t colorEnd;		/* 0x04 */
	float sizeIni;			/* 0x08 */
	float sizeEnd;			/* 0x0c */
	float sizeVar;			/* 0x10 */
	float sizeLife;			/* 0x14 */
	int birthRate;			/* 0x18 */
	float lifeTime;			/* 0x1c */
	vec3_t initPos;			/* 0x20 */
	vec3_t initPosMin;		/* 0x2c */
	vec3_t initPosMax;		/* 0x38 */
	vec3_t initVel;			/* 0x44 */
	vec3_t initVelMin;		/* 0x50 */
	vec3_t initVelMax;		/* 0x5c */
	vec3_t initAcc;			/* 0x68 */
	vec3_t initAccMin;		/* 0x74 */
	vec3_t initAccMax;		/* 0x80 */
	float rotIni;			/* 0x8c */
	float rotEnd;			/* 0x90 */
	float rotVar;			/* 0x94 */
	byte spriteIni;			/* 0x98 */
	byte spriteEnd;			/* 0x99 */
	byte pad_9a[2];
	float sparkVelScale;		/* 0x9c */
	float sparkMaxSize;		/* 0xa0 */
	byte bounce;			/* 0xa4 */
	byte pad_a5[3];
	CRenderNode *Node;			/* 0xa8 a reference (CGameModel_RenderNodeRef) */
	CParticleSystem *ParticleSystem;		/* 0xac the object of Node */
} CGameParticles;
SD_OFFSET(CGameParticles, rotIni, 0x8c);
SD_OFFSET(CGameParticles, sparkVelScale, 0x9c);
SD_OFFSET(CGameParticles, Node, 0xa8);
SD_SIZE(CGameParticles, 0xb0);

CGameParticles *__fastcall CGameParticles_ctor(CGameParticles *self);
/* no node, the default parameters */
void __fastcall CGameParticles_Init(CGameParticles *self);
/* the node released (also the destructor) */
void __fastcall CGameParticles_Release(CGameParticles *self);
/* the particle system of render node i of model */
void __thiscall CGameParticles_Attach(CGameParticles *self, CGameModel *model, uint32_t i);
/* the parameters into the particle system */
void __fastcall CGameParticles_Apply(CGameParticles *self);
/* all particles out */
void __fastcall CGameParticles_KillAll(CGameParticles *self);
void __thiscall CGameParticles_Kill(CGameParticles *self, int i);
/* the particles from .. to - 1 out */
void __thiscall CGameParticles_KillRange(CGameParticles *self, int from, int to);
/* particle i made active at x y z; sprite is the sprite of the multi sprite texture */
void __thiscall CGameParticles_SetParticle(CGameParticles *self, int i, float x, float y, float z, float size,
	uint32_t color, int sprite, float rot);
/* the members of particle i, NULL for the ones not wanted (vel and t only for active particles); its active flag */
byte __thiscall CGameParticles_GetParticle(CGameParticles *self, int i, vec3_t *pos, vec3_t *vel, float *t,
	uint32_t *color, float *size);
int __fastcall CGameParticles_NumActive(const CGameParticles *self);
int __fastcall CGameParticles_NumParticles(const CGameParticles *self);
float __fastcall CGameParticles_GetAlpha(const CGameParticles *self);
void __thiscall CGameParticles_SetAlpha(CGameParticles *self, float alpha);
byte __fastcall CGameParticles_IsEnabled(const CGameParticles *self);
void __thiscall CGameParticles_SetEnabled(CGameParticles *self, byte on);
byte __fastcall CGameParticles_IsVisible(const CGameParticles *self);
void __thiscall CGameParticles_SetVisible(CGameParticles *self, byte on);
void __thiscall CGameParticles_SetColorRampOn(CGameParticles *self, byte on);
void __thiscall CGameParticles_SetSparksOn(CGameParticles *self, byte on);
void __thiscall CGameParticles_SetColorRamp(CGameParticles *self, uint32_t *ramp);
void __thiscall CGameParticles_SetEmitPos(CGameParticles *self, vec3_t *pos, byte n);
void __thiscall CGameParticles_EmitN(CGameParticles *self, int n, byte force);
void __thiscall CGameParticles_EmitM(CGameParticles *self, int n, byte force, const matrix_t *m);
void __thiscall CGameParticles_EmitDt(CGameParticles *self, int n, byte force, float dt);
void __thiscall CGameParticles_Emit(CGameParticles *self, int n, byte force, float dt, const matrix_t *m);

/* rva 0x216ad0 (ScrapEngMain.cpp): returns m, the matrix of the game as the matrix of the engine */

#endif
