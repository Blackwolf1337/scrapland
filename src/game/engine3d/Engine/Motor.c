/*
 * Engine/Motor.cpp of the engine3d: CMotorApp, the D3D application of the engine (a CD3DApplication of the DX8 SDK
 * framework, 0x2a600 bytes, g_Motor3D) with its three callbacks of the game, the global scene of the world and the
 * small methods; CEntradaNula, the input that does nothing. The frame, the device objects, the menu, the dialogs
 * and the ini reader are still the exe's.
 */
#include <stdarg.h>
#include <string.h>

#include "scrap/dxutil.h"
#include "scrap/entrada.h"
#include "scrap/escena.h"
#include "scrap/e3dutil.h"
#include "scrap/app.h"
#include "scrap/crt.h"
#include "scrap/d3d8.h"
#include "scrap/fondo2d.h"
#include "scrap/lensflare.h"
#include "scrap/luz.h"
#include "scrap/mallad3d.h"
#include "scrap/archio.h"
#include "scrap/flare.h"
#include "scrap/fuentes.h"
#include "scrap/grafico2d.h"
#include "scrap/material.h"
#include "scrap/nodefx.h"
#include "scrap/render.h"
#include "scrap/stencil.h"
#include "scrap/textura.h"
#include "scrap/trails.h"
#include "scrap/utild3d.h"
#include "scrap/modelo.h"
#include "scrap/mundo.h"
#include "scrap/nodo3d.h"
#include "scrap/prof.h"
#include "scrap/scrapmain.h"
#include "scrap/tabla.h"
#include "scrap/texto.h"
#include "scrap/varios.h"
#include "scrap/win32.h"

#define CMOTORAPP_VFTABLE 0x3d59e0
#define CENTRADANULA_VFTABLE 0x3d5934
#define CMOTORAPP_SIZE 0x2a600
#define CENTRADANULA_SIZE 0x114

#define MOTOR_TITLE(m) (*(const char **)((m) + 0x2a5ac))
#define MOTOR_I2A5B4(m) (*(int *)((m) + 0x2a5b4))
#define MOTOR_I2A5B8(m) (*(int *)((m) + 0x2a5b8))
#define MOTOR_MINW(m) (*(int *)((m) + 0x2a5bc))
#define MOTOR_MINH(m) (*(int *)((m) + 0x2a5c0))
#define MOTOR_MENU(m) (*(void **)((m) + 0x2a5dc))
#define MOTOR_I2A5E0(m) (*(int *)((m) + 0x2a5e0))
#define MOTOR_TABLA(m) ((CTabla *)((m) + 0x2a5e4))
#define MOTOR_ESCENA(m) (*(CEscena **)((m) + 0x2a5ec))	/* EscenaGlobal, counted */
#define MOTOR_W2A5F0(m) ((int *)((m) + 0x2a5f0))
#define MOTOR_B2A5F9(m) (*(byte *)((m) + 0x2a5f9))

#define g_Motor3D GAME_DATA(byte *, 0x4a9038)
/* the callbacks of Engine3D_Create: once after the device, each frame, at the end */
#define g_MotorInit GAME_DATA(void *, 0x4a8f90)
#define g_MotorFrame GAME_DATA(void *, 0x4a8f94)
#define g_MotorRelease GAME_DATA(void *, 0x4a8f98)
#define g_MotorFrameCount GAME_DATA(int, 0x4a8f78)
/* the values m3d.ini gave (Motor_LoadIni) */
#define g_MotorIni7c GAME_DATA(int, 0x4a8f7c)
#define g_MotorIni80 GAME_DATA(int, 0x4a8f80)
#define g_MotorIni84 GAME_DATA(int, 0x4a8f84)
#define g_MotorIni88 GAME_DATA(int, 0x4a8f88)
#define g_MotorDef1cf4 GAME_DATA(int, 0x431cf4)
#define g_MotorMode GAME_DATA(int, 0x431d70)		/* 1 or 2 */
#define g_ColorStr ((char *)Game_Ptr(0x4a9050))	/* 0x40 */

typedef void(__stdcall *initcommoncontrols_t)(void);
#define exe_InitCommonControls GAME_DATA(initcommoncontrols_t, 0x3a104c)

/* the console command handler of the engine (E3D_Command) */
#define MOTOR_CMDPROC 0x22eda0
#define g_Entrada GAME_DATA(void *, 0x4db418)	/* the input of the engine, a CEntradaNula */
#define MOTOR_OVERLAY(m) (*(void **)((m) + 0x2a5f0))	/* 8 bytes with a CGrafico2D */
#define MOTOR_OBJ2A5F4(m) (*(void ***)((m) + 0x2a5f4))
#define MOTOR_TIME(m) (*(float *)((m) + 0x2a518))	/* s */
#define MOTOR_ELAPSED(m) (*(float *)((m) + 0x2a51c))
/* the clock of the engine */
#define g_FrameDelta GAME_DATA(float, 0x4a8ed4)
#define g_RealFrameSecs GAME_DATA(float, 0x4a8ee4)
#define g_TimeD GAME_DATA(double, 0x4a8ed8)
#define g_TimeMsInt GAME_DATA(int, 0x4a8ee0)
#define g_GameTimeD GAME_DATA(double, 0x4a8ee8)
#define g_LastTime GAME_DATA(float, 0x4a903c)
#define g_FpsTime GAME_DATA(float, 0x4a9040)
#define g_FpsFrames GAME_DATA(int, 0x4a9044)
#define g_Fps GAME_DATA(float, 0x431ba8)
#define MOTOR_MAXFRAME 0.1f

#define WM_KEYDOWN 0x100
#define WM_KEYUP 0x101
#define WM_COMMAND 0x111

byte *__fastcall CMotorApp_ctor(byte *self) {
	CD3DApp_ctor(self);
	*(void **)self = Game_Ptr(CMOTORAPP_VFTABLE);
	MOTOR_TABLA(self)->p = NULL;
	MOTOR_TABLA(self)->n = 0;
	MOTOR_TITLE(self) = "ScrapEngine [DX8]";
	MOTOR_USEDEPTH(self) = 1;
	MOTOR_MINW(self) = 640;
	MOTOR_MINH(self) = 480;
	MOTOR_MENU(self) = NULL;
	MOTOR_I2A5E0(self) = 0;
	MOTOR_ESCENA(self) = NULL;
	MOTOR_W2A5F0(self)[0] = 0;
	MOTOR_W2A5F0(self)[1] = 0;
	MOTOR_W2A5F0(self)[2] = 0;
	MOTOR_W2A5F0(self)[3] = 0;
	Motor_LoadIni(g_M3dIni, NULL);
	MOTOR_VSYNC(self) = g_VSync;
	MOTOR_B2A5F9(self) = g_MotorIni7c != 0;
	g_VertexSpecular = g_MotorIni80 != 0;
	MOTOR_I2A5B8(self) = g_MotorIni84;
	MOTOR_I2A5B4(self) = g_MotorDef1cf4;
	g_MotorMode = (g_MotorIni88 != 0) + 1;
	DbgWin_Init(NULL);
	return self;
}

static void Motor_Free(byte *m) {
	Tabla_Free(MOTOR_TABLA(m));
	CD3DApp_dtor(m);
	operator_delete_sized(m, CMOTORAPP_SIZE);
}

/* the engine made (destroy 0) or freed */
void Engine3D_Create(void *hInst, void *hWnd, void *init, void *frame, void *release, int destroy) {
	(void)hInst;
	if (!destroy) {
		void *mem;
		byte *m;

		CHECK(!g_Motor3D);
		mem = operator_new(CMOTORAPP_SIZE);
		m = mem ? CMotorApp_ctor(mem) : NULL;
		MOTOR_HWND(m) = hWnd;
		g_MotorInit = init;
		g_MotorFrame = frame;
		g_Motor3D = m;
		g_MotorRelease = release;
		exe_InitCommonControls();
		return;
	}
	if (!g_Motor3D) {
		CHECK(g_Motor3D);
		if (!g_Motor3D) {
			g_Motor3D = NULL;
			return;
		}
	}
	Motor_Free(g_Motor3D);
	g_Motor3D = NULL;
}

void Engine3D_Shutdown(void) {
	if (!g_Motor3D) {
		CHECK(g_Motor3D);
		if (!g_Motor3D) {
			g_Motor3D = NULL;
			return;
		}
	}
	Motor_Free(g_Motor3D);
	g_Motor3D = NULL;
}

/* OneTimeSceneInit: the init callback of the game, the input, the fonts and trails, the global scene */
int __thiscall CMotorApp_OneTimeSceneInit(byte *self) {
	void *mem;

	g_E3DCmdProc = Game_Ptr(MOTOR_CMDPROC);
	E3D_DebugPrintf("\nprg: '%s'", E3D_GetExePath());
	if (g_MotorInit && !((byte(*)(void))g_MotorInit)()) {
		return (int)0x80004005;
	}
	CHECK(g_Mundo);
	mem = operator_new(CENTRADANULA_SIZE);
	if (mem) {
		IEntrada_ctor(mem);
		*(void **)mem = Game_Ptr(CENTRADANULA_VFTABLE);
	}
	g_Entrada = mem;
	mem = operator_new(8);
	MOTOR_OVERLAY(self) = mem ? CListaGrf2D_ctor(mem) : NULL;
	Fonts_Init();
	Trails_InitLists();
	LensFlare_InitList();
	Escenas_Msg(1, 0, 0, 0, 0);
	Escenas_Msg(2, 0, 0, 0, 0);
	Motor_GetEscena(self);
	return 0;
}

/* FrameMove: the clock of the engine (real and game time, fps once a second) */
int __thiscall CMotorApp_FrameMove(byte *self) {
	float t = MOTOR_TIME(self), d;

	g_FrameDelta = t - g_LastTime;
	g_LastTime = t;
	g_TimeMsF = (float)((double)t * 1000.0);
	g_RealFrameSecs = MOTOR_ELAPSED(self);
	g_TimeD = (double)MOTOR_TIME(self);
	g_TimeMsInt = (int)(g_TimeD * 1000.0);
	g_FrameSec = World_TimeScale() * MOTOR_ELAPSED(self);
	if (!(MOTOR_MAXFRAME > g_FrameSec)) {
		g_FrameSec = MOTOR_MAXFRAME;
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
			Win_SetTextf(MOTOR_HWND(self), "Scrapland dedicated server");
		}
	}
	return 0;
}

/* DeleteDeviceObjects: the device objects of the input, the textures, materials, shadows, trails and the world */
int __thiscall CMotorApp_DeleteDeviceObjects(byte *self) {
	((void(__thiscall *)(void *))(*(void ***)g_Entrada)[2])(g_Entrada);
	Tabla_Free(MOTOR_TABLA(self));
	Textura_ReleaseAllD3D();
	CListaMat_Release(g_ListaMat);
	Stencil_Free();
	Trails_Release();
	LensFlare_Release();
	UtilEscena_ReleaseD3D();
	CMundo_ReleaseD3D(Mundo_Get());
	Render_FinD3D();
	return 0;
}

/* FinalCleanup: the global scene, the release callback of the game, everything of the engine */
int __thiscall CMotorApp_FinalCleanup(byte *self) {
	void *ov;

	Motor_ReleaseEscena(self);
	if (g_MotorRelease) {
		((byte(*)(void))g_MotorRelease)();
	}
	CHECK(!g_Mundo);
	Fonts_Free();
	Debug_PuntosClear();
	ov = MOTOR_OVERLAY(self);
	if (ov) {
		CListaGrf2D_DeleteAll(ov);
		operator_delete_sized(ov, 8);
		MOTOR_OVERLAY(self) = NULL;
	}
	if (MOTOR_OBJ2A5F4(self)) {
		void **obj = MOTOR_OBJ2A5F4(self);

		((void *(__thiscall *)(void *, int))((void **)obj[0])[0])(obj, 1);
		MOTOR_OBJ2A5F4(self) = NULL;
	}
	UtilEscena_FreeAll();
	Trails_ResetAll();
	LensFlare_ClearAll();
	Fondo2D_Release();
	CListaMat_DeleteAll(g_ListaMat);
	Textura_DeleteAll();
	Tex_ClearPaths();
	Tex_ClearExtensions();
	if (g_Entrada) {
		((void *(__thiscall *)(void *, int))(*(void ***)g_Entrada)[0])(g_Entrada, 1);
		g_Entrada = NULL;
	}
	return 0;
}

/* ConfirmDevice: no device with the pure behaviour 0x10 */
int __thiscall CMotorApp_ConfirmDevice(byte *self, void *caps, uint32_t behavior, int format) {
	(void)self;
	(void)caps;
	(void)format;
	return (byte)(behavior & 0x10) ? (int)0x80004005 : 0;
}

int __thiscall CMotorApp_RestoreDeviceObjects(byte *self) {
	(void)self;
	g_MotorFrameCount++;
	return 0;
}

int __thiscall CD3DApplication_Nothing(byte *self) {
	(void)self;
	return 0;
}

/* "r g b a i" of a color with intensity (4 bytes and a float), in a static buffer */
char *Motor_ColorStr(const byte *c) {
	Bprintf(g_ColorStr, 0x40, "%d %d %d %d %f (rgbai)", c[0], c[1], c[2], c[3], (double)*(const float *)(c + 4));
	return g_ColorStr;
}

const char *Motor_BoolStr(int b) {
	return b ? "true" : "false";
}

/* the out of line copy of the mundo.h accessor */
CMundo *Mundo_GetChecked(void) {
	return Mundo_Get();
}

/* the statistics of R_Profiler */
#define g_RenderCounts ((int *)Game_Ptr(0x4a8f00))	/* 8, g_RenderTris and g_RenderVerts first */
#define g_ProfResetTime GAME_DATA(float, 0x4a94a0)
#define g_ProfNoReset GAME_DATA(int, 0x4a8fb8)	/* 0: the counters taken every frame, else once a second */
#define g_CollisionCacheHit GAME_DATA(uint32_t, 0x4af7e0)
#define g_CollisionCacheMiss GAME_DATA(uint32_t, 0x4af7e4)
#define g_TexturePool GAME_DATA(int, 0x4b2670)
#define g_TexCreated GAME_DATA(int, 0x4b25f0)
#define g_TexCached GAME_DATA(int, 0x4b25f4)
#define g_TexUsed GAME_DATA(int, 0x4b25f8)
#define g_TexUsedPixels GAME_DATA(int, 0x4b25fc)
#define PROF_FRAME3D 31
#define D3DDEV_GETAVAILABLETEXTUREMEM 0x10
typedef uint32_t(__stdcall *d3dGetAvailableTextureMem_t)(void *dev);

/* the sections of the profiler page (mode 5) */
static const struct {
	int section;
	float y;
	const char *fmt;
} g_ProfLines[] = {
	{ 0x1f, 128.0f, "frame3d_31: %.2f %.2f %d" },
	{ 0x10, 144.0f, "SphSli3D_16: %.2f %.2f %d" },
	{ 0x11, 160.0f, "SphSli2D_17: %.2f %.2f %d" },
	{ 0x12, 176.0f, "SphTest_18: %.2f %.2f %d" },
	{ 0x13, 192.0f, "LineTest_19: %.2f %.2f %d" },
	{ 0x14, 208.0f, "BoxTest_20: %.2f %.2f %d" },
	{ 0x15, 224.0f, "LineSphe_21: %.2f %.2f %d" },
	{ 0x16, 240.0f, "SphCheck_22: %.2f %.2f %d" },
	{ 0x17, 256.0f, "LinePort_23: %.2f %.2f %d" },
	{ 0x1e, 272.0f, "TestVis__30: %.2f %.2f %d" },
	{ 0x1d, 288.0f, "EnLightR_29: %.2f %.2f %d" },
};

void Motor_DrawStats(void) {
	CCamara *cam;
	uint32_t ms[16];		/* MEMORYSTATUSEX */
	uint64_t used;
	float tm, mx;
	int i, cnt, d3d, act, tot, mode;
	byte now, reset;

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
			g_MotorFrameCount, g_MotorInits);
		mode = g_ProfMode;
	}
	if (mode == 2) {
		Font_SetColor(g_FntComic14, 0xffffff);
		cam = Mundo_Get()->cam;
		Font_Printf(g_FntComic14, 10.0f, 32.0f, 2, "cam: %s  %.2f %.2f  %.2f", Vec3_StrDeg(&cam->rot, 0, 2),
			(double)cam->zNear, (double)cam->zFar, (double)cam->fov * 180.0 / 3.141592653589793);
		d3d = Luz_NumD3D();
		act = Luz_NumActivas();
		tot = Luz_NumTotal();
		Font_Printf(g_FntComic14, 10.0f, 48.0f, 2, "luz: %d %d %d", tot, act, d3d);
		cam = Mundo_Get()->cam;
		Font_Printf(g_FntComic14, 10.0f, 80.0f, 2, "poscam: %.1f %.1f %.1f", (double)cam->pos.x, (double)cam->pos.y,
			(double)cam->pos.z);
		mode = g_ProfMode;
	}
	if (mode == 3) {
		Font_Printf(g_FntComic14, 10.0f, 32.0f, 2, "FreeTextMem: %d Mb (pool %d)",
			D3D_METHOD(g_pd3dDevice, d3dGetAvailableTextureMem_t, D3DDEV_GETAVAILABLETEXTUREMEM)(g_pd3dDevice) >> 20,
			g_TexturePool);
		ms[0] = 0x40;
		exe_GlobalMemoryStatusEx(ms);
		/* ullTotalVirtual - ullAvailVirtual */
		used = ((uint64_t)ms[11] << 32 | ms[10]) - ((uint64_t)ms[13] << 32 | ms[12]);
		Font_Printf(g_FntComic14, 10.0f, 96.0f, 2, "virtual memory used: %4d MB", (uint32_t)(used >> 20));
		mode = g_ProfMode;
	}
	if (mode == 4) {
		Motor_DrawFpsGraph(160.0f);
		Font_PrintfXY(g_FntComic14, 10.0f, 32.0f, "%.2f MTri/s  %.2f MVert/s",
			(double)((float)g_RenderCounts[0] * g_Fps) / 1000000.0, (double)((float)g_RenderCounts[1] * g_Fps) / 1000000.0);
		mode = g_ProfMode;
	}
	if (mode == 5) {
		now = g_TimeMsF - g_ProfResetTime > 1000.0f;
		reset = g_ProfNoReset ? now : 1;
		if (now) {
			g_ProfResetTime = g_TimeMsF;
		}
		/* the time of this page is left out of the frame section */
		g_ProfTicks[PROF_FRAME3D] += __builtin_ia32_rdtsc();
		Font_PrintfXY(g_FntComic14, 10.0f, 112.0f, "CPU Mhz: %.2f", X87_Div((double)Tsc_CpuFreq(0), 1000000.0));
		for (i = 0; i < (int)(sizeof(g_ProfLines) / sizeof(g_ProfLines[0])); i++) {
			cnt = Prof_GetCount(g_ProfLines[i].section, reset);
			mx = Prof_GetMaxTime(g_ProfLines[i].section, now);
			tm = Prof_GetTime(g_ProfLines[i].section, reset);
			Font_PrintfXY(g_FntComic14, 10.0f, g_ProfLines[i].y, g_ProfLines[i].fmt, (double)tm, (double)mx, cnt);
		}
		Font_PrintfXY(g_FntTerm8, 10.0f, 310.0f, "ncomp: %-5d  hit: %-5d  miss: %-5d  peak: %-5d  total: %-5d",
			g_CollisionNComp, g_CollisionCacheHit, g_CollisionCacheMiss, g_CollisionTestsMax, g_CollisionTestsTotal);
		g_CollisionTestsTotal = 0;
		g_CollisionTestsMax = 0;
		g_CollisionCacheMiss = 0;
		g_CollisionCacheHit = 0;
		g_ProfTicks[PROF_FRAME3D] -= __builtin_ia32_rdtsc();
		mode = g_ProfMode;
	}
	Texto_Printf(10, mode == 2 ? 4 : 20, NULL);
	if (g_ProfMode == 4 && g_TexCached) {
		Font_PrintfXY(g_FntComic14, 10.0f, 192.0f, "%d %d %d  %.1f Kb", g_TexCached, g_TexUsed, g_TexCreated,
			(double)g_TexUsedPixels * 0.0009765625);
		g_TexCached = 0;
		g_TexUsed = 0;
		g_TexUsedPixels = 0;
	}
}

/* EscenaGlobal: the scene of the world (or its first one, or bmp/vacia.m3d), counted while the level runs */
void __fastcall Motor_ReleaseEscena(byte *self) {
	CEscena *e;

	CHECK(MOTOR_ESCENA(self));
	if (MOTOR_ESCENA(self)) {
		MOTOR_ESCENA(self)->Refs--;
		e = MOTOR_ESCENA(self);
		if (!e->Refs && e) {
			((void *(__thiscall *)(CEscena *, int))e->vftable[0])(e, 1);
		}
		MOTOR_ESCENA(self) = NULL;
	}
}

void __fastcall Motor_GetEscena(byte *self) {
	CMundo *mundo;

	CHECK(!MOTOR_ESCENA(self));
	mundo = Mundo_Get();
	MOTOR_ESCENA(self) = mundo->EscSuelo;
	if (!MOTOR_ESCENA(self)) {
		MOTOR_ESCENA(self) = Escena_Next(NULL);
	}
	if (!MOTOR_ESCENA(self)) {
		MOTOR_ESCENA(self) = Crear_Modelo2("bmp/vacia.m3d", NULL, 2)->escena;
		CHECK(MOTOR_ESCENA(self));
	}
	MOTOR_ESCENA(self)->Refs++;
}

/* ---- m3d.ini: the variables of the engine, in the order of the original ---- */

enum { INI_BOOL, INI_LONG, INI_FLOAT, INI_TEXT };
#define INI_TEXT_SIZE 0x40

static const struct {
	byte kind;
	const char *sect;
	const char *key;
	uint32_t rva;
} motorIni[] = {
	{ INI_BOOL, "video", "TexPersp", 0x431bd0 },
	{ INI_BOOL, "video", "Tramas", 0x431bd4 },
	{ INI_BOOL, "video", "Filtrado", 0x431bc8 },
	{ INI_BOOL, "video", "UsarTexturas", 0x431bcc },
	{ INI_BOOL, "video", "UsarEnvMap", 0x431bd8 },
	{ INI_BOOL, "video", "UsarHWEnvMap", 0x431bdc },
	{ INI_BOOL, "video", "UsarBumpMap", 0x431be0 },
	{ INI_BOOL, "video", "UsarGlowMap", 0x431be4 },
	{ INI_BOOL, "video", "DiffuseMod4x", 0x431be8 },
	{ INI_BOOL, "video", "Sharpen", 0x4b25e8 },
	{ INI_BOOL, "video", "ErrDif", 0x4b25ec },
	{ INI_LONG, "video", "TextBPP", 0x432938 },
	{ INI_LONG, "video", "TextOpBPP", 0x43293c },
	{ INI_BOOL, "video", "gamma2x", 0x4db878 },
	{ INI_BOOL, "video", "sobredib", 0x4a8f2c },
	{ INI_BOOL, "video", "tricolis", 0x4a8f70 },
	{ INI_BOOL, "video", "bufferw", 0x4a8f7c },
	{ INI_FLOAT, "video", "limitaFPS", 0x4a8f8c },
	{ INI_FLOAT, "video", "despMipmap", 0x4ad73c },
	{ INI_BOOL, "video", "LinearMipmap", 0x4a8f88 },
	{ INI_LONG, "video", "MaxAnisoFlt", 0x431d74 },
	{ INI_BOOL, "video", "ConsolaWnd", 0x4ab8f0 },
	{ INI_BOOL, "video", "ConsolaTxt", 0x4ab8f4 },
	{ INI_BOOL, "video", "IlumSpec", 0x4a8f80 },
	{ INI_LONG, "video", "Stencil", 0x4a8f84 },
	{ INI_LONG, "video", "Depth", 0x431cf4 },
	{ INI_FLOAT, "sombras", "sombras", 0x431bb0 },
	{ INI_LONG, "sombras", "objeto_ini", 0x4a8f28 },
	{ INI_LONG, "sombras", "objeto_fin", 0x431bb4 },
	{ INI_BOOL, "sombras", "modif_vnorm", 0x431bb8 },
	{ INI_BOOL, "sombras", "proc_objetos", 0x431bbc },
	{ INI_TEXT, "sombras", "nombre_luz", 0x4a8f30 },
	{ INI_BOOL, "dinput", "raton_excl", 0x431cfc },
	{ INI_BOOL, "dinput", "usar_di", 0x431cf8 },
	{ INI_BOOL, "dinput", "raton_inv_x", 0x4db41c },
	{ INI_BOOL, "dinput", "raton_inv_y", 0x4db420 },
	{ INI_BOOL, "dinput", "raton_inv_z", 0x4db424 },
	{ INI_LONG, "suelo", "suelo_caras_cuadr", 0x431bc0 },
};

/* the ini name (in dir, NULL for the game directory) into the variables */
void Motor_LoadIni(const char *name, const char *dir) {
	char path[MAX_RUTA];
	cArchDatos *dat;
	unsigned i;

	Ruta_Merge(path, dir, name);
	Ruta_Copy(path, path, -1);
	E3D_DebugPrintf("\nINI: %s", path);
	dat = Arch_New();
	if (!Arch_Load(dat, path, Arch_Count(dat))) {
		for (i = 0; i < sizeof(motorIni) / sizeof(motorIni[0]); i++) {
			void *v = Game_Ptr(motorIni[i].rva);

			switch (motorIni[i].kind) {
			case INI_BOOL:
				Dat_GetBool(dat, motorIni[i].sect, motorIni[i].key, v);
				break;
			case INI_LONG:
				Dat_GetLong(dat, motorIni[i].sect, motorIni[i].key, v);
				break;
			case INI_FLOAT:
				Dat_GetFloat(dat, motorIni[i].sect, motorIni[i].key, v);
				break;
			default:
				Dat_GetText(dat, motorIni[i].sect, motorIni[i].key, v, INI_TEXT_SIZE);
				break;
			}
		}
		E3D_LogInfo(g_FileLog, "*INI: [Video,etc.]: %s%s", path, " <ok>");
	} else {
		E3D_DebugPrintf("\nINI: no encontrado");
		E3D_LogInfo(g_FileLog, "*INI: [Video,etc.]: %s%s", path, " <not found>");
		E3D_LogBlue(g_FileLog, "INI: %s%s", path, " <not found>");
	}
	Arch_Free(dat);
}

/* ---- the lights dialog and the scene tree ---- */

#define GWL_HINSTANCE (-6)
#define MOTOR_LIGHTSPROC 0x22dcf0	/* Motor_SceneDlgProc */
#define MOTOR_LIGHTSDIALOG 0x99
#define TVM_INSERTITEM 0x1100
#define MOTOR_VT_PAUSE 0x34

/* the modal dialog of the lights of the global scene, the application paused meanwhile */
void __thiscall CMotorApp_LightsDialog(byte *self, void *hwnd, void *inst) {
	typedef void(__thiscall * pause_t)(byte *, int);

	((pause_t)(*(void ***)self)[MOTOR_VT_PAUSE / 4])(self, 1);
	exe_DialogBoxParamA(inst, (const char *)MOTOR_LIGHTSDIALOG, hwnd, Game_Ptr(MOTOR_LIGHTSPROC),
		(intptr_t)MOTOR_ESCENA(self));
	((pause_t)(*(void ***)self)[MOTOR_VT_PAUSE / 4])(self, 0);
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
#define IDC_ESCENA_INFO 0x3fc
#define IDC_ESCENA_TREE 0x3f7
#define CAMARA_TARGET 0x463
#define RAD_TO_DEG 57.29578f

/* the names of the flags of the nodes (NODO_*), from bit 0 */
static const struct {
	uint32_t bit;
	const char *name;
} g_NodoFlagNames[] = {
	{ 0, " RAIZ" }, { 1, " GRUPO" }, { 2, " SELEC" }, { 3, " OCULTO" }, { 4, " HVISIBLE" },
	{ 5, " NO_CASTSHADOWS" }, { 6, " NO_RCVSHADOWS" }, { 7, " NO_RENDER" }, { 8, " BOXMODE" }, { 12, " COLLIDE" },
	{ 13, " NO_COLLIDE" }, { 16, " NO_ANIMPOS" }, { 17, " NO_TRANS" }, { 18, " SLERP2" }, { 19, " EFFECT" },
	{ 20, " BONE" }, { 21, " BIPED" }, { 22, " NO_TABNODOR" }, { 24, " 2LADOS" }, { 25, " RT_LIGHTING" },
	{ 26, " RT_SHADOWS" }, { 27, " NO_LIGHTMAP" }, { 28, " NO_SECTOR" }, { 29, " AREA_LIGHT" }, { 30, " DUDV_PASS" },
};

static const char *Nodo_RefName(const void *nodo) {
	return nodo ? ((const CNodo3D *)nodo)->name : "(null)";
}

/* the items of a node under its own item: flags, position, rotation, object and the data of its class */
static void Motor_SceneTreeNode(void *tree, TvInsert *tv, CNodo3D *n) {
	const char *f[25];
	vec3_t axis, out, tmp, v;
	const vec3_t *ang;
	matrix_t m;
	float a, rz, rx, ry;
	uint32_t i;

	for (i = 0; i < 25; i++) {
		f[i] = (n->flags >> g_NodoFlagNames[i].bit) & 1 ? g_NodoFlagNames[i].name : "";
	}
	TreeView_InsertItemf(tree, tv, "Flags:%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s", f[0], f[1], f[2], f[3],
		f[4], f[5], f[6], f[7], f[8], f[9], f[10], f[11], f[12], f[13], f[14], f[15], f[16], f[17], f[18], f[19],
		f[20], f[21], f[22], f[23], f[24]);
	TreeView_InsertItemf(tree, tv, "iObjeto: %d", n->indice);
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
	tv->hParent = TreeView_InsertItemf(tree, tv, "obj3D: %s", Obj3D_ClaseName(CNodo3D_Clase(n)));
	tv->hInsertAfter = TVI_LAST;
	tv->mask = TVIF_TEXT | TVIF_PARAM;
	tv->lParam = (intptr_t)n;
	if (CNodo3D_Clase(n) == CLASE_OBJ3D_CAMARA) {
		CCamara *cam = Obj3D_Cast(n->obj3D, CLASE_OBJ3D_CAMARA);

		TreeView_InsertItemf(tree, tv, "Tipo: %s", cam->mode == CAMARA_TARGET ? "Target" : "Free");
		TreeView_InsertItemf(tree, tv, "Pos: %s", Vec3_Str(&cam->pos, 0, 6));
		TreeView_InsertItemf(tree, tv, "Rot: %s", Vec3_StrDeg(&cam->rot, 0, 2));
		TreeView_InsertItemf(tree, tv, "FOV: %f", (double)cam->fov * 180.0 / 3.141592653589793);
		TreeView_InsertItemf(tree, tv, "Clip: %f %f", (double)cam->zNear, (double)cam->zFar);
		TreeView_InsertItemf(tree, tv, "Range: %f %f", (double)cam->f34, (double)cam->f38);
		TreeView_InsertItemf(tree, tv, "Target: %s", Nodo_RefName(cam->nodoObjetivo));
		TreeView_InsertItemf(tree, tv, "LookAt: %s", Vec3_Str(&cam->target, 0, 6));
		TreeView_InsertItemf(tree, tv, "PhysAspect: %f", (double)cam->aspect);
		TreeView_InsertItemf(tree, tv, "ViewAspect: %f", (double)cam->aspectScreen);
	}
	if (CNodo3D_Clase(n) == CLASE_OBJ3D_LUZ) {
		CLuz *l = Obj3D_Cast(n->obj3D, CLASE_OBJ3D_LUZ);

		TreeView_InsertItemf(tree, tv, "Tipo: %s", Luz_TipoStr(l->tipo));
		TreeView_InsertItemf(tree, tv, "Activa: %s", l->activa ? "si" : "no");
		TreeView_InsertItemf(tree, tv, "Sombras: %s", l->sombra ? "si" : "no");
		v = l->d3d.Position;
		TreeView_InsertItemf(tree, tv, "PosRender: %s", Vec3_Str(&v, 0, 6));
		v = l->d3d.Direction;
		TreeView_InsertItemf(tree, tv, "DirRender: %s", Vec3_Str(&v, 0, 6));
		TreeView_InsertItemf(tree, tv, "Color: %s", Motor_ColorStr(l->color));
		TreeView_InsertItemf(tree, tv, "Rango: %f %f", (double)l->f30, (double)l->rango);
		TreeView_InsertItemf(tree, tv, "Objetivo: %s", Nodo_RefName(l->nodoObjetivo));
	}
	if (CNodo3D_Clase(n) == CLASE_OBJ3D_MALLAD3D) {
		CMallaD3D *malla = Obj3D_Cast(n->obj3D, CLASE_OBJ3D_MALLAD3D);
		int count = 0, tris = 0, verts = 0;

		TreeView_InsertItemf(tree, tv, "nTri  nVert  Mat");
		for (; malla; malla = malla->next) {
			count++;
			tris += (int)malla->nTri;
			verts += malla->vert.nVert;
			TreeView_InsertItemf(tree, tv, "%-4d    %-4d    [%s]  (%p)", malla->nTri, malla->vert.nVert,
				malla->mat ? malla->mat->Nombre : "NULL", (void *)malla->mat);
		}
		TreeView_InsertItemf(tree, tv, "Total: %d  (%d tri, %d vert)", count, tris, verts);
	}
}

intptr_t __stdcall Motor_SceneDlgProc(void *hwnd, uint32_t msg, uintptr_t wParam, intptr_t lParam) {
	char buf[0x104];
	CEscena *esc = (CEscena *)(intptr_t)exe_GetWindowLongA(hwnd, GWL_USERDATA);
	CNodo3D *n;
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
		n = *(CNodo3D **)(lParam + 0x5c);
		if (n) {
			Bprintf(buf, 0x100, "%s\niObjeto: %d\niNodeTab: %d\niNodeXRef: %d\nAniMask: %04X\nRefs: %d\nant: %s\nsig: %s",
				n->name, n->indice, n->i48, n->nodoR, (uint16_t)n->i50, n->Refs, Nodo_RefName(n->ant),
				Nodo_RefName(n->sig));
		} else {
			Bprintf(buf, 0x100, "AnimM3D: %s\nFileINI: %s\nCollideMesh: %s\nNodeTab: %s", Motor_BoolStr(esc->AnimM3D != NULL),
				Motor_BoolStr(esc->ini != NULL), Motor_BoolStr(esc->CollideMesh != NULL), "false");
		}
		exe_SetDlgItemTextA(hwnd, IDC_ESCENA_INFO, buf);
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
	esc = (CEscena *)lParam;
	exe_SetWindowLongA(hwnd, GWL_USERDATA, (int32_t)lParam);
	Bprintf(buf, 0x100, "Escena: \"%s\"", esc ? esc->name : "(null)");
	exe_SetWindowTextA(hwnd, buf);
	if (!esc) {
		return 1;
	}
	c[10] = CEscena_CountClase(esc, CLASE_OBJ3D_PORTAL);
	c[9] = CEscena_CountClase(esc, CLASE_OBJ3D_FLARE);
	c[8] = CEscena_CountClase(esc, CLASE_OBJ3D_GRAFICO3D);
	c[7] = CEscena_CountClase(esc, CLASE_OBJ3D_SISTPART);
	c[6] = CEscena_CountClase(esc, CLASE_OBJ3D_SUELO);
	c[5] = CEscena_CountClase(esc, CLASE_OBJ3D_LUZ);
	c[4] = CEscena_CountClase(esc, CLASE_OBJ3D_CAMARA);
	c[3] = CEscena_CountClase(esc, CLASE_OBJ3D_MALLAD3D);
	c[2] = CEscena_CountClase(esc, CLASE_OBJ3D_MALLATRI);
	c[1] = CEscena_CountClase(esc, 0);
	c[0] = CEscena_NumNodos(esc);
	Bprintf(buf, 0x100,
		"%d Nodes:\n  %d Dummies\n  %d TriMesh\n  %d D3DMesh\n  %d Cameras\n  %d Lights\n  %d World\n  %d Particle\n"
		"  %d Sprite3D\n  %d Flares\n  %d Portals",
		c[0], c[1], c[2], c[3], c[4], c[5], c[6], c[7], c[8], c[9], c[10]);
	exe_SetDlgItemTextA(hwnd, IDC_ESCENA_INFO, buf);
	tree = exe_GetDlgItem(hwnd, IDC_ESCENA_TREE);
	tv.hParent = TVI_ROOT;
	tv.hInsertAfter = TVI_LAST;
	tv.mask = TVIF_TEXT | TVIF_PARAM;
	tv.lParam = 0;
	tv.hParent = TreeView_InsertItemf(tree, &tv, "-- Scene Info --");
	TreeView_InsertItemf(tree, &tv, "Ambient: %s", Motor_ColorStr((const byte *)&esc->ambiente));
	TreeView_InsertItemf(tree, &tv, "Backgnd: %s", Motor_ColorStr((const byte *)&esc->niebla));
	s1 = Vec3_Str(&esc->box.max, 1, 0);
	s0 = Vec3_Str(&esc->box.min, 0, 0);
	TreeView_InsertItemf(tree, &tv, "Extens: [%s] - [%s]", s0, s1);
	d.x = esc->box.max.x - esc->box.min.x;
	d.y = esc->box.max.y - esc->box.min.y;
	d.z = esc->box.max.z - esc->box.min.z;
	len = (float)__builtin_sqrt((double)(d.y * d.y + d.x * d.x + d.z * d.z)) * 0.5f;
	TreeView_InsertItemf(tree, &tv, "Dimens: [%s]  %.1f", Vec3_Str(&d, 0, 0), (double)len);
	TreeView_InsertItemf(tree, &tv, "Root: %s", esc->nodo);
	TreeView_InsertItemf(tree, &tv, "MainLOD: %s", esc->mainLOD);
	CEscena_SetNodos54(esc, 0);
	for (n = esc->Nodo3D; n; n = n->sig) {
		tv.hParent = n->padre ? n->padre->i54 : TVI_ROOT;
		tv.hInsertAfter = TVI_LAST;
		tv.mask = TVIF_TEXT | TVIF_STATE | TVIF_PARAM;
		tv.stateMask = TVIS_EXPANDED;
		tv.state = n == esc->Nodo3D ? TVIS_EXPANDED : 0;
		tv.lParam = (intptr_t)n;
		n->i54 = (int)TreeView_InsertItemf(tree, &tv, "%s", n->name);
		tv.hParent = n->i54;
		tv.hInsertAfter = TVI_LAST;
		tv.mask = TVIF_TEXT | TVIF_PARAM;
		tv.lParam = (intptr_t)n;
		Motor_SceneTreeNode(tree, &tv, n);
	}
	return 1;
}

/* the frame time graph of R_Profiler 4 (Motor_DrawStats) */
#define g_FpsText ((char *)Game_Ptr(0x4a9090))	/* 0x40, "frame: fps fps  ms" when the fps jumped */
#define g_FpsTextTime GAME_DATA(float, 0x4a90d0)
#define g_FpsGraph ((float *)Game_Ptr(0x4a90d8))	/* FPS_GRAPH bars of 2 vertices (x, 0) (x, frame ms) */
#define g_FpsGraphPeak ((float *)Game_Ptr(0x4a92b8))	/* the same with the falling peak */
#define g_FpsGraphPos GAME_DATA(int, 0x4a9498)
#define g_FpsScaleTime GAME_DATA(float, 0x4a949c)
#define g_FpsScale GAME_DATA(float, 0x431d00)	/* the height of the graph: 200 ms per 200 fps */
#define FPS_GRAPH 30

void Motor_DrawFpsGraph(float y) {
	CGrafico2D g;
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
	CGrafico2D_ctorTex(&g, NULL, NULL, 0);
	g.x = 10.0f;
	g.y = 460.0f;
	g.escala[0] = 4.0f;
	g.escala[1] = g_FpsScale * -10.0f;
	g.pantalla = 1;
	g.tamPantalla = 1;
	g.sinTextura = 1;
	g.color = 0x800000;
	CGrafico2D_Draw(&g, g_FpsGraphPeak, NULL, NULL, FPS_GRAPH * 2, D3DPT_TRIANGLESTRIP);
	g.color = 0xffffff;
	CGrafico2D_Draw(&g, g_FpsGraph, NULL, NULL, FPS_GRAPH * 2, D3DPT_TRIANGLESTRIP);
	Font_PrintfXY(g_FntComic14, 10.0f, 460.0f, "x%.1f", (double)g_FpsScale);
	CGrafico2D_dtor(&g);
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
void Escena_LogTree(CEscena *esc, CNodo3D *nodo, int indent) {
	CNodo3D *n;

	if (!esc) {
		return;
	}
	if (!nodo) {
		E3D_LogPrint(g_FileLog, "escena: %*s%s", indent, "", esc->name);
		nodo = esc->Nodo3D;
	}
	E3D_LogPrint(g_FileLog, "%*s%s", indent, "", nodo->name);
	n = nodo;
	do {
		if (n->padre == nodo) {
			Escena_LogTree(esc, n, indent + 4);
		}
		n = n->sig;
	} while (n);
}

/* the commands of the debug console of the engine (g_E3DCmdProc) */
#define g_VerUniones GAME_DATA(byte, 0x4ad6f0)	/* the joints of the meshes drawn */
void E3D_Command(char *cmd) {
	char *s;
	CEscena *e;
	void *hwnd;

	E3D_DebugPrintf("\n> cmd: %s", cmd);
	if (!crt_stricmp(cmd, "listar luces")) {
		Luz_List();
		return;
	}
	if (!crt_stricmp(cmd, "listar")) {
		UtilEscena_Dump(NULL);
		return;
	}
	if (!crt_strnicmp(cmd, "arbol ", 6)) {
		/* the tree of the first scene whose file name matches, in the log and in the lights dialog */
		for (s = cmd + 5; *s == ' '; s++) {
		}
		for (e = Escena_Next(NULL); e; e = Escena_Next(e)) {
			if (Wild_Match(s, Ruta_FileName(e->name))) {
				break;
			}
		}
		Escena_LogTree(e, NULL, 0);
		hwnd = MOTOR_HWND(g_Motor);
		exe_CreateDialogParamA((void *)exe_GetWindowLongA(hwnd, GWL_HINSTANCE), (const char *)MOTOR_LIGHTSDIALOG, hwnd,
			Game_Ptr(MOTOR_LIGHTSPROC), (intptr_t)e);
		return;
	}
	if (!crt_stricmp(cmd, "mem")) {
		return;
	}
	if (!crt_stricmp(cmd, "ver uniones")) {
		g_VerUniones = g_VerUniones == 0;
		E3D_DebugPrintf("\nver uniones: %d", g_VerUniones);
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
#define IDM_MUNDO0 0x9c62
#define IDM_TILE 0x9c63
#define IDM_9C64 0x9c64
#define IDM_SHARPEN 0x9c65
#define IDM_RELOADMATS 0x9c66
#define IDM_CAMERA 50000		/* the cameras of the global scene from here */
#define MOTOR_B2A5F8(m) (*(byte *)((m) + 0x2a5f8))
#define MOTOR_B2A5FA(m) (*(byte *)((m) + 0x2a5fa))
#define MOTOR_B2A5FB(m) (*(byte *)((m) + 0x2a5fb))
#define MOTOR_I2A5FC(m) (*(int *)((m) + 0x2a5fc))
#define g_Motor431bae GAME_DATA(byte, 0x431bae)
#define g_Motor4af7d4 GAME_DATA(byte, 0x4af7d4)
#define g_Sharpen GAME_DATA(int, 0x4b25e8)		/* [video] Sharpen of m3d.ini */
#define g_RatonExcl GAME_DATA(int, 0x431cfc)		/* [dinput] raton_excl */

static uint32_t Menu_Check(int on) {
	return on ? MF_CHECKED : 0;
}

/* the menu items greyed and checked by the state */
void __fastcall CMotorApp_UpdateMenu(byte *self) {
	void *menu = MOTOR_MENU(self);

	if (!menu) {
		return;
	}
	exe_EnableMenuItem(menu, IDM_MODE0, g_MipmapsOn == 0);
	exe_EnableMenuItem(MOTOR_MENU(self), IDM_MODE1, g_MipmapsOn == 0);
	exe_EnableMenuItem(MOTOR_MENU(self), IDM_MODE2, g_MipmapsOn == 0);
	exe_CheckMenuItem(MOTOR_MENU(self), IDM_MODE0, Menu_Check(g_MotorMode == 0));
	exe_CheckMenuItem(MOTOR_MENU(self), IDM_MODE1, Menu_Check(g_MotorMode == 1));
	exe_CheckMenuItem(MOTOR_MENU(self), IDM_MODE2, Menu_Check(g_MotorMode == 2));
	exe_EnableMenuItem(MOTOR_MENU(self), IDM_9C5B, !MOTOR_B2A5FA(self) && !MOTOR_B2A5FB(self) ? MF_GRAYED : 0);
	exe_EnableMenuItem(MOTOR_MENU(self), IDM_9C5D, MOTOR_B2A5FA(self) == 0);
	exe_EnableMenuItem(MOTOR_MENU(self), IDM_9C5C, MOTOR_B2A5FB(self) == 0);
	exe_CheckMenuItem(MOTOR_MENU(self), IDM_9C5B, Menu_Check(MOTOR_I2A5FC(self) == 0));
	exe_CheckMenuItem(MOTOR_MENU(self), IDM_9C5D, Menu_Check(MOTOR_I2A5FC(self) == 1));
	exe_CheckMenuItem(MOTOR_MENU(self), IDM_9C5C, Menu_Check(MOTOR_I2A5FC(self) == 1));
	exe_EnableMenuItem(MOTOR_MENU(self), IDM_9C5A, MOTOR_B2A5F8(self) == 0);
	exe_CheckMenuItem(MOTOR_MENU(self), IDM_9C5A, Menu_Check(MOTOR_B2A5F9(self) != 0));
	exe_CheckMenuItem(MOTOR_MENU(self), IDM_SPECULAR, Menu_Check(g_VertexSpecular != 0));
	exe_CheckMenuItem(MOTOR_MENU(self), IDM_RENDERMODE100, Menu_Check(g_RenderMode100 != 0));
	exe_CheckMenuItem(MOTOR_MENU(self), IDM_9C57, Menu_Check(g_Motor431bae != 0));
	exe_CheckMenuItem(MOTOR_MENU(self), IDM_USARVB, Menu_Check(g_UsarVB != 0));
	exe_CheckMenuItem(MOTOR_MENU(self), IDM_MUNDO0, Menu_Check(*(byte *)Mundo_Get() != 0));
	exe_CheckMenuItem(MOTOR_MENU(self), IDM_SOLOB38, Menu_Check(g_RenderSoloB38 != 0));
	exe_CheckMenuItem(MOTOR_MENU(self), IDM_9C64, Menu_Check(g_Motor4af7d4 != 0));
	exe_CheckMenuItem(MOTOR_MENU(self), IDM_SHARPEN, Menu_Check(g_Sharpen != 0));
}

#define MOTOR_SUBMENU(m) (*(void **)((m) + 0x2a5e0))	/* the cameras of the global scene */
#define MOTOR_MAXCAMERAS 0x14
#define MF_BYPOSITION 0x400
#define D3DRS_88 0x88
#define D3DRS_89 0x89
#define DEV_VT_SETRENDERSTATE 0xc8
#define ENGINE_BUILD 0x15f8

/* InitDeviceObjects: the device objects of the engine, the cameras menu, the title, the input on the window */
int __thiscall CMotorApp_InitDeviceObjects(byte *self) {
	typedef int(__stdcall * setrenderstate_t)(void *, uint32_t, uint32_t);
	void *dev = MOTOR_DEV(self);
	char title[0x100];
	int hr;

	g_MotorInits++;
	MOTOR_B2A5F8(self) = (byte)(MOTOR_RASTERCAPS(self) >> 18) & 1;
	Render_IniD3D();
	CMundo_InitD3D(Mundo_Get());
	UtilEscena_InitD3D();
	Stencil_Init();
	Trails_Init();
	LensFlare_Init();
	Fondo2D_Reset();
	CListaMat_Prepare(g_ListaMat, dev);
	Render_SetCamara(Mundo_Get()->cam);
	Render_SetWorldMatrix(g_MatIdentity, 0);
	CEscena_FindClase(MOTOR_ESCENA(self), NULL, CLASE_OBJ3D_LUZ);
	((setrenderstate_t)(*(void ***)dev)[DEV_VT_SETRENDERSTATE / 4])(dev, D3DRS_88, 1);
	((setrenderstate_t)(*(void ***)dev)[DEV_VT_SETRENDERSTATE / 4])(dev, D3DRS_89, 1);
	MOTOR_MENU(self) = exe_GetMenu(MOTOR_HWND(self));
	CMotorApp_UpdateMenu(self);
	CHECK(!MOTOR_TABLA(self)->p);
	MOTOR_TABLA(self)->n = MOTOR_MAXCAMERAS;
	MOTOR_TABLA(self)->p = operator_new_array(MOTOR_MAXCAMERAS * sizeof(void *));
	CHECK(MOTOR_TABLA(self)->p);
	CHECK(MOTOR_TABLA(self)->p);
	memset(MOTOR_TABLA(self)->p, 0, (uint32_t)MOTOR_TABLA(self)->n * sizeof(void *));
	MOTOR_SUBMENU(self) = exe_GetSubMenu(MOTOR_MENU(self), 2);
	if (MOTOR_SUBMENU(self)) {
		CNodo3D *n;
		uint32_t i = 0;

		while (exe_DeleteMenu(MOTOR_SUBMENU(self), 0, MF_BYPOSITION)) {
		}
		for (n = CEscena_FindClase(MOTOR_ESCENA(self), NULL, CLASE_OBJ3D_CAMARA); n;
			n = CEscena_FindClase(MOTOR_ESCENA(self), n, CLASE_OBJ3D_CAMARA)) {
			CCamara *cam;

			if (i == (uint32_t)MOTOR_TABLA(self)->n) {
				exe_AppendMenuA(MOTOR_SUBMENU(self), 0, i + IDM_CAMERA, "...");
				break;
			}
			cam = Obj3D_Cast(n->obj3D, CLASE_OBJ3D_CAMARA);
			if (!MOTOR_TABLA(self)->p || i >= (uint32_t)MOTOR_TABLA(self)->n) {
				CHECK(0);
			}
			((CCamara **)MOTOR_TABLA(self)->p)[i] = cam;
			exe_AppendMenuA(MOTOR_SUBMENU(self), 0, i + IDM_CAMERA, n->name);
			i++;
		}
	}
	Bprintf(title, sizeof(title), "%s (%d)%s - [%s]", MOTOR_TITLE(self), ENGINE_BUILD, "", MOTOR_ESCENA(self)->name);
	if (!MOTOR_WINDOWED(self)) {
		Bprintf(title, sizeof(title), "Scrapland (%d)%s", ENGINE_BUILD, "");
	}
	exe_SetWindowTextA(MOTOR_HWND(self), title);
	{
		typedef int(__thiscall * create_t)(void *, void *, void *, int);
		void **vt = *(void ***)g_Entrada;
		int excl = g_RatonExcl != 0;

		hr = ((create_t)vt[1])(g_Entrada, MOTOR_HWND(self),
			(void *)exe_GetWindowLongA(MOTOR_HWND(self), GWL_HINSTANCE), excl);
	}
	if (hr < 0) {
		return hr;
	}
	((void(__thiscall *)(void *, int))(*(void ***)g_Entrada)[5])(g_Entrada, 1);
	return 0;
}

/* MsgProc: the input first, the keys, the menu commands of the window, then the one of CD3DApplication */
intptr_t __thiscall CMotorApp_MsgProc(byte *self, void *hwnd, uint32_t msg, uintptr_t wParam, intptr_t lParam) {
	if (g_Entrada) {
		((void(__thiscall *)(void *, void *, uint32_t, uintptr_t, intptr_t))(*(void ***)g_Entrada)[7])(g_Entrada, hwnd,
			msg, wParam, lParam);
	}
	if (msg == WM_KEYDOWN) {
		Teclado_KeyDown((uint16_t)wParam, lParam);
	} else if (msg == WM_KEYUP) {
		Teclado_KeyUp((uint16_t)wParam, lParam);
	} else if (msg == WM_COMMAND) {
		uint32_t id = (uint16_t)wParam;

		switch (id) {
		case IDM_MODE0:
		case IDM_MODE1:
		case IDM_MODE2:
			g_MotorMode = (int)(id - IDM_MODE0);
			break;
		case IDM_SPECULAR:
			g_VertexSpecular = g_VertexSpecular == 0;
			break;
		case IDM_RENDERMODE100:
			g_RenderMode100 = g_RenderMode100 == 0;
			break;
		case IDM_9C57:
			g_Motor431bae = g_Motor431bae == 0;
			break;
		case IDM_USARVB:
			g_UsarVB = g_UsarVB == 0;
			break;
		case IDM_9C5A:
			MOTOR_B2A5F9(self) = MOTOR_B2A5F9(self) == 0;
			break;
		case IDM_9C5B:
			MOTOR_I2A5FC(self) = 0;
			break;
		case IDM_9C5C:
		case IDM_9C5D:
			MOTOR_I2A5FC(self) = 1;
			break;
		case IDM_LIGHTS:
			CMotorApp_LightsDialog(self, hwnd, (void *)exe_GetWindowLongA(hwnd, GWL_HINSTANCE));
			break;
		case IDM_SOLOB38:
			g_RenderSoloB38 = g_RenderSoloB38 == 0;
			break;
		case IDM_MUNDO0: {
			byte off = *(byte *)Mundo_GetChecked() == 0;

			*(byte *)Mundo_GetChecked() = off;
			break;
		}
		case IDM_TILE:
			g_TileOn = 1;
			break;
		case IDM_9C64:
			g_Motor4af7d4 = g_Motor4af7d4 == 0;
			break;
		case IDM_SHARPEN:
			g_Sharpen = g_Sharpen == 0;
			[[fallthrough]];
		case IDM_RELOADMATS:
			/* the textures and materials made again */
			Textura_ReleaseAllD3D();
			CListaMat_Release(g_ListaMat);
			CListaMat_Prepare(g_ListaMat, MOTOR_DEV(self));
			break;
		default: {
			/* a camera of the global scene into the world, the menu item checked */
			int i = (int)id - IDM_CAMERA;

			if (i >= 0 && i < MOTOR_TABLA(self)->n) {
				CCamara **cams;
				int k;

				for (k = 0; k < MOTOR_TABLA(self)->n; k++) {
					exe_CheckMenuItem(MOTOR_MENU(self), (uint32_t)(k + IDM_CAMERA), i == k ? MF_CHECKED : 0);
				}
				if (!MOTOR_TABLA(self)->p || (uint32_t)i >= (uint32_t)MOTOR_TABLA(self)->n) {
					CHECK(0);
				}
				cams = MOTOR_TABLA(self)->p;
				CMundo_SetCamara(Mundo_Get(), cams[i]);
			}
			break;
		}
		}
		CMotorApp_UpdateMenu(self);
	}
	return CD3DApp_MsgProc(self, hwnd, msg, wParam, lParam);
}

/* ---- CEntradaNula: every method of the input does nothing ---- */

void *__thiscall CEntradaNula_Delete(void *self, byte flags) {
	*(void **)self = Game_Ptr(CENTRADANULA_VFTABLE);
	IEntrada_dtor(self);
	if (flags & 1) {
		operator_delete_sized(self, CENTRADANULA_SIZE);
	}
	return self;
}

int __thiscall CEntradaNula_Nothing3(void *self, int a, int b, int c) {
	(void)self;
	(void)a;
	(void)b;
	(void)c;
	return 0;
}

int __thiscall CEntradaNula_Nothing1(void *self, int a) {
	(void)self;
	(void)a;
	return 0;
}

int __thiscall CEntradaNula_Nothing(void *self) {
	(void)self;
	return 0;
}

/* the original leaves eax as it was */
void __thiscall CEntradaNula_Nothing4(void *self, int a, int b, int c, int d) {
	(void)self;
	(void)a;
	(void)b;
	(void)c;
	(void)d;
}
