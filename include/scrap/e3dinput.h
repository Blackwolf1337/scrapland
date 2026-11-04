/*
 * Engine/teclado.cpp and Engine/entrada.cpp of the engine3d: the keyboard state the window messages feed (a byte per
 * scan code, extended keys with bit 7, and a ring of the last 32 characters), and IInput, the base of the mouse and
 * keyboard of the engine, of which only the input that reads nothing is made (CEntradaNula). Only one IInput may exist
 * at a time.
 */
#ifndef SCRAP_E3DINPUT_H
#define SCRAP_E3DINPUT_H

#include "scrap/game.h"

/* teclado.cpp */
#define KEYBOARD_BUF 32
extern byte g_KeyboardKeys[256];		/* 256, 1 while held */

uint32_t Time_Get(void);
void Keyboard_Clear(void);
/* WM_KEYDOWN and WM_KEYUP: the scan code of lparam (right shift as left, extended Ctrl and Alt as normal), the
 * character of a first press into the ring */
void Keyboard_KeyDown(uint16_t ch, uint32_t lparam);
void Keyboard_KeyUp(uint32_t wparam, uint32_t lparam);

typedef struct IInput IInput;

/* the slots of IInput; only the null input is made (the platform layer reads the devices) */
typedef struct IInputVtbl {
	void (*Free)(IInput *self);						/* 0x00 */
	int(*Init)(IInput *self, void *hwnd, void *hinst, byte exclusive);			/* 0x04 */
	int(*Release)(IInput *self);								/* 0x08 */
	int(*ReadMouse)(IInput *self, int *out);						/* 0x0c dx, dy, dz and the buttons */
	int(*ReadKeys)(IInput *self);								/* 0x10 */
	int(*AcquireMouse)(IInput *self, byte on);						/* 0x14 */
	int(*AcquireKeys)(IInput *self, byte on);						/* 0x18 */
	void(*WndProc)(IInput *self, void *hwnd, uint32_t msg, uint32_t wparam, uint32_t lparam);	/* 0x1c */
} IInputVtbl;

/* calls through the slots of IInputVtbl */
static inline int Input3D_AcquireMouse(void *self, byte on) {
	return VT(IInputVtbl, self)->AcquireMouse(self, on);
}

static inline int Input3D_Release(void *self) {
	return VT(IInputVtbl, self)->Release(self);
}

static inline int Input3D_Init(void *self, void *hwnd, void *hinst, byte exclusive) {
	return VT(IInputVtbl, self)->Init(self, hwnd, hinst, exclusive);
}

static inline void Input3D_Free(void *self) {
	VT(IInputVtbl, self)->Free(self);
}

struct IInput {
	const IInputVtbl *vftable;	/* 0x000 */
	int32_t mouse[3];		/* 0x004 dx, dy, dz */
	byte buttons[4];		/* 0x010 */
	byte keys[256];			/* 0x014 0x80 while held */
};

/* CEntradaNula: the input that reads nothing (g_Input before the real one), no members of its own */
typedef IInput CNullInput;

IInput *IInput_ctor(IInput *self);
void IInput_dtor(IInput *self);
int IInput_AcquireKeysFalse(IInput *self, byte on);
int IInput_AcquireMouseFalse(IInput *self, byte on);
int IInput_InitFalse(IInput *self, void *hwnd, void *hinst, byte exclusive);
int IInput_ReadKeysFalse(IInput *self);
int IInput_ReadMouseFalse(IInput *self, int *out);
int IInput_ReleaseFalse(IInput *self);
void IInput_WndProcNop(IInput *self, void *hwnd, uint32_t msg, uint32_t wparam, uint32_t lparam);

#endif
