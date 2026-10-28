/*
 * Engine/mundo.cpp of the engine3d: CMundo, the world (gMundo). It holds the floor model of the level (its scene with
 * the sectors, the lights and the CSuelo object), the sky model drawn behind it, the camera, the fog (CNiebla of
 * niebla.cpp, the base of CMundo) and the world dummies: the DM_* nodes of the floor scene, taken out of it into a
 * list and kept in a .dum file next to the scene (read from it when the scene comes from the .sm3), and the
 * additional ones of MoreDummies.ini. Also here: the debug points of the engine, lines, boxes and points drawn every
 * frame until they are cleared.
 */
#ifndef SCRAP_MUNDO_H
#define SCRAP_MUNDO_H

#include "scrap/camara.h"
#include "scrap/modelo.h"
#include "scrap/niebla.h"
#include "scrap/suelo.h"

typedef struct MundoDummy {
	struct MundoDummy *next;	/* 0x00 */
	char *name;			/* 0x04 Cad, DM_* */
	vec3_t pos;			/* 0x08 */
	vec3_t rot;			/* 0x14 radians */
	CArchINI *ini;			/* 0x20 the user properties of the node, owned */
} MundoDummy;
SD_SIZE(MundoDummy, 0x24);

typedef struct CMundo {
	CNiebla niebla;			/* 0x00 */
	float cieloPos;			/* 0x24 how much the sky follows the camera (times SkyPosScale), 1 */
	float cieloRot;			/* 0x28 radians, added to SkyRotation */
	CCamara camara;			/* 0x2c the camera of the world without another one */
	CCamara *cam;			/* 0xa4 */
	CModelo *Suelo;			/* 0xa8 the floor model */
	CEscena *EscSuelo;		/* 0xac its scene */
	CSuelo *ObjSuelo;		/* 0xb0 the floor object of the scene */
	CModelo *Cielo;			/* 0xb4 the sky model */
	CEscena *EscCielo;		/* 0xb8 its scene */
	CModelo *tubo;			/* 0xbc drawn after the sky with the far plane SkyTubeTest */
	byte ini_D3D;			/* 0xc0 */
	byte pad_c1[3];
	MundoDummy *dummies;		/* 0xc4 */
} CMundo;
SD_OFFSET(CMundo, camara, 0x2c);
SD_OFFSET(CMundo, dummies, 0xc4);
SD_SIZE(CMundo, 0xc8);

#define g_Mundo GAME_GLOBAL(CMundo *, g_Mundo, 0x4afde8)

/* the inline accessors of mundo.h */
static inline CMundo *Mundo_Get(void) {
	if (!g_Mundo) {
		CheckFailed("gMundo", "Engine/Mundo.h", __TIMESTAMP__, __LINE__);
	}
	return g_Mundo;
}

static inline CSuelo *Mundo_Suelo(void) {
	if (!g_Mundo || !g_Mundo->ObjSuelo) {
		CheckFailed("gMundo && gMundo->obtSuelo()", "Engine/Mundo.h", __TIMESTAMP__, __LINE__);
	}
	return g_Mundo->ObjSuelo;
}

/* the engine variables of the world (Gvars) */
#define g_RenderWorld GAME_GLOBAL_IN(int, GVar_RenderWorld, 16, 0x4afdfc)
#define g_RenderSky GAME_GLOBAL_IN(int, GVar_RenderSky, 16, 0x4afe14)
#define g_WorldPreIllum GAME_GLOBAL_IN(int, GVar_WorldPreIllum, 16, 0x4afe2c)	/* 0 skip, 1 normal, 2 self illumination */
#define g_SkyRotation GAME_GLOBAL_IN(float, GVar_SkyRotation, 16, 0x4afe44)	/* degrees */
#define g_SkyZBuffer GAME_GLOBAL_IN(int, GVar_SkyZBuffer, 16, 0x4afe5c)
#define g_SkyPosScale GAME_GLOBAL_IN(float, GVar_SkyPosScale, 16, 0x4afe74)
#define g_SkyTubeTest GAME_GLOBAL_IN(float, GVar_SkyTubeTest, 16, 0x4afe8c)	/* the far plane of the sky tube */
#define g_ShowWorldDummies GAME_GLOBAL_IN(const char *, GVar_ShowWorldDummies, 16, 0x4afea4)	/* a pattern of the names */
#define g_SkyInit GAME_GLOBAL_IN(int, GVar_SkyInit, 16, 0x4afebc)
/* [mapa_sombras] of the ini: the shadow maps of bsp.cpp */
#define g_MapaSombrasUsar GAME_GLOBAL(int, g_MapaSombrasUsar, 0x4b2af8)
#define g_MapaSombrasCalcular GAME_GLOBAL(int, g_MapaSombrasCalcular, 0x4b2afc)
#define g_MapaSombrasDlg GAME_GLOBAL(int, g_MapaSombrasDlg, 0x43294c)

CMundo *__fastcall CMundo_ctor(CMundo *self);
void __fastcall CMundo_dtor(CMundo *self);
void __fastcall CMundo_ReleaseD3D(CMundo *self);
void __fastcall CMundo_InitD3D(CMundo *self);
/* the sky, the floor, the shadow maps and the world dummies of ShowWorldDummies */
void __fastcall CMundo_Render(CMundo *self);
void __fastcall CMundo_DrawDummies(CMundo *self);
/* appended; a name that is in the list already is warned about */
void __thiscall CMundo_AddDummy(CMundo *self, MundoDummy *d);
/* the sections of the ini next to the floor scene (Pos, Rot in degrees) as dummies; 1 */
byte __thiscall CMundo_LoadMoreDummies(CMundo *self, const char *ini);
/* Pos and Rot of the dummies into the ini next to the floor scene, saved if one changed; whether one did */
byte __thiscall CMundo_SaveMoreDummies(CMundo *self, const char *ini);
/* 100 follows the camera, 0 does not */
void __thiscall CMundo_SetCieloPos(CMundo *self, float pct);
void __thiscall CMundo_SetCieloRot(CMundo *self, float deg);
void __thiscall CMundo_SetLucesCielo(CMundo *self, byte activa);
/* the lights of the floor scene but the ones of the nodes with 0x6000000 */
void __thiscall CMundo_SetLucesSuelo(CMundo *self, byte activa);
/* zNear and zFar of the cameras of the floor scene and of the own camera, 0 keeps one */
void __thiscall CMundo_SetClip(CMundo *self, float zNear, float zFar);
/* NULL: the own camera, put back to its start */
void __thiscall CMundo_SetCamara(CMundo *self, CCamara *cam);
void __thiscall CMundo_LoadCielo(CMundo *self, const char *name);
void __thiscall CMundo_LoadCieloEscala(CMundo *self, const char *name, const vec3_t *escala);
void __thiscall CMundo_LoadSuelo(CMundo *self, const char *name);
void __thiscall CMundo_LoadSueloEscala(CMundo *self, const char *name, const vec3_t *escala);
/* the first camera of the floor scene, its vectors updated with update */
CCamara *__thiscall CMundo_GetCamaraSuelo(CMundo *self, byte update);
CNodo3D *__thiscall CMundo_FindClase(CMundo *self, CNodo3D *from, uint32_t clase);
CNodo3D *__thiscall CMundo_FindNodo(CMundo *self, const char *name);
CNodo3D *__thiscall CMundo_FindNodoWild(CMundo *self, const char *pat);

/* a DM_ node: its name, position, rotation and user properties (taken from the node) */
MundoDummy *__thiscall MundoDummy_ctorNodo(MundoDummy *self, CNodo3D *nodo);
MundoDummy *__thiscall MundoDummy_ctor(MundoDummy *self, const char *name, const vec3_t *pos, const vec3_t *rot);
MundoDummy *__fastcall MundoDummy_ctorEmpty(MundoDummy *self);
void __fastcall MundoDummy_dtor(MundoDummy *self);
void __fastcall MundoDummy_Clear(MundoDummy *self);
int __fastcall MundoDummy_Count(const MundoDummy *list);
void MundoDummy_FreeList(MundoDummy **list);
/* the dummy named name from self on (from the next one without self) */
MundoDummy *__thiscall MundoDummy_Find(MundoDummy *self, const char *name, byte self_too, byte case_sens);
/* the .dum file of the scene path; 0, -1 when there is none */
int Mundo_LoadDUM(const char *path, MundoDummy **list);
int Mundo_SaveDUM(const char *path, const MundoDummy *list);

/* the debug points: a line from a to b in two colors */
int Debug_PuntoLinea(const vec3_t *a, const vec3_t *b, uint32_t ca, uint32_t cb);
/* tipo >= 0: a point of that size; -1..0: the start of a line to the next point; < -1: a box to the next point */
int Debug_Punto(const vec3_t *pos, uint32_t color, float tipo);
void Debug_PuntosClear(void);
int Debug_PuntosDraw(void);

/* other modules (src/imports.inc) */
/* bsp.cpp, the shadow maps (rva 0x290520, 0x286c80, 0x286cb0, 0x291000, 0x296cd0) */
void Bsp_Free(void);
void Bsp_ReleaseD3D(void);
void Bsp_InitD3D(void);
void Bsp_Crear(void);
void Bsp_Render(void);
/* bsp_q3.cpp (rva 0x2c9d50, 0x2c98a0, 0x2c98b0, 0x2c9d80, 0x2c9d90) */
void BspQ3_Free(void);
void BspQ3_ReleaseD3D(void);
void BspQ3_InitD3D(void);
void BspQ3_Crear(void);
void BspQ3_Render(void);

#endif
