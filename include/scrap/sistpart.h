/*
 * Engine/SistPart.cpp of the engine3d: CSistPart, the particle system of a node (class CLASE_OBJ3D_SISTPART), read
 * from the [Particles] section of the user properties of the node and the section of its type in the particle ini.
 * The particles are CParticulaActive (moving, with a life) or, for static systems, CParticulaStatic. Also a few
 * functions of Engine/grafico2d.cpp next to it.
 */
#ifndef SCRAP_SISTPART_H
#define SCRAP_SISTPART_H

#include "scrap/cstring.h"
#include "scrap/escena.h"
#include "scrap/mallad3d.h"
#include "scrap/sueloportal.h"
#include "scrap/material.h"

#define CSISTPART_VFTABLE 0x3ddc6c
#define CPARTICULAACTIVE_VFTABLE 0x3ddc5c
#define CPARTICULASTATIC_VFTABLE 0x3ddc4c
#define COBJBASE3D_VFTABLE 0x3ddc1c
#define IOBJETO3D_VFTABLE 0x3d79dc

/* CSistPart.params.flags */
#define SISTPART_LOCALPOS 0x01		/* the positions follow the node */
#define SISTPART_LOCAL 0x02		/* the vectors are not turned by the node */
#define SISTPART_STATIC 0x04
#define SISTPART_SPARKS 0x08
#define SISTPART_MULTISPRITE 0x10
#define SISTPART_COLORRAMP 0x20
#define SISTPART_MIPMAP 0x40

/* the slots of the vftable of the particles */
enum { PARTICULA_VF_TYPE = 0, PARTICULA_VF_INIT = 1, PARTICULA_VF_UPDATE = 2 };
enum { PARTICULA_STATIC = 1, PARTICULA_ACTIVE = 2 };

typedef struct CParticula {
	void **vftable;			/* 0x00 PARTICULA_VF_* */
	byte active;			/* 0x04 */
	byte pad_5[3];
	vec3_t pos;			/* 0x08 */
	float escala;			/* 0x14 */
	float size;			/* 0x18 */
	uint32_t color;			/* 0x1c */
	float rot;			/* 0x20 */
	float uv0[2];			/* 0x24 the sprite rectangle */
	float uv1[2];			/* 0x2c */
} CParticula;
SD_SIZE(CParticula, 0x34);
typedef CParticula CParticulaStatic;

typedef struct CParticulaActive {
	CParticula p;			/* 0x00 */
	byte b34;			/* 0x34 */
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
	uint32_t *colorRamp;		/* 0x88 9 colors, NULL without */
	byte bounce;			/* 0x8c on the floor */
	byte pad_8d[3];
} CParticulaActive;
SD_SIZE(CParticulaActive, 0x90);

/* the parameters of a type of particle system (the section of the type in the particle ini) */
typedef struct SistPartParams {
	int nParticulas;		/* 0x00 Particles */
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
} SistPartParams;
SD_SIZE(SistPartParams, 0xe8);

typedef struct CSistPart {
	void **vftable;			/* 0x000 OBJ3D_VF_* */
	uint32_t clase;			/* 0x004 CLASE_OBJ3D_SISTPART */
	CNodo3D *nodo;			/* 0x008 CObjBase3D_SetNodo */
	SistPartParams params;		/* 0x00c */
	float escala;			/* 0x0f4 Particles Scale of the node */
	float alpha;			/* 0x0f8 */
	vec3_t *posList;		/* 0x0fc new[] of params.nParticulas, the positions of a static system */
	vec3_t lastPos;			/* 0x100 the position of the node at the last update */
	byte numActive;			/* 0x10c */
	byte nextFree;			/* 0x10d where the next free particle is looked for */
	byte pad_10e[2];
	char *type;			/* 0x110 new[] */
	CMaterial *mat;			/* 0x114 */
	void *MultiSpr;			/* 0x118 CMultiSprite of the texture with SISTPART_MULTISPRITE */
	CParticula **Particles;		/* 0x11c new[] of params.nParticulas */
	SectorVis vis;			/* 0x120 the sector visibility of the debug output */
	uint32_t lastTime;		/* 0x134 g_TimeMs of the last update */
	uint32_t birthTime;		/* 0x138 of the last particle born */
	int lastFrame;			/* 0x13c g_FrameCount of the last update */
	byte enabled;			/* 0x140 */
	byte visible;			/* 0x141 */
	byte inView;			/* 0x142 */
	byte pad_143;
	uint32_t *colorRamp;		/* 0x144 instead of params.colorRamp */
	vec3_t *emitPos;		/* 0x148 the positions new particles take in turn */
	byte nEmitPos;			/* 0x14c */
	byte emitIdx;			/* 0x14d */
	byte pad_14e[2];
} CSistPart;
SD_SIZE(CSistPart, 0x150);
SD_OFFSET(CSistPart, params, 0x0c);
SD_OFFSET(CSistPart, Particles, 0x11c);

/* the switches of the particle systems */
#define g_SistPartOn GAME_DATA(int, 0x4b2528)
#define g_SistPartFlags GAME_DATA(int, 0x4b2570)	/* -1, else the flags of all systems */
#define g_SistPartDebug GAME_DATA(int, 0x4b25a0)	/* 1: visibility, 2: drawn, 3: with the name */
#define g_SistPartCount GAME_DATA(int, 0x4b2588)	/* the particles of all systems */
#define g_SistPartIni ((CArchINI *)Game_Ptr(0x4b25d8))	/* the particle ini */
#define g_SistPartPosScale GAME_DATA(float, 0x4b2558)
#define g_SistPartSizeScale GAME_DATA(float, 0x4b2540)
#define g_SistPartLifeScale GAME_DATA(float, 0x4b25d0)
#define g_SistPartBirthAdd GAME_DATA(int, 0x4b25b8)

/* grafico2d.cpp: the screen size and its inverses */
#define g_Pantalla2DW GAME_DATA(int, 0x432598)
#define g_Pantalla2DH GAME_DATA(int, 0x43259c)
#define g_Pantalla2DInvW GAME_DATA(double, 0x4325a0)
#define g_Pantalla2DInvH GAME_DATA(double, 0x4325a8)
void Grafico2D_SetPantalla(int w, int h);
void Grafico2D_GetPantalla(int *w, int *h);
/* the primitives of nVert vertices (4: triangle list, 5: strip, 6: fan) */
uint32_t Grafico2D_NumPrims(int tipo, uint32_t nVert);

CParticula *__fastcall CParticulaActive_ctor(CParticula *self);
CParticula *__fastcall CParticulaStatic_ctor(CParticula *self);
void __fastcall CParticulaActive_dtor(CParticula *self);
int __fastcall CParticulaActive_Type(CParticula *self);
int __fastcall CParticulaStatic_Type(CParticula *self);
void __fastcall CParticulaActive_Init(CParticula *self);
void __fastcall CParticulaStatic_Init(CParticula *self);
/* dt seconds on; at the end of its life the particle goes out */
void __thiscall CParticulaActive_Update(CParticulaActive *self, CSistPart *sp, float dt, void *multiSpr);

CSistPart *__thiscall CSistPart_ctorCopy(CSistPart *self, const CSistPart *orig);
CSistPart *__thiscall CSistPart_ctorParams(CSistPart *self, CNodo3D *nodo, const char *type, float escala, float alpha,
	const vec3_t *posList, CArchINI *ini);
CSistPart *__fastcall CSistPart_ctor(CSistPart *self);
void __fastcall CObjBase3D_dtor(IObjeto3D *self);
void __fastcall SistPartParams_dtor(SistPartParams *self);
void __fastcall CSistPart_dtor(CSistPart *self);
SistPartParams *__thiscall SistPartParams_Assign(SistPartParams *self, const SistPartParams *src);
IObjeto3D *__thiscall CObjBase3D_Delete(IObjeto3D *self, byte flags);
CSistPart *__thiscall CSistPart_Delete(CSistPart *self, byte flags);
/* the node, the type and its parameters (copied, or from ini, or from the particle ini) */
void __thiscall CSistPart_Init(CSistPart *self, CNodo3D *nodo, const char *type, const SistPartParams *params,
	CArchINI *ini);
/* a free particle, made active; with force the oldest one if none is free */
CParticula *__thiscall CSistPart_GetFree(CSistPart *self, byte force);
/* the system of the [Particles] section of the user properties of nodo, with the ini of the scene; NULL without a
 * type */
CSistPart *SistPart_FromNode(CNodo3D *nodo, CArchINI *ini);
void __thiscall CSistPart_SetColorRamp(CSistPart *self, uint32_t *ramp);
void __thiscall CSistPart_SetEmitPos(CSistPart *self, vec3_t *pos, byte n);
void __thiscall CSistPart_GetEscala2(CSistPart *self, float *out);
void __thiscall CSistPart_GetEscala(CSistPart *self, vec3_t *out);
/* the box of the particles of a static system; 0 for the others */
byte __thiscall CSistPart_GetBox(CSistPart *self, box_t *box);
/* the parameters from the section of the type in ini */
void __thiscall CSistPart_ReadParams(CSistPart *self, CArchINI *ini);
/* p born with the parameters */
void __thiscall CSistPart_GenParticle(CSistPart *self, CParticula *p);
/* the vectors of p turned by m and its position moved by m, as the flags say */
void __thiscall CSistPart_PlaceParticle(CSistPart *self, CParticula *p, const matrix_t *m);
/* n particles born at the node matrix m, delay i * dt each; force: see CSistPart_GetFree */
void __thiscall CSistPart_Emit(CSistPart *self, int n, byte force, float dt, const matrix_t *m);
void __thiscall CSistPart_EmitN(CSistPart *self, int n, byte force);
void __thiscall CSistPart_EmitM(CSistPart *self, int n, byte force, const matrix_t *m);
void __thiscall CSistPart_EmitDt(CSistPart *self, int n, byte force, float dt);
byte __thiscall CSistPart_Read(CSistPart *self, CArchIO *f, MallaD3D_FileCtx *ctx);
byte __thiscall CSistPart_Write(CSistPart *self, CArchIO *f, MallaD3D_FileCtx *ctx);
void __fastcall CSistPart_CreateMat(CSistPart *self);
void __thiscall CSistPart_SetActive(CSistPart *self, CParticula *p, byte active);
void __fastcall CSistPart_CreateParticles(CSistPart *self);
/* each coordinate a random value between a and b */
vec3_t *SistPart_RandVec(vec3_t *out, const vec3_t *a, const vec3_t *b);
/* the particle ini loaded (cleared with no name) */
void SistPart_LoadIni(const char *name, const char *dir);
void __thiscall CSistPart_Render(CSistPart *self, void *dev, CNodoR *r);
void __thiscall CSistPart_Render2(CSistPart *self, void *dev);
void __thiscall CSistPart_SetAlpha(CSistPart *self, float alpha);
void __thiscall CSistPart_Update(CSistPart *self, CNodoR *r);

/* other modules (src/imports.inc) */
/* rva 0x2cad70: the node of a 3D object, counted */
void __thiscall CObjBase3D_SetNodo(IObjeto3D *self, CNodo3D *nodo);
/* rva 0x29d800: the rectangle of sprite i */
void __thiscall CMultiSprite_GetUV(void *self, int i, float *uv0, float *uv1);
/* rva 0x26f120 (suelo.cpp): the segment from a to b against the floor of the world */
byte __thiscall CSuelo_TraceSeg(void *suelo, const vec3_t *a, const vec3_t *b, vec3_t *hit, void *tri, int flags);
/* rva 0x26bff0 and 0x26bc30 (SueloPortal.cpp): the visibility of a sphere for the debug output */
byte __thiscall SectorVis_Sphere(SectorVis *vis, const vec3_t *pos, const vec3_t *center, float radius, const char *name);
/* rva 0x299d50, a debug text at pos */
void Debug_Text3D(void *font, float x, float y, const vec3_t *pos, uint32_t color, const char *fmt, ...);
/* rva 0x2b7150 and 0x2b55d0 (the particle renderer) */
void SistPart_Draw(CSistPart *sp, CNodoR *r);
void SistPart_Draw2(CSistPart *sp, void *dev);

#endif
