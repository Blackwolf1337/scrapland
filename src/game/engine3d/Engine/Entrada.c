/*
 * The mouse and keyboard of the engine3d, original source Engine/entrada.cpp (include/scrap/entrada.h).
 */
#include <string.h>

#include "scrap/d3d8.h"
#include "scrap/entrada.h"
#include "scrap/win32.h"

/* dinput8 and the cursor through the import table of the exe */
typedef int(__stdcall *dinputcreate_t)(void *hinst, uint32_t version, const void *iid, void **out, void *outer);
typedef int(__stdcall *cursorpos_t)(int32_t *pt);
typedef int(__stdcall *setcursorpos_t)(int x, int y);
#define exe_DirectInput8Create GAME_DATA(dinputcreate_t, 0x3a1064)
#define exe_GetCursorPos GAME_DATA(cursorpos_t, 0x3a1384)
#define exe_SetCursorPos GAME_DATA(setcursorpos_t, 0x3a13b0)

#define IID_IDirectInput8A Game_Ptr(0x3d116c)
#define GUID_SysMouse Game_Ptr(0x3d100c)
#define GUID_SysKeyboard Game_Ptr(0x3d0ffc)
#define c_dfDIMouse Game_Ptr(0x3cfdf4)
#define c_dfDIKeyboard Game_Ptr(0x3cfbec)
#define DIRECTINPUT_VERSION 0x800
#define DISCL_EXCLUSIVE_FOREGROUND 5
#define DISCL_NONEXCLUSIVE_FOREGROUND 6
#define DIERR_INPUTLOST ((int)0x8007001e)
#define DIERR_OTHERAPPHASPRIO ((int)0x80070005)

/* IDirectInput8 and IDirectInputDevice8 */
#define DI_CREATEDEVICE 0xc
#define DIDEV_SETDATAFORMAT 0x2c
#define DIDEV_SETCOOPERATIVELEVEL 0x34
#define DIDEV_ACQUIRE 0x1c
#define DIDEV_UNACQUIRE 0x20
#define DIDEV_GETDEVICESTATE 0x24
typedef int(__stdcall *diCreateDevice_t)(void *di, const void *guid, void **dev, void *outer);
typedef int(__stdcall *diSetDataFormat_t)(void *dev, const void *fmt);
typedef int(__stdcall *diSetCooperativeLevel_t)(void *dev, void *hwnd, uint32_t flags);
typedef int(__stdcall *diDevFn_t)(void *dev);
typedef int(__stdcall *diGetDeviceState_t)(void *dev, uint32_t size, void *data);

#define WM_ACTIVATE 0x6
#define WM_LBUTTONDOWN 0x201
#define WM_LBUTTONUP 0x202
#define WM_RBUTTONDOWN 0x204
#define WM_RBUTTONUP 0x205
#define WM_MBUTTONDOWN 0x207
#define WM_MBUTTONUP 0x208
#define WM_ENTERMENULOOP 0x211
#define WM_EXITMENULOOP 0x212
#define WM_ENTERSIZEMOVE 0x231
#define WM_EXITSIZEMOVE 0x232

typedef int(__thiscall *entradaAcquire_t)(IEntrada *self, byte on);

/* the part of the constructors of IEntrada: one at a time, the state cleared */
static void IEntrada_Init(IEntrada *self) {
	self->vftable = Game_Ptr(IENTRADA_VFTABLE);
	CHECK(!g_EntradaInic);
	g_EntradaInic = 1;
	memset(self->mouse, 0, sizeof(*self) - offsetof(IEntrada, mouse));
}

IEntrada *__fastcall IEntrada_ctor(IEntrada *self) {
	IEntrada_Init(self);
	return self;
}

void __fastcall IEntrada_dtor(IEntrada *self) {
	self->vftable = Game_Ptr(IENTRADA_VFTABLE);
	g_EntradaInic = 0;
}

static void *Entrada_Delete(IEntrada *self, byte flags, uint32_t size) {
	IEntrada_dtor(self);
	if (flags & 1) {
		operator_delete_sized(self, size);
	}
	return self;
}

IEntrada *__thiscall IEntrada_Delete(IEntrada *self, byte flags) {
	return Entrada_Delete(self, flags, sizeof(IEntrada));
}

static void IEntrada_AcquireMouse(IEntrada *self, byte on) {
	((entradaAcquire_t)self->vftable[ENTRADA_VF_ACQUIREMOUSE])(self, on);
}

void __thiscall IEntrada_WndProc(IEntrada *self, void *hwnd, uint32_t msg, uint32_t wparam, uint32_t lparam) {
	(void)lparam;
	switch (msg) {
	case WM_ACTIVATE:
		/* WA_ACTIVE or WA_CLICKACTIVE and not minimized */
		IEntrada_AcquireMouse(self, (uint16_t)wparam && !(wparam >> 16));
		break;
	case WM_ENTERMENULOOP:
	case WM_ENTERSIZEMOVE:
		IEntrada_AcquireMouse(self, 0);
		break;
	case WM_EXITMENULOOP:
	case WM_EXITSIZEMOVE:
		if (exe_GetActiveWindow() != hwnd && exe_IsIconic(hwnd)) {
			IEntrada_AcquireMouse(self, 0);
		} else {
			IEntrada_AcquireMouse(self, 1);
		}
		break;
	}
}

/* ---- CEntradaDInput ---- */

CEntradaDInput *__fastcall CEntradaDInput_ctor(CEntradaDInput *self) {
	IEntrada_Init(&self->base);
	self->base.vftable = Game_Ptr(CENTRADADINPUT_VFTABLE);
	self->di = NULL;
	self->mouse = NULL;
	self->keyboard = NULL;
	return self;
}

CEntradaDInput *__thiscall CEntradaDInput_Delete(CEntradaDInput *self, byte flags) {
	return Entrada_Delete(&self->base, flags, sizeof(CEntradaDInput));
}

/* the device of guid with its data format and cooperative level */
static int Entrada_CreateDevice(CEntradaDInput *self, const void *guid, void **dev, const void *fmt, void *hwnd,
	uint32_t coop) {
	int hr = D3D_METHOD(self->di, diCreateDevice_t, DI_CREATEDEVICE)(self->di, guid, dev, NULL);
	if (hr < 0) {
		return hr;
	}
	hr = D3D_METHOD(*dev, diSetDataFormat_t, DIDEV_SETDATAFORMAT)(*dev, fmt);
	if (hr < 0) {
		return hr;
	}
	return D3D_METHOD(*dev, diSetCooperativeLevel_t, DIDEV_SETCOOPERATIVELEVEL)(*dev, hwnd, coop);
}

int __thiscall CEntradaDInput_Init(CEntradaDInput *self, void *hwnd, void *hinst, byte exclusive) {
	int hr = exe_DirectInput8Create(hinst, DIRECTINPUT_VERSION, IID_IDirectInput8A, &self->di, NULL);
	if (hr < 0) {
		return hr;
	}
	hr = Entrada_CreateDevice(self, GUID_SysMouse, &self->mouse, c_dfDIMouse, hwnd,
		exclusive ? DISCL_EXCLUSIVE_FOREGROUND : DISCL_NONEXCLUSIVE_FOREGROUND);
	if (hr < 0) {
		return hr;
	}
	hr = Entrada_CreateDevice(self, GUID_SysKeyboard, &self->keyboard, c_dfDIKeyboard, hwnd,
		DISCL_EXCLUSIVE_FOREGROUND);
	return hr < 0 ? hr : 0;
}

/* unacquired and released */
static void Entrada_ReleaseDevice(void **dev) {
	if (*dev) {
		D3D_METHOD(*dev, diDevFn_t, DIDEV_UNACQUIRE)(*dev);
		COM_RELEASE(*dev);
		*dev = NULL;
	}
}

int __fastcall CEntradaDInput_Release(CEntradaDInput *self) {
	Entrada_ReleaseDevice(&self->keyboard);
	Entrada_ReleaseDevice(&self->mouse);
	if (self->di) {
		COM_RELEASE(self->di);
		self->di = NULL;
	}
	return 0;
}

/* DIMOUSESTATE */
typedef struct DIMouseState {
	int32_t lX, lY, lZ;
	byte rgbButtons[4];
} DIMouseState;

int __thiscall CEntradaDInput_ReadMouse(CEntradaDInput *self, int32_t *out) {
	DIMouseState st;
	int hr;

	if (!self->mouse) {
		return 0;
	}
	/* after a lost device the state is not read again once it is acquired, as in the original */
	for (;;) {
		hr = D3D_METHOD(self->mouse, diGetDeviceState_t, DIDEV_GETDEVICESTATE)(self->mouse, sizeof(st), &st);
		if (hr != DIERR_INPUTLOST) {
			break;
		}
		hr = D3D_METHOD(self->mouse, diDevFn_t, DIDEV_ACQUIRE)(self->mouse);
		if (hr < 0) {
			return hr;
		}
		if (hr != DIERR_INPUTLOST) {
			break;
		}
	}
	if (hr < 0) {
		return hr;
	}
	self->base.mouse[0] = g_EntradaInvX ? -st.lX : st.lX;
	self->base.mouse[1] = g_EntradaInvY ? -st.lY : st.lY;
	self->base.mouse[2] = g_EntradaInvZ ? -st.lZ : st.lZ;
	memcpy(self->base.buttons, st.rgbButtons, sizeof(self->base.buttons));
	if (out) {
		memcpy(out, (const byte *)self + offsetof(IEntrada, mouse), 16);
	}
	return 0;
}

int __fastcall CEntradaDInput_ReadKeys(CEntradaDInput *self) {
	if (!self->keyboard) {
		return 0;
	}
	int hr = D3D_METHOD(self->keyboard, diGetDeviceState_t, DIDEV_GETDEVICESTATE)(self->keyboard, 0x100,
		self->base.keys);
	if (hr >= 0) {
		return 0;
	}
	/* differs from the original on purpose (no blocking waits): one Acquire, the original tried again at once until
	 * it worked (another app with the focus kept it spinning); the next call tries again */
	hr = D3D_METHOD(self->keyboard, diDevFn_t, DIDEV_ACQUIRE)(self->keyboard);
	if (hr == DIERR_INPUTLOST || hr == DIERR_OTHERAPPHASPRIO) {
		return hr;
	}
	hr = D3D_METHOD(self->keyboard, diGetDeviceState_t, DIDEV_GETDEVICESTATE)(self->keyboard, 0x100,
		self->base.keys);
	return hr < 0 ? hr : 0;
}

static int Entrada_Acquire(void *dev, byte on) {
	if (!dev) {
		return 1;
	}
	if (on) {
		D3D_METHOD(dev, diDevFn_t, DIDEV_ACQUIRE)(dev);
	} else {
		D3D_METHOD(dev, diDevFn_t, DIDEV_UNACQUIRE)(dev);
	}
	return 0;
}

int __thiscall CEntradaDInput_AcquireMouse(CEntradaDInput *self, byte on) {
	return Entrada_Acquire(self->mouse, on);
}

int __thiscall CEntradaDInput_AcquireKeys(CEntradaDInput *self, byte on) {
	return Entrada_Acquire(self->keyboard, on);
}

void __thiscall CEntradaDInput_WndProc(CEntradaDInput *self, void *hwnd, uint32_t msg, uint32_t wparam,
	uint32_t lparam) {
	IEntrada_WndProc(&self->base, hwnd, msg, wparam, lparam);
}

/* ---- CEntradaWindows ---- */

CEntradaWindows *__fastcall CEntradaWindows_ctor(CEntradaWindows *self) {
	IEntrada_Init(&self->base);
	self->base.vftable = Game_Ptr(CENTRADAWINDOWS_VFTABLE);
	self->acquired = 0;
	self->buttons = 0;
	return self;
}

CEntradaWindows *__thiscall CEntradaWindows_Delete(CEntradaWindows *self, byte flags) {
	return Entrada_Delete(&self->base, flags, sizeof(CEntradaWindows));
}

int __thiscall CEntradaWindows_Init(CEntradaWindows *self, void *hwnd, void *hinst, byte exclusive) {
	(void)self;
	(void)hwnd;
	(void)hinst;
	(void)exclusive;
	return 0;
}

int __fastcall CEntradaWindows_Release(CEntradaWindows *self) {
	(void)self;
	return 0;
}

/* the motion since the last read, the cursor put back to the center */
int __thiscall CEntradaWindows_ReadMouse(CEntradaWindows *self, int32_t *out) {
	int32_t pt[2];

	(void)out;
	if (!self->acquired) {
		return 1;
	}
	exe_GetCursorPos(pt);
	exe_SetCursorPos(ENTRADA_CURSOR_CENTER, ENTRADA_CURSOR_CENTER);
	self->base.mouse[0] = g_EntradaInvX ? ENTRADA_CURSOR_CENTER - pt[0] : pt[0] - ENTRADA_CURSOR_CENTER;
	self->base.mouse[1] = g_EntradaInvY ? ENTRADA_CURSOR_CENTER - pt[1] : pt[1] - ENTRADA_CURSOR_CENTER;
	byte b = (byte)self->buttons;
	self->base.mouse[2] = 0;
	self->base.buttons[3] = 0;
	self->base.buttons[0] = b & 1;
	self->base.buttons[1] = b & 2;
	self->base.buttons[2] = b & 4;
	return 0;
}

int __fastcall CEntradaWindows_ReadKeys(CEntradaWindows *self) {
	if (!self->acquired) {
		return 1;
	}
	for (int i = 0; i < 256; i++) {
		self->base.keys[i] = g_TecladoKeys[i] ? 0x80 : 0;
	}
	return 0;
}

int __thiscall CEntradaWindows_AcquireMouse(CEntradaWindows *self, byte on) {
	self->acquired = on;
	if (on) {
		exe_GetCursorPos(g_EntradaCursor);
		exe_SetCursorPos(ENTRADA_CURSOR_CENTER, ENTRADA_CURSOR_CENTER);
	} else {
		exe_SetCursorPos(g_EntradaCursor[0], g_EntradaCursor[1]);
	}
	return 0;
}

int __thiscall CEntradaWindows_AcquireKeys(CEntradaWindows *self, byte on) {
	(void)self;
	(void)on;
	return 0;
}

void __thiscall CEntradaWindows_WndProc(CEntradaWindows *self, void *hwnd, uint32_t msg, uint32_t wparam,
	uint32_t lparam) {
	switch (msg) {
	case WM_LBUTTONDOWN:
		self->buttons |= 1;
		break;
	case WM_LBUTTONUP:
		self->buttons &= ~1u;
		break;
	case WM_RBUTTONDOWN:
		self->buttons |= 2;
		break;
	case WM_RBUTTONUP:
		self->buttons &= ~2u;
		break;
	case WM_MBUTTONDOWN:
		self->buttons |= 4;
		break;
	case WM_MBUTTONUP:
		self->buttons &= ~4u;
		break;
	}
	IEntrada_WndProc(&self->base, hwnd, msg, wparam, lparam);
}
