/*
 * ScrapEngMain.cpp of the game: the glue between the game and the engine3d. The start and the end of the engine
 * (the D3D application g_Engine and its three callbacks), the level start and end with the loading screens, the
 * video modes of the adapter, the music and video players, world to screen, the engine printf, screenshots and AVI
 * capture, the model list and info, the viewer camera and the engine options of m3d.ini.
 */
#ifndef SCRAP_SCRAPMAIN_H
#define SCRAP_SCRAPMAIN_H

#include "scrap/gvar.h"
#include "scrap/archio.h"
#include "scrap/camera.h"
#include "scrap/dxutil.h"
#include "scrap/e3dinput.h"
#include "scrap/e3dutil.h"
#include "scrap/model.h"
#include "scrap/musicfmod.h"
#include "scrap/node3d.h"
#include "scrap/scrap.h"
#include "scrap/scrapcamera.h"
#include "scrap/scrapworld.h"
#include "scrap/table.h"

/* CMotorApp of Engine/Motor.cpp, 0x2a600 bytes (g_Engine3D, also g_Engine as its CD3DApplication) */
typedef struct CEngineApp {
	CD3DApplication base;			/* 0x00000 */
	void *menu;				/* 0x2a5dc HMENU */
	void *cameraMenu;			/* 0x2a5e0 the submenu of the cameras of the global scene */
	CTable table;				/* 0x2a5e4 */
	CScene *scene;				/* 0x2a5ec EscenaGlobal, counted */
	struct CGraphic2DList *overlay;		/* 0x2a5f0 */
	void **obj2a5f4;			/* 0x2a5f4 an object deleted by FinalCleanup */
	byte canWBuffer;			/* 0x2a5f8 D3DPRASTERCAPS_WBUFFER of the device */
	byte useWBuffer;			/* 0x2a5f9 the W buffer (g_ZEnable 2) when the device can */
	/* the menu items 0x9c5b..0x9c5d: two of them enabled by these, the third checked by menuMode 0, the other two by 1 */
	byte canMenu9c5d;			/* 0x2a5fa */
	byte canMenu9c5c;			/* 0x2a5fb */
	int menuMode;				/* 0x2a5fc */
} CEngineApp;

/* a camera of the game (0x3c bytes) over a CCamera of the engine */

/* the watermark drawn with ShowSplashScreen 100 (0x44 bytes, a .psa image) */
typedef struct CPsaImage CPsaImage;

/* an image of a .psa sprite sheet (0x44 bytes): the state of the next draw */
struct CPsaImage {
	float x, y;			/* 0x00 */
	float pivotX, pivotY;		/* 0x08 0..1 of the size */
	float scaleX, scaleY;		/* 0x10 1 */
	float f18, f1c;			/* 0x18 the second scale of the CGraphic2D (g_Sprite2DScale) */
	byte screen;			/* 0x20 SPR2D 1: the position in pixels (the CGraphic2D gets the inverse) */
	byte screenSize;		/* 0x21 SPR2D 2 */
	byte unit;			/* 0x22 SPR2D 4, the scorer "Unit": sizes of 1 (no texture size, no second scale) */
	byte pad_23;
	float uv0[2];			/* 0x24 the texture rectangle of frame 0 */
	float uv1[2];			/* 0x2c 1, 1 */
	uint32_t color;			/* 0x34 0xAARRGGBB, 0xffffff */
	int flip;			/* 0x38 0..3 (DrawFrameFlip) */
	void *sprite;			/* 0x3c */
	int frame;			/* 0x40 the frame drawn, 0 between the draws */
};

/* the size in pixels of a frame of the sheet */
byte CPsaImage_GetFrameSize(CPsaImage *self, int frame, int *w, int *h);
/* the texture rectangle of a frame (frame 0: uv0/uv1 of the image); 0 for frame 0 */
byte CPsaImage_GetFrameUV(CPsaImage *self, int frame, float *uv0, float *uv1);
void CPsaImage_DrawFrame(CPsaImage *self, int frame, float x, float y);
/* with the mirror modes 0..3 of the sprite (mode) and of the image (flip) */
void CPsaImage_DrawFrameFlip(CPsaImage *self, int frame, float x, float y, int mode, int flip);

/* the callbacks of Engine_SetCallback (1..4): before each frame, after the frame setup, two more */
extern typeof(void (*)(void)) g_EngCallbacks[4];	/* [4], 2 and 3 are g_LoadingDraw2 and g_LoadingDraw */
extern byte g_DrawProgressBar;
#define g_ProgressBarCount g_LoadingCount
extern byte g_CallbacksOn;
extern char g_ScreenshotName[256];	/* 0x100 */
extern int g_BspSectorSeen;
#define g_VideoStatus g_BspSectorSeen
extern char str_M3dIni[128];
#define g_M3dIni str_M3dIni		/* "m3d.ini" */
/* the video to play at the start and after a level ("" when played, unless bit 0 of its flags) */
extern cGVar GVar_isDemo;
#define g_IsDemo (GVar_isDemo.value.i)
extern cGVar GVar_isXbox;
#define g_IsXbox (GVar_isXbox.value.i)
extern const int g_EngineVersion;

extern int g_GroundTrisPerNode;

extern cGVar GVar_TimerFreqMult;
#define g_TimerFreqMult (GVar_TimerFreqMult.value.i)
extern cGVar GVar_VSync;
#define g_VSync (GVar_VSync.value.i)
extern cGVar GVar_ShowSplashScreen;
#define g_ShowSplashScreen (GVar_ShowSplashScreen.value.i)
extern cGVar GVar_ProgressBar;
#define g_ProgressBar (GVar_ProgressBar.value.i)
extern cGVar GVar_R_StencilShadows;
#define g_StencilShadows (GVar_R_StencilShadows.value.i)	/* R_StencilShadows */
extern cGVar g_GvEnvMap;	/* R_EnvMap, R_EnvBump and R_GlowMap as variables */
extern cGVar g_GvEnvBump;
extern cGVar g_GvGlowMap;
/* what the frame setup takes from the variables */
extern int g_ShowCollision;
extern int g_WireFrame;
extern int g_LightmapMode2;
extern int g_LightmapOff;
extern int g_CalcLmap2;		/* [mapa_sombras] calc_lmap2 */
extern int g_MaxLightmap;
extern int g_TimerQPMult;
extern byte g_VertexSpecular;
extern cGVar GVar_R_UseMLOD;
#define g_RenderFlag6e0 (GVar_R_UseMLOD.value.i)	/* taken from 0x4ae6e4 at the start */
#define g_RenderFlag6e4 (GVar_R_UseMLOD.def.i)

void Engine_Close(void);
void Engine_Init(void);
/* 1 while Engine_Init, Engine_LevelEnd or Video_PlayStart waits for its video (the message loop plays it) */
int Engine_VideoWait(void);
/* the rest of the one that waits, once its video has ended: 1 when that is done, 0 while a video plays (again) */
int Engine_VideoResume(void);
int Engine_IsFullScreen(void);
void Engine_LevelInit(void);
void Engine_LevelLoaded(void);
void Engine_LevelEnd(void);
void Engine_Render(void);
byte Engine_FrameSetup(void);
int Engine_ToggleOption(int opt);
void Engine_SetCallback(void (*fn)(void), int which);
char *Engine_SetIniName(const char *name);
void Engine_SetGround(const char *dir, const char *name, const vec3_t *scale);
const void *Eng_Identity(const void *v);

CGameCamera *Camera_GetCurrent(void);
void Camera_SetCurrent(CGameCamera *cam);

int Video_GetStatus(void);

/* a random loading screen name of the level into buf (with a %02d for the number) */
void Loading_RandomName(char *buf, int size);
void Loading_Fade(void);

/* Engine_WorldToScreen (declared in scorer.h): the position of pos on the 2D screen into out (NULL or 2 floats), 1 / w
 * into depth (NULL or 1 float); 0 when |x| or |y| reach w, then out is the direction to pos at the edge */
/* the direction of pos from the middle of the screen at the edge of the screen */
void Engine_DirToScreen(const vec3_t *pos, float *dest);
int Eng_RenderTargetEnd(void);
int Eng_RenderTargetBegin(const char *name);
byte Eng_ResetViewRect(void);
/* the view in 640 x 480 coordinates */
byte Eng_SetViewRect(float x, float y, float w, float h);

int Timer_GetFreq64(int64_t *out);
int Timer_GetQPFreq64(int64_t *out);

/* a sphere of lines: divV rings, divH segments */
void DebugUtils_DrawSphere(const vec3_t *c, float r, uint32_t color, int divV, int divH);
void DebugUtils_DrawBox(const vec3_t *mn, const vec3_t *mx, uint32_t color);
void DebugUtils_DrawLine(const vec3_t *a, const vec3_t *b, uint32_t color);
void DebugUtils_DrawTri(const vec3_t *a, const vec3_t *b, const vec3_t *c, uint32_t color);

/* other modules (src/imports.inc) */
/* the CMotorApp made (arg 0) or freed (Engine/Motor.cpp, src/game/engine3d/Engine/Engine.c) */
void Engine3D_Create(void *hInst, void *hWnd, void *init, void *frame, void *release, int arg);
void Engine3D_Shutdown(void);
void CEngineApp_UpdateMenu(CEngineApp *self);
/* 0 while the frame limiter (limitaFPS, R_LimitFPS; none with vsync) holds the next frame back */
byte Engine_FrameDue(CD3DApplication *self);
int CD3DApplication_Nothing(CD3DApplication *self);
int CD3DApp_Cleanup(CD3DApplication *self);
int CD3DApp_Render(CD3DApplication *self);
int CD3DApp_ResetDevice(CD3DApplication *self);
int CD3DApp_Reset(CD3DApplication *self);
int Timer_GetFreq(int64_t *out);
int Timer_GetQPFreq(int64_t *out);
/* Video_PlayFile: video.h */
void ShadowsStencil_Set(byte on);
CPsaImage *CPsaImage_ctor(CPsaImage *self, const char *name, int flags);
void CPsaImage_dtor(CPsaImage *self);
void CPsaImage_Draw(CPsaImage *self, float x, float y);
/* the camera of ScrapEngCamera.cpp */
/* Cmd_ReadFile: cmd.h */

#endif
