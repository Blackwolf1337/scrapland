/*
 * The mouse and keyboard of the engine3d, original source Engine/entrada.cpp (include/scrap/e3dinput.h).
 */
#include <string.h>

#include "scrap/d3d8.h"
#include "scrap/e3dinput.h"
#include "scrap/win32.h"

/* the cursor through the import table of the exe (dinput8 in win32.h) */
typedef int(__stdcall *cursorpos_t)(int32_t *pt);
typedef int(__stdcall *setcursorpos_t)(int x, int y);
#define exe_GetCursorPos GAME_IMPORT(cursorpos_t, GetCursorPos, 4)
#define exe_SetCursorPos GAME_IMPORT(setcursorpos_t, SetCursorPos, 8)

#define IID_IDirectInput8A GAME_PTR(IID_IDIRECTINPUT8A, 0)
#define GUID_SysMouse GAME_PTR(GUID_SYSMOUSE, 0)
#define GUID_SysKeyboard GAME_PTR(GUID_SYSKEYBOARD, 0)
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

typedef int(__thiscall *inputAcquire_t)(IInput *self, byte on);

/* the part of the constructors of IInput: one at a time, the state cleared */
static void IInput_Init(IInput *self) {
	self->vftable = IINPUT_VFTABLE;
	CHECK(!g_InputInit);
	g_InputInit = 1;
	memset(self->mouse, 0, sizeof(*self) - offsetof(IInput, mouse));
}

IInput *__fastcall IInput_ctor(IInput *self) {
	IInput_Init(self);
	return self;
}

void __fastcall IInput_dtor(IInput *self) {
	self->vftable = IINPUT_VFTABLE;
	g_InputInit = 0;
}

static void *Input_Delete(IInput *self, byte flags, uint32_t size) {
	IInput_dtor(self);
	if (flags & 1) {
		operator_delete_sized(self, size);
	}
	return self;
}

IInput *__thiscall IInput_Delete(IInput *self, byte flags) {
	return Input_Delete(self, flags, sizeof(IInput));
}

static void IInput_AcquireMouse(IInput *self, byte on) {
	((inputAcquire_t)self->vftable[INPUT_VF_ACQUIREMOUSE])(self, on);
}

void __thiscall IInput_WndProc(IInput *self, void *hwnd, uint32_t msg, uint32_t wparam, uint32_t lparam) {
	(void)lparam;
	switch (msg) {
	case WM_ACTIVATE:
		/* WA_ACTIVE or WA_CLICKACTIVE and not minimized */
		IInput_AcquireMouse(self, (uint16_t)wparam && !(wparam >> 16));
		break;
	case WM_ENTERMENULOOP:
	case WM_ENTERSIZEMOVE:
		IInput_AcquireMouse(self, 0);
		break;
	case WM_EXITMENULOOP:
	case WM_EXITSIZEMOVE:
		if (exe_GetActiveWindow() != hwnd && exe_IsIconic(hwnd)) {
			IInput_AcquireMouse(self, 0);
		} else {
			IInput_AcquireMouse(self, 1);
		}
		break;
	}
}

/* ---- CInputDInput ---- */

CInputDInput *__fastcall CInputDInput_ctor(CInputDInput *self) {
	IInput_Init(&self->base);
	self->base.vftable = CINPUTDINPUT_VFTABLE;
	self->di = NULL;
	self->mouse = NULL;
	self->keyboard = NULL;
	return self;
}

CInputDInput *__thiscall CInputDInput_Delete(CInputDInput *self, byte flags) {
	return Input_Delete(&self->base, flags, sizeof(CInputDInput));
}

/* the device of guid with its data format and cooperative level */
static int Input_CreateDevice(CInputDInput *self, const void *guid, void **dev, const void *fmt, void *hwnd,
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

int __thiscall CInputDInput_Init(CInputDInput *self, void *hwnd, void *hinst, byte exclusive) {
	int hr = exe_DirectInput8Create(hinst, DIRECTINPUT_VERSION, IID_IDirectInput8A, &self->di, NULL);
	if (hr < 0) {
		return hr;
	}
	hr = Input_CreateDevice(self, GUID_SysMouse, &self->mouse, GAME_DF_MOUSE, hwnd,
		exclusive ? DISCL_EXCLUSIVE_FOREGROUND : DISCL_NONEXCLUSIVE_FOREGROUND);
	if (hr < 0) {
		return hr;
	}
	hr = Input_CreateDevice(self, GUID_SysKeyboard, &self->keyboard, GAME_DF_KEYBOARD, hwnd,
		DISCL_EXCLUSIVE_FOREGROUND);
	return hr < 0 ? hr : 0;
}

/* unacquired and released */
static void Input_ReleaseDevice(void **dev) {
	if (*dev) {
		D3D_METHOD(*dev, diDevFn_t, DIDEV_UNACQUIRE)(*dev);
		COM_RELEASE(*dev);
		*dev = NULL;
	}
}

int __fastcall CInputDInput_Release(CInputDInput *self) {
	Input_ReleaseDevice(&self->keyboard);
	Input_ReleaseDevice(&self->mouse);
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

int __thiscall CInputDInput_ReadMouse(CInputDInput *self, int32_t *out) {
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
	self->base.mouse[0] = g_InputInvX ? -st.lX : st.lX;
	self->base.mouse[1] = g_InputInvY ? -st.lY : st.lY;
	self->base.mouse[2] = g_InputInvZ ? -st.lZ : st.lZ;
	memcpy(self->base.buttons, st.rgbButtons, sizeof(self->base.buttons));
	if (out) {
		memcpy(out, (const byte *)self + offsetof(IInput, mouse), 16);
	}
	return 0;
}

int __fastcall CInputDInput_ReadKeys(CInputDInput *self) {
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

static int Input_Acquire(void *dev, byte on) {
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

int __thiscall CInputDInput_AcquireMouse(CInputDInput *self, byte on) {
	return Input_Acquire(self->mouse, on);
}

int __thiscall CInputDInput_AcquireKeys(CInputDInput *self, byte on) {
	return Input_Acquire(self->keyboard, on);
}

void __thiscall CInputDInput_WndProc(CInputDInput *self, void *hwnd, uint32_t msg, uint32_t wparam,
	uint32_t lparam) {
	IInput_WndProc(&self->base, hwnd, msg, wparam, lparam);
}

/* ---- CInputWindows ---- */

CInputWindows *__fastcall CInputWindows_ctor(CInputWindows *self) {
	IInput_Init(&self->base);
	self->base.vftable = CINPUTWINDOWS_VFTABLE;
	self->acquired = 0;
	self->buttons = 0;
	return self;
}

CInputWindows *__thiscall CInputWindows_Delete(CInputWindows *self, byte flags) {
	return Input_Delete(&self->base, flags, sizeof(CInputWindows));
}

int __thiscall CInputWindows_Init(CInputWindows *self, void *hwnd, void *hinst, byte exclusive) {
	(void)self;
	(void)hwnd;
	(void)hinst;
	(void)exclusive;
	return 0;
}

int __fastcall CInputWindows_Release(CInputWindows *self) {
	(void)self;
	return 0;
}

/* the motion since the last read, the cursor put back to the center */
int __thiscall CInputWindows_ReadMouse(CInputWindows *self, int32_t *out) {
	int32_t pt[2];

	(void)out;
	if (!self->acquired) {
		return 1;
	}
	exe_GetCursorPos(pt);
	exe_SetCursorPos(INPUT_CURSOR_CENTER, INPUT_CURSOR_CENTER);
	self->base.mouse[0] = g_InputInvX ? INPUT_CURSOR_CENTER - pt[0] : pt[0] - INPUT_CURSOR_CENTER;
	self->base.mouse[1] = g_InputInvY ? INPUT_CURSOR_CENTER - pt[1] : pt[1] - INPUT_CURSOR_CENTER;
	byte b = (byte)self->buttons;
	self->base.mouse[2] = 0;
	self->base.buttons[3] = 0;
	self->base.buttons[0] = b & 1;
	self->base.buttons[1] = b & 2;
	self->base.buttons[2] = b & 4;
	return 0;
}

int __fastcall CInputWindows_ReadKeys(CInputWindows *self) {
	if (!self->acquired) {
		return 1;
	}
	for (int i = 0; i < 256; i++) {
		self->base.keys[i] = g_KeyboardKeys[i] ? 0x80 : 0;
	}
	return 0;
}

int __thiscall CInputWindows_AcquireMouse(CInputWindows *self, byte on) {
	self->acquired = on;
	if (on) {
		exe_GetCursorPos(g_InputCursor);
		exe_SetCursorPos(INPUT_CURSOR_CENTER, INPUT_CURSOR_CENTER);
	} else {
		exe_SetCursorPos(g_InputCursor[0], g_InputCursor[1]);
	}
	return 0;
}

int __thiscall CInputWindows_AcquireKeys(CInputWindows *self, byte on) {
	(void)self;
	(void)on;
	return 0;
}

void __thiscall CInputWindows_WndProc(CInputWindows *self, void *hwnd, uint32_t msg, uint32_t wparam,
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
	IInput_WndProc(&self->base, hwnd, msg, wparam, lparam);
}
