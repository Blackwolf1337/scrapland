/*
 * ScrapEngMain.cpp of the game: the glue between the game and the engine3d. The start and the end of the engine
 * (the D3D application g_Engine and its three callbacks), the level start and end with the loading screens, the
 * video modes of the adapter, the music and video players, world to screen, the engine printf, screenshots and AVI
 * capture, the model list and info, the viewer camera and the engine options of m3d.ini.
 */
#ifndef SCRAP_SCRAPMAIN_H
#define SCRAP_SCRAPMAIN_H

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
SD_OFFSET(CEngineApp, table, 0x2a5e4);
SD_OFFSET(CEngineApp, overlay, 0x2a5f0);
SD_OFFSET(CEngineApp, canWBuffer, 0x2a5f8);
SD_SIZE(CEngineApp, 0x2a600);

/* a camera of the game (0x3c bytes) over a CCamera of the engine */

/* the watermark drawn with ShowSplashScreen 100 (0x44 bytes, a .psa image) */
typedef struct CPsaImage CPsaImage;
#define PSAIMAGE_COLOR(p) (*(uint32_t *)((byte *)(p) + 0x34))

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
SD_SIZE(CPsaImage, 0x44);
SD_OFFSET(CPsaImage, color, 0x34);

/* the size in pixels of a frame of the sheet */
byte __thiscall CPsaImage_GetFrameSize(CPsaImage *self, int frame, int *w, int *h);	/* rva 0x22d2e0 */
/* the texture rectangle of a frame (frame 0: uv0/uv1 of the image); 0 for frame 0 */
byte __thiscall CPsaImage_GetFrameUV(CPsaImage *self, int frame, float *uv0, float *uv1);	/* rva 0x22d320 */
void __thiscall CPsaImage_DrawFrame(CPsaImage *self, int frame, float x, float y);		/* rva 0x22d440 */
/* with the mirror modes 0..3 of the sprite (mode) and of the image (flip) */
void __thiscall CPsaImage_DrawFrameFlip(CPsaImage *self, int frame, float x, float y, int mode, int flip);	/* rva 0x22d480 */

/* the state of the engine glue */
#define g_CamCurrent GAME_GLOBAL(CGameCamera *, g_CamCurrent, 0x4a8308)	/* the camera drawn */
#define g_ViewerCamera GAME_GLOBAL(CGameCamera *, g_ViewerCamera, 0x4a830c)	/* gViewerCamera, made with the engine */
#define g_Watermark GAME_GLOBAL(CPsaImage *, g_Watermark, 0x4a8310)
/* the callbacks of Engine_SetCallback (1..4): before each frame, after the frame setup, two more */
#define g_EngCallbacks (GAME_ARRAY(void (*)(void), g_EngCallbacks, 0x4a8314))	/* [4], 2 and 3 are g_LoadingDraw2 and g_LoadingDraw */
#define g_DrawProgressBar GAME_GLOBAL_IN(byte, group_4a8324, 0, 0x4a8324)
#define g_ResetPending GAME_GLOBAL_IN(byte, group_4a8324, 1, 0x4a8325)	/* Video_SetCurrentMode: reset the device at the next frame */
#define g_ProgressBarCount GAME_GLOBAL(int, g_LoadingCount, 0x4a8328)
#define g_MapTextDataFile GAME_GLOBAL(void *, g_MapTextDataFile, 0x4a832c)	/* FILE of the exe's CRT */
#define g_MapTextSaving GAME_GLOBAL(int, g_MapTextSaving, 0x4b2604)
#define g_VideoBufferSize GAME_GLOBAL(int, g_VideoBufferSize, 0x4b2600)
#define g_CallbacksOn GAME_GLOBAL(byte, g_CallbacksOn, 0x431ae4)
#define g_ScreenshotName (GAME_ARRAY(char, g_ScreenshotName, 0x4b2740))	/* 0x100 */
#define g_DVFName (GAME_ARRAY(char, g_DVFName, 0x4b2840))		/* 0x100 */
#define g_VideoStatus GAME_GLOBAL(int, g_BspSectorSeen, 0x4b2b34)
#define g_LoadingScreen GAME_GLOBAL(int, g_LoadingScreen, 0x431b08)	/* the last random loading screen */
#define g_M3dIni GAME_STR(str_431bf0, 0x431bf0)		/* "m3d.ini" */
/* the video to play at the start and after a level ("" when played, unless bit 0 of its flags) */
#define g_StartVideo (GAME_ARRAY_IN(cString, GVar_PlayVideo, 16, 0x4a8708))
#define g_StartVideoFlags GAME_GLOBAL_IN(uint32_t, GVar_PlayVideo, 12, 0x4a8704)
#define g_IsDemo GAME_GLOBAL_IN(int, GVar_isDemo, 16, 0x467ad4)
#define g_IsXbox GAME_GLOBAL_IN(int, GVar_isXbox, 16, 0x467aec)
#define g_EngineVersion GAME_GLOBAL(int, g_EngineVersion, 0x3d5838)

/* m3d.ini of the level (Engine_LoadIni) */
#define g_GroundPath (GAME_ARRAY(char, g_GroundPath, 0x4a8440))	/* [suelo] ruta_m3d, 0x104 */
#define g_GroundScale (GAME_ARRAY(vec3_t, g_GroundScale, 0x431af8))
#define g_ModelAmbientIni (GAME_ARRAY(ColorIntens, g_ModelAmbientIni, 0x4a8630))	/* [suelo] model_ambient */
#define g_ClipNear GAME_GLOBAL(float, g_ClipNear, 0x4a8330)		/* [camara] forzar_clipNear */
#define g_ClipFar GAME_GLOBAL(float, g_ClipFar, 0x4a8334)
#define g_SkyPath (GAME_ARRAY(char, g_SkyPath, 0x4a8338))	/* [cielo], 0x104 */
#define g_SkyPosScaleIni GAME_GLOBAL(float, g_SkyPosScaleIni, 0x4a843c)
#define g_SkyRotationIni GAME_GLOBAL(float, g_SkyRotationIni, 0x4a8544)
#define g_FogActiveIni GAME_GLOBAL(int, g_FogActiveIni, 0x4a8548)	/* [niebla] */
#define g_FogTableMode GAME_GLOBAL(int, g_FogTableMode, 0x4a854c)
#define g_FogLinear GAME_GLOBAL(int, g_FogLinear, 0x431ae8)
#define g_FogRangeIni GAME_GLOBAL(float, g_FogRangeIni, 0x431aec)
#define g_FogRangeEnd GAME_GLOBAL(float, g_FogRangeEnd, 0x431af0)
#define g_FogDensityIni GAME_GLOBAL(float, g_FogDensityIni, 0x431af4)
#define g_FogColorIni (GAME_ARRAY(ColorIntens, g_FogColorIni, 0x4a8568))
#define g_GroundTrisPerNode GAME_GLOBAL(int, g_GroundTrisPerNode, 0x431bc0)

/* the engine variables (Gvars) */
#define g_RenderLightmap GAME_GLOBAL_IN(int, GVar_RenderLightmap, 16, 0x4a8690)
#define g_GvCollisionEpsilon GAME_GLOBAL_IN(float, GVar_CollisionEpsilon, 16, 0x4a8780)	/* CollisionEpsilon */
#define g_FullScreen GAME_GLOBAL_IN(int, GVar_FullScreen, 16, 0x4a87f8)
#define g_ComputeMapTextSaving GAME_GLOBAL_IN(int, GVar_ComputeMapTextSaving, 16, 0x4a86a8)
#define g_ViewerCameraAnim GAME_GLOBAL_IN(int, GVar_ViewerCameraAnim, 16, 0x4a8798)
#define g_ShowMapCollision GAME_GLOBAL_IN(int, GVar_ShowMapCollision, 16, 0x4a87e0)
#define g_ShowMapWireFrame GAME_GLOBAL_IN(int, GVar_ShowMapWireFrame, 16, 0x4a85f8)
#define g_RenderUseVB GAME_GLOBAL_IN(int, GVar_RenderUseVB, 16, 0x4a8560)
#define g_RenderModels GAME_GLOBAL_IN(int, GVar_RenderModels, 16, 0x4a86c0)
#define g_ModelAmbient GAME_GLOBAL_IN(uint32_t, GVar_ModelAmbient, 16, 0x4a85e0)	/* D3D color, 0: the one of m3d.ini */
#define g_TimerFreqMult GAME_GLOBAL_IN(int, GVar_TimerFreqMult, 16, 0x4a8628)
#define g_VideoWidth GAME_GLOBAL_IN(int, GVar_VideoWidth, 16, 0x4a8750)
#define g_VideoHeight GAME_GLOBAL_IN(int, GVar_VideoHeight, 16, 0x4a8738)
#define g_VideoBPP GAME_GLOBAL_IN(int, GVar_VideoBPP, 16, 0x4a8768)
#define g_VSync GAME_GLOBAL_IN(int, GVar_VSync, 16, 0x4a9000)
#define g_ShowSplashScreen GAME_GLOBAL_IN(int, GVar_ShowSplashScreen, 16, 0x4a85c8)
#define g_ProgressBar GAME_GLOBAL_IN(int, GVar_ProgressBar, 16, 0x4a86d8)
#define g_StencilShadows GAME_GLOBAL_IN(int, GVar_R_StencilShadows, 16, 0x3902a8)	/* R_StencilShadows */
#define g_GvEnvMap (GAME_ARRAY(cGVar, g_GvEnvMap, 0x4add90))	/* R_EnvMap, R_EnvBump and R_GlowMap as variables */
#define g_GvEnvBump (GAME_ARRAY(cGVar, g_GvEnvBump, 0x4ad828))
#define g_GvGlowMap (GAME_ARRAY(cGVar, g_GvGlowMap, 0x4adaa0))
/* what the frame setup takes from the variables */
#define g_ShowCollision GAME_GLOBAL(int, g_ShowCollision, 0x4a8f70)
#define g_WireFrame GAME_GLOBAL(int, g_WireFrame, 0x4b2b30)
#define g_LightmapMode2 GAME_GLOBAL(int, g_LightmapMode2, 0x4b2b28)
#define g_LightmapOff GAME_GLOBAL(int, g_LightmapOff, 0x4b2b2c)
#define g_CalcLmap2 GAME_GLOBAL(int, g_CalcLmap2, 0x4b2b20)		/* [mapa_sombras] calc_lmap2 */
#define g_MaxLightmap GAME_GLOBAL(int, g_MaxLightmap, 0x432970)
#define g_TimerQPMult GAME_GLOBAL(int, g_TimerQPMult, 0x432588)
#define g_MapTextSavingOn GAME_GLOBAL(byte, g_MapTextSavingOn, 0x4b2608)
#define g_VertexSpecular GAME_GLOBAL_IN(byte, group_4a8efc, 0, 0x4a8efc)
#define g_RenderFlag6e0 GAME_GLOBAL_IN(int, GVar_R_UseMLOD, 16, 0x4ae6e0)	/* taken from 0x4ae6e4 at the start */
#define g_RenderFlag6e4 GAME_GLOBAL_IN(int, GVar_R_UseMLOD, 20, 0x4ae6e4)

void Engine_Close(void);
/* the three callbacks of the engine: once after the device, each frame, at the end */
byte Engine_OneTimeInit(void);
byte Engine_FrameMove(void);
byte Engine_FinalCleanup(void);
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
byte Engine_DebugKeys(void);
void Engine_ResetAll(void);
void Engine_ResetTextures(void);
/* the options of Engine_Option (rva 0x214a30, ScrapEngWorld.cpp) */
int Engine_GetOption(int opt);
int Engine_SetOption(int opt, int value);
int Engine_ToggleOption(int opt);
void Engine_SetCallback(void (*fn)(void), int which);
byte Engine_Unsupported(void);
char *Engine_SetIniName(const char *name);
void Engine_SetGround(const char *dir, const char *name, const vec3_t *scale);
/* m3d.ini: [suelo], [camara], [cielo], [niebla], [mapa_sombras], [sombras_stencil], [flares], [texturas] */
void Engine_LoadIni(const char *name, const char *dir);
const void *Eng_Identity(const void *v);

CGameCamera *Camera_GetCurrent(void);
void Camera_SetCurrent(CGameCamera *cam);
/* camera.set: position, angles and target of the camera, save != 0 writes it */
void Camera_SaveRestore(CGameCamera *cam, byte save);

/* the bits of the color of a D3DFORMAT (32 or 16, 0 for the others) */
int Format_Bpp(int fmt);
int Video_GetStatus(void);
/* the start video played if there is one (play != 0) */
byte Video_PlayStart(byte play);
/* escape, space or enter held */
byte Keys_SkipPressed(void);

/* a random loading screen name of the level into buf (with a %02d for the number) */
void Loading_RandomName(char *buf, int size);
void Loading_Fade(void);

/* Engine_WorldToScreen (declared in scorer.h): the position of pos on the 2D screen into out (NULL or 2 floats), 1 / w
 * into depth (NULL or 1 float); 0 when |x| or |y| reach w, then out is the direction to pos at the edge */
/* the direction of pos from the middle of the screen at the edge of the screen */
void Engine_DirToScreen(const vec3_t *pos, float *dest);
void Eng_Printf3D(const vec3_t *pos, const char *fmt, ...);
void Eng_PrintfRel(int dx, int dy, const char *fmt, ...);
void Eng_PrintfXY(int x, int y, const char *fmt, ...);
int Eng_RenderTargetEnd(void);
int Eng_RenderTargetBegin(const char *name);
byte Eng_ResetViewRect(void);
/* the view in 640 x 480 coordinates */
byte Eng_SetViewRect(float x, float y, float w, float h);

void GVar_SetIntVerbose(cGVar *v, int value);
int Timer_GetFreq64(int64_t *out);
int Timer_GetQPFreq64(int64_t *out);
byte Wild_MatchCase(const char *pattern, const char *name, byte ignoreCase);
void Ini_GetColor(const char *sect, const char *key, ColorIntens *color, CIniFile *ini);
/* the path of key relative to the directory of base into dest; 0 or -1 */
int Ini_GetPath(const char *sect, const char *key, char *dest, int size, const char *base, CIniFile *ini);

/* DebugUtils.cpp (src/game/ScrapEng/DebugUtils.c): util_d3d.cpp lines with the identity of 0x431b10 */
void DebugUtils_DrawAxes(const matrix_t *m, const vec3_t *pos, float size);	/* red, green, blue */
void DebugUtils_DrawPoint(const vec3_t *pos, float size, uint32_t color);
/* a sphere of lines: divV rings, divH segments */
void DebugUtils_DrawSphere(const vec3_t *c, float r, uint32_t color, int divV, int divH);
void DebugUtils_DrawBox(const vec3_t *mn, const vec3_t *mx, uint32_t color);
void DebugUtils_DrawLine(const vec3_t *a, const vec3_t *b, uint32_t color);
/* a sphere of lines, divV rings (rva 0x219d20, not decompiled) */
void DebugUtils_DrawSphere(const vec3_t *c, float r, uint32_t color, int divV, int divH);
void DebugUtils_DrawTri(const vec3_t *a, const vec3_t *b, const vec3_t *c, uint32_t color);
/* divV meridians, divH (0: divV) parts of each, the axes at the center */
void DebugUtils_DrawSphereM(const matrix_t *m, const vec3_t *c, float r, uint32_t color, int divV, int divH);

/* other modules (src/imports.inc) */
/* name NULL: a random loading screen; the fade and the time in ms */
void Loading_Step(int percent);					/* rva 0x2157a0, -1: one step */
/* the CMotorApp made (arg 0) or freed (Engine/Motor.cpp, src/game/engine3d/Engine/Engine.c) */
void Engine3D_Create(void *hInst, void *hWnd, void *init, void *frame, void *release, int arg);
void Engine3D_Shutdown(void);
CEngineApp *__fastcall CEngineApp_ctor(CEngineApp *self);
/* the ini name (in dir, NULL for the game directory) into the variables of the engine */
void Engine_LoadVarsIni(const char *name, const char *dir);
void __thiscall CEngineApp_LightsDialog(CEngineApp *self, void *hwnd, void *inst);
intptr_t TreeView_InsertItemf(void *hwnd, void *item, const char *fmt, ...);
void Scene_LogTree(CScene *scn, CNode3D *node, int indent);
/* a command of the debug console of the engine: listar, listar lights, arbol <scene>, ver uniones */
void E3D_Command(char *cmd);
void __fastcall CEngineApp_UpdateMenu(CEngineApp *self);
int __thiscall CEngineApp_InitDeviceObjects(CEngineApp *self);
intptr_t __thiscall CEngineApp_MsgProc(CEngineApp *self, void *hwnd, uint32_t msg, uintptr_t wParam, intptr_t lParam);
int __thiscall CEngineApp_ConfirmDevice(CEngineApp *self, D3DCaps8 *caps, uint32_t behavior, int format);
int __thiscall CEngineApp_RestoreDeviceObjects(CEngineApp *self);
int __thiscall CEngineApp_OneTimeSceneInit(CEngineApp *self);
int __thiscall CEngineApp_FrameMove(CEngineApp *self);
/* Render (vtable 0x18): the frame, 1 to quit; at once 0 while Engine_FrameDue is 0 */
int __thiscall CEngineApp_Render(CEngineApp *self);
/* the reset of the device objects that g_ResetDev asks for, the HRESULT */
int __fastcall CEngineApp_ResetDevice(CEngineApp *self);
/* 0 while the frame limiter (limitaFPS, R_LimitFPS; none with vsync) holds the next frame back */
byte Engine_FrameDue(CD3DApplication *self);
int __thiscall CEngineApp_DeleteDeviceObjects(CEngineApp *self);
int __thiscall CEngineApp_FinalCleanup(CEngineApp *self);
int __thiscall CD3DApplication_Nothing(CD3DApplication *self);
char *Engine_ColorStr(const byte *c);
const char *Engine_BoolStr(int b);
CWorld *World_GetChecked(void);
/* the statistics on the screen by R_Profiler: 1 fps, 2 camera and lights, 3 memory, 4 frame time graph, 5 profiler */
void Engine_DrawStats(void);
/* the frame time graph at the bottom, the fps text at y */
void Engine_DrawFpsGraph(float y);
/* the modal dialog of the global scene: its counts and a tree of the nodes, the node selected in the tree */
intptr_t __stdcall Engine_SceneDlgProc(void *hwnd, uint32_t msg, uintptr_t wParam, intptr_t lParam);
void __fastcall Engine_ReleaseScene(CEngineApp *self);
void __fastcall Engine_GetScene(CEngineApp *self);
CNullInput *__thiscall CNullInput_Delete(CNullInput *self, byte flags);
int __thiscall CNullInput_Nothing3(CNullInput *self, int a, int b, int c);
int __thiscall CNullInput_Nothing1(CNullInput *self, int a);
int __thiscall CNullInput_Nothing(CNullInput *self);
void __thiscall CNullInput_Nothing4(CNullInput *self, int a, int b, int c, int d);
int __thiscall CD3DApp_Cleanup(CD3DApplication *self);			/* rva 0x273d90 */
int __thiscall CD3DApp_Render(CD3DApplication *self);			/* rva 0x275120 */
int __thiscall CD3DApp_ResetDevice(CD3DApplication *self);		/* rva 0x275470 */
int __thiscall CD3DApp_Reset(CD3DApplication *self);			/* rva 0x2754b0 */
void *App_GetHInstance(void);					/* rva 0x20d1a0 */
void *App_GetHWnd(void);					/* rva 0x20d290 */
int Timer_GetFreq(int64_t *out);				/* rva 0x276cb0 */
int Timer_GetQPFreq(int64_t *out);				/* rva 0x276d00 */
/* Video_PlayFile: video.h */
/* AVI capture of DVF_Process (ScreenCapture.c): Save and Load close the file (Save with a log line), Frame is 0 while
 * frame is past the frames written */
byte Avi_Save(const char *name);
byte Avi_Frame(const char *name, int frame);
byte Avi_Load(const char *name);
/* a frame of lx * ly RGB bytes into the AVI file name (made on the first frame, the compressor chosen in a
 * dialog); without buf nothing, without name the file closed */
void Avi_Write(const char *name, int lx, int ly, const void *buf, int bufsize);
void ShadowsStencil_Set(byte on);				/* rva 0x267460 */
CPsaImage *__thiscall CPsaImage_ctor(CPsaImage *self, const char *name, int flags);	/* rva 0x22d120 */
void __fastcall CPsaImage_dtor(CPsaImage *self);			/* rva 0x22d160 */
void __thiscall CPsaImage_Draw(CPsaImage *self, float x, float y);	/* rva 0x22d3c0 */
/* ScrapEngSprite2D.cpp, the rest (src/game/ScrapEng/ScrapEngSprite2D.c) */
void __fastcall CPsaImage_Init(CPsaImage *self);
CPsaImage *__fastcall CPsaImage_ctorEmpty(CPsaImage *self);
void __thiscall CPsaImage_DrawScaled(CPsaImage *self, float x, float y, float scale);
/* the CGraphic2D of the sheet name (opac: the opacity map) with a blend mode (BlendMode_*) and the SPR2D flags */
void __thiscall CPsaImage_Load(CPsaImage *self, const char *name, const char *opac, int blend, uint32_t flags);
void __thiscall CPsaImage_LoadOpac(CPsaImage *self, const char *name, const char *opac, uint32_t flags);
void __thiscall CPsaImage_LoadName(CPsaImage *self, const char *name, uint32_t flags);
/* ScrapEngMaterial.cpp: the source and destination blend of a blend mode (CHECK mode < 7) */
uint32_t BlendMode_GetSrc(uint32_t mode);	/* rva 0x2a1a60 */
uint32_t BlendMode_GetDst(uint32_t mode);	/* rva 0x2a1a00 */
/* the camera of ScrapEngCamera.cpp (rva 0x21c6c0, 0x21c7c0, 0x21c980, 0x21ce80, 0x21ccf0) */
/* the viewer camera moved by the keys of the second action set (rva 0x218080, this file, still the exe's) */
void ViewerCamera_Move(CGameCamera *cam);
/* Cmd_ReadFile: cmd.h */

#endif
