/*
 * Engine/Motor.cpp of the engine3d: CMotorApp, the D3D application of the engine (a CD3DApplication of the DX8 SDK
 * framework, 0x2a600 bytes, g_Motor3D) with its three callbacks of the game, the global scene of the world and the
 * small methods; CEntradaNula, the input that does nothing. The frame, the device objects, the menu, the dialogs
 * and the ini reader are still the exe's.
 */
#include "scrap/dxutil.h"
#include "scrap/e3dutil.h"
#include "scrap/app.h"
#include "scrap/archio.h"
#include "scrap/flare.h"
#include "scrap/fuentes.h"
#include "scrap/material.h"
#include "scrap/nodefx.h"
#include "scrap/render.h"
#include "scrap/stencil.h"
#include "scrap/textura.h"
#include "scrap/trails.h"
#include "scrap/modelo.h"
#include "scrap/mundo.h"
#include "scrap/nodo3d.h"
#include "scrap/scrapmain.h"
#include "scrap/tabla.h"
#include "scrap/varios.h"

#define CMOTORAPP_VFTABLE 0x3d59e0
#define CENTRADANULA_VFTABLE 0x3d5934
#define CMOTORAPP_SIZE 0x2a600
#define CENTRADANULA_SIZE 0x114

#define MOTOR_TITLE(m) (*(const char **)((m) + 0x2a5ac))
#define MOTOR_I2A5B0(m) (*(int *)((m) + 0x2a5b0))
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

/* the console command handler of the engine (rva 0x22eda0, still the exe's) */
#define MOTOR_CMDPROC 0x22eda0
#define g_E3DCmdProc GAME_DATA(void *, 0x4ab8f8)
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

/* of the 0x2a3000.. part of the engine (still the exe's) */
void *__fastcall IEntrada_ctor(void *self);		/* rva 0x2a39d0 */
void *__fastcall MotorOverlay_ctor(void *self);		/* rva 0x2a4e90 */
void __fastcall MotorOverlay_dtor(void *self);		/* rva 0x2a4ea0 */
void Window_SetTextf(void *hwnd, const char *fmt, ...);	/* rva 0x2a5cc0 */
int LensFlares_ReleaseD3D(void);			/* rva 0x2a6050, 0 */
void LensFlares_Off(void);				/* rva 0x2a5fe0 */
void LensFlares_FreeSprite(void);			/* rva 0x2a56f0 */
int LensFlares_Init(void);				/* rva 0x2a6140 */
/* the base class of the inputs (rva 0x2a3a40) */
void __fastcall IEntrada_dtor(void *self);

byte *__fastcall CMotorApp_ctor(byte *self) {
	CD3DApp_ctor(self);
	*(void **)self = Game_Ptr(CMOTORAPP_VFTABLE);
	MOTOR_TABLA(self)->p = NULL;
	MOTOR_TABLA(self)->n = 0;
	MOTOR_TITLE(self) = "ScrapEngine [DX8]";
	MOTOR_I2A5B0(self) = 1;
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
	MOTOR_OVERLAY(self) = mem ? MotorOverlay_ctor(mem) : NULL;
	Fonts_Init();
	Trails_InitLists();
	LensFlares_Init();
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
			Window_SetTextf(MOTOR_HWND(self), "Scrapland dedicated server");
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
	LensFlares_ReleaseD3D();
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
		MotorOverlay_dtor(ov);
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
	LensFlares_Off();
	LensFlares_FreeSprite();
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
