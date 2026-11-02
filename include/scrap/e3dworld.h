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

/* the engine variables of the world (Gvars) */
extern cGVar GVar_RenderWorld;
#define g_RenderWorld (GVar_RenderWorld.value.i)
extern cGVar GVar_RenderSky;
#define g_RenderSky (GVar_RenderSky.value.i)
extern cGVar GVar_WorldPreIllum;
#define g_WorldPreIllum (GVar_WorldPreIllum.value.i)	/* 0 skip, 1 normal, 2 self illumination */
extern cGVar GVar_SkyRotation;
#define g_SkyRotation (GVar_SkyRotation.value.f)	/* degrees */
extern cGVar GVar_SkyZBuffer;
#define g_SkyZBuffer (GVar_SkyZBuffer.value.i)
extern cGVar GVar_SkyPosScale;
#define g_SkyPosScale (GVar_SkyPosScale.value.f)
extern cGVar GVar_SkyTubeTest;
#define g_SkyTubeTest (GVar_SkyTubeTest.value.f)	/* the far plane of the sky tube */
extern cGVar GVar_ShowWorldDummies;
#define g_ShowWorldDummies (GVar_ShowWorldDummies.value.s)	/* a pattern of the names */
extern cGVar GVar_SkyInit;
#define g_SkyInit (GVar_SkyInit.value.i)
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
void CWorld_DrawDummies(CWorld *self);
/* appended; a name that is in the list already is warned about */
void CWorld_AddDummy(CWorld *self, WorldDummy *d);
/* the sections of the ini next to the floor scene (Pos, Rot in degrees) as dummies; 1 */
byte CWorld_LoadMoreDummies(CWorld *self, const char *ini);
/* 100 follows the camera, 0 does not */
void CWorld_SetSkyPos(CWorld *self, float pct);
void CWorld_SetSkyRot(CWorld *self, float deg);
void CWorld_SetSkyLights(CWorld *self, byte active);
/* the lights of the floor scene but the ones of the nodes with 0x6000000 */
void CWorld_SetGroundLights(CWorld *self, byte active);
/* zNear and zFar of the cameras of the floor scene and of the own camera, 0 keeps one */
void CWorld_SetClip(CWorld *self, float zNear, float zFar);
/* NULL: the own camera, put back to its start */
void CWorld_SetCamera(CWorld *self, CCamera *cam);
void CWorld_LoadSky(CWorld *self, const char *name);
void CWorld_LoadSkyScale(CWorld *self, const char *name, const vec3_t *scale);
void CWorld_LoadGroundScale(CWorld *self, const char *name, const vec3_t *scale);
/* the first camera of the floor scene, its vectors updated with update */
CCamera *CWorld_GetCameraGround(CWorld *self, byte update);
CNode3D *CWorld_FindClass(CWorld *self, CNode3D *from, uint32_t class);
CNode3D *CWorld_FindNode(CWorld *self, const char *name);
CNode3D *CWorld_FindNodeWild(CWorld *self, const char *pat);

/* a DM_ node: its name, position, rotation and user properties (taken from the node) */
WorldDummy *WorldDummy_ctorNode(WorldDummy *self, CNode3D *node);
WorldDummy *WorldDummy_ctor(WorldDummy *self, const char *name, const vec3_t *pos, const vec3_t *rot);
WorldDummy *WorldDummy_ctorEmpty(WorldDummy *self);
void WorldDummy_dtor(WorldDummy *self);
int WorldDummy_Count(const WorldDummy *list);
void WorldDummy_FreeList(WorldDummy **list);
/* the dummy named name from self on (from the next one without self) */
WorldDummy *WorldDummy_Find(WorldDummy *self, const char *name, byte self_too, byte case_sens);
/* the .dum file of the scene path; 0, -1 when there is none */
int World_LoadDUM(const char *path, WorldDummy **list);
int World_SaveDUM(const char *path, const WorldDummy *list);

/* the debug points: a line from a to b in two colors */
int Debug_PointLine(const vec3_t *a, const vec3_t *b, uint32_t ca, uint32_t cb);
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
