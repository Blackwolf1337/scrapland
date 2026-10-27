/*
 * Engine/Motor.cpp of the engine3d: CMotorApp, the D3D application of the engine (a CD3DApplication of the DX8 SDK
 * framework, 0x2a600 bytes, g_Motor3D) with its three callbacks of the game, the global scene of the world and the
 * small methods; CEntradaNula, the input that does nothing. The frame, the device objects, the menu, the dialogs
 * and the ini reader are still the exe's.
 */
#include "scrap/dxutil.h"
#include "scrap/e3dutil.h"
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

/* the ini of the engine into the variables (rva 0x230440) */
void Motor_LoadIni(const char *name, int arg);
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
	Motor_LoadIni(g_M3dIni, 0);
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

/* ConfirmDevice: no device with the pure behaviour 0x10 */
int __thiscall CMotorApp_ConfirmDevice(byte *self, void *caps, uint32_t behavior, int format) {
	(void)self;
	(void)caps;
	(void)format;
	return (byte)(behavior & 0x10) ? (int)0x80004005 : 0;
}

int __thiscall CMotorApp_CountFrame(byte *self) {
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
