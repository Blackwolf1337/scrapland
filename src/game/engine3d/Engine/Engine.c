/*
 * Engine/Motor.cpp of the engine3d: CMotorApp, the D3D application of the engine (a CD3DApplication of the DX8 SDK
 * framework, 0x2a600 bytes, g_Engine3D) with its three callbacks of the game, the global scene of the world and the
 * small methods; CEntradaNula, the input that does nothing. The frame, the device objects, the menu, the dialogs
 * and the ini reader are still the exe's.
 */
#include <stdarg.h>
#include <string.h>

#include "scrap/rdev.h"
#include "scrap/gvar.h"
#include "scrap/dxutil.h"
#include "scrap/e3dinput.h"
#include "scrap/scene.h"
#include "scrap/e3dutil.h"
#include "scrap/app.h"
#include "scrap/crt.h"
#include "scrap/d3d8.h"
#include "scrap/background2d.h"
#include "scrap/lensflare.h"
#include "scrap/light.h"
#include "scrap/meshd3d.h"
#include "scrap/archio.h"
#include "scrap/bsp.h"
#include "scrap/flare.h"
#include "scrap/fonts.h"
#include "scrap/graphic2d.h"
#include "scrap/material.h"
#include "scrap/nodefx.h"
#include "scrap/render.h"
#include "scrap/rendertarget.h"
#include "scrap/stencil.h"
#include "scrap/texture.h"
#include "scrap/trails.h"
#include "scrap/utild3d.h"
#include "scrap/model.h"
#include "scrap/e3dworld.h"
#include "scrap/fog.h"
#include "scrap/node3d.h"
#include "scrap/prof.h"
#include "scrap/scrapmain.h"
#include "scrap/scrapworld.h"
#include "scrap/ground.h"
#include "scrap/table.h"
#include "scrap/text.h"
#include "scrap/misc.h"
#include "scrap/win32.h"

#define CMOTORAPP_VFTABLE GAME_VFTABLE(CEngineApp_vftable)
#define CNULLINPUT_VFTABLE GAME_VFTABLE(CNullInput_vftable)
#define CNULLINPUT_SIZE 0x114

#define g_Engine3D GAME_GLOBAL(CEngineApp *, g_Engine3D)
/* the callbacks of Engine3D_Create: once after the device, each frame, at the end */
extern void *g_EngineInit;
extern void *g_EngineFrame;
extern void *g_EngineRelease;
extern int g_EngineFrameCount;
/* the values m3d.ini gave (Engine_LoadVarsIni) */
extern int g_EngineIni7c;
extern int g_EngineIni80;
extern int g_EngineIni84;
extern int g_EngineIni88;
extern int g_EngineDef1cf4;
#define g_EngineMode GAME_GLOBAL_IN(int, g_FilterMip, 0)		/* 1 or 2 */
extern char g_ColorStr[64];	/* 0x40 */

typedef void(__stdcall *initcommoncontrols_t)(void);
#define exe_InitCommonControls GAME_IMPORT(initcommoncontrols_t, InitCommonControls, 0)

/* the console command handler of the engine (E3D_Command) */
#define ENGINE_CMDPROC 0x22eda0
extern void *g_Input;	/* the input of the engine, a CEntradaNula */
/* the clock of the engine */
#define g_FrameDelta GAME_GLOBAL(float, g_FrameMs)
extern float g_RealFrameSecs;
extern double g_TimeD;
extern int g_TimeMsInt;
extern double g_GameTimeD;
extern float g_LastTime;
extern float g_FpsTime;
extern int g_FpsFrames;
extern float g_Fps;
#define ENGINE_MAXFRAME 0.1f

#define WM_KEYDOWN 0x100
#define WM_KEYUP 0x101
#define WM_COMMAND 0x111

CEngineApp *CEngineApp_ctor(CEngineApp *self) {
	CD3DApp_ctor(&self->base);
	self->base.vftable = CMOTORAPP_VFTABLE;
	self->table.p = NULL;
	self->table.n = 0;
	self->base.windowTitle = "ScrapEngine [DX8]";
	self->base.useDepthBuffer = 1;
	self->base.creationWidth = 640;
	self->base.creationHeight = 480;
	self->menu = NULL;
	self->cameraMenu = NULL;
	self->scene = NULL;
	self->overlay = NULL;
	self->obj2a5f4 = NULL;
	self->canWBuffer = 0;
	self->useWBuffer = 0;
	self->canMenu9c5d = 0;
	self->canMenu9c5c = 0;
	self->menuMode = 0;
	Engine_LoadVarsIni(g_M3dIni, NULL);
	self->base.vsync = g_VSync;
	self->useWBuffer = g_EngineIni7c != 0;
	g_VertexSpecular = g_EngineIni80 != 0;
	self->base.minStencilBits = g_EngineIni84;
	self->base.minDepthBits = g_EngineDef1cf4;
	g_EngineMode = (g_EngineIni88 != 0) + 1;
	DbgWin_Init(NULL);
	return self;
}

static void Engine_Free(CEngineApp *m) {
	Table_Free(&m->table);
	CD3DApp_dtor(&m->base);
	operator_delete_sized(m, sizeof(CEngineApp));
}

/* the engine made (destroy 0) or freed */
void Engine3D_Create(void *hInst, void *hWnd, void *init, void *frame, void *release, int destroy) {
	(void)hInst;
	if (!destroy) {
		void *mem;
		CEngineApp *m;

		CHECK(!g_Engine3D);
		mem = operator_new(sizeof(CEngineApp));
		m = mem ? CEngineApp_ctor(mem) : NULL;
		m->base.hWnd = hWnd;
		g_EngineInit = init;
		g_EngineFrame = frame;
		g_Engine3D = m;
		g_EngineRelease = release;
		exe_InitCommonControls();
		return;
	}
	if (!g_Engine3D) {
		CHECK(g_Engine3D);
		if (!g_Engine3D) {
			g_Engine3D = NULL;
			return;
		}
	}
	Engine_Free(g_Engine3D);
	g_Engine3D = NULL;
}

void Engine3D_Shutdown(void) {
	if (!g_Engine3D) {
		CHECK(g_Engine3D);
		if (!g_Engine3D) {
			g_Engine3D = NULL;
			return;
		}
	}
	Engine_Free(g_Engine3D);
	g_Engine3D = NULL;
}

/* OneTimeSceneInit: the init callback of the game, the input, the fonts and trails, the global scene */
int CEngineApp_OneTimeSceneInit(CEngineApp *self) {
	void *mem;

	g_E3DCmdProc = (void *)E3D_Command;
	E3D_DebugPrintf("\nprg: '%s'", E3D_GetExePath());
	if (g_EngineInit && !((byte(*)(void))g_EngineInit)()) {
		return (int)0x80004005;
	}
	CHECK(g_World);
	mem = operator_new(CNULLINPUT_SIZE);
	if (mem) {
		IInput_ctor(mem);
		*(void **)mem = CNULLINPUT_VFTABLE;
	}
	g_Input = mem;
	mem = operator_new(8);
	self->overlay = mem ? CGraphic2DList_ctor(mem) : NULL;
	Fonts_Init();
	Trails_InitLists();
	LensFlare_InitList();
	Scenes_Msg(1, 0, 0, 0, 0);
	Scenes_Msg(2, 0, 0, 0, 0);
	Engine_GetScene(self);
	return 0;
}

/* FrameMove: the clock of the engine (real and game time, fps once a second) */
int CEngineApp_FrameMove(CEngineApp *self) {
	float t = self->base.time, d;

	g_FrameDelta = t - g_LastTime;
	g_LastTime = t;
	g_TimeMsF = (float)((double)t * 1000.0);
	g_RealFrameSecs = self->base.elapsedTime;
	g_TimeD = (double)self->base.time;
	g_TimeMsInt = (int)(g_TimeD * 1000.0);
	g_FrameSec = World_TimeScale() * self->base.elapsedTime;
	if (!(ENGINE_MAXFRAME > g_FrameSec)) {
		g_FrameSec = ENGINE_MAXFRAME;
	}
	g_GameTimeD = (double)g_FrameSec + g_GameTimeD;
	g_TimeMs = (uint32_t)(int)(g_GameTimeD * 1000.0);
	d = t - g_FpsTime;
	if (d > 1.0f && g_FrameCount > 0) {
		int n = g_FrameCount;

		g_FpsTime = t;
		g_Fps = (float)(n - g_FpsFrames) / d;
		g_FpsFrames = n;
		if (g_RenderOff) {
			Win_SetTextf(self->base.hWnd, "Scrapland dedicated server");
		}
	}
	return 0;
}

extern int g_RenderCounts[10];	/* [10], 8 counts (g_RenderTris and g_RenderVerts first), the frame clears 10 */
#define g_Engine431bae GAME_GLOBAL_IN(byte, group_431bac, 2)
extern int g_Dither;		/* [video] Tramas: dithering */
/* the frame limiter: [video] limitaFPS of m3d.ini and the console variable R_LimitFPS, the larger counts */
extern float g_LimitFps;
extern cGVar GVar_R_LimitFPS;
#define g_RLimitFps (GVar_R_LimitFPS.value.f)
extern uint32_t g_LimitNext;	/* the start of the next frame, timeGetTime * 3 (0: none yet) */
extern cGVar GVar_R_ForceFPS;
#define g_RForceFps (GVar_R_ForceFPS.value.f)		/* R_ForceFPS: frames of a fixed length (Timer_FixedStep) */
extern int g_TimeMult;
extern int g_TimeScaled;	/* Time_Get() * g_TimeMult / 1000 */
extern cGVar GVar_AutoScreenShot;
#define g_AutoShot (GVar_AutoScreenShot.value.i)		/* the console variable AutoScreenShot */
#define g_StencilClear GAME_GLOBAL_IN(uint32_t, GVar_R_StencilRefClear, 16)
#define g_ResetDev GAME_GLOBAL_IN(byte, group_4a8efc, 2)		/* the device objects made again at the next frame */
#define g_ResetDevOnly GAME_GLOBAL_IN(byte, group_431bac, 3)	/* 1: without the world, 0: the game callbacks too */
/* the callbacks of the game the reset puts in place */
extern void *g_EngineInitNext;
extern void *g_EngineFrameNext;
extern void *g_EngineReleaseNext;
#define D3DRS_SHADEMODE 9
#define D3DRS_DITHERENABLE 0x1a
#define D3DRS_SPECULARENABLE 0x1d

/* Present waits for the vertical retrace: full screen with an interval other than immediate, windowed only with
 * the swap effect COPY_VSYNC (D3DPRESENT_PARAMETERS: 5 SwapEffect, 7 Windowed, 12 FullScreen_PresentationInterval) */
static byte Engine_PresentVSync(CD3DApplication *self) {
	const D3DPresentParams *pp = &self->presentParams;

	return pp->Windowed ? pp->SwapEffect == D3DSWAPEFFECT_COPY_VSYNC :
		pp->FullScreen_PresentationInterval != D3DPRESENT_INTERVAL_IMMEDIATE;
}

/* differs from the original on purpose (no sleeps): the original slept at the end of the frame until its time had
 * passed, the C checks before the next one: App_WinMain (also CD3DApp_Render and CD3DApp_Run) skips the frame while
 * this is 0 and its PeekMessage loop asks again; with vsync there is no own limiter */
byte Engine_FrameDue(CD3DApplication *self) {
	if (!g_LimitNext) {
		return 1;
	}
	float fps = Float_MaxSS(g_LimitFps, g_RLimitFps);
	if (!((double)fps > 0.1) || Engine_PresentVSync(self)) {
		return 1;
	}
	return exe_timeGetTime() * 3 >= g_LimitNext;
}

/* a pending reset of the device objects (g_ResetDev): with g_ResetDevOnly 0 also the world through the release
 * and init callbacks of the game; the HRESULT */
int CEngineApp_ResetDevice(CEngineApp *self) {
	if (!g_ResetDev) {
		return 0;
	}
	g_ResetDev = 0;
	CHECK(self->base.device);
	/* differs from the original on purpose (no sleeps): no Sleep(250) before the reset */
	int hr;
	if (!g_ResetDevOnly) {
		hr = self->base.vftable->DeleteDeviceObjects(&self->base);
		if (hr < 0) {
			return hr;
		}
		Engine_ReleaseScene(self);
		if (g_EngineRelease && !((byte(*)(void))g_EngineRelease)()) {
			return E_FAIL;
		}
		CHECK(!g_World);
		g_EngineFrame = g_EngineFrameNext;
		g_EngineInit = g_EngineInitNext;
		g_EngineRelease = g_EngineReleaseNext;
		if (g_EngineInit && !((byte(*)(void))g_EngineInit)()) {
			return E_FAIL;
		}
		CHECK(g_World);
		Engine_GetScene(self);
		hr = self->base.vftable->InitDeviceObjects(&self->base);
		return hr < 0 ? hr : 0;
	}
	hr = self->base.vftable->DeleteDeviceObjects(&self->base);
	if (hr < 0) {
		return hr;
	}
	hr = self->base.vftable->FinalCleanup(&self->base);
	if (hr < 0) {
		return hr;
	}
	g_EngineInit = g_EngineInitNext;
	g_EngineFrame = g_EngineFrameNext;
	g_EngineRelease = g_EngineReleaseNext;
	Keyboard_Clear();
	void *dev = self->base.device;
	self->base.device = NULL;
	hr = self->base.vftable->OneTimeSceneInit(&self->base);
	if (hr < 0) {
		return hr;
	}
	self->base.device = dev;
	hr = self->base.vftable->InitDeviceObjects(&self->base);
	return hr < 0 ? hr : 0;
}

/* Render (vtable 0x18): the scene into the target "Scene", the 2D and the frame callback of the game on top, the
 * screenshot of AutoScreenShot, the frame limiter; 1 when the game callback asks to quit or the reset failed */
int CEngineApp_Render(CEngineApp *self) {
	if (!Engine_FrameDue(&self->base)) {
		return 0;
	}
	if (CEngineApp_ResetDevice(self)) {
		return 1;
	}
	void *dev = self->base.device;
	g_FrameCount++;
	int quit = 0;
	g_TimeScaled = (int)((int64_t)g_TimeMult * (int64_t)Time_Get() / 1000);
	for (int i = 0; i < 10; i++) {
		g_RenderCounts[i] = 0;
	}
	RenderTarget_Begin("Scene");
	Render_Clear(NULL, 3, Color_ToD3D(&self->scene->fog), 1.0f, g_StencilClear);
	g_ZEnable = self->useWBuffer && self->canWBuffer ? 2 : 1;
	Rd_DevSetRenderState(dev, D3DRS_ZENABLE, g_ZEnable);
	Rd_DevSetRenderState(dev, D3DRS_ZWRITEENABLE, 1);
	Rd_DevSetRenderState(dev, D3DRS_ZFUNC, D3DCMP_LESSEQUAL);
	Rd_DevSetRenderState(dev, D3DRS_FILLMODE, (g_RenderMode100 == 0) + 2);
	Rd_DevSetRenderState(dev, D3DRS_SHADEMODE, (g_Engine431bae != 0) + 1);
	Rd_DevSetRenderState(dev, D3DRS_SPECULARENABLE, g_VertexSpecular);
	Rd_DevSetRenderState(dev, D3DRS_DITHERENABLE, g_Dither);
	g_AmbientD3D = Color_ToD3D(&self->scene->ambient);
	if (Rd_DevBeginScene(dev) >= 0) {
		Render_SetCamera(World_Get()->cam);
		Render_BeginFrame();
		ShadowsStencil_Capture(1);
		Bsp_RenderPortals();
		Engine_FrameSetup();
		CWorld_Render(World_Get());
		Models_Render();
		E3D_FlushBatch();
		ShadowsStencil_Render();
		Bsp_RenderTransp();
		Ground_DrawDebug();
		MeshEffect_Render();
		Trails_Render(-1);
		LensFlare_RenderAll();
		CGraphic2DList_Render(self->overlay);
		RenderTarget_End("Scene");
		E3D_FlushBatch();
		Background2D_Render();
		if (g_EngineFrame) {
			quit = !((byte(*)(void))g_EngineFrame)();
		}
		E3D_FlushBatch();
		Debug_PointsDraw();
		Text_DrawStack();
		Engine_DrawStats();
		Render_EndView();
		RenderLock_Frame();
		Rd_DevEndScene(dev);
	}
	if (g_TileOn) {
		void *bb, *d = self->base.device;

		Rd_DevGetBackBuffer(d, 0, 0, &bb);
		Surface_ShotCopy(bb);
		Rd_Release(bb);
		g_TileOn = g_AutoShot != 0;
	}
	float fps = Float_MaxSS(g_LimitFps, g_RLimitFps);
	if ((double)fps > 0.1) {
		if (Engine_PresentVSync(&self->base)) {
			/* differs from the original on purpose (no sleeps): with vsync no own limiter, started again without */
			g_LimitNext = 0;
		} else {
			int frame = (int)(3000.0 / (double)fps);

			if (!g_LimitNext) {
				g_LimitNext = exe_timeGetTime() * 3;
			}
			if (exe_timeGetTime() * 3 >= g_LimitNext + frame) {
				E3D_LogVerbose(g_FileLog, "WARN: LimitFPS: Framerate below fps limit by %d ms",
					(exe_timeGetTime() * 3 - g_LimitNext - frame) / 3);
			}
			if (exe_timeGetTime() * 3 - g_LimitNext > 15000) {
				g_LimitNext = exe_timeGetTime() * 3;
				E3D_LogBlue(g_FileLog, "ERROR: LimitFPS: Out of sync (frame exceeded 5 sec)");
			}
			/* differs from the original on purpose (no sleeps): the wait for g_LimitNext is Engine_FrameDue */
			g_LimitNext += frame;
		}
	}
	Timer_FixedStep(g_RForceFps);
	return quit;
}

/* DeleteDeviceObjects: the device objects of the input, the textures, materials, shadows, trails and the world */
int CEngineApp_DeleteDeviceObjects(CEngineApp *self) {
	((void(*)(void *))(*(void ***)g_Input)[2])(g_Input);
	Table_Free(&self->table);
	Texture_ReleaseAllD3D();
	CMatList_Release(g_MatList);
	Stencil_Free();
	Trails_Release();
	LensFlare_Release();
	UtilScene_ReleaseD3D();
	CWorld_ReleaseD3D(World_Get());
	Render_EndD3D();
	return 0;
}

/* FinalCleanup: the global scene, the release callback of the game, everything of the engine */
int CEngineApp_FinalCleanup(CEngineApp *self) {
	CGraphic2DList *ov;

	Engine_ReleaseScene(self);
	if (g_EngineRelease) {
		((byte(*)(void))g_EngineRelease)();
	}
	CHECK(!g_World);
	Fonts_Free();
	Debug_PointsClear();
	ov = self->overlay;
	if (ov) {
		CGraphic2DList_DeleteAll(ov);
		operator_delete_sized(ov, 8);
		self->overlay = NULL;
	}
	if (self->obj2a5f4) {
		void **obj = self->obj2a5f4;

		((void *(*)(void *, int))((void **)obj[0])[0])(obj, 1);
		self->obj2a5f4 = NULL;
	}
	UtilScene_FreeAll();
	Trails_ResetAll();
	LensFlare_ClearAll();
	Background2D_Release();
	CMatList_DeleteAll(g_MatList);
	Texture_DeleteAll();
	Tex_ClearPaths();
	Tex_ClearExtensions();
	if (g_Input) {
		((void *(*)(void *, int))(*(void ***)g_Input)[0])(g_Input, 1);
		g_Input = NULL;
	}
	return 0;
}

/* ConfirmDevice: no device with the pure behaviour 0x10 */
int CEngineApp_ConfirmDevice(CEngineApp *self, D3DCaps8 *caps, uint32_t behavior, int format) {
	(void)self;
	(void)caps;
	(void)format;
	return (byte)(behavior & 0x10) ? (int)0x80004005 : 0;
}

int CEngineApp_RestoreDeviceObjects(CEngineApp *self) {
	(void)self;
	g_EngineFrameCount++;
	return 0;
}

int CD3DApplication_Nothing(CD3DApplication *self) {
	(void)self;
	return 0;
}

/* "r g b a i" of a color with intensity (4 bytes and a float), in a static buffer */
char *Engine_ColorStr(const byte *c) {
	Bprintf(g_ColorStr, 0x40, "%d %d %d %d %f (rgbai)", c[0], c[1], c[2], c[3], (double)*(const float *)(c + 4));
	return g_ColorStr;
}

const char *Engine_BoolStr(int b) {
	return b ? "true" : "false";
}

/* the out of line copy of the world.h accessor */
CWorld *World_GetChecked(void) {
	return World_Get();
}

/* the info overlay of R_ShowInfo (Engine_DrawStats, modes 1 to 4) */
extern cGVar GVar_TexturePool;
#define g_TexturePool (GVar_TexturePool.value.i)
extern int g_TexCreated;
extern int g_TexCached;
extern int g_TexUsed;
extern int g_TexUsedPixels;

void Engine_DrawStats(void) {
	CCamera *cam;
	uint32_t ms[16];		/* MEMORYSTATUSEX */
	uint64_t used;
	int d3d, act, tot, mode;

	mode = g_ProfMode;
	if (mode == 1) {
		Eng_Printf(1, 16.0f, 10.0f, 1, 0xffffff, "%.2f", (double)g_Fps);
		mode = g_ProfMode;
	}
	if (mode > 1) {
		Font_SetColor(g_FntComic14, 0xffffff);
		Font_Printf(g_FntComic14, 10.0f, 16.0f, 2, "tri: %d %d %d %d %d %d %d %d", g_RenderCounts[0], g_RenderCounts[1],
			g_RenderCounts[2], g_RenderCounts[3], g_RenderCounts[4], g_RenderCounts[5], g_RenderCounts[6],
			g_RenderCounts[7]);
		Font_Printf(g_FntComic14, 10.0f, 0.0f, 2, "fps: %.2f (%.2f ms)", (double)g_Fps, 1000.0 / (double)g_Fps);
		Font_Printf(g_FntComic14, 10.0f, 64.0f, 2, "mem: %d %d %d %d", g_MemStats->bytes, g_MemStats->allocs,
			g_EngineFrameCount, g_EngineInits);
		mode = g_ProfMode;
	}
	if (mode == 2) {
		Font_SetColor(g_FntComic14, 0xffffff);
		cam = World_Get()->cam;
		Font_Printf(g_FntComic14, 10.0f, 32.0f, 2, "cam: %s  %.2f %.2f  %.2f", Vec3_StrDeg(&cam->rot, 0, 2),
			(double)cam->zNear, (double)cam->zFar, (double)cam->fov * 180.0 / 3.141592653589793);
		d3d = Light_NumD3D();
		act = Light_NumActive();
		tot = Light_NumTotal();
		Font_Printf(g_FntComic14, 10.0f, 48.0f, 2, "luz: %d %d %d", tot, act, d3d);
		cam = World_Get()->cam;
		Font_Printf(g_FntComic14, 10.0f, 80.0f, 2, "poscam: %.1f %.1f %.1f", (double)cam->pos.x, (double)cam->pos.y,
			(double)cam->pos.z);
		mode = g_ProfMode;
	}
	if (mode == 3) {
		Font_Printf(g_FntComic14, 10.0f, 32.0f, 2, "FreeTextMem: %d Mb (pool %d)",
			Rd_DevGetAvailableTextureMem(g_pd3dDevice) >> 20,
			g_TexturePool);
		ms[0] = 0x40;
		exe_GlobalMemoryStatusEx(ms);
		/* ullTotalVirtual - ullAvailVirtual */
		used = ((uint64_t)ms[11] << 32 | ms[10]) - ((uint64_t)ms[13] << 32 | ms[12]);
		Font_Printf(g_FntComic14, 10.0f, 96.0f, 2, "virtual memory used: %4d MB", (uint32_t)(used >> 20));
		mode = g_ProfMode;
	}
	if (mode == 4) {
		Engine_DrawFpsGraph(160.0f);
		Font_PrintfXY(g_FntComic14, 10.0f, 32.0f, "%.2f MTri/s  %.2f MVert/s",
			(double)((float)g_RenderCounts[0] * g_Fps) / 1000000.0, (double)((float)g_RenderCounts[1] * g_Fps) / 1000000.0);
		mode = g_ProfMode;
	}
	Text_Printf(10, mode == 2 ? 4 : 20, NULL);
	if (g_ProfMode == 4 && g_TexCached) {
		Font_PrintfXY(g_FntComic14, 10.0f, 192.0f, "%d %d %d  %.1f Kb", g_TexCached, g_TexUsed, g_TexCreated,
			(double)g_TexUsedPixels * 0.0009765625);
		g_TexCached = 0;
		g_TexUsed = 0;
		g_TexUsedPixels = 0;
	}
}

/* EscenaGlobal: the scene of the world (or its first one, or bmp/vacia.m3d), counted while the level runs */
void Engine_ReleaseScene(CEngineApp *self) {
	CScene *e;

	CHECK(self->scene);
	if (self->scene) {
		self->scene->Refs--;
		e = self->scene;
		if (!e->Refs && e) {
			((void *(*)(CScene *, int))e->vftable[0])(e, 1);
		}
		self->scene = NULL;
	}
}

void Engine_GetScene(CEngineApp *self) {
	CWorld *world;

	CHECK(!self->scene);
	world = World_Get();
	self->scene = world->GroundScale;
	if (!self->scene) {
		self->scene = Scene_Next(NULL);
	}
	if (!self->scene) {
		self->scene = Create_Model2("bmp/vacia.m3d", NULL, 2)->scene;
		CHECK(self->scene);
	}
	self->scene->Refs++;
}

/* ---- m3d.ini: the variables of the engine, in the order of the original ---- */

enum { INI_BOOL, INI_LONG, INI_FLOAT, INI_TEXT };
#define INI_TEXT_SIZE 0x40

/* the ini name (in dir, NULL for the game directory) into the variables */
void Engine_LoadVarsIni(const char *name, const char *dir) {
	const struct {
		byte kind;
		const char *sect;
		const char *key;
		void *var;
	} engineIni[] = {
		{ INI_BOOL, "video", "TexPersp", GAME_PTR(engineIni_431bd0, 0) },
		{ INI_BOOL, "video", "Tramas", GAME_PTR(g_Dither, 0) },
		{ INI_BOOL, "video", "Filtrado", GAME_PTR(engineIni_431bc8, 0) },
		{ INI_BOOL, "video", "UsarTexturas", GAME_PTR(g_UseTextures, 0) },
		{ INI_BOOL, "video", "UsarEnvMap", GAME_PTR(g_UseEnvMap, 0) },
		{ INI_BOOL, "video", "UsarHWEnvMap", GAME_PTR(g_UseHWEnvMap, 0) },
		{ INI_BOOL, "video", "UsarBumpMap", GAME_PTR(g_UseBumpMap, 0) },
		{ INI_BOOL, "video", "UsarGlowMap", GAME_PTR(g_UseGlowMap, 0) },
		{ INI_BOOL, "video", "DiffuseMod4x", GAME_PTR(g_DiffuseMod4x, 0) },
		{ INI_BOOL, "video", "Sharpen", GAME_PTR(g_Sharpen, 0) },
		{ INI_BOOL, "video", "ErrDif", GAME_PTR(g_TexErrDif, 0) },
		{ INI_LONG, "video", "TextBPP", GAME_PTR(g_TextBPP, 0) },
		{ INI_LONG, "video", "TextOpBPP", GAME_PTR(g_TextOpBPP, 0) },
		{ INI_BOOL, "video", "gamma2x", GAME_PTR(g_Gamma2x, 0) },
		{ INI_BOOL, "video", "sobredib", GAME_PTR(g_Overdraw, 0) },
		{ INI_BOOL, "video", "tricolis", GAME_PTR(g_ShowCollision, 0) },
		{ INI_BOOL, "video", "bufferw", GAME_PTR(g_EngineIni7c, 0) },
		{ INI_FLOAT, "video", "limitaFPS", GAME_PTR(g_LimitFps, 0) },
		{ INI_FLOAT, "video", "despMipmap", GAME_PTR(g_MipmapBias2, 0) },
		{ INI_BOOL, "video", "LinearMipmap", GAME_PTR(g_EngineIni88, 0) },
		{ INI_LONG, "video", "MaxAnisoFlt", GAME_PTR(engineIni_431d74, 0) },
		{ INI_BOOL, "video", "ConsolaWnd", GAME_PTR(g_DbgConWindow, 0) },
		{ INI_BOOL, "video", "ConsolaTxt", GAME_PTR(g_DbgConStd, 0) },
		{ INI_BOOL, "video", "IlumSpec", GAME_PTR(g_EngineIni80, 0) },
		{ INI_LONG, "video", "Stencil", GAME_PTR(g_EngineIni84, 0) },
		{ INI_LONG, "video", "Depth", GAME_PTR(g_EngineDef1cf4, 0) },
		{ INI_FLOAT, "sombras", "sombras", GAME_PTR(engineIni_431bb0, 0) },
		{ INI_LONG, "sombras", "objeto_ini", GAME_PTR(engineIni_4a8f28, 0) },
		{ INI_LONG, "sombras", "objeto_fin", GAME_PTR(engineIni_431bb4, 0) },
		{ INI_BOOL, "sombras", "modif_vnorm", GAME_PTR(engineIni_431bb8, 0) },
		{ INI_BOOL, "sombras", "proc_objetos", GAME_PTR(engineIni_431bbc, 0) },
		{ INI_TEXT, "sombras", "nombre_luz", GAME_PTR(engineIni_4a8f30, 0) },
		{ INI_BOOL, "dinput", "raton_excl", GAME_PTR(g_MouseExcl, 0) },
		{ INI_BOOL, "dinput", "usar_di", GAME_PTR(engineIni_431cf8, 0) },
		{ INI_BOOL, "dinput", "raton_inv_x", GAME_PTR(g_InputInvX, 0) },
		{ INI_BOOL, "dinput", "raton_inv_y", GAME_PTR(g_InputInvY, 0) },
		{ INI_BOOL, "dinput", "raton_inv_z", GAME_PTR(g_InputInvZ, 0) },
		{ INI_LONG, "suelo", "suelo_caras_cuadr", GAME_PTR(g_GroundTrisPerNode, 0) },
	};
	char path[MAX_FILEPATH];
	cDataFile *dat;
	unsigned i;

	Path_Merge(path, dir, name);
	Path_Copy(path, path, -1);
	E3D_DebugPrintf("\nINI: %s", path);
	dat = DataFile_New();
	if (!DataFile_Load(dat, path, DataFile_Count(dat))) {
		for (i = 0; i < sizeof(engineIni) / sizeof(engineIni[0]); i++) {
			void *v = engineIni[i].var;

			switch (engineIni[i].kind) {
			case INI_BOOL:
				Dat_GetBool(dat, engineIni[i].sect, engineIni[i].key, v);
				break;
			case INI_LONG:
				Dat_GetLong(dat, engineIni[i].sect, engineIni[i].key, v);
				break;
			case INI_FLOAT:
				Dat_GetFloat(dat, engineIni[i].sect, engineIni[i].key, v);
				break;
			default:
				Dat_GetText(dat, engineIni[i].sect, engineIni[i].key, v, INI_TEXT_SIZE);
				break;
			}
		}
		E3D_LogInfo(g_FileLog, "*INI: [Video,etc.]: %s%s", path, " <ok>");
	} else {
		E3D_DebugPrintf("\nINI: no encontrado");
		E3D_LogInfo(g_FileLog, "*INI: [Video,etc.]: %s%s", path, " <not found>");
		E3D_LogBlue(g_FileLog, "INI: %s%s", path, " <not found>");
	}
	DataFile_Free(dat);
}

/* ---- the lights dialog and the scene tree ---- */

#define GWL_HINSTANCE (-6)
#define ENGINE_LIGHTSPROC 0x22dcf0	/* Engine_SceneDlgProc */
#define ENGINE_LIGHTSDIALOG 0x99
#define TVM_INSERTITEM 0x1100

/* the modal dialog of the lights of the global scene, the application paused meanwhile */
void CEngineApp_LightsDialog(CEngineApp *self, void *hwnd, void *inst) {
	self->base.vftable->Pause(&self->base, 1);
	exe_DialogBoxParamA(inst, (const char *)ENGINE_LIGHTSDIALOG, hwnd, (void *)Engine_SceneDlgProc,
		(intptr_t)self->scene);
	self->base.vftable->Pause(&self->base, 0);
}

/* TVINSERTSTRUCT of the tree view of the scene dialog */
typedef struct TvInsert {
	intptr_t hParent;		/* 0x00 */
	intptr_t hInsertAfter;		/* 0x04 */
	uint32_t mask;			/* 0x08 TVITEM */
	intptr_t hItem;			/* 0x0c */
	uint32_t state;			/* 0x10 */
	uint32_t stateMask;		/* 0x14 */
	char *pszText;			/* 0x18 TreeView_InsertItemf */
	int cchTextMax;			/* 0x1c */
	int iImage;			/* 0x20 */
	int iSelectedImage;		/* 0x24 */
	int cChildren;			/* 0x28 */
	intptr_t lParam;		/* 0x2c the node */
} TvInsert;
SD_SIZE(TvInsert, 0x30);

#define WM_CLOSE 0x10
#define WM_NOTIFY 0x4e
#define WM_INITDIALOG 0x110
#define GWL_USERDATA (-21)
#define TVN_SELCHANGED (-402)
#define TVI_ROOT ((intptr_t)0xffff0000)
#define TVI_LAST ((intptr_t)0xffff0002)
#define TVIF_TEXT 0x1
#define TVIF_STATE 0x8
#define TVIF_PARAM 0x4
#define TVIS_EXPANDED 0x20
#define IDC_SCENE_INFO 0x3fc
#define IDC_SCENE_TREE 0x3f7
#define CAMERA_TARGET 0x463
#define RAD_TO_DEG 57.29578f

/* the names of the flags of the nodes (NODO_*), from bit 0 */
static const struct {
	uint32_t bit;
	const char *name;
} g_NodeFlagNames[] = {
	{ 0, " RAIZ" }, { 1, " GRUPO" }, { 2, " SELEC" }, { 3, " OCULTO" }, { 4, " HVISIBLE" },
	{ 5, " NO_CASTSHADOWS" }, { 6, " NO_RCVSHADOWS" }, { 7, " NO_RENDER" }, { 8, " BOXMODE" }, { 12, " COLLIDE" },
	{ 13, " NO_COLLIDE" }, { 16, " NO_ANIMPOS" }, { 17, " NO_TRANS" }, { 18, " SLERP2" }, { 19, " EFFECT" },
	{ 20, " BONE" }, { 21, " BIPED" }, { 22, " NO_TABNODOR" }, { 24, " 2LADOS" }, { 25, " RT_LIGHTING" },
	{ 26, " RT_SHADOWS" }, { 27, " NO_LIGHTMAP" }, { 28, " NO_SECTOR" }, { 29, " AREA_LIGHT" }, { 30, " DUDV_PASS" },
};

static const char *Node_RefName(const void *node) {
	return node ? ((const CNode3D *)node)->name : "(null)";
}

/* the items of a node under its own item: flags, position, rotation, object and the data of its class */
static void Engine_SceneTreeNode(void *tree, TvInsert *tv, CNode3D *n) {
	const char *f[25];
	vec3_t axis, out, tmp, v;
	const vec3_t *ang;
	matrix_t m;
	float a, rz, rx, ry;
	uint32_t i;

	for (i = 0; i < 25; i++) {
		f[i] = (n->flags >> g_NodeFlagNames[i].bit) & 1 ? g_NodeFlagNames[i].name : "";
	}
	TreeView_InsertItemf(tree, tv, "Flags:%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s", f[0], f[1], f[2], f[3],
		f[4], f[5], f[6], f[7], f[8], f[9], f[10], f[11], f[12], f[13], f[14], f[15], f[16], f[17], f[18], f[19],
		f[20], f[21], f[22], f[23], f[24]);
	TreeView_InsertItemf(tree, tv, "iObjeto: %d", n->index);
	TreeView_InsertItemf(tree, tv, "Pos: %s", Vec3_Str(&n->pos, 0, 6));
	Quat_ToAxisAngle(&axis, &a, &n->rot);
	Quat_ToMatrixD3D(&m, &n->rot);
	v.x = 0.0f;
	v.y = 0.0f;
	v.z = 1.0f;
	Vec3_TransformH(&out, &v, &m);
	ang = Vec3_GetRotAng(&tmp, &out);
	rx = ang->x * RAD_TO_DEG;
	ry = ang->y * RAD_TO_DEG;
	rz = ang->z * RAD_TO_DEG;
	TreeView_InsertItemf(tree, tv, "Rot:  eje: %s  ang: %f", Vec3_Str(&axis, 0, 6), (double)a * 180.0 / 3.141592653589793);
	TreeView_InsertItemf(tree, tv, "Rot:  z: %f  x: %f  y: %f", (double)rz, (double)rx, (double)ry);
	tv->hParent = TreeView_InsertItemf(tree, tv, "obj3D: %s", Obj3D_ClassName(CNode3D_Class(n)));
	tv->hInsertAfter = TVI_LAST;
	tv->mask = TVIF_TEXT | TVIF_PARAM;
	tv->lParam = (intptr_t)n;
	if (CNode3D_Class(n) == CLASS_OBJ3D_CAMERA) {
		CCamera *cam = Obj3D_Cast(n->obj3D, CLASS_OBJ3D_CAMERA);

		TreeView_InsertItemf(tree, tv, "Tipo: %s", cam->mode == CAMERA_TARGET ? "Target" : "Free");
		TreeView_InsertItemf(tree, tv, "Pos: %s", Vec3_Str(&cam->pos, 0, 6));
		TreeView_InsertItemf(tree, tv, "Rot: %s", Vec3_StrDeg(&cam->rot, 0, 2));
		TreeView_InsertItemf(tree, tv, "FOV: %f", (double)cam->fov * 180.0 / 3.141592653589793);
		TreeView_InsertItemf(tree, tv, "Clip: %f %f", (double)cam->zNear, (double)cam->zFar);
		TreeView_InsertItemf(tree, tv, "Range: %f %f", (double)cam->rangeNear, (double)cam->rangeFar);
		TreeView_InsertItemf(tree, tv, "Target: %s", Node_RefName(cam->nodeTarget));
		TreeView_InsertItemf(tree, tv, "LookAt: %s", Vec3_Str(&cam->target, 0, 6));
		TreeView_InsertItemf(tree, tv, "PhysAspect: %f", (double)cam->aspect);
		TreeView_InsertItemf(tree, tv, "ViewAspect: %f", (double)cam->aspectScreen);
	}
	if (CNode3D_Class(n) == CLASS_OBJ3D_LIGHT) {
		CLight *l = Obj3D_Cast(n->obj3D, CLASS_OBJ3D_LIGHT);

		TreeView_InsertItemf(tree, tv, "Tipo: %s", Light_TypeStr(l->type));
		TreeView_InsertItemf(tree, tv, "Activa: %s", l->active ? "si" : "no");
		TreeView_InsertItemf(tree, tv, "Sombras: %s", l->shadow ? "si" : "no");
		v = l->d3d.Position;
		TreeView_InsertItemf(tree, tv, "PosRender: %s", Vec3_Str(&v, 0, 6));
		v = l->d3d.Direction;
		TreeView_InsertItemf(tree, tv, "DirRender: %s", Vec3_Str(&v, 0, 6));
		TreeView_InsertItemf(tree, tv, "Color: %s", Engine_ColorStr(l->color));
		TreeView_InsertItemf(tree, tv, "Rango: %f %f", (double)l->rangeStart, (double)l->range);
		TreeView_InsertItemf(tree, tv, "Objetivo: %s", Node_RefName(l->nodeTarget));
	}
	if (CNode3D_Class(n) == CLASS_OBJ3D_MESHD3D) {
		CMeshD3D *mesh = Obj3D_Cast(n->obj3D, CLASS_OBJ3D_MESHD3D);
		int count = 0, tris = 0, verts = 0;

		TreeView_InsertItemf(tree, tv, "nTri  nVert  Mat");
		for (; mesh; mesh = mesh->next) {
			count++;
			tris += (int)mesh->nTri;
			verts += mesh->vert.nVert;
			TreeView_InsertItemf(tree, tv, "%-4d    %-4d    [%s]  (%p)", mesh->nTri, mesh->vert.nVert,
				mesh->mat ? mesh->mat->Name : "NULL", (void *)mesh->mat);
		}
		TreeView_InsertItemf(tree, tv, "Total: %d  (%d tri, %d vert)", count, tris, verts);
	}
}

intptr_t __stdcall Engine_SceneDlgProc(void *hwnd, uint32_t msg, uintptr_t wParam, intptr_t lParam) {
	char buf[0x104];
	CScene *scn = (CScene *)(intptr_t)exe_GetWindowLongA(hwnd, GWL_USERDATA);
	CNode3D *n;
	TvInsert tv;
	vec3_t d;
	void *tree;
	const char *s0, *s1;
	int c[11];
	float len;

	switch (msg) {
	case WM_NOTIFY:
		if (*(int *)(lParam + 8) != TVN_SELCHANGED) {
			return 0;
		}
		/* NMTREEVIEW itemNew.lParam */
		n = *(CNode3D **)(lParam + 0x5c);
		if (n) {
			Bprintf(buf, 0x100, "%s\niObjeto: %d\niNodeTab: %d\niNodeXRef: %d\nAniMask: %04X\nRefs: %d\nant: %s\nsig: %s",
				n->name, n->index, n->slot, n->renderNode, (uint16_t)n->aniLocked, n->Refs, Node_RefName(n->prev),
				Node_RefName(n->next));
		} else {
			Bprintf(buf, 0x100, "AnimM3D: %s\nFileINI: %s\nCollideMesh: %s\nNodeTab: %s", Engine_BoolStr(scn->AnimM3D != NULL),
				Engine_BoolStr(scn->ini != NULL), Engine_BoolStr(scn->CollideMesh != NULL), "false");
		}
		exe_SetDlgItemTextA(hwnd, IDC_SCENE_INFO, buf);
		return 1;
	case WM_CLOSE:
		exe_EndDialog(hwnd, 1);
		return 1;
	case WM_COMMAND:
		/* IDOK, IDCANCEL */
		if (wParam - 1 <= 1) {
			exe_PostMessageA(hwnd, WM_CLOSE, 0, 0);
			return 1;
		}
		return 0;
	case WM_INITDIALOG:
		break;
	default:
		return 0;
	}
	scn = (CScene *)lParam;
	exe_SetWindowLongA(hwnd, GWL_USERDATA, (int32_t)lParam);
	Bprintf(buf, 0x100, "Escena: \"%s\"", scn ? scn->name : "(null)");
	exe_SetWindowTextA(hwnd, buf);
	if (!scn) {
		return 1;
	}
	c[10] = CScene_CountClass(scn, CLASS_OBJ3D_PORTAL);
	c[9] = CScene_CountClass(scn, CLASS_OBJ3D_FLARE);
	c[8] = CScene_CountClass(scn, CLASS_OBJ3D_GRAPHIC3D);
	c[7] = CScene_CountClass(scn, CLASS_OBJ3D_PARTICLESYSTEM);
	c[6] = CScene_CountClass(scn, CLASS_OBJ3D_GROUND);
	c[5] = CScene_CountClass(scn, CLASS_OBJ3D_LIGHT);
	c[4] = CScene_CountClass(scn, CLASS_OBJ3D_CAMERA);
	c[3] = CScene_CountClass(scn, CLASS_OBJ3D_MESHD3D);
	c[2] = CScene_CountClass(scn, CLASS_OBJ3D_TRIMESH);
	c[1] = CScene_CountClass(scn, 0);
	c[0] = CScene_NumNodes(scn);
	Bprintf(buf, 0x100,
		"%d Nodes:\n  %d Dummies\n  %d TriMesh\n  %d D3DMesh\n  %d Cameras\n  %d Lights\n  %d World\n  %d Particle\n"
		"  %d Sprite3D\n  %d Flares\n  %d Portals",
		c[0], c[1], c[2], c[3], c[4], c[5], c[6], c[7], c[8], c[9], c[10]);
	exe_SetDlgItemTextA(hwnd, IDC_SCENE_INFO, buf);
	tree = exe_GetDlgItem(hwnd, IDC_SCENE_TREE);
	tv.hParent = TVI_ROOT;
	tv.hInsertAfter = TVI_LAST;
	tv.mask = TVIF_TEXT | TVIF_PARAM;
	tv.lParam = 0;
	tv.hParent = TreeView_InsertItemf(tree, &tv, "-- Scene Info --");
	TreeView_InsertItemf(tree, &tv, "Ambient: %s", Engine_ColorStr((const byte *)&scn->ambient));
	TreeView_InsertItemf(tree, &tv, "Backgnd: %s", Engine_ColorStr((const byte *)&scn->fog));
	s1 = Vec3_Str(&scn->box.max, 1, 0);
	s0 = Vec3_Str(&scn->box.min, 0, 0);
	TreeView_InsertItemf(tree, &tv, "Extens: [%s] - [%s]", s0, s1);
	d.x = scn->box.max.x - scn->box.min.x;
	d.y = scn->box.max.y - scn->box.min.y;
	d.z = scn->box.max.z - scn->box.min.z;
	len = (float)__builtin_sqrt((double)(d.y * d.y + d.x * d.x + d.z * d.z)) * 0.5f;
	TreeView_InsertItemf(tree, &tv, "Dimens: [%s]  %.1f", Vec3_Str(&d, 0, 0), (double)len);
	TreeView_InsertItemf(tree, &tv, "Root: %s", scn->node);
	TreeView_InsertItemf(tree, &tv, "MainLOD: %s", scn->mainLOD);
	CScene_SetMarks(scn, 0);
	for (n = scn->Node3D; n; n = n->next) {
		tv.hParent = n->parent ? n->parent->mark : TVI_ROOT;
		tv.hInsertAfter = TVI_LAST;
		tv.mask = TVIF_TEXT | TVIF_STATE | TVIF_PARAM;
		tv.stateMask = TVIS_EXPANDED;
		tv.state = n == scn->Node3D ? TVIS_EXPANDED : 0;
		tv.lParam = (intptr_t)n;
		n->mark = (int)TreeView_InsertItemf(tree, &tv, "%s", n->name);
		tv.hParent = n->mark;
		tv.hInsertAfter = TVI_LAST;
		tv.mask = TVIF_TEXT | TVIF_PARAM;
		tv.lParam = (intptr_t)n;
		Engine_SceneTreeNode(tree, &tv, n);
	}
	return 1;
}

/* the frame time graph of R_Profiler 4 (Engine_DrawStats) */
extern char g_FpsText[64];	/* 0x40, "frame: fps fps  ms" when the fps jumped */
extern float g_FpsTextTime;
extern float g_FpsGraph[120];	/* FPS_GRAPH bars of 2 vertices (x, 0) (x, frame ms) */
extern float g_FpsGraphPeak[122];	/* the same with the falling peak */
#define g_FpsGraphPos GAME_GLOBAL_IN(int, g_FpsGraphPeak, 480)
#define g_FpsScaleTime GAME_GLOBAL_IN(float, g_FpsGraphPeak, 484)
extern float g_FpsScale;	/* the height of the graph: 200 ms per 200 fps */
#define FPS_GRAPH 30

void Engine_DrawFpsGraph(float y) {
	CGraphic2D g;
	float fps = (float)(1.0 / (double)(g_FrameDelta + 1e-05f));
	float *bar, *peak, fi, dt, k;
	int i;

	if (__builtin_fabs((double)(fps - g_Fps)) > (double)(g_Fps * 0.25f)) {
		Bprintf(g_FpsText, 0x40, "%d: %.2f %.2f  %.1f ms", g_FrameCount, (double)fps, (double)g_Fps,
			(double)(g_TimeMsF - g_FpsTextTime));
		g_FpsTextTime = g_TimeMsF;
	}
	Font_PrintfXY(g_FntComic14, 10.0f, y, "%s", g_FpsText);
	if (g_TimeMsF - g_FpsScaleTime > 2000.0f) {
		g_FpsScaleTime = g_TimeMsF;
		g_FpsScale = (float)((1 - (int)(g_Fps / -200.0f)) * 200);
	}
	i = g_FpsGraphPos;
	dt = g_FrameDelta;
	k = dt * 0.2f;
	fi = (float)i;
	bar = g_FpsGraph + i * 4;
	peak = g_FpsGraphPeak + i * 4;
	bar[0] = fi;
	bar[1] = 0.0f;
	bar[2] = fi;
	bar[3] = dt;
	peak[0] = fi;
	peak[1] = 0.0f;
	peak[2] = fi;
	peak[3] = Float_MaxSS(dt, peak[3] * 0.95f - k);
	g_FpsGraphPos = (i + 1) % FPS_GRAPH;
	CGraphic2D_ctorTex(&g, NULL, NULL, 0);
	g.x = 10.0f;
	g.y = 460.0f;
	g.scale[0] = 4.0f;
	g.scale[1] = g_FpsScale * -10.0f;
	g.screen = 1;
	g.screenSize = 1;
	g.noTexture = 1;
	g.color = 0x800000;
	CGraphic2D_Draw(&g, g_FpsGraphPeak, NULL, NULL, FPS_GRAPH * 2, D3DPT_TRIANGLESTRIP);
	g.color = 0xffffff;
	CGraphic2D_Draw(&g, g_FpsGraph, NULL, NULL, FPS_GRAPH * 2, D3DPT_TRIANGLESTRIP);
	Font_PrintfXY(g_FntComic14, 10.0f, 460.0f, "x%.1f", (double)g_FpsScale);
	CGraphic2D_dtor(&g);
}

/* a tree view item with the printf text (TVINSERTSTRUCT, its pszText at 0x18); the new item */
intptr_t TreeView_InsertItemf(void *hwnd, void *item, const char *fmt, ...) {
	char buf[0x400];
	intptr_t r;
	va_list ap;

	va_start(ap, fmt);
	VBprintf(buf, sizeof(buf), fmt, ap);
	va_end(ap);
	*(char **)((byte *)item + 0x18) = buf;
	r = exe_SendMessageA(hwnd, TVM_INSERTITEM, 0, (intptr_t)item);
	*(char **)((byte *)item + 0x18) = NULL;
	return r;
}

/* the nodes of the scene into the log, the children indented by 4 */
void Scene_LogTree(CScene *scn, CNode3D *node, int indent) {
	CNode3D *n;

	if (!scn) {
		return;
	}
	if (!node) {
		E3D_LogPrint(g_FileLog, "escena: %*s%s", indent, "", scn->name);
		node = scn->Node3D;
	}
	E3D_LogPrint(g_FileLog, "%*s%s", indent, "", node->name);
	n = node;
	do {
		if (n->parent == node) {
			Scene_LogTree(scn, n, indent + 4);
		}
		n = n->next;
	} while (n);
}

/* the commands of the debug console of the engine (g_E3DCmdProc) */
#define g_ShowJoins GAME_GLOBAL_IN(byte, group_4ad6f0, 0)	/* the joints of the meshes drawn */
void E3D_Command(char *cmd) {
	char *s;
	CScene *e;
	void *hwnd;

	E3D_DebugPrintf("\n> cmd: %s", cmd);
	if (!crt_stricmp(cmd, "listar luces")) {
		Light_List();
		return;
	}
	if (!crt_stricmp(cmd, "listar")) {
		UtilScene_Dump(NULL);
		return;
	}
	if (!crt_strnicmp(cmd, "arbol ", 6)) {
		/* the tree of the first scene whose file name matches, in the log and in the lights dialog */
		for (s = cmd + 5; *s == ' '; s++) {
		}
		for (e = Scene_Next(NULL); e; e = Scene_Next(e)) {
			if (Wild_Match(s, Path_FileName(e->name))) {
				break;
			}
		}
		Scene_LogTree(e, NULL, 0);
		hwnd = g_Engine->hWnd;
		exe_CreateDialogParamA((void *)exe_GetWindowLongA(hwnd, GWL_HINSTANCE), (const char *)ENGINE_LIGHTSDIALOG, hwnd,
			(void *)Engine_SceneDlgProc, (intptr_t)e);
		return;
	}
	if (!crt_stricmp(cmd, "mem")) {
		return;
	}
	if (!crt_stricmp(cmd, "ver uniones")) {
		g_ShowJoins = g_ShowJoins == 0;
		E3D_DebugPrintf("\nver uniones: %d", g_ShowJoins);
		return;
	}
	if (crt_stricmp(cmd, "imbecil") && crt_stricmp(cmd, "idiota") && crt_stricmp(cmd, "capullo")) {
		E3D_DebugPrintf("\ncomando no reconocido.");
		return;
	}
	E3D_DebugPrintf(crt_rand() & 1 ? "\nque te zurzan" : "\npirate");
}

/* ---- the menu of the D3D window ---- */

#define MF_GRAYED 1
#define MF_CHECKED 8
#define IDM_MODE0 0x9c4b
#define IDM_MODE1 0x9c4c
#define IDM_MODE2 0x9c4d
#define IDM_SPECULAR 0x9c54
#define IDM_RENDERMODE100 0x9c56
#define IDM_9C57 0x9c57
#define IDM_USARVB 0x9c59
#define IDM_9C5A 0x9c5a
#define IDM_9C5B 0x9c5b
#define IDM_9C5C 0x9c5c
#define IDM_9C5D 0x9c5d
#define IDM_LIGHTS 0x9c5e
#define IDM_SOLOB38 0x9c5f
#define IDM_WORLD0 0x9c62
#define IDM_TILE 0x9c63
#define IDM_9C64 0x9c64
#define IDM_SHARPEN 0x9c65
#define IDM_RELOADMATS 0x9c66
#define IDM_CAMERA 50000		/* the cameras of the global scene from here */
extern byte g_Engine4af7d4;
extern int g_Sharpen;		/* [video] Sharpen of m3d.ini */
extern int g_MouseExcl;		/* [dinput] raton_excl */

static uint32_t Menu_Check(int on) {
	return on ? MF_CHECKED : 0;
}

/* the menu items greyed and checked by the state */
void CEngineApp_UpdateMenu(CEngineApp *self) {
	void *menu = self->menu;

	if (!menu) {
		return;
	}
	exe_EnableMenuItem(menu, IDM_MODE0, g_MipmapsOn == 0);
	exe_EnableMenuItem(self->menu, IDM_MODE1, g_MipmapsOn == 0);
	exe_EnableMenuItem(self->menu, IDM_MODE2, g_MipmapsOn == 0);
	exe_CheckMenuItem(self->menu, IDM_MODE0, Menu_Check(g_EngineMode == 0));
	exe_CheckMenuItem(self->menu, IDM_MODE1, Menu_Check(g_EngineMode == 1));
	exe_CheckMenuItem(self->menu, IDM_MODE2, Menu_Check(g_EngineMode == 2));
	exe_EnableMenuItem(self->menu, IDM_9C5B, !self->canMenu9c5d && !self->canMenu9c5c ? MF_GRAYED : 0);
	exe_EnableMenuItem(self->menu, IDM_9C5D, self->canMenu9c5d == 0);
	exe_EnableMenuItem(self->menu, IDM_9C5C, self->canMenu9c5c == 0);
	exe_CheckMenuItem(self->menu, IDM_9C5B, Menu_Check(self->menuMode == 0));
	exe_CheckMenuItem(self->menu, IDM_9C5D, Menu_Check(self->menuMode == 1));
	exe_CheckMenuItem(self->menu, IDM_9C5C, Menu_Check(self->menuMode == 1));
	exe_EnableMenuItem(self->menu, IDM_9C5A, self->canWBuffer == 0);
	exe_CheckMenuItem(self->menu, IDM_9C5A, Menu_Check(self->useWBuffer != 0));
	exe_CheckMenuItem(self->menu, IDM_SPECULAR, Menu_Check(g_VertexSpecular != 0));
	exe_CheckMenuItem(self->menu, IDM_RENDERMODE100, Menu_Check(g_RenderMode100 != 0));
	exe_CheckMenuItem(self->menu, IDM_9C57, Menu_Check(g_Engine431bae != 0));
	exe_CheckMenuItem(self->menu, IDM_USARVB, Menu_Check(g_UseVB != 0));
	exe_CheckMenuItem(self->menu, IDM_WORLD0, Menu_Check(*(byte *)World_Get() != 0));
	exe_CheckMenuItem(self->menu, IDM_SOLOB38, Menu_Check(g_RenderOnlyB38 != 0));
	exe_CheckMenuItem(self->menu, IDM_9C64, Menu_Check(g_Engine4af7d4 != 0));
	exe_CheckMenuItem(self->menu, IDM_SHARPEN, Menu_Check(g_Sharpen != 0));
}

#define ENGINE_MAXCAMERAS 0x14
#define MF_BYPOSITION 0x400
#define D3DRS_88 0x88
#define D3DRS_89 0x89
#define ENGINE_BUILD 0x15f8

/* InitDeviceObjects: the device objects of the engine, the cameras menu, the title, the input on the window */
int CEngineApp_InitDeviceObjects(CEngineApp *self) {
	void *dev = self->base.device;
	char title[0x100];
	int hr;

	g_EngineInits++;
	self->canWBuffer = (byte)(self->base.caps.RasterCaps >> 18) & 1;
	Render_IniD3D();
	CWorld_InitD3D(World_Get());
	UtilScene_InitD3D();
	Stencil_Init();
	Trails_Init();
	LensFlare_Init();
	Background2D_Reset();
	CMatList_Prepare(g_MatList, dev);
	Render_SetCamera(World_Get()->cam);
	Render_SetWorldMatrix(g_MatIdentity, 0);
	CScene_FindClass(self->scene, NULL, CLASS_OBJ3D_LIGHT);
	Rd_DevSetRenderState(dev, D3DRS_88, 1);
	Rd_DevSetRenderState(dev, D3DRS_89, 1);
	self->menu = exe_GetMenu(self->base.hWnd);
	CEngineApp_UpdateMenu(self);
	CHECK(!self->table.p);
	self->table.n = ENGINE_MAXCAMERAS;
	self->table.p = operator_new_array(ENGINE_MAXCAMERAS * sizeof(void *));
	CHECK(self->table.p);
	CHECK(self->table.p);
	memset(self->table.p, 0, (uint32_t)self->table.n * sizeof(void *));
	self->cameraMenu = exe_GetSubMenu(self->menu, 2);
	if (self->cameraMenu) {
		CNode3D *n;
		uint32_t i = 0;

		while (exe_DeleteMenu(self->cameraMenu, 0, MF_BYPOSITION)) {
		}
		for (n = CScene_FindClass(self->scene, NULL, CLASS_OBJ3D_CAMERA); n;
			n = CScene_FindClass(self->scene, n, CLASS_OBJ3D_CAMERA)) {
			CCamera *cam;

			if (i == (uint32_t)self->table.n) {
				exe_AppendMenuA(self->cameraMenu, 0, i + IDM_CAMERA, "...");
				break;
			}
			cam = Obj3D_Cast(n->obj3D, CLASS_OBJ3D_CAMERA);
			if (!self->table.p || i >= (uint32_t)self->table.n) {
				CHECK(0);
			}
			((CCamera **)self->table.p)[i] = cam;
			exe_AppendMenuA(self->cameraMenu, 0, i + IDM_CAMERA, n->name);
			i++;
		}
	}
	Bprintf(title, sizeof(title), "%s (%d)%s - [%s]", self->base.windowTitle, ENGINE_BUILD, "", self->scene->name);
	if (!self->base.windowed) {
		Bprintf(title, sizeof(title), "Scrapland (%d)%s", ENGINE_BUILD, "");
	}
	exe_SetWindowTextA(self->base.hWnd, title);
	{
		typedef int(* create_t)(void *, void *, void *, int);
		void **vt = *(void ***)g_Input;
		int excl = g_MouseExcl != 0;

		hr = ((create_t)vt[1])(g_Input, self->base.hWnd,
			(void *)exe_GetWindowLongA(self->base.hWnd, GWL_HINSTANCE), excl);
	}
	if (hr < 0) {
		return hr;
	}
	((void(*)(void *, int))(*(void ***)g_Input)[5])(g_Input, 1);
	return 0;
}

/* MsgProc: the input first, the keys, the menu commands of the window, then the one of CD3DApplication */
intptr_t CEngineApp_MsgProc(CEngineApp *self, void *hwnd, uint32_t msg, uintptr_t wParam, intptr_t lParam) {
	if (g_Input) {
		((void(*)(void *, void *, uint32_t, uintptr_t, intptr_t))(*(void ***)g_Input)[7])(g_Input, hwnd,
			msg, wParam, lParam);
	}
	if (msg == WM_KEYDOWN) {
		Keyboard_KeyDown((uint16_t)wParam, lParam);
	} else if (msg == WM_KEYUP) {
		Keyboard_KeyUp((uint16_t)wParam, lParam);
	} else if (msg == WM_COMMAND) {
		uint32_t id = (uint16_t)wParam;

		switch (id) {
		case IDM_MODE0:
		case IDM_MODE1:
		case IDM_MODE2:
			g_EngineMode = (int)(id - IDM_MODE0);
			break;
		case IDM_SPECULAR:
			g_VertexSpecular = g_VertexSpecular == 0;
			break;
		case IDM_RENDERMODE100:
			g_RenderMode100 = g_RenderMode100 == 0;
			break;
		case IDM_9C57:
			g_Engine431bae = g_Engine431bae == 0;
			break;
		case IDM_USARVB:
			g_UseVB = g_UseVB == 0;
			break;
		case IDM_9C5A:
			self->useWBuffer = self->useWBuffer == 0;
			break;
		case IDM_9C5B:
			self->menuMode = 0;
			break;
		case IDM_9C5C:
		case IDM_9C5D:
			self->menuMode = 1;
			break;
		case IDM_LIGHTS:
			CEngineApp_LightsDialog(self, hwnd, (void *)exe_GetWindowLongA(hwnd, GWL_HINSTANCE));
			break;
		case IDM_SOLOB38:
			g_RenderOnlyB38 = g_RenderOnlyB38 == 0;
			break;
		case IDM_WORLD0: {
			byte off = *(byte *)World_GetChecked() == 0;

			*(byte *)World_GetChecked() = off;
			break;
		}
		case IDM_TILE:
			g_TileOn = 1;
			break;
		case IDM_9C64:
			g_Engine4af7d4 = g_Engine4af7d4 == 0;
			break;
		case IDM_SHARPEN:
			g_Sharpen = g_Sharpen == 0;
			[[fallthrough]];
		case IDM_RELOADMATS:
			/* the textures and materials made again */
			Texture_ReleaseAllD3D();
			CMatList_Release(g_MatList);
			CMatList_Prepare(g_MatList, self->base.device);
			break;
		default: {
			/* a camera of the global scene into the world, the menu item checked */
			int i = (int)id - IDM_CAMERA;

			if (i >= 0 && i < self->table.n) {
				CCamera **cams;
				int k;

				for (k = 0; k < self->table.n; k++) {
					exe_CheckMenuItem(self->menu, (uint32_t)(k + IDM_CAMERA), i == k ? MF_CHECKED : 0);
				}
				if (!self->table.p || (uint32_t)i >= (uint32_t)self->table.n) {
					CHECK(0);
				}
				cams = self->table.p;
				CWorld_SetCamera(World_Get(), cams[i]);
			}
			break;
		}
		}
		CEngineApp_UpdateMenu(self);
	}
	return CD3DApp_MsgProc(&self->base, hwnd, msg, wParam, lParam);
}

/* ---- CEntradaNula: every method of the input does nothing ---- */

CNullInput *CNullInput_Delete(CNullInput *self, byte flags) {
	self->vftable = CNULLINPUT_VFTABLE;
	IInput_dtor(self);
	if (flags & 1) {
		operator_delete_sized(self, sizeof(*self));
	}
	return self;
}

int CNullInput_Nothing3(CNullInput *self, int a, int b, int c) {
	(void)self;
	(void)a;
	(void)b;
	(void)c;
	return 0;
}

int CNullInput_Nothing1(CNullInput *self, int a) {
	(void)self;
	(void)a;
	return 0;
}

int CNullInput_Nothing(CNullInput *self) {
	(void)self;
	return 0;
}

/* the original leaves eax as it was */
void CNullInput_Nothing4(CNullInput *self, int a, int b, int c, int d) {
	(void)self;
	(void)a;
	(void)b;
	(void)c;
	(void)d;
}
