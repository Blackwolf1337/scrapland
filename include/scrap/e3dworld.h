/*
 * Engine/mundo.cpp of the engine3d: CWorld, the world (gMundo). It holds the floor model of the level (its scene with
 * the sectors, the lights and the CGround object), the sky model drawn behind it, the camera, the fog (CFog of
 * niebla.cpp, the base of CWorld) and the world dummies: the DM_* nodes of the floor scene, taken out of it into a
 * list and kept in a .dum file next to the scene (read from it when the scene comes from the .sm3), and the
 * additional ones of MoreDummies.ini. Also here: the debug points of the engine, lines, boxes and points drawn every
 * frame until they are cleared.
 */
#ifndef SCRAP_E3DWORLD_H
#define SCRAP_E3DWORLD_H

#include "scrap/gvar.h"
#include "scrap/camera.h"
#include "scrap/model.h"
#include "scrap/fog.h"
#include "scrap/ground.h"

typedef struct WorldDummy {
	struct WorldDummy *next;	/* 0x00 */
	char *name;			/* 0x04 Cad, DM_* */
	vec3_t pos;			/* 0x08 */
	vec3_t rot;			/* 0x14 radians */
	CIniFile *ini;			/* 0x20 the user properties of the node, owned */
} WorldDummy;

typedef struct CWorld {
	CFog fog;			/* 0x00 */
	float skyPos;			/* 0x24 how much the sky follows the camera (times SkyPosScale), 1 */
	float skyRot;			/* 0x28 radians, added to SkyRotation */
	CCamera camera;			/* 0x2c the camera of the world without another one */
	CCamera *cam;			/* 0xa4 */
	CModel *Ground;			/* 0xa8 the floor model */
	CScene *GroundScale;		/* 0xac its scene */
	CGround *ObjGround;		/* 0xb0 the floor object of the scene */
	CModel *Sky;			/* 0xb4 the sky model */
	CScene *SkyScale;		/* 0xb8 its scene */
	CModel *tube;			/* 0xbc drawn after the sky with the far plane SkyTubeTest */
	byte ini_D3D;			/* 0xc0 */
	byte pad_c1[3];
	WorldDummy *dummies;		/* 0xc4 */
} CWorld;

extern CWorld *g_World;

/* the inline accessors of world.h */
static inline CWorld *World_Get(void) {
	if (!g_World) {
		CheckFailed("gMundo", "Engine/Mundo.h", __TIMESTAMP__, __LINE__);
	}
	return g_World;
}

static inline CGround *World_Ground(void) {
	if (!g_World || !g_World->ObjGround) {
		CheckFailed("gMundo && gMundo->obtSuelo()", "Engine/Mundo.h", __TIMESTAMP__, __LINE__);
	}
	return g_World->ObjGround;
}

/* [mapa_sombras] of the ini: the shadow maps of bsp.cpp */
extern int g_ShadowMapUse;
extern int g_ShadowMapCalculate;
extern int g_ShadowMapDlg;

CWorld *CWorld_ctor(CWorld *self);
void CWorld_dtor(CWorld *self);
void CWorld_ReleaseD3D(CWorld *self);
void CWorld_InitD3D(CWorld *self);
/* the sky, the floor, the shadow maps and the world dummies of ShowWorldDummies */
void CWorld_Render(CWorld *self);
/* 100 follows the camera, 0 does not */
void CWorld_SetSkyPos(CWorld *self, float pct);
void CWorld_SetSkyRot(CWorld *self, float deg);
/* zNear and zFar of the cameras of the floor scene and of the own camera, 0 keeps one */
void CWorld_SetClip(CWorld *self, float zNear, float zFar);
/* NULL: the own camera, put back to its start */
void CWorld_SetCamera(CWorld *self, CCamera *cam);
void CWorld_LoadSky(CWorld *self, const char *name);
void CWorld_LoadGroundScale(CWorld *self, const char *name, const vec3_t *scale);
/* the first camera of the floor scene, its vectors updated with update */
CCamera *CWorld_GetCameraGround(CWorld *self, byte update);
CNode3D *CWorld_FindClass(CWorld *self, CNode3D *from, uint32_t class);
CNode3D *CWorld_FindNodeWild(CWorld *self, const char *pat);

/* type >= 0: a point of that size; -1..0: the start of a line to the next point; < -1: a box to the next point */
int Debug_Point(const vec3_t *pos, uint32_t color, float type);
void Debug_PointsClear(void);
int Debug_PointsDraw(void);

/* other modules (src/imports.inc) */
/* bsp.cpp, the shadow maps */
void Bsp_Free(void);
void Bsp_ReleaseD3D(void);
void Bsp_InitD3D(void);
void Bsp_Create(void);
void Bsp_Render(void);
/* bsp_q3.cpp */
void BspQ3_Free(void);
void BspQ3_ReleaseD3D(void);
void BspQ3_InitD3D(void);
void BspQ3_Create(void);
void BspQ3_Render(void);

#endif
