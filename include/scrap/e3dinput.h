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
#define g_KeyboardKeys (GAME_ARRAY(byte, g_KeyboardKeys))		/* 256, 1 while held */
#define g_KeyboardBuf (GAME_ARRAY(uint16_t, g_KeyboardBuf))	/* KEYBOARD_BUF characters */
extern int g_KeyboardBufStart;
extern int g_KeyboardBufCount;
/* the time of the engine without the pauses (next to teclado.cpp, its source is not known) */
extern uint32_t g_TimePaused;	/* the sum of the pauses */
extern int g_TimeResumed;	/* set when a pause ended */
extern uint32_t g_TimePause;	/* timeGetTime at the start of the pause, 0 without */

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
#define IINPUT_VFTABLE GAME_VFTABLE(IInput_vftable)
#define CINPUTDINPUT_VFTABLE GAME_VFTABLE(CInputDInput_vftable)
#define CINPUTWINDOWS_VFTABLE GAME_VFTABLE(CInputWindows_vftable)
extern byte g_InputInit;
/* the mouse axes inverted */
extern int g_InputInvX;
extern int g_InputInvY;
extern int g_InputInvZ;
/* the cursor position while CInputWindows holds the mouse */
#define g_InputCursor (GAME_ARRAY(int32_t, g_InputCursor))	/* [2], the POINT of GetCursorPos */
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

/* CEntradaNula: the input that reads nothing (g_Input before the real one), no members of its own */
typedef IInput CNullInput;

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

IInput *IInput_ctor(IInput *self);
void IInput_dtor(IInput *self);
IInput *IInput_Delete(IInput *self, byte flags);
/* WM_ACTIVATE, the menu loops and the size moves acquire or release the mouse */
void IInput_WndProc(IInput *self, void *hwnd, uint32_t msg, uint32_t wparam, uint32_t lparam);

CInputDInput *CInputDInput_ctor(CInputDInput *self);
CInputDInput *CInputDInput_Delete(CInputDInput *self, byte flags);
int CInputDInput_Init(CInputDInput *self, void *hwnd, void *hinst, byte exclusive);
int CInputDInput_Release(CInputDInput *self);
int CInputDInput_ReadMouse(CInputDInput *self, int32_t *out);
int CInputDInput_ReadKeys(CInputDInput *self);
int CInputDInput_AcquireMouse(CInputDInput *self, byte on);
int CInputDInput_AcquireKeys(CInputDInput *self, byte on);
void CInputDInput_WndProc(CInputDInput *self, void *hwnd, uint32_t msg, uint32_t wparam,
	uint32_t lparam);

CInputWindows *CInputWindows_ctor(CInputWindows *self);
CInputWindows *CInputWindows_Delete(CInputWindows *self, byte flags);
int CInputWindows_Init(CInputWindows *self, void *hwnd, void *hinst, byte exclusive);
int CInputWindows_Release(CInputWindows *self);
int CInputWindows_ReadMouse(CInputWindows *self, int32_t *out);
int CInputWindows_ReadKeys(CInputWindows *self);
int CInputWindows_AcquireMouse(CInputWindows *self, byte on);
int CInputWindows_AcquireKeys(CInputWindows *self, byte on);
void CInputWindows_WndProc(CInputWindows *self, void *hwnd, uint32_t msg, uint32_t wparam,
	uint32_t lparam);

#endif
