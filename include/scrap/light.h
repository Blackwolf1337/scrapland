/*
 * CLight of the engine3d (original source Engine/luz.cpp): a light of the scenes. The active lights are in a global
 * list of up to MAX_LIGHTS; each light owns a D3D light index and keeps a D3DLIGHT8 filled from its members. The
 * lights for one object are chosen per draw (Light_Select) and switched on through the device, R_LightEnaD3D caches
 * the enable state of the device in ena_D3D.
 */
#ifndef SCRAP_LIGHT_H
#define SCRAP_LIGHT_H

#include "scrap/archio.h"
#include "scrap/mathlib.h"
#include "scrap/node3d.h"

#define MAX_LIGHTS 1024
#define MAX_LIGHTS_OBJ 16	/* the size of the output of Light_Select */

enum { LIGHT_OMNI = 5000, LIGHT_SPOT = 5001, LIGHT_DIR = 5002 };
/* D3DLIGHTTYPE */
enum { D3DLIGHT_POINT = 1, D3DLIGHT_SPOT = 2, D3DLIGHT_DIRECTIONAL = 3 };

/* the inline CLight::tipoStr of light.h */
static inline const char *Light_TypeStr(int type) {
	switch (type) {
	case LIGHT_OMNI:
		return "omni";
	case LIGHT_SPOT:
		return "foco";
	case LIGHT_DIR:
		return "direc";
	}
	CHECK(0);
	return NULL;
}

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

/* Engine/RenderCalcIlum.cpp (RenderCalcIllum.c): a light with the distance from which it fades out to its range */
typedef struct IllumLight {
	D3DLight d3d;			/* 0x00 */
	float rangeIni;			/* 0x68 */
} IllumLight;
SD_SIZE(IllumLight, 0x6c);
#define ILLUM_SOFT 0x1		/* smooth fades instead of the D3D attenuation */
/* v smoothed by the weight g_IllumSmooth */
float Illum_Smooth(float v);
struct CVertList;
struct CMaterial;
struct CLight;
/* the light Light at the vertices of lv added to their colors in dest (one a vertex, no smoothing); 0 */
int Illum_VertList(color4_t *dest, struct CVertList *lv, struct CMaterial *Mat, struct CLight *Light);
/* the light at pos with normal n added to dest; mat: the diffuse and ambient color (D3DMATERIAL8); 1 for no light */
int Illum_Light(color4_t *dest, const vec3_t *pos, const vec3_t *n, const float *mat, const IllumLight *light,
	uint32_t flags);

typedef struct CLight {
	void *vftable;			/* 0x00 */
	uint32_t class;			/* 0x04 CLASS_OBJ3D_LIGHT */
	int type;			/* 0x08 LUZ_* */
	byte show;			/* 0x0c drawn also without R_ShowLights */
	byte shadow;			/* 0x0d casts shadows (with NODE_SHADOW) */
	byte pad_e[2];
	vec3_t pos;			/* 0x10 */
	vec3_t dir;			/* 0x1c in the node's frame */
	byte color[4];			/* 0x28 r, g, b, a */
	float intensity;		/* 0x2c */
	float rangeStart;		/* 0x30 "AtenIni": the attenuation starts there */
	float range;			/* 0x34 0: no limit */
	float theta;			/* 0x38 degrees */
	float phi;			/* 0x3c degrees */
	float mult;			/* 0x40 */
	float f44;			/* 0x44 */
	float weight;			/* 0x48 sort key of Light_Select, lower first */
	D3DLight d3d;			/* 0x4c */
	uint32_t indexD3D;		/* 0xb4 the index of the device light */
	byte act_D3D;			/* 0xb8 set in the device */
	byte ena_D3D;			/* 0xb9 enabled in the device */
	byte active;			/* 0xba in the global list */
	byte pad_bb;
	int index_list;			/* 0xbc */
	int sector;			/* 0xc0 -1: all */
	CNode3D *node;			/* 0xc4 */
	CNode3D *nodeTarget;		/* 0xc8 the direction points at it (R_LightTarget) */
} CLight;
SD_OFFSET(CLight, d3d, 0x4c);
SD_OFFSET(CLight, indexD3D, 0xb4);
SD_OFFSET(CLight, node, 0xc4);
SD_SIZE(CLight, 0xcc);

CLight *__fastcall CLight_ctor(CLight *self);
void __fastcall CLight_dtor(CLight *self);
CLight *__thiscall CLight_Delete(CLight *self, byte flags);
void __fastcall IObject3D_dtor(IObject3D *self);
IObject3D *__thiscall IObject3D_Delete(IObject3D *self, byte flags);
/* the virtual Read and Write of IObject3D: nothing, 0 */
byte __thiscall IObject3D_Read(IObject3D *self, CFileIO *f, void *ctx);
byte __thiscall IObject3D_Write(IObject3D *self, CFileIO *f, void *ctx);
/* the lights for an object at pos (sphere center, radius) into out, sorted by weight; the first maxOn are enabled in
 * the device, the others and the rejected ones disabled; the count */
int Light_Select(const vec3_t *pos, int sector, const vec3_t *center, float radius, CLight **out, int maxOn);
/* on: three fixed directional lights (0..2) and the ambient of EnableMonitorLights instead of the scene lights; 0 */
int Light_SetDefault(byte on);
/* the virtual methods 1, 3 and 4; arg is not used */
void __thiscall CLight_Update(CLight *self, int arg);
void __thiscall CLight_Reset(CLight *self, int arg);
void __thiscall CLight_Hide(CLight *self, int arg);
/* the virtual Render: the debug drawing of the light (R_ShowLights) */
void __thiscall CLight_Render(CLight *self, void *dev, CRenderNode *r);
/* a directional light without position into device light idx */
void Light_SetDirLight(uint32_t idx, const color4_t *color, const vec3_t *dir);
void __thiscall CLight_EnableD3D(CLight *self, byte enable);
/* the "LUZ" block; ctx holds the node at 0x4 */
byte __thiscall CLight_Read(CLight *self, CFileIO *f, void *ctx);
byte __thiscall CLight_Write(CLight *self, CFileIO *f, void *ctx);
/* the list of the lights to the debug console */
void Light_List(void);
/* all lights of the list enabled in the device as they are set (act_D3D), or disabled */
void Light_EnableAll(byte enable);
/* the D3D light with the engine variables (multipliers, attenuation, gamma, desaturation, clamp) applied */
void __thiscall CLight_AdjustD3D(CLight *self, D3DLight *l);
void __thiscall CLight_SetNode(CLight *self, CNode3D *node);
void __thiscall CLight_SetNodeTarget(CLight *self, CNode3D *node);
/* into the global list or out of it */
void __thiscall CLight_SetActive(CLight *self, byte active);
/* qsort order of CLight pointers by weight, never 0 */
int Light_Compare(const void *a, const void *b);
void __thiscall CLight_FillD3D(CLight *self, D3DLight *l);
int Light_NumD3D(void);
int Light_NumActive(void);
int Light_NumTotal(void);
/* act_D3D from active (hide: off), the device light set while act_D3D */
void __thiscall CLight_UpdateD3D(CLight *self, byte hide);
/* a box of zeros (in the range of luz.cpp, used by the scene nodes) */
box_t *__fastcall Box_Zero(box_t *self);

/* other modules, called from luz.cpp (src/imports.inc) */
/* rva 0x2682d0, the light of the shadows (NULL: none); the second argument is 0 here */
void E3D_SetShadowLight(CLight *l, int arg);
/* rva 0x26e490, gMundo->obtSuelo(): 1 if the segment from-to hits the ground */
byte __thiscall CGround_Trace(void *self, const vec3_t *from, const vec3_t *to, void *a, float b, void *c);

#endif
