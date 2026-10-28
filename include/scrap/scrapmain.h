/*
 * ScrapEngMain.cpp of the game: the glue between the game and the engine3d. The start and the end of the engine
 * (the D3D application g_Motor and its three callbacks), the level start and end with the loading screens, the
 * video modes of the adapter, the music and video players, world to screen, the engine printf, screenshots and AVI
 * capture, the model list and info, the viewer camera and the engine options of m3d.ini.
 */
#ifndef SCRAP_SCRAPMAIN_H
#define SCRAP_SCRAPMAIN_H

#include "scrap/archio.h"
#include "scrap/camara.h"
#include "scrap/e3dutil.h"
#include "scrap/modelo.h"
#include "scrap/musicfmod.h"
#include "scrap/nodo3d.h"
#include "scrap/scrap.h"
#include "scrap/scrapcamera.h"
#include "scrap/scrapworld.h"

/* the D3D framework of the DX8 SDK inside g_Motor (CD3DApplication): the adapters with their devices and modes */
typedef struct D3DModeInfo {
	uint32_t Width;			/* 0x00 */
	uint32_t Height;		/* 0x04 */
	uint32_t Format;		/* 0x08 D3DFORMAT */
	uint32_t dwBehavior;		/* 0x0c */
	uint32_t DepthStencilFormat;	/* 0x10 */
} D3DModeInfo;
SD_SIZE(D3DModeInfo, 0x14);

#define MAX_D3DMODES 150
#define MAX_D3DDEVICES 5

typedef struct D3DDeviceInfo {
	uint32_t DeviceType;		/* 0x00 1 HAL, 2 REF */
	uint32_t d3dCaps[0x35];		/* 0x04 D3DCAPS8: Caps2 at word 3, DevCaps at word 7 */
	const char *strDesc;		/* 0xd8 "HAL", "REF" (strings of the exe) */
	int bCanDoWindowed;		/* 0xdc */
	int dwNumModes;			/* 0xe0 */
	D3DModeInfo modes[MAX_D3DMODES];	/* 0xe4 */
	int dwCurrentMode;		/* 0xc9c */
	int bWindowed;			/* 0xca0 */
	uint32_t MultiSampleType;	/* 0xca4 */
} D3DDeviceInfo;
SD_OFFSET(D3DDeviceInfo, dwCurrentMode, 0xc9c);
SD_SIZE(D3DDeviceInfo, 0xca8);

typedef struct D3DAdapterInfo {
	char Driver[0x200];			/* 0x0000 D3DADAPTER_IDENTIFIER8 */
	char Description[0x200];		/* 0x0200 */
	byte identRest[0x2c];			/* 0x0400 */
	uint32_t desktopMode[4];		/* 0x042c d3ddmDesktop: width, height, refresh rate, format */
	uint32_t dwNumDevices;			/* 0x043c */
	D3DDeviceInfo devices[MAX_D3DDEVICES];	/* 0x0440 */
	int dwCurrentDevice;			/* 0x4388 */
} D3DAdapterInfo;
SD_OFFSET(D3DAdapterInfo, dwNumDevices, 0x43c);
SD_OFFSET(D3DAdapterInfo, dwCurrentDevice, 0x4388);
SD_SIZE(D3DAdapterInfo, 0x438c);

/* more of g_Motor (see e3dutil.h) */
#define MOTOR_ADAPTERINFO(m) ((D3DAdapterInfo *)MOTOR_ADAPTERS(m) + MOTOR_ADAPTER(m))
#define MOTOR_WINDOWED(m) (*(int *)((m) + 0x2a384))
#define MOTOR_PP(m) ((uint32_t *)((m) + 0x2a398))	/* D3DPRESENT_PARAMETERS, 13 words */
#define MOTOR_VSYNC(m) (*(int *)((m) + 0x2a5c8))
#define MOTOR_FULLSCREEN(m) (*(int *)((m) + 0x2a5cc))
#define MOTOR_WIDTH(m) (*(int *)((m) + 0x2a5d0))
#define MOTOR_HEIGHT(m) (*(int *)((m) + 0x2a5d4))
#define MOTOR_BPP(m) (*(int *)((m) + 0x2a5d8))
/* the D3D framework methods of g_Motor (thiscall, HRESULT) by their vtable offset */
#define MOTOR_CALL(m, off) (((int(__thiscall *)(byte *)) (*(void ***)(m))[(off) / 4])(m))
#define MOTOR_VT_RESTORE 0x08		/* InitDeviceObjects/RestoreDeviceObjects of the level */
#define MOTOR_VT_RESTORE2 0x0c
#define MOTOR_VT_INVALIDATE 0x20
#define MOTOR_VT_DELETE 0x24
#define MOTOR_VT_CREATE 0x28		/* (hInstance) */

/* a camera of the game (0x3c bytes) over a CCamara of the engine */

/* the watermark drawn with ShowSplashScreen 100 (0x44 bytes, a .psa image) */
typedef struct CPsaImage CPsaImage;
#define PSAIMAGE_COLOR(p) (*(uint32_t *)((byte *)(p) + 0x34))

/* an image of a .psa sprite sheet (0x44 bytes): the state of the next draw */
struct CPsaImage {
	float x, y;			/* 0x00 */
	float pivotX, pivotY;		/* 0x08 0..1 of the size */
	float scaleX, scaleY;		/* 0x10 1 */
	float f18, f1c;			/* 0x18 the second scale of the CGrafico2D (g_Sprite2DScale) */
	byte pantalla;			/* 0x20 SPR2D 1: the position in pixels (the CGrafico2D gets the inverse) */
	byte tamPantalla;		/* 0x21 SPR2D 2 */
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
#define g_CamActual GAME_DATA(CGameCamera *, 0x4a8308)	/* the camera drawn */
#define g_ViewerCamera GAME_DATA(CGameCamera *, 0x4a830c)	/* gViewerCamera, made with the engine */
#define g_Watermark GAME_DATA(CPsaImage *, 0x4a8310)
/* the callbacks of Engine_SetCallback (1..4): before each frame, after the frame setup, two more */
#define g_EngCallbacks ((void (**)(void))Game_Ptr(0x4a8314))
#define g_DrawProgressBar GAME_DATA(byte, 0x4a8324)
#define g_ResetPending GAME_DATA(byte, 0x4a8325)	/* Video_SetCurrentMode: reset the device at the next frame */
#define g_ProgressBarCount GAME_DATA(int, 0x4a8328)
#define g_MapTextDataFile GAME_DATA(void *, 0x4a832c)	/* FILE of the exe's CRT */
#define g_MapTextSaving GAME_DATA(int, 0x4b2604)
#define g_VideoBufferSize GAME_DATA(int, 0x4b2600)
#define g_CallbacksOn GAME_DATA(byte, 0x431ae4)
#define g_ScreenshotName ((char *)Game_Ptr(0x4b2740))	/* 0x100 */
#define g_DVFName ((char *)Game_Ptr(0x4b2840))		/* 0x100 */
#define g_VideoStatus GAME_DATA(int, 0x4b2b34)
#define g_LoadingScreen GAME_DATA(int, 0x431b08)	/* the last random loading screen */
#define g_M3dIni ((char *)Game_Ptr(0x431bf0))		/* "m3d.ini" */
/* the video to play at the start and after a level ("" when played, unless bit 0 of its flags) */
#define g_StartVideo ((cString *)Game_Ptr(0x4a8708))
#define g_StartVideoFlags GAME_DATA(uint32_t, 0x4a8704)
#define g_IsDemo GAME_DATA(int, 0x467ad4)
#define g_IsXbox GAME_DATA(int, 0x467aec)
#define g_EngineVersion GAME_DATA(int, 0x3d5838)

/* m3d.ini of the level (Engine_LoadIni) */
#define g_SueloPath ((char *)Game_Ptr(0x4a8440))	/* [suelo] ruta_m3d, 0x104 */
#define g_SueloEscala ((vec3_t *)Game_Ptr(0x431af8))
#define g_ModelAmbientIni ((ColorIntens *)Game_Ptr(0x4a8630))	/* [suelo] model_ambient */
#define g_ClipNear GAME_DATA(float, 0x4a8330)		/* [camara] forzar_clipNear */
#define g_ClipFar GAME_DATA(float, 0x4a8334)
#define g_CieloPath ((char *)Game_Ptr(0x4a8338))	/* [cielo], 0x104 */
#define g_CieloPosScale GAME_DATA(float, 0x4a843c)
#define g_CieloRotation GAME_DATA(float, 0x4a8544)
#define g_NieblaActiva GAME_DATA(int, 0x4a8548)	/* [niebla] */
#define g_NieblaModoTabla GAME_DATA(int, 0x4a854c)
#define g_NieblaLineal GAME_DATA(int, 0x431ae8)
#define g_NieblaRangoIni GAME_DATA(float, 0x431aec)
#define g_NieblaRangoFin GAME_DATA(float, 0x431af0)
#define g_NieblaDensidad GAME_DATA(float, 0x431af4)
#define g_NieblaColor ((ColorIntens *)Game_Ptr(0x4a8568))
#define g_SueloTrisPerNode GAME_DATA(int, 0x431bc0)

/* the engine variables (Gvars) */
#define g_RenderLightmap GAME_DATA(int, 0x4a8690)
#define g_GvCollisionEpsilon GAME_DATA(float, 0x4a8780)	/* CollisionEpsilon */
#define g_FullScreen GAME_DATA(int, 0x4a87f8)
#define g_ComputeMapTextSaving GAME_DATA(int, 0x4a86a8)
#define g_ViewerCameraAnim GAME_DATA(int, 0x4a8798)
#define g_ShowMapCollision GAME_DATA(int, 0x4a87e0)
#define g_ShowMapWireFrame GAME_DATA(int, 0x4a85f8)
#define g_RenderUseVB GAME_DATA(int, 0x4a8560)
#define g_RenderModels GAME_DATA(int, 0x4a86c0)
#define g_ModelAmbient GAME_DATA(uint32_t, 0x4a85e0)	/* D3D color, 0: the one of m3d.ini */
#define g_TimerFreqMult GAME_DATA(int, 0x4a8628)
#define g_VideoWidth GAME_DATA(int, 0x4a8750)
#define g_VideoHeight GAME_DATA(int, 0x4a8738)
#define g_VideoBPP GAME_DATA(int, 0x4a8768)
#define g_VSync GAME_DATA(int, 0x4a9000)
#define g_ShowSplashScreen GAME_DATA(int, 0x4a85c8)
#define g_ProgressBar GAME_DATA(int, 0x4a86d8)
#define g_StencilShadows GAME_DATA(int, 0x3902a8)	/* R_StencilShadows */
#define g_GvEnvMap ((cGVar *)Game_Ptr(0x4add90))	/* R_EnvMap, R_EnvBump and R_GlowMap as variables */
#define g_GvEnvBump ((cGVar *)Game_Ptr(0x4ad828))
#define g_GvGlowMap ((cGVar *)Game_Ptr(0x4adaa0))
/* what the frame setup takes from the variables */
#define g_ShowCollision GAME_DATA(int, 0x4a8f70)
#define g_WireFrame GAME_DATA(int, 0x4b2b30)
#define g_LightmapMode2 GAME_DATA(int, 0x4b2b28)
#define g_LightmapOff GAME_DATA(int, 0x4b2b2c)
#define g_CalcLmap2 GAME_DATA(int, 0x4b2b20)		/* [mapa_sombras] calc_lmap2 */
#define g_MaxLightmap GAME_DATA(int, 0x432970)
#define g_TimerQPMult GAME_DATA(int, 0x432588)
#define g_MapTextSavingOn GAME_DATA(byte, 0x4b2608)
#define g_VertexSpecular GAME_DATA(byte, 0x4a8efc)
#define g_RenderFlag6e0 GAME_DATA(int, 0x4ae6e0)	/* taken from 0x4ae6e4 at the start */
#define g_RenderFlag6e4 GAME_DATA(int, 0x4ae6e4)

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
void Engine_SetSuelo(const char *dir, const char *name, const vec3_t *escala);
/* m3d.ini: [suelo], [camara], [cielo], [niebla], [mapa_sombras], [sombras_stencil], [flares], [texturas] */
void Engine_LoadIni(const char *name, const char *dir);
const void *Eng_Identity(const void *v);

CGameCamera *Camera_GetActual(void);
void Camera_SetActual(CGameCamera *cam);
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
void Ini_GetColor(const char *sect, const char *key, ColorIntens *color, CArchINI *ini);
/* the path of key relative to the directory of base into dest; 0 or -1 */
int Ini_GetPath(const char *sect, const char *key, char *dest, int size, const char *base, CArchINI *ini);

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
/* the CMotorApp made (arg 0) or freed (Engine/Motor.cpp, src/game/engine3d/Engine/Motor.c) */
void Engine3D_Create(void *hInst, void *hWnd, void *init, void *frame, void *release, int arg);
void Engine3D_Shutdown(void);
byte *__fastcall CMotorApp_ctor(byte *self);
/* the ini name (in dir, NULL for the game directory) into the variables of the engine */
void Motor_LoadIni(const char *name, const char *dir);
void __thiscall CMotorApp_LightsDialog(byte *self, void *hwnd, void *inst);
intptr_t TreeView_InsertItemf(void *hwnd, void *item, const char *fmt, ...);
void Escena_LogTree(CEscena *esc, CNodo3D *nodo, int indent);
/* a command of the debug console of the engine: listar, listar luces, arbol <scene>, ver uniones */
void E3D_Command(char *cmd);
void __fastcall CMotorApp_UpdateMenu(byte *self);
int __thiscall CMotorApp_InitDeviceObjects(byte *self);
intptr_t __thiscall CMotorApp_MsgProc(byte *self, void *hwnd, uint32_t msg, uintptr_t wParam, intptr_t lParam);
int __thiscall CMotorApp_ConfirmDevice(byte *self, void *caps, uint32_t behavior, int format);
int __thiscall CMotorApp_RestoreDeviceObjects(byte *self);
int __thiscall CMotorApp_OneTimeSceneInit(byte *self);
int __thiscall CMotorApp_FrameMove(byte *self);
/* Render (vtable 0x18): the frame, 1 to quit; at once 0 while Motor_FrameDue is 0 */
int __thiscall CMotorApp_Render(byte *self);
/* the reset of the device objects that g_ResetDev asks for, the HRESULT */
int __fastcall CMotorApp_ResetDevice(byte *self);
/* 0 while the frame limiter (limitaFPS, R_LimitFPS; none with vsync) holds the next frame back */
byte Motor_FrameDue(byte *self);
int __thiscall CMotorApp_DeleteDeviceObjects(byte *self);
int __thiscall CMotorApp_FinalCleanup(byte *self);
int __thiscall CD3DApplication_Nothing(byte *self);
char *Motor_ColorStr(const byte *c);
const char *Motor_BoolStr(int b);
CMundo *Mundo_GetChecked(void);
/* the statistics on the screen by R_Profiler: 1 fps, 2 camera and lights, 3 memory, 4 frame time graph, 5 profiler */
void Motor_DrawStats(void);
/* the frame time graph at the bottom, the fps text at y */
void Motor_DrawFpsGraph(float y);
/* the modal dialog of the global scene: its counts and a tree of the nodes, the node selected in the tree */
intptr_t __stdcall Motor_SceneDlgProc(void *hwnd, uint32_t msg, uintptr_t wParam, intptr_t lParam);
void __fastcall Motor_ReleaseEscena(byte *self);
void __fastcall Motor_GetEscena(byte *self);
void *__thiscall CEntradaNula_Delete(void *self, byte flags);
int __thiscall CEntradaNula_Nothing3(void *self, int a, int b, int c);
int __thiscall CEntradaNula_Nothing1(void *self, int a);
int __thiscall CEntradaNula_Nothing(void *self);
void __thiscall CEntradaNula_Nothing4(void *self, int a, int b, int c, int d);
int __thiscall CD3DApp_Cleanup(byte *self);			/* rva 0x273d90 */
int __thiscall CD3DApp_Render(byte *self);			/* rva 0x275120 */
int __thiscall CD3DApp_ResetDevice(byte *self);		/* rva 0x275470 */
int __thiscall CD3DApp_Reset(byte *self);			/* rva 0x2754b0 */
void *App_GetHInstance(void);					/* rva 0x20d1a0 */
void *App_GetHWnd(void);					/* rva 0x20d290 */
int Timer_GetFreq(int64_t *out);				/* rva 0x276cb0 */
int Timer_GetQPFreq(int64_t *out);				/* rva 0x276d00 */
/* Video_PlayFile: video.h */
/* AVI capture of DVF_Process (CaptPant.c): Save and Load close the file (Save with a log line), Frame is 0 while
 * frame is past the frames written */
byte Avi_Save(const char *name);
byte Avi_Frame(const char *name, int frame);
byte Avi_Load(const char *name);
/* a frame of lx * ly RGB bytes into the AVI file name (made on the first frame, the compressor chosen in a
 * dialog); without buf nothing, without name the file closed */
void Avi_Write(const char *name, int lx, int ly, const void *buf, int bufsize);
void SombrasStencil_Set(byte on);				/* rva 0x267460 */
CPsaImage *__thiscall CPsaImage_ctor(CPsaImage *self, const char *name, int flags);	/* rva 0x22d120 */
void __fastcall CPsaImage_dtor(CPsaImage *self);			/* rva 0x22d160 */
void __thiscall CPsaImage_Draw(CPsaImage *self, float x, float y);	/* rva 0x22d3c0 */
/* ScrapEngSprite2D.cpp, the rest (src/game/ScrapEng/ScrapEngSprite2D.c) */
void __fastcall CPsaImage_Init(CPsaImage *self);
CPsaImage *__fastcall CPsaImage_ctorEmpty(CPsaImage *self);
void __thiscall CPsaImage_DrawScaled(CPsaImage *self, float x, float y, float scale);
/* the CGrafico2D of the sheet name (opac: the opacity map) with a blend mode (BlendMode_*) and the SPR2D flags */
void __thiscall CPsaImage_Load(CPsaImage *self, const char *name, const char *opac, int blend, uint32_t flags);
void __thiscall CPsaImage_LoadOpac(CPsaImage *self, const char *name, const char *opac, uint32_t flags);
void __thiscall CPsaImage_LoadName(CPsaImage *self, const char *name, uint32_t flags);
/* ScrapEngMaterial.cpp: the source and destination blend of a blend mode (CHECK mode < 7) */
int BlendMode_GetSrc(int mode);		/* rva 0x2a1a60 */
int BlendMode_GetDst(int mode);		/* rva 0x2a1a00 */
/* the camera of ScrapEngCamera.cpp (rva 0x21c6c0, 0x21c7c0, 0x21c980, 0x21ce80, 0x21ccf0) */
/* the viewer camera moved by the keys of the second action set (rva 0x218080, this file, still the exe's) */
void ViewerCamera_Move(CGameCamera *cam);
/* Cmd_ReadFile: cmd.h */

#endif
