/*
 * ScrapEng/ScrapEngWorld.cpp of the game: the game side of the world of the engine3d (gMundo). The world node info
 * walks the world dummies and then the nodes without a 3D object of the floor scene by name (a case-insensitive name
 * or a lowercase wildcard pattern) and gives their name, position and matrix; the user properties of the current
 * dummy are read through it. Also here: the box of the floor, the sector at a position, the identity matrix.
 * The rest of the code up to ScrapEngMain.cpp (without CHECKs, by the address range): RenderMode, the D3D color of
 * a color with intensity, the clear of a 640x480 rectangle, the caps check of the video card, the splash screens
 * and the loading bar.
 */
#ifndef SCRAP_SCRAPWORLD_H
#define SCRAP_SCRAPWORLD_H

#include "scrap/gvar.h"
#include "scrap/e3dworld.h"

/* what the world node info functions give for one dummy or node */
typedef struct WorldNodeInfo {
	char Name[64];			/* 0x00 */
	vec3_t pos;			/* 0x40 */
	matrix_t m;			/* 0x4c */
	byte dummy;			/* 0x8c 1 for a world dummy; for a node: it has no 3D object */
} WorldNodeInfo;
SD_OFFSET(WorldNodeInfo, pos, 0x40);
SD_OFFSET(WorldNodeInfo, m, 0x4c);
SD_OFFSET(WorldNodeInfo, dummy, 0x8c);

/* the state of the walk */
extern int g_CountSprite2D;
extern CNode3D *g_WorldInfoNode;
extern WorldDummy *g_WorldInfoDummy;	/* DummyInfo */
extern byte g_WorldInfoWild;
extern char g_WorldInfoPattern[260];	/* 0x104, "" for all */

/* RenderMode */
#define RENDERMODE_PROFILER 0x10	/* cycles the profiler mode 0..2 */
#define RENDERMODE_TILE 0x20		/* only set to 1 */
#define RENDERMODE_100 0x100
#define RENDERMODE_SPECULAR 0x101	/* toggles the vertex specular */
#define RENDERMODE_TEXFILTER 0x102	/* cycles point, linear, anisotropic */
#define RENDERMODE_SHADOWS 0x103	/* toggles the stencil shadows */
#define g_RenderMode100 GAME_GLOBAL_IN(byte, group_4a8efc, 1)
#define g_VertexSpecular GAME_GLOBAL_IN(byte, group_4a8efc, 0)
extern int g_ShadowsStencil;	/* [sombras_stencil] use */

/* the splash screens and the loading bar */
extern cGVar GVar_ShowSplashScreen;
#define g_SplashOn (GVar_ShowSplashScreen.value.i)
#define g_SplashFade GAME_GLOBAL_IN(uint32_t, GVar_SplashScreenFade, 16)	/* ms */
extern uint32_t g_SplashTime;	/* timeGetTime at the end of the last one */
extern char g_SplashLast[260];	/* 0x104, the last one shown */
#define g_LoadingOn GAME_GLOBAL_IN(byte, group_4a8324, 0)
extern cGVar GVar_ShowProgressBar;
#define g_LoadingBar (GVar_ShowProgressBar.value.i)
extern int g_LoadingCount;		/* the steps of Loading_Step(-1) */
extern cGVar GVar_ProgressBar;
#define g_LoadingTotal (GVar_ProgressBar.value.i)
#define g_LoadingDrawOn GAME_GLOBAL(byte, g_CallbacksOn)
typedef void (*loadingDraw_t)(void);
#define g_LoadingDraw GAME_GLOBAL_IN(loadingDraw_t, g_EngCallbacks, 12)	/* draws the loading screen without the bar */
#define g_LoadingDraw2 GAME_GLOBAL_IN(loadingDraw_t, g_EngCallbacks, 8)

/* the world node info */
void WorldInfo_FromDummy(WorldNodeInfo *info, const WorldDummy *dummy);
void WorldInfo_FromNode(WorldNodeInfo *info, CNode3D *node);
/* the walk from the start: the names that match pattern (NULL for all), lowercase wildcards with wild */
void WorldInfo_Begin(const char *pattern, byte wild);
void WorldInfo_Reset(void);
/* the next dummy (0 at the end); the next node is a CHECK */
byte WorldInfo_Next(WorldNodeInfo *info);
byte WorldInfo_NextDummy(WorldNodeInfo *info);
byte WorldInfo_NextNode(WorldNodeInfo *info);
/* the user properties of the current dummy */
byte WorldInfo_HasUserProp(void);
byte WorldInfo_GetBool(const char *sect, const char *key, byte def);
float WorldInfo_GetFloat(const char *sect, const char *key, float def);
int WorldInfo_GetInt(const char *sect, const char *key, int def);
char *WorldInfo_GetText(const char *sect, const char *key, char *dest, int size, const char *def);
vec3_t *WorldInfo_GetVec3(vec3_t *out, const char *sect, const char *key, const vec3_t *def);

/* the box of the floor scene, of the collision of the floor */
box_t *World_GetBox(box_t *out);
box_t *World_GetCollideBox(box_t *out);
/* the inline of ground.h */
box_t *CGround_GetCollideExt(const CGround *self, box_t *out);
byte Sector_Visible(int sector);
/* returns m (an inline of the matrix header, not optimized away) */
matrix_t *Matrix_Self(matrix_t *m);

/* the old value of *flag, set to value with set */
int Bool_GetSet(byte *flag, int value, byte set);
/* RENDERMODE_*: the value after the change of set, 0 for an unknown mode */
int RenderMode(int mode, int value, byte set);
/* the first n (0..99, 99 also when all exist) with the file fmt % n missing */
void File_FreeIndex(const char *fmt, int *n);
/* a clear of the rectangle in 640x480 coordinates, the color too with target; whether the device cleared */
byte Render_ClearRect(float x, float y, float w, float h, byte target, uint32_t color);
/* the back buffer through a copy into the front buffer and back after a Present */
void Render_CopyBackBuffer(void);

/* a splash screen ("" the last one, NULL the one of the level), faded in and out in g_SplashFade ms with fadeIn and
 * fadeOut, shown waitMs ms ("": waitMs after the last one) or until Esc, space or enter */
void Splash_Show(const char *name, byte fadeIn, uint32_t waitMs, byte fadeOut);
/* one frame with the texture over the screen in color */
void Splash_Draw(const box_t *rect, uint32_t color, void *tex);
void Splash_Black(void);
void Splash_Promos(void);
void Splash_Logos(void);
void Splash_Reset(void);
/* the loading bar at percent (-1: one step of g_LoadingTotal), else the loading screen */
void Loading_Step(int percent);
void Loading_DrawBack(void);
void Loading_DrawBack2(void);

/* other modules (src/imports.inc) */
/* rva 0x2178e0 (ScrapEngMain.cpp): the splash screen name of the level */
/* rva 0x267460: the stencil shadows on or off */
/* rva 0x20d290: the window of the application */

#endif
