/*
 * Engine/teclado.cpp and Engine/entrada.cpp of the engine3d: the keyboard state the window messages feed (a byte per
 * scan code, extended keys with bit 7, and a ring of the last 32 characters), and IInput, the mouse and keyboard of
 * the engine through DirectInput (CInputDInput) or the window messages and the cursor (CInputWindows). Only one
 * IInput may exist at a time.
 */
#ifndef SCRAP_E3DINPUT_H
#define SCRAP_E3DINPUT_H

#include "scrap/game.h"

/* teclado.cpp */
#define KEYBOARD_BUF 32
#define g_KeyboardKeys (GAME_ARRAY(byte, g_KeyboardKeys, 0x4db2d0))		/* 256, 1 while held */
#define g_KeyboardBuf (GAME_ARRAY(uint16_t, g_KeyboardBuf, 0x4db3d0))	/* KEYBOARD_BUF characters */
#define g_KeyboardBufStart GAME_GLOBAL(int, g_KeyboardBufStart, 0x4db410)
#define g_KeyboardBufCount GAME_GLOBAL(int, g_KeyboardBufCount, 0x4db414)
/* the time of the engine without the pauses (next to teclado.cpp, its source is not known) */
#define g_TimePaused GAME_GLOBAL(uint32_t, g_TimePaused, 0x4db2c4)	/* the sum of the pauses */
#define g_TimeResumed GAME_GLOBAL(int, g_TimeResumed, 0x4db2c8)	/* set when a pause ended */
#define g_TimePause GAME_GLOBAL(uint32_t, g_TimePause, 0x4db2cc)	/* timeGetTime at the start of the pause, 0 without */

uint32_t Time_Get(void);
/* on: the pause starts (once), off: it ends */
void Time_Pause(int on);
void Keyboard_Clear(void);
/* the last n (up to 32) characters of the ring into dest, the count */
int Keyboard_GetChars(uint16_t *dest, int n);
/* WM_KEYDOWN and WM_KEYUP: the scan code of lparam (right shift as left, extended Ctrl and Alt as normal), the
 * character of a first press into the ring */
void Keyboard_KeyDown(uint16_t ch, uint32_t lparam);
void Keyboard_KeyUp(uint32_t wparam, uint32_t lparam);

/* entrada.cpp */
#define IENTRADA_VFTABLE GAME_VFTABLE(IInput_vftable, 0x3e25ec)
#define CENTRADADINPUT_VFTABLE GAME_VFTABLE(CInputDInput_vftable, 0x3e2610)
#define CENTRADAWINDOWS_VFTABLE GAME_VFTABLE(CInputWindows_vftable, 0x3e2634)
#define g_InputInit GAME_GLOBAL(byte, g_InputInit, 0x4db428)
/* the mouse axes inverted */
#define g_InputInvX GAME_GLOBAL(int, g_InputInvX, 0x4db41c)
#define g_InputInvY GAME_GLOBAL(int, g_InputInvY, 0x4db420)
#define g_InputInvZ GAME_GLOBAL(int, g_InputInvZ, 0x4db424)
/* the cursor position while CInputWindows holds the mouse */
#define g_InputCursor (GAME_ARRAY(int32_t, g_InputCursor, 0x432a08))	/* [2], the POINT of GetCursorPos */
#define INPUT_CURSOR_CENTER 100

/* the vftable of IInput */
enum {
	INPUT_VF_DELETE = 0,		/* (byte flags) */
	INPUT_VF_INIT = 1,		/* int (void *hwnd, void *hinst, byte exclusive) */
	INPUT_VF_RELEASE = 2,		/* int () */
	INPUT_VF_READMOUSE = 3,	/* int (int *out) dx, dy, dz and the buttons */
	INPUT_VF_READKEYS = 4,	/* int () */
	INPUT_VF_ACQUIREMOUSE = 5,	/* int (byte on) */
	INPUT_VF_ACQUIREKEYS = 6,	/* int (byte on) */
	INPUT_VF_WNDPROC = 7,		/* (void *hwnd, uint32_t msg, uint32_t wparam, uint32_t lparam) */
};

typedef struct IInput {
	void **vftable;			/* 0x000 */
	int32_t mouse[3];		/* 0x004 dx, dy, dz */
	byte buttons[4];		/* 0x010 */
	byte keys[256];			/* 0x014 0x80 while held */
} IInput;
SD_SIZE(IInput, 0x114);

typedef struct CInputDInput {
	IInput base;
	void *di;			/* 0x114 IDirectInput8 */
	void *mouse;			/* 0x118 IDirectInputDevice8 */
	void *keyboard;			/* 0x11c */
} CInputDInput;
SD_SIZE(CInputDInput, 0x120);

typedef struct CInputWindows {
	IInput base;
	byte acquired;			/* 0x114 the mouse is held at the center */
	byte pad_115[3];
	uint32_t buttons;		/* 0x118 1 left, 2 right, 4 middle from the messages */
} CInputWindows;
SD_SIZE(CInputWindows, 0x11c);

IInput *__fastcall IInput_ctor(IInput *self);
void __fastcall IInput_dtor(IInput *self);
IInput *__thiscall IInput_Delete(IInput *self, byte flags);
/* WM_ACTIVATE, the menu loops and the size moves acquire or release the mouse */
void __thiscall IInput_WndProc(IInput *self, void *hwnd, uint32_t msg, uint32_t wparam, uint32_t lparam);

CInputDInput *__fastcall CInputDInput_ctor(CInputDInput *self);
CInputDInput *__thiscall CInputDInput_Delete(CInputDInput *self, byte flags);
int __thiscall CInputDInput_Init(CInputDInput *self, void *hwnd, void *hinst, byte exclusive);
int __fastcall CInputDInput_Release(CInputDInput *self);
int __thiscall CInputDInput_ReadMouse(CInputDInput *self, int32_t *out);
int __fastcall CInputDInput_ReadKeys(CInputDInput *self);
int __thiscall CInputDInput_AcquireMouse(CInputDInput *self, byte on);
int __thiscall CInputDInput_AcquireKeys(CInputDInput *self, byte on);
void __thiscall CInputDInput_WndProc(CInputDInput *self, void *hwnd, uint32_t msg, uint32_t wparam,
	uint32_t lparam);

CInputWindows *__fastcall CInputWindows_ctor(CInputWindows *self);
CInputWindows *__thiscall CInputWindows_Delete(CInputWindows *self, byte flags);
int __thiscall CInputWindows_Init(CInputWindows *self, void *hwnd, void *hinst, byte exclusive);
int __fastcall CInputWindows_Release(CInputWindows *self);
int __thiscall CInputWindows_ReadMouse(CInputWindows *self, int32_t *out);
int __fastcall CInputWindows_ReadKeys(CInputWindows *self);
int __thiscall CInputWindows_AcquireMouse(CInputWindows *self, byte on);
int __thiscall CInputWindows_AcquireKeys(CInputWindows *self, byte on);
void __thiscall CInputWindows_WndProc(CInputWindows *self, void *hwnd, uint32_t msg, uint32_t wparam,
	uint32_t lparam);

#endif
