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

/* D3DLIGHT8 (the same layout as D3DLIGHT9) */
typedef struct D3DLight {
	int Type;
	color4_t Diffuse;
	color4_t Specular;
	color4_t Ambient;
	vec3_t Position;
	vec3_t Direction;
	float Range;
	float Falloff;
	float Attenuation0;
	float Attenuation1;
	float Attenuation2;
	float Theta;			/* radians */
	float Phi;
} D3DLight;

/* Engine/RenderCalcIlum.cpp (RenderCalcIllum.c): a light with the distance from which it fades out to its range */
typedef struct IllumLight {
	D3DLight d3d;
	float rangeIni;
} IllumLight;
#define ILLUM_SOFT 0x1		/* smooth fades instead of the D3D attenuation */
struct CVertList;
struct CMaterial;
struct CLight;
/* the light Light at the vertices of lv added to their colors in dest (one a vertex, no smoothing); 0 */
int Illum_VertList(color4_t *dest, struct CVertList *lv, struct CMaterial *Mat, struct CLight *Light);
/* the light at pos with normal n added to dest; mat: the diffuse and ambient color (D3DMATERIAL8); 1 for no light */
int Illum_Light(color4_t *dest, const vec3_t *pos, const vec3_t *n, const float *mat, const IllumLight *light,
	uint32_t flags);

typedef struct CLight {
	union {
		IObject3D base;
		struct {
			const IObject3DVtbl *vftable;
			uint32_t class;			/* CLASS_OBJ3D_LIGHT */
		};
	};
	int type;			/* LUZ_* */
	byte show;			/* drawn also without R_ShowLights */
	byte shadow;			/* casts shadows (with NODE_SHADOW) */
	byte pad_e[2];
	vec3_t pos;
	vec3_t dir;			/* in the node's frame */
	byte color[4];			/* r, g, b, a */
	float intensity;
	float rangeStart;		/* "AtenIni": the attenuation starts there */
	float range;			/* 0: no limit */
	float theta;			/* degrees */
	float phi;			/* degrees */
	float mult;
	float radCoef;		/* "radcoef" of the node name / 100, 1: the radiosity factor (BspLight.rad) */
	float weight;			/* sort key of Light_Select, lower first */
	D3DLight d3d;
	uint32_t indexD3D;		/* the index of the device light */
	byte act_D3D;			/* set in the device */
	byte ena_D3D;			/* enabled in the device */
	byte active;			/* in the global list */
	byte pad_bb;
	int index_list;
	int sector;			/* -1: all */
	CNode3D *node;
	CNode3D *nodeTarget;		/* the direction points at it (R_LightTarget) */
} CLight;

CLight *CLight_ctor(CLight *self);
void IObject3D_Free(IObject3D *self);
void IObject3D_InitD3DNop(IObject3D *self, void *dev);
byte IObject3D_ReadFalse(IObject3D *self, CFileIO *f, MeshD3D_FileCtx *ctx);
void IObject3D_ReleaseD3DNop(IObject3D *self, void *dev);
void IObject3D_RenderNop(IObject3D *self, void *dev, CRenderNode *r);
void IObject3D_UpdateNop(IObject3D *self, CRenderNode *r);
byte IObject3D_WriteFalse(IObject3D *self, CFileIO *f, MeshD3D_FileCtx *ctx);
/* the lights for an object at pos (sphere center, radius) into out, sorted by weight; the first maxOn are enabled in
 * the device, the others and the rejected ones disabled; the count */
int Light_Select(const vec3_t *pos, int sector, const vec3_t *center, float radius, CLight **out, int maxOn);
/* on: three fixed directional lights (0..2) and the ambient of EnableMonitorLights instead of the scene lights; 0 */
int Light_SetDefault(byte on);
/* the list of the lights to the debug console */
void Light_List(void);
/* the D3D light with the engine variables (multipliers, attenuation, gamma, desaturation, clamp) applied */
void CLight_AdjustD3D(CLight *self, D3DLight *l);
void CLight_SetNode(CLight *self, CNode3D *node);
/* into the global list or out of it */
void CLight_SetActive(CLight *self, byte active);
void CLight_FillD3D(CLight *self, D3DLight *l);
int Light_NumD3D(void);
int Light_NumActive(void);
int Light_NumTotal(void);
/* a box of zeros (in the range of luz.cpp, used by the scene nodes) */
box_t *Box_Zero(box_t *self);

/* other modules, called from luz.cpp (src/imports.inc) */
/* the light of the shadows (NULL: none); the second argument is 0 here */
void E3D_SetShadowLight(CLight *l, int arg);

#endif
