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

/* Engine/objbase3d.cpp, 0xc bytes: a 3D object on a node */
typedef struct CObjBase3D {
	IObject3D base;			/* 0x0 */
	CNode3D *node;			/* 0x8 counted (CObjBase3D_SetNode) */
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
	int(*Type)(CParticle *self);								/* 0x0 PARTICLE_ */
	void(*Init)(CParticle *self);								/* 0x4 */
	void(*Update)(CParticle *self, struct CParticleSystem *sp, float dt, void *multiSpr);	/* 0x8 */
} CParticleVtbl;

/* calls through the slots of CParticleVtbl */
static inline void Particle_Update(void *self, struct CParticleSystem *sp, float dt, void *multiSpr) {
	VT(CParticleVtbl, self)->Update(self, sp, dt, multiSpr);
}

static inline int Particle_Type(void *self) {
	return VT(CParticleVtbl, self)->Type(self);
}

struct CParticle {
	const CParticleVtbl *vftable;	/* 0x00 */
	byte active;			/* 0x04 */
	byte pad_5[3];
	vec3_t pos;			/* 0x08 */
	float scale;			/* 0x14 */
	float size;			/* 0x18 */
	uint32_t color;			/* 0x1c */
	float rot;			/* 0x20 */
	float uv0[2];			/* 0x24 the sprite rectangle */
	float uv1[2];			/* 0x2c */
};

/* CParticle is the abstract CParticulaBase (vftable rva 0x3ddc3c, all pure); the static particle adds no members */
typedef CParticle CParticleBase;
typedef CParticle CParticleStatic;

typedef struct CParticleActive {
	CParticle p;			/* 0x00 */
	byte manual;		/* 0x34 placed by CGameParticles_SetParticle: the system does not update it */
	byte pad_35[3];
	float t;			/* 0x38 the life, 0 to 1 */
	float life;			/* 0x3c the life time */
	float delay;			/* 0x40 born after this */
	byte sparks;			/* 0x44 */
	byte pad_45[3];
	vec3_t vel;			/* 0x48 */
	vec3_t acc;			/* 0x54 */
	float sizeIni;			/* 0x60 */
	float sizeEnd;			/* 0x64 */
	float sizeLife;			/* 0x68 1 / SizeLife */
	uint32_t colorIni;		/* 0x6c */
	uint32_t colorEnd;		/* 0x70 */
	float rotIni;			/* 0x74 */
	float rotEnd;			/* 0x78 */
	byte spriteIni;			/* 0x7c */
	byte spriteEnd;			/* 0x7d */
	byte spriteCur;			/* 0x7e */
	byte pad_7f;
	float sparkVelScale;		/* 0x80 */
	float sparkMaxSize;		/* 0x84 */
	const uint32_t *colorRamp;	/* 0x88 9 colors, NULL without */
	byte bounce;			/* 0x8c on the floor */
	byte pad_8d[3];
} CParticleActive;

/* the parameters of a type of particle system (the section of the type in the particle ini) */
typedef struct ParticleSystemParams {
	int nParticles;		/* 0x00 Particles */
	uint32_t flags;			/* 0x04 SISTPART_* */
	int birthRate;			/* 0x08 per second, 0 for none after the first ones */
	float lifeTime;			/* 0x0c */
	float scale;			/* 0x10 */
	uint32_t colorIni;		/* 0x14 */
	uint32_t colorEnd;		/* 0x18 */
	uint32_t colorRamp[9];		/* 0x1c ColorRamp_01 .. 09 */
	float sizeIni;			/* 0x40 */
	float sizeEnd;			/* 0x44 */
	float sizeVar;			/* 0x48 */
	float sizeLife;			/* 0x4c */
	vec3_t initPos;			/* 0x50 */
	vec3_t initPosMin;		/* 0x5c */
	vec3_t initPosMax;		/* 0x68 */
	vec3_t initVel;			/* 0x74 */
	vec3_t initVelMin;		/* 0x80 */
	vec3_t initVelMax;		/* 0x8c */
	vec3_t initAcc;			/* 0x98 */
	vec3_t initAccMin;		/* 0xa4 */
	vec3_t initAccMax;		/* 0xb0 */
	float rotIni;			/* 0xbc radians */
	float rotEnd;			/* 0xc0 */
	float rotVar;			/* 0xc4 */
	byte spriteIni;			/* 0xc8 */
	byte spriteEnd;			/* 0xc9 */
	byte spriteRand;		/* 0xca */
	byte pad_cb;
	cString texture;		/* 0xcc */
	byte additiveAlpha;		/* 0xd0 */
	byte alphaMod;			/* 0xd1 */
	byte pad_d2[2];
	float sparkVelScale;		/* 0xd4 */
	float sparkMaxSize;		/* 0xd8 */
	float alphaDist;		/* 0xdc */
	byte bounce;			/* 0xe0 */
	byte pad_e1[3];
	float zBias;			/* 0xe4 */
} ParticleSystemParams;

typedef struct CParticleSystem {
	union {
		IObject3D base;
		struct {
			const IObject3DVtbl *vftable;	/* 0x00 */
			uint32_t class;			/* 0x004 CLASS_OBJ3D_PARTICLESYSTEM */
		};
	};
	CNode3D *node;			/* 0x008 CObjBase3D_SetNode */
	ParticleSystemParams params;		/* 0x00c */
	float scale;			/* 0x0f4 Particles Scale of the node */
	float alpha;			/* 0x0f8 */
	vec3_t *posList;		/* 0x0fc new[] of params.nParticles, the positions of a static system */
	vec3_t lastPos;			/* 0x100 the position of the node at the last update */
	byte numActive;			/* 0x10c */
	byte nextFree;			/* 0x10d where the next free particle is looked for */
	byte pad_10e[2];
	char *type;			/* 0x110 new[] */
	CMaterial *mat;			/* 0x114 */
	void *MultiSpr;			/* 0x118 CMultiSprite of the texture with PARTICLESYSTEM_MULTISPRITE */
	CParticle **Particles;		/* 0x11c new[] of params.nParticles */
	SectorVis vis;			/* 0x120 the sector visibility of the debug output */
	uint32_t lastTime;		/* 0x134 g_TimeMs of the last update */
	uint32_t birthTime;		/* 0x138 of the last particle born */
	int lastFrame;			/* 0x13c g_FrameCount of the last update */
	byte enabled;			/* 0x140 */
	byte visible;			/* 0x141 */
	byte inView;			/* 0x142 */
	byte pad_143;
	const uint32_t *colorRamp;	/* 0x144 instead of params.colorRamp */
	vec3_t *emitPos;		/* 0x148 the positions new particles take in turn */
	byte nEmitPos;			/* 0x14c */
	byte emitIdx;			/* 0x14d */
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
void CParticleSystem_SetAlpha(CParticleSystem *self, float alpha);

/* other modules (src/imports.inc) */
/* rva 0x2cad70: the node of a 3D object, counted */
void CObjBase3D_SetNode(CObjBase3D *self, CNode3D *node);
/* rva 0x26bff0 and 0x26bc30 (SueloPortal.cpp): the visibility of a sphere for the debug output */
/* src/game/engine3d/Engine/RenderMeshD3D.c (RenderObjBase3D.cpp): the system (or flare) queued with the render node
 * r for the draw sorted by distance after the meshes; HRESULT */
int ParticleSystem_Draw(CParticleSystem *sp, CRenderNode *r);
/* src/game/engine3d/Engine/RenderEVA.c: the active particles drawn, in the space of r or of the node; 0 */
int ParticleSystem_Draw2(CParticleSystem *sp, CRenderNode *r);

#endif
