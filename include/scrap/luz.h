/*
 * CLuz of the engine3d (original source Engine/luz.cpp): a light of the scenes. The active lights are in a global
 * list of up to MAX_LUCES; each light owns a D3D light index and keeps a D3DLIGHT8 filled from its members. The
 * lights for one object are chosen per draw (Luz_Select) and switched on through the device, R_LightEnaD3D caches
 * the enable state of the device in ena_D3D.
 */
#ifndef SCRAP_LUZ_H
#define SCRAP_LUZ_H

#include "scrap/archio.h"
#include "scrap/mathlib.h"
#include "scrap/nodo3d.h"

#define CLASE_OBJ3D_LUZ 0xa3000008
#define MAX_LUCES 1024
#define MAX_LUCES_OBJ 16	/* the size of the output of Luz_Select */

enum { LUZ_OMNI = 5000, LUZ_FOCO = 5001, LUZ_DIREC = 5002 };
/* D3DLIGHTTYPE */
enum { D3DLIGHT_POINT = 1, D3DLIGHT_SPOT = 2, D3DLIGHT_DIRECTIONAL = 3 };

typedef struct {
	float r, g, b, a;
} color4_t;
SD_SIZE(color4_t, 16);

/* D3DLIGHT8 (the same layout as D3DLIGHT9) */
typedef struct D3DLight {
	int Type;			/* 0x00 */
	color4_t Diffuse;		/* 0x04 */
	color4_t Specular;		/* 0x14 */
	color4_t Ambient;		/* 0x24 */
	vec3_t Position;		/* 0x34 */
	vec3_t Direction;		/* 0x40 */
	float Range;			/* 0x4c */
	float Falloff;			/* 0x50 */
	float Attenuation0;		/* 0x54 */
	float Attenuation1;		/* 0x58 */
	float Attenuation2;		/* 0x5c */
	float Theta;			/* 0x60 radians */
	float Phi;			/* 0x64 */
} D3DLight;
SD_SIZE(D3DLight, 0x68);

typedef struct CLuz {
	void *vftable;			/* 0x00 */
	uint32_t clase;			/* 0x04 CLASE_OBJ3D_LUZ */
	int tipo;			/* 0x08 LUZ_* */
	byte mostrar;			/* 0x0c drawn also without R_ShowLights */
	byte sombra;			/* 0x0d casts shadows (with NODO_SOMBRA) */
	byte pad_e[2];
	vec3_t pos;			/* 0x10 */
	vec3_t dir;			/* 0x1c in the node's frame */
	byte color[4];			/* 0x28 r, g, b, a */
	float intensidad;		/* 0x2c */
	float f30;			/* 0x30 */
	float rango;			/* 0x34 0: no limit */
	float theta;			/* 0x38 degrees */
	float phi;			/* 0x3c degrees */
	float mult;			/* 0x40 */
	float f44;			/* 0x44 */
	float peso;			/* 0x48 sort key of Luz_Select, lower first */
	D3DLight d3d;			/* 0x4c */
	uint32_t indD3D;		/* 0xb4 the index of the device light */
	byte act_D3D;			/* 0xb8 set in the device */
	byte ena_D3D;			/* 0xb9 enabled in the device */
	byte activa;			/* 0xba in the global list */
	byte pad_bb;
	int ind_lista;			/* 0xbc */
	int sector;			/* 0xc0 -1: all */
	CNodo3D *nodo;			/* 0xc4 */
	CNodo3D *nodoObjetivo;		/* 0xc8 the direction points at it (R_LightTarget) */
} CLuz;
SD_OFFSET(CLuz, d3d, 0x4c);
SD_OFFSET(CLuz, indD3D, 0xb4);
SD_OFFSET(CLuz, nodo, 0xc4);
SD_SIZE(CLuz, 0xcc);

CLuz *__fastcall CLuz_ctor(CLuz *self);
void __fastcall CLuz_dtor(CLuz *self);
CLuz *__thiscall CLuz_Delete(CLuz *self, byte flags);
void __fastcall IObjeto3D_dtor(IObjeto3D *self);
IObjeto3D *__thiscall IObjeto3D_Delete(IObjeto3D *self, byte flags);
/* the virtual Read and Write of IObjeto3D: nothing, 0 */
byte __thiscall IObjeto3D_Read(IObjeto3D *self, CArchIO *f, void *ctx);
byte __thiscall IObjeto3D_Write(IObjeto3D *self, CArchIO *f, void *ctx);
/* the lights for an object at pos (sphere center, radius) into out, sorted by peso; the first maxOn are enabled in
 * the device, the others and the rejected ones disabled; the count */
int Luz_Select(const vec3_t *pos, int sector, const vec3_t *center, float radius, CLuz **out, int maxOn);
/* on: three fixed directional lights (0..2) and the ambient of EnableMonitorLights instead of the scene lights; 0 */
int Luz_SetDefault(byte on);
/* the virtual methods 1, 3 and 4; arg is not used */
void __thiscall CLuz_Update(CLuz *self, int arg);
void __thiscall CLuz_Reset(CLuz *self, int arg);
void __thiscall CLuz_Hide(CLuz *self, int arg);
/* a directional light without position into device light idx */
void Luz_SetDirLight(uint32_t idx, const color4_t *color, const vec3_t *dir);
void __thiscall CLuz_EnableD3D(CLuz *self, byte enable);
/* the "LUZ" block; ctx holds the node at 0x4 */
byte __thiscall CLuz_Read(CLuz *self, CArchIO *f, void *ctx);
byte __thiscall CLuz_Write(CLuz *self, CArchIO *f, void *ctx);
/* the list of the lights to the debug console */
void Luz_List(void);
/* all lights of the list enabled in the device as they are set (act_D3D), or disabled */
void Luz_EnableAll(byte enable);
/* the D3D light with the engine variables (multipliers, attenuation, gamma, desaturation, clamp) applied */
void __thiscall CLuz_AdjustD3D(CLuz *self, D3DLight *l);
void __thiscall CLuz_SetNodo(CLuz *self, CNodo3D *nodo);
void __thiscall CLuz_SetNodoObjetivo(CLuz *self, CNodo3D *nodo);
/* into the global list or out of it */
void __thiscall CLuz_SetActiva(CLuz *self, byte activa);
/* qsort order of CLuz pointers by peso, never 0 */
int Luz_Compare(const void *a, const void *b);
void __thiscall CLuz_FillD3D(CLuz *self, D3DLight *l);
int Luz_NumD3D(void);
int Luz_NumActivas(void);
int Luz_NumTotal(void);
/* act_D3D from activa (hide: off), the device light set while act_D3D */
void __thiscall CLuz_UpdateD3D(CLuz *self, byte hide);
/* a box of zeros (in the range of luz.cpp, used by the scene nodes) */
box_t *__fastcall Box_Zero(box_t *self);

/* other modules, called from luz.cpp (src/imports.inc) */
void E3D_FlushBatch(void);	/* rva 0x24cec0, the pending primitives of the renderer */
/* rva 0x2682d0, the light of the shadows (NULL: none); the second argument is 0 here */
void E3D_SetShadowLight(CLuz *l, int arg);
int Prof_Count(int section, int n);	/* rva 0x273040, a profiler counter, the new value */
byte Sph_IsVisible(const vec3_t *center, float radius, int arg);	/* rva 0x24d2e0, the view frustum */
/* rva 0x26e490, gMundo->obtSuelo(): 1 if the segment from-to hits the ground */
byte __thiscall CSuelo_Trace(void *self, const vec3_t *from, const vec3_t *to, void *a, float b, void *c);

#endif
