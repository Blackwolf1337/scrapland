/*
 * Engine/teclado.cpp and Engine/entrada.cpp of the engine3d: the keyboard state the window messages feed (a byte per
 * scan code, extended keys with bit 7, and a ring of the last 32 characters), and IEntrada, the mouse and keyboard of
 * the engine through DirectInput (CEntradaDInput) or the window messages and the cursor (CEntradaWindows). Only one
 * IEntrada may exist at a time.
 */
#ifndef SCRAP_ENTRADA_H
#define SCRAP_ENTRADA_H

#include "scrap/game.h"

/* teclado.cpp */
#define TECLADO_BUF 32
#define g_TecladoKeys (GAME_ARRAY(byte, g_TecladoKeys, 0x4db2d0))		/* 256, 1 while held */
#define g_TecladoBuf (GAME_ARRAY(uint16_t, g_TecladoBuf, 0x4db3d0))	/* TECLADO_BUF characters */
#define g_TecladoBufStart GAME_GLOBAL(int, g_TecladoBufStart, 0x4db410)
#define g_TecladoBufCount GAME_GLOBAL(int, g_TecladoBufCount, 0x4db414)
/* the time of the engine without the pauses (next to teclado.cpp, its source is not known) */
#define g_TiempoPausado GAME_GLOBAL(uint32_t, g_TiempoPausado, 0x4db2c4)	/* the sum of the pauses */
#define g_TiempoReanudado GAME_GLOBAL(int, g_TiempoReanudado, 0x4db2c8)	/* set when a pause ended */
#define g_TiempoPausa GAME_GLOBAL(uint32_t, g_TiempoPausa, 0x4db2cc)	/* timeGetTime at the start of the pause, 0 without */

uint32_t Tiempo_Get(void);
/* on: the pause starts (once), off: it ends */
void Tiempo_Pausa(int on);
void Teclado_Clear(void);
/* the last n (up to 32) characters of the ring into dest, the count */
int Teclado_GetChars(uint16_t *dest, int n);
/* WM_KEYDOWN and WM_KEYUP: the scan code of lparam (right shift as left, extended Ctrl and Alt as normal), the
 * character of a first press into the ring */
void Teclado_KeyDown(uint16_t ch, uint32_t lparam);
void Teclado_KeyUp(uint32_t wparam, uint32_t lparam);

/* entrada.cpp */
#define IENTRADA_VFTABLE GAME_VFTABLE(IEntrada_vftable, 0x3e25ec)
#define CENTRADADINPUT_VFTABLE GAME_VFTABLE(CEntradaDInput_vftable, 0x3e2610)
#define CENTRADAWINDOWS_VFTABLE GAME_VFTABLE(CEntradaWindows_vftable, 0x3e2634)
#define g_EntradaInic GAME_GLOBAL(byte, g_EntradaInic, 0x4db428)
/* the mouse axes inverted */
#define g_EntradaInvX GAME_GLOBAL(int, g_EntradaInvX, 0x4db41c)
#define g_EntradaInvY GAME_GLOBAL(int, g_EntradaInvY, 0x4db420)
#define g_EntradaInvZ GAME_GLOBAL(int, g_EntradaInvZ, 0x4db424)
/* the cursor position while CEntradaWindows holds the mouse */
#define g_EntradaCursor (GAME_ARRAY(int32_t, g_EntradaCursor, 0x432a08))	/* [2], the POINT of GetCursorPos */
#define ENTRADA_CURSOR_CENTER 100

/* the vftable of IEntrada */
enum {
	ENTRADA_VF_DELETE = 0,		/* (byte flags) */
	ENTRADA_VF_INIT = 1,		/* int (void *hwnd, void *hinst, byte exclusive) */
	ENTRADA_VF_RELEASE = 2,		/* int () */
	ENTRADA_VF_READMOUSE = 3,	/* int (int *out) dx, dy, dz and the buttons */
	ENTRADA_VF_READKEYS = 4,	/* int () */
	ENTRADA_VF_ACQUIREMOUSE = 5,	/* int (byte on) */
	ENTRADA_VF_ACQUIREKEYS = 6,	/* int (byte on) */
	ENTRADA_VF_WNDPROC = 7,		/* (void *hwnd, uint32_t msg, uint32_t wparam, uint32_t lparam) */
};

typedef struct IEntrada {
	void **vftable;			/* 0x000 */
	int32_t mouse[3];		/* 0x004 dx, dy, dz */
	byte buttons[4];		/* 0x010 */
	byte keys[256];			/* 0x014 0x80 while held */
} IEntrada;
SD_SIZE(IEntrada, 0x114);

typedef struct CEntradaDInput {
	IEntrada base;
	void *di;			/* 0x114 IDirectInput8 */
	void *mouse;			/* 0x118 IDirectInputDevice8 */
	void *keyboard;			/* 0x11c */
} CEntradaDInput;
SD_SIZE(CEntradaDInput, 0x120);

typedef struct CEntradaWindows {
	IEntrada base;
	byte acquired;			/* 0x114 the mouse is held at the center */
	byte pad_115[3];
	uint32_t buttons;		/* 0x118 1 left, 2 right, 4 middle from the messages */
} CEntradaWindows;
SD_SIZE(CEntradaWindows, 0x11c);

IEntrada *__fastcall IEntrada_ctor(IEntrada *self);
void __fastcall IEntrada_dtor(IEntrada *self);
IEntrada *__thiscall IEntrada_Delete(IEntrada *self, byte flags);
/* WM_ACTIVATE, the menu loops and the size moves acquire or release the mouse */
void __thiscall IEntrada_WndProc(IEntrada *self, void *hwnd, uint32_t msg, uint32_t wparam, uint32_t lparam);

CEntradaDInput *__fastcall CEntradaDInput_ctor(CEntradaDInput *self);
CEntradaDInput *__thiscall CEntradaDInput_Delete(CEntradaDInput *self, byte flags);
int __thiscall CEntradaDInput_Init(CEntradaDInput *self, void *hwnd, void *hinst, byte exclusive);
int __fastcall CEntradaDInput_Release(CEntradaDInput *self);
int __thiscall CEntradaDInput_ReadMouse(CEntradaDInput *self, int32_t *out);
int __fastcall CEntradaDInput_ReadKeys(CEntradaDInput *self);
int __thiscall CEntradaDInput_AcquireMouse(CEntradaDInput *self, byte on);
int __thiscall CEntradaDInput_AcquireKeys(CEntradaDInput *self, byte on);
void __thiscall CEntradaDInput_WndProc(CEntradaDInput *self, void *hwnd, uint32_t msg, uint32_t wparam,
	uint32_t lparam);

CEntradaWindows *__fastcall CEntradaWindows_ctor(CEntradaWindows *self);
CEntradaWindows *__thiscall CEntradaWindows_Delete(CEntradaWindows *self, byte flags);
int __thiscall CEntradaWindows_Init(CEntradaWindows *self, void *hwnd, void *hinst, byte exclusive);
int __fastcall CEntradaWindows_Release(CEntradaWindows *self);
int __thiscall CEntradaWindows_ReadMouse(CEntradaWindows *self, int32_t *out);
int __fastcall CEntradaWindows_ReadKeys(CEntradaWindows *self);
int __thiscall CEntradaWindows_AcquireMouse(CEntradaWindows *self, byte on);
int __thiscall CEntradaWindows_AcquireKeys(CEntradaWindows *self, byte on);
void __thiscall CEntradaWindows_WndProc(CEntradaWindows *self, void *hwnd, uint32_t msg, uint32_t wparam,
	uint32_t lparam);

#endif
