/*
 * Engine/SistPart.cpp of the engine3d: CParticleSystem, the particle system of a node (class CLASS_OBJ3D_PARTICLESYSTEM), read
 * from the [Particles] section of the user properties of the node and the section of its type in the particle ini.
 * The particles are CParticleActive (moving, with a life) or, for static systems, CParticleStatic. Also a few
 * functions of Engine/grafico2d.cpp next to it.
 */
#ifndef SCRAP_PARTICLESYSTEM_H
#define SCRAP_PARTICLESYSTEM_H

#include "scrap/gvar.h"
#include "scrap/cstring.h"
#include "scrap/scene.h"
#include "scrap/meshd3d.h"
#include "scrap/groundportal.h"
#include "scrap/material.h"

/* Engine/objbase3d.cpp: a 3D object on a node */
typedef struct CObjBase3D {
	IObject3D base;
	CNode3D *node;			/* counted (CObjBase3D_SetNode) */
} CObjBase3D;

/* CParticleSystem.params.flags */
#define PARTICLESYSTEM_LOCALPOS 0x01		/* the positions follow the node */
#define PARTICLESYSTEM_LOCAL 0x02		/* the vectors are not turned by the node */
#define PARTICLESYSTEM_STATIC 0x04
#define PARTICLESYSTEM_SPARKS 0x08
#define PARTICLESYSTEM_MULTISPRITE 0x10
#define PARTICLESYSTEM_COLORRAMP 0x20
#define PARTICLESYSTEM_MIPMAP 0x40

enum { PARTICLE_STATIC = 1, PARTICLE_ACTIVE = 2 };

typedef struct CParticle CParticle;
struct CParticleSystem;

typedef struct CParticleVtbl {
	int(*Type)(CParticle *self);								/* PARTICLE_ */
	void(*Init)(CParticle *self);
	void(*Update)(CParticle *self, struct CParticleSystem *sp, float dt, void *multiSpr);
} CParticleVtbl;

/* calls through the slots of CParticleVtbl */
static inline void Particle_Update(void *self, struct CParticleSystem *sp, float dt, void *multiSpr) {
	VT(CParticleVtbl, self)->Update(self, sp, dt, multiSpr);
}

static inline int Particle_Type(void *self) {
	return VT(CParticleVtbl, self)->Type(self);
}

struct CParticle {
	const CParticleVtbl *vftable;
	byte active;
	byte pad_5[3];
	vec3_t pos;
	float scale;
	float size;
	uint32_t color;
	float rot;
	float uv0[2];			/* the sprite rectangle */
	float uv1[2];
};

/* CParticle is the abstract CParticulaBase (vftable all pure); the static particle adds no members */
typedef CParticle CParticleBase;
typedef CParticle CParticleStatic;

typedef struct CParticleActive {
	CParticle p;
	byte manual;		/* placed by CGameParticles_SetParticle: the system does not update it */
	byte pad_35[3];
	float t;			/* the life, 0 to 1 */
	float life;			/* the life time */
	float delay;			/* born after this */
	byte sparks;
	byte pad_45[3];
	vec3_t vel;
	vec3_t acc;
	float sizeIni;
	float sizeEnd;
	float sizeLife;			/* 1 / SizeLife */
	uint32_t colorIni;
	uint32_t colorEnd;
	float rotIni;
	float rotEnd;
	byte spriteIni;
	byte spriteEnd;
	byte spriteCur;
	byte pad_7f;
	float sparkVelScale;
	float sparkMaxSize;
	const uint32_t *colorRamp;	/* 9 colors, NULL without */
	byte bounce;			/* on the floor */
	byte pad_8d[3];
} CParticleActive;

/* the parameters of a type of particle system (the section of the type in the particle ini) */
typedef struct ParticleSystemParams {
	int nParticles;		/* Particles */
	uint32_t flags;			/* SISTPART_* */
	int birthRate;			/* per second, 0 for none after the first ones */
	float lifeTime;
	float scale;
	uint32_t colorIni;
	uint32_t colorEnd;
	uint32_t colorRamp[9];		/* ColorRamp_01 .. 09 */
	float sizeIni;
	float sizeEnd;
	float sizeVar;
	float sizeLife;
	vec3_t initPos;
	vec3_t initPosMin;
	vec3_t initPosMax;
	vec3_t initVel;
	vec3_t initVelMin;
	vec3_t initVelMax;
	vec3_t initAcc;
	vec3_t initAccMin;
	vec3_t initAccMax;
	float rotIni;			/* radians */
	float rotEnd;
	float rotVar;
	byte spriteIni;
	byte spriteEnd;
	byte spriteRand;
	byte pad_cb;
	cString texture;
	byte additiveAlpha;
	byte alphaMod;
	byte pad_d2[2];
	float sparkVelScale;
	float sparkMaxSize;
	float alphaDist;
	byte bounce;
	byte pad_e1[3];
	float zBias;
} ParticleSystemParams;

typedef struct CParticleSystem {
	union {
		IObject3D base;
		struct {
			const IObject3DVtbl *vftable;
			uint32_t class;			/* CLASS_OBJ3D_PARTICLESYSTEM */
		};
	};
	CNode3D *node;			/* CObjBase3D_SetNode */
	ParticleSystemParams params;
	float scale;			/* Particles Scale of the node */
	float alpha;
	vec3_t *posList;		/* new[] of params.nParticles, the positions of a static system */
	vec3_t lastPos;			/* the position of the node at the last update */
	byte numActive;
	byte nextFree;			/* where the next free particle is looked for */
	byte pad_10e[2];
	char *type;			/* new[] */
	CMaterial *mat;
	void *MultiSpr;			/* CMultiSprite of the texture with PARTICLESYSTEM_MULTISPRITE */
	CParticle **Particles;		/* new[] of params.nParticles */
	SectorVis vis;			/* the sector visibility of the debug output */
	uint32_t lastTime;		/* g_TimeMs of the last update */
	uint32_t birthTime;		/* of the last particle born */
	int lastFrame;			/* g_FrameCount of the last update */
	byte enabled;
	byte visible;
	byte inView;
	byte pad_143;
	const uint32_t *colorRamp;	/* instead of params.colorRamp */
	vec3_t *emitPos;		/* the positions new particles take in turn */
	byte nEmitPos;
	byte emitIdx;
	byte pad_14e[2];
} CParticleSystem;

CParticleSystem *CParticleSystem_ctorCopy(CParticleSystem *self, const CParticleSystem *orig);
void CObjBase3D_dtor(CObjBase3D *self);
/* the system of the [Particles] section of the user properties of node, with the ini of the scene; NULL without a
 * type */
CParticleSystem *ParticleSystem_FromNode(CNode3D *node, CIniFile *ini);
void CParticleSystem_SetColorRamp(CParticleSystem *self, const uint32_t *ramp);
void CParticleSystem_SetEmitPos(CParticleSystem *self, vec3_t *pos, byte n);
/* the box of the particles of a static system; 0 for the others */
byte CParticleSystem_GetBox(CParticleSystem *self, box_t *box);
/* n particles born at the node matrix m, delay i * dt each; force: see CParticleSystem_GetFree */
void CParticleSystem_Emit(CParticleSystem *self, int n, byte force, float dt, const matrix_t *m);
void CParticleSystem_EmitN(CParticleSystem *self, int n, byte force);
void CParticleSystem_EmitM(CParticleSystem *self, int n, byte force, const matrix_t *m);
void CParticleSystem_EmitDt(CParticleSystem *self, int n, byte force, float dt);
void CParticleSystem_SetActive(CParticleSystem *self, CParticle *p, byte active);
/* the particle ini loaded (cleared with no name) */
void ParticleSystem_LoadIni(const char *name, const char *dir);
void CParticleSystem_Render2(CParticleSystem *self, void *dev);
/* the particle systems the last frame drew (the test channel, not in the original): type, active particles and the
 * position of the node */
typedef struct ParticleDrawn {
	char type[32];
	int particles;
	vec3_t pos;
} ParticleDrawn;
#define PARTICLE_DRAWN_MAX 512
const ParticleDrawn *ParticleSystem_Drawn(int *n);
void CParticleSystem_SetAlpha(CParticleSystem *self, float alpha);

/* other modules (src/imports.inc) */
/* the node of a 3D object, counted */
void CObjBase3D_SetNode(CObjBase3D *self, CNode3D *node);
/* and 0x26bc30 (SueloPortal.cpp): the visibility of a sphere for the debug output */
/* src/game/engine3d/Engine/RenderMeshD3D.c (RenderObjBase3D.cpp): the system (or flare) queued with the render node
 * r for the draw sorted by distance after the meshes; HRESULT */
int ParticleSystem_Draw(CParticleSystem *sp, CRenderNode *r);
/* src/game/engine3d/Engine/RenderEVA.c: the active particles drawn, in the space of r or of the node; 0 */
int ParticleSystem_Draw2(CParticleSystem *sp, CRenderNode *r);

#endif
