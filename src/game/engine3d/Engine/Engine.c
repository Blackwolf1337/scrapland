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
#include "scrap/platform.h"

extern const CD3DApplicationVtbl CEngineApp_vftable;
#define CMOTORAPP_VFTABLE (&CEngineApp_vftable)
extern const IInputVtbl CNullInput_vftable;
#define CNULLINPUT_VFTABLE (&CNullInput_vftable)
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
	DbgCon_Init();
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
int CEngineApp_OneTimeSceneInit(CD3DApplication *base) {
	CEngineApp *self = (CEngineApp *)base;
	void *mem;

	g_E3DCmdProc = (void *)E3D_Command;
	E3D_DebugPrintf("\nprg: '%s'", E3D_GetExePath());
	if (g_EngineInit && !((byte(*)(void))g_EngineInit)()) {
		return (int)0x80004005;
	}
	CHECK(g_World);
	mem = operator_new(sizeof(CNullInput));
	if (mem) {
		IInput_ctor(mem);
		((IInput *)mem)->vftable = CNULLINPUT_VFTABLE;
	}
	g_Input = mem;
	mem = operator_new(sizeof(CGraphic2DList));
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
int CEngineApp_FrameMove(CD3DApplication *elem) {
	CEngineApp *self = (CEngineApp *)elem;
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
			Plat_SetTitle("Scrapland dedicated server");
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
#define g_StencilClear (*(uint32_t *)&GAME_GVAR(GVar_R_StencilRefClear)->value)
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
	return Plat_Ticks() * 3 >= g_LimitNext;
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
int CEngineApp_Render(CD3DApplication *elem) {
	CEngineApp *self = (CEngineApp *)elem;
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
				g_LimitNext = Plat_Ticks() * 3;
			}
			if (Plat_Ticks() * 3 >= g_LimitNext + frame) {
				E3D_LogVerbose(g_FileLog, "WARN: LimitFPS: Framerate below fps limit by %d ms",
					(Plat_Ticks() * 3 - g_LimitNext - frame) / 3);
			}
			if (Plat_Ticks() * 3 - g_LimitNext > 15000) {
				g_LimitNext = Plat_Ticks() * 3;
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
int CEngineApp_DeleteDeviceObjects(CD3DApplication *base) {
	CEngineApp *self = (CEngineApp *)base;
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
int CEngineApp_FinalCleanup(CD3DApplication *base) {
	CEngineApp *self = (CEngineApp *)base;
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
int CEngineApp_ConfirmDevice(CD3DApplication *base, D3DCaps8 *caps, uint32_t behavior, uint32_t format) {
	CEngineApp *self = (CEngineApp *)base;
	(void)self;
	(void)caps;
	(void)format;
	return (byte)(behavior & 0x10) ? (int)0x80004005 : 0;
}

int CEngineApp_RestoreDeviceObjects(CD3DApplication *base) {
	CEngineApp *self = (CEngineApp *)base;
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
		used = Plat_VirtualMemoryUsed();
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
		/* the tree of the first scene whose file name matches, in the log */
		for (s = cmd + 5; *s == ' '; s++) {
		}
		for (e = Scene_Next(NULL); e; e = Scene_Next(e)) {
			if (Wild_Match(s, Path_FileName(e->name))) {
				break;
			}
		}
		Scene_LogTree(e, NULL, 0);
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

extern byte g_Engine4af7d4;
extern int g_Sharpen;		/* [video] Sharpen of m3d.ini */
extern int g_MouseExcl;		/* [dinput] raton_excl */
#define ENGINE_MAXCAMERAS 0x14
#define D3DRS_88 0x88
#define D3DRS_89 0x89
#define ENGINE_BUILD 0x15f8

/* InitDeviceObjects: the device objects of the engine, the cameras of the scene, the title, the input on the window */
int CEngineApp_InitDeviceObjects(CD3DApplication *elem) {
	CEngineApp *self = (CEngineApp *)elem;
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
	CHECK(!self->table.p);
	self->table.n = ENGINE_MAXCAMERAS;
	self->table.p = operator_new_array(ENGINE_MAXCAMERAS * sizeof(void *));
	CHECK(self->table.p);
	CHECK(self->table.p);
	memset(self->table.p, 0, (uint32_t)self->table.n * sizeof(void *));
	/* the cameras of the scene (the original listed them in the debug menu of the window, which is gone) */
	{
		uint32_t i = 0;

		for (CNode3D *n = CScene_FindClass(self->scene, NULL, CLASS_OBJ3D_CAMERA); n && i < (uint32_t)self->table.n;
			n = CScene_FindClass(self->scene, n, CLASS_OBJ3D_CAMERA)) {
			((CCamera **)self->table.p)[i++] = Obj3D_Cast(n->obj3D, CLASS_OBJ3D_CAMERA);
		}
	}
	Bprintf(title, sizeof(title), "%s (%d)%s - [%s]", self->base.windowTitle, ENGINE_BUILD, "", self->scene->name);
	if (!self->base.windowed) {
		Bprintf(title, sizeof(title), "Scrapland (%d)%s", ENGINE_BUILD, "");
	}
	Plat_SetTitle(title);
	{
		typedef int(* create_t)(void *, void *, void *, int);
		void **vt = *(void ***)g_Input;
		int excl = g_MouseExcl != 0;

		hr = ((create_t)vt[1])(g_Input, self->base.hWnd,
			NULL, excl);
	}
	if (hr < 0) {
		return hr;
	}
	((void(*)(void *, int))(*(void ***)g_Input)[5])(g_Input, 1);
	return 0;
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

/* the vtable of CEngineApp */
const CD3DApplicationVtbl CEngineApp_vftable = {
	.AdjustWindowForChange = CD3DApp_AdjustWindowForChange,
	.ConfirmDevice = CEngineApp_ConfirmDevice,
	.OneTimeSceneInit = CEngineApp_OneTimeSceneInit,
	.InitDeviceObjects = CEngineApp_InitDeviceObjects,
	.RestoreDeviceObjects = CEngineApp_RestoreDeviceObjects,
	.FrameMove = CEngineApp_FrameMove,
	.Render = CEngineApp_Render,
	.InvalidateDeviceObjects = CD3DApplication_Nothing,
	.DeleteDeviceObjects = CEngineApp_DeleteDeviceObjects,
	.FinalCleanup = CEngineApp_FinalCleanup,
	.Create = CD3DApp_Create,
	.Run = NULL,
	.msgProc = NULL,
	.Pause = CD3DApp_Pause,
};

/* the vtable of CNullInput */
const IInputVtbl CNullInput_vftable = {
	.Delete = CNullInput_Delete,
	.Init = IInput_InitFalse,
	.Release = IInput_ReleaseFalse,
	.ReadMouse = IInput_ReadMouseFalse,
	.ReadKeys = IInput_ReadKeysFalse,
	.AcquireMouse = IInput_AcquireMouseFalse,
	.AcquireKeys = IInput_AcquireKeysFalse,
	.WndProc = IInput_WndProcNop,
};
