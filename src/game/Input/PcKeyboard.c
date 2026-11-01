/*
 * Input/PcKeyboard.cpp of the game: the DirectInput keyboard, 256 entries by DIK_ code. See include/scrap/input.h.
 */
#include <stdio.h>
#include <string.h>

#include "scrap/app.h"
#include "scrap/console.h"
#include "scrap/global.h"
#include "scrap/input.h"
#include "scrap/scrapmain.h"
#include "scrap/video.h"

#define GUID_SYSKEYBOARD ((const void *)GAME_PTR(GUID_SYSKEYBOARD, 0))
#define KB_NAME "Kb"
/* DIDEVICEOBJECTINSTANCEA of the last label, its tszName at 0x20 */
extern byte g_KeyObjInfo[316];
extern char g_KeyNameBuf[40];	/* 0x28 bytes */

/* the DIK_ names of the config file (the jump table of the original) */
static const char *const kb_names[0xee] = {
	[0x01] = "ESCAPE", [0x02] = "1", [0x03] = "2", [0x04] = "3", [0x05] = "4", [0x06] = "5", [0x07] = "6",
	[0x08] = "7", [0x09] = "8", [0x0a] = "9", [0x0b] = "0", [0x0c] = "MINUS", [0x0d] = "EQUALS", [0x0e] = "BACK",
	[0x0f] = "TAB", [0x10] = "Q", [0x11] = "W", [0x12] = "E", [0x13] = "R", [0x14] = "T", [0x15] = "Y",
	[0x16] = "U", [0x17] = "I", [0x18] = "O", [0x19] = "P", [0x1a] = "LBRACKET", [0x1b] = "RBRACKET",
	[0x1c] = "RETURN", [0x1d] = "LCONTROL", [0x1e] = "A", [0x1f] = "S", [0x20] = "D", [0x21] = "F", [0x22] = "G",
	[0x23] = "H", [0x24] = "J", [0x25] = "K", [0x26] = "L", [0x27] = "SEMICOLON", [0x28] = "APOSTROPHE",
	[0x29] = "GRAVE", [0x2a] = "LSHIFT", [0x2b] = "BACKSLASH", [0x2c] = "Z", [0x2d] = "X", [0x2e] = "C",
	[0x2f] = "V", [0x30] = "B", [0x31] = "N", [0x32] = "M", [0x33] = "COMMA", [0x34] = "PERIOD", [0x35] = "SLASH",
	[0x36] = "RSHIFT", [0x37] = "MULTIPLY", [0x38] = "LMENU", [0x39] = "SPACE", [0x3a] = "CAPITAL", [0x3b] = "F1",
	[0x3c] = "F2", [0x3d] = "F3", [0x3e] = "F4", [0x3f] = "F5", [0x40] = "F6", [0x41] = "F7", [0x42] = "F8",
	[0x43] = "F9", [0x44] = "F10", [0x45] = "NUMLOCK", [0x46] = "SCROLL", [0x47] = "NUMPAD7", [0x48] = "NUMPAD8",
	[0x49] = "NUMPAD9", [0x4a] = "SUBTRACT", [0x4b] = "NUMPAD4", [0x4c] = "NUMPAD5", [0x4d] = "NUMPAD6",
	[0x4e] = "ADD", [0x4f] = "NUMPAD1", [0x50] = "NUMPAD2", [0x51] = "NUMPAD3", [0x52] = "NUMPAD0",
	[0x53] = "DECIMAL", [0x56] = "OEM_102", [0x57] = "F11", [0x58] = "F12", [0x64] = "F13", [0x65] = "F14",
	[0x66] = "F15", [0x70] = "KANA", [0x73] = "C1", [0x79] = "CONVERT", [0x7b] = "NOCONVERT", [0x7d] = "YEN",
	[0x7e] = "ABNT_C2", [0x8d] = "NUMPADEQUALS", [0x90] = "PREVTRACK", [0x91] = "AT", [0x92] = "COLON",
	[0x93] = "UNDERLINE", [0x94] = "KANJI", [0x95] = "STOP", [0x96] = "AX", [0x97] = "UNLABELED",
	[0x99] = "NEXTTRACK", [0x9c] = "NUMPADENTER", [0x9d] = "RCONTROL", [0xa0] = "MUTE", [0xa1] = "CALCULATOR",
	[0xa2] = "PLAYPAUSE", [0xa4] = "MEDIASTOP", [0xae] = "VOLUMEDOWN", [0xb0] = "VOLUMEUP", [0xb2] = "WEBHOME",
	[0xb3] = "NUMPADCOMMA", [0xb5] = "DIVIDE", [0xb7] = "SYSRQ", [0xb8] = "RMENU", [0xc5] = "PAUSE",
	[0xc7] = "HOME", [0xc8] = "UP", [0xc9] = "PRIOR", [0xcb] = "LEFT", [0xcd] = "RIGHT", [0xcf] = "END",
	[0xd0] = "DOWN", [0xd1] = "NEXT", [0xd2] = "INSERT", [0xd3] = "DELETE", [0xdb] = "LWIN", [0xdc] = "RWIN",
	[0xdd] = "APPS", [0xde] = "POWER", [0xdf] = "SLEEP", [0xe3] = "WAKE", [0xe5] = "WEBSEARCH",
	[0xe6] = "WEBFAVORITES", [0xe7] = "WEBREFRESH", [0xe8] = "WEBSTOP", [0xe9] = "WEBFORWARD", [0xea] = "WEBBACK",
	[0xeb] = "MYCOMPUTER", [0xec] = "MAIL", [0xed] = "MEDIASELECT",
};

/* menu (set 0), debug (2) and the reserved keys of every set */
static const byte kb_defaults[][3] = {
	{ 0xcd, 0, 9 }, { 0xcb, 0, 8 }, { 0xd0, 0, 7 }, { 0xc8, 0, 6 }, { 0x48, 0, 2 }, { 0x50, 0, 3 },
	{ 0x4b, 0, 4 }, { 0x4d, 0, 5 }, { 0x1c, 0, 10 }, { 0x9c, 0, 10 }, { 0x39, 0, 10 }, { 0xc9, 0, 11 },
	{ 0x49, 0, 11 }, { 0x4e, 0, 11 }, { 0xd1, 0, 12 }, { 0x51, 0, 12 }, { 0x4a, 0, 12 }, { 0xd3, 0, 13 },
	{ 0x0e, 0, 13 }, { 0x01, 0, 0 }, { 0x01, 1, 0 }, { 0x01, 2, 0 }, { 0x01, 3, 0 }, { 0x29, 0, 1 },
	{ 0x29, 1, 1 }, { 0x29, 2, 1 }, { 0x29, 3, 1 }, { 0x3b, 3, 23 }, { 0x3b, 1, 39 }, { 0xc8, 2, 2 },
	{ 0xd0, 2, 3 }, { 0x39, 2, 6 },
};

/* character (set 1) and vehicle (3) */
static const byte kb_gameDefaults[][3] = {
	{ 0x32, 1, 8 }, { 0xc8, 1, 2 }, { 0xd0, 1, 3 }, { 0xcb, 1, 4 }, { 0xcd, 1, 5 }, { 0x11, 1, 6 },
	{ 0x1f, 1, 7 }, { 0x1e, 1, 10 }, { 0x20, 1, 11 }, { 0x4a, 1, 12 }, { 0x4e, 1, 13 }, { 0x49, 1, 15 },
	{ 0x47, 1, 14 }, { 0x12, 1, 19 }, { 0x10, 1, 21 }, { 0x1d, 1, 16 }, { 0x39, 1, 17 }, { 0x2a, 1, 18 },
	{ 0xd2, 1, 9 }, { 0x0f, 1, 23 }, { 0x19, 1, 31 }, { 0x02, 1, 24 }, { 0x03, 1, 25 }, { 0x04, 1, 26 },
	{ 0x05, 1, 27 }, { 0x06, 1, 28 }, { 0x07, 1, 29 }, { 0x08, 1, 30 }, { 0x14, 1, 32 }, { 0x3c, 1, 33 },
	{ 0x3d, 1, 34 }, { 0x3e, 1, 35 }, { 0x3f, 1, 36 }, { 0x40, 1, 37 }, { 0x41, 1, 38 }, { 0xc8, 3, 2 },
	{ 0xd0, 3, 3 }, { 0xcb, 3, 4 }, { 0xcd, 3, 5 }, { 0x11, 3, 6 }, { 0x1f, 3, 7 }, { 0x1e, 3, 8 },
	{ 0x20, 3, 9 }, { 0x2a, 3, 15 }, { 0x52, 3, 16 }, { 0x10, 3, 22 }, { 0x12, 3, 21 }, { 0x39, 3, 19 },
};

cPcKeyboard *cPcKeyboard_ctor(cPcKeyboard *self) {
	cAbstractIDevice_ctor(self);
	self->vftable = CPCKEYBOARD_VFTABLE;
	cAbstractIDevice_InitEntries(self, 0x100);
	cPcKeyboard_SetGameDefaults(self);
	cFuncGvarDevices *g = operator_new(sizeof(cFuncGvarDevices));
	self->gvar = g ? cFuncGvarDevices_ctor(g, self) : NULL;
	return self;
}

cPcKeyboard *cPcKeyboard_Delete(cPcKeyboard *self, byte flags) {
	cAbstractIDevice_dtor(self);
	if (flags & 1) {
		operator_delete_sized(self, sizeof(*self));
	}
	return self;
}

void Keyboard_Acquire(byte on) {
	exe_IDirectInputDevice8 *d = g_KeyboardDev;

	if (on) {
		d->lpVtbl->Acquire(d);
	} else {
		d->lpVtbl->Unacquire(d);
	}
}

/* ESCAPE, GRAVE and F1 keep their bindings; check spares the reserved keys too */
byte cPcKeyboard_SetEntry(cPcKeyboard *self, int entry, int set, int player, int action, byte check) {
	if (entry == 0x01 || entry == 0x29 || entry == 0x3b) {
		return 0;
	}
	if (self->vftable->IsReserved(self, entry) && check) {
		return 0;
	}
	return cAbstractIDevice_SetEntry(self, entry, set, player, action, 0);
}

/* WM_CHAR: the character moves the cursor of player 0 and is typed */
void Input_OnChar(int c) {
	if (!g_KeyboardDev) {
		return;
	}
	char ch = (char)c == '\r' ? '\n' : (char)c;
	if (cPlayerInput_SetCursorChar(&g_PlayerInput[0], ch)) {
		cPlayerInput_PressKey(&g_PlayerInput[0], 0);
		g_PlayerInput[0].active = g_IsXbox != 0;
	}
}

void Keyboard_Close(void) {
	exe_IDirectInputDevice8 *d = g_KeyboardDev;

	if (d) {
		d->lpVtbl->Unacquire(d);
		d = g_KeyboardDev;
		d->lpVtbl->Release(d);
		g_KeyboardDev = NULL;
	}
}

const char *cPcKeyboard_GetName(cPcKeyboard *self) {
	(void)self;
	return KB_NAME;
}

/* the DIK_ name; another code is its hex digits and "Code" */
const char *cPcKeyboard_GetEntryName(cPcKeyboard *self, int entry) {
	(void)self;
	if ((uint32_t)(entry - 1) < 0xed && kb_names[entry]) {
		return kb_names[entry];
	}
	snprintf(g_KeyNameBuf, 0x28, "%xCode", (uint32_t)entry);
	return g_KeyNameBuf;
}

/* the name DirectInput gives the key (keyboard layout and language) */
const char *cPcKeyboard_GetEntryLabel(cPcKeyboard *self, int entry) {
	exe_IDirectInputDevice8 *d = g_KeyboardDev;

	(void)self;
	*(uint32_t *)g_KeyObjInfo = 0x13c;
	d->lpVtbl->GetObjectInfo(d, g_KeyObjInfo, entry, 1);	/* DIPH_BYOFFSET */
	return (const char *)g_KeyObjInfo + 0x20;
}

int32_t Keyboard_Create(void) {
	exe_IDirectInput8 *di = g_DInput;
	int32_t hr = di->lpVtbl->CreateDevice(di, GUID_SYSKEYBOARD, &g_KeyboardDev, NULL);

	if (hr < 0) {
		return hr;
	}
	exe_IDirectInputDevice8 *d = g_KeyboardDev;
	hr = d->lpVtbl->SetDataFormat(d, GAME_DF_KEYBOARD);
	if (hr >= 0) {
		const exe_IDirectInputDevice8Vtbl *vt = g_KeyboardDev->lpVtbl;
		void *hwnd = App_GetHWnd();
		hr = vt->SetCooperativeLevel(g_KeyboardDev, hwnd, 0x16);	/* DISCL_NONEXCLUSIVE | FOREGROUND | NOWINKEY */
		if (hr >= 0) {
			return 0;
		}
	}
	g_KeyboardDev = NULL;
	return hr;
}

void cPcKeyboard_SetDefaults(cPcKeyboard *self) {
	Device_SetEntries(self, kb_defaults, sizeof(kb_defaults) / sizeof(kb_defaults[0]));
}

void Keyboard_Init(void) {
	if (Keyboard_Create() < 0) {
		Con_Print(0xff, "ERROR: Keyboard not found.", 0);
		CHECK(0);
		return;
	}
	Con_Printf("Input device: %s initialized.\n", KB_NAME);
	cPcKeyboard_SetDefaults(g_Keyboard);
	cAbstractIDevice *d = g_Keyboard;
	InputDevices_Add(&g_InputDevices, &d);
}

void cPcKeyboard_Update(cPcKeyboard *self, float dt) {
	(void)dt;
	Keyboard_Poll();
	for (int i = 0; i < 0x100; i++) {
		cAbstractIDevice_SetValue(self, i, g_KeyState[i] < 0 ? 1.0f : 0.0f);
	}
}

void cPcKeyboard_SetGameDefaults(cPcKeyboard *self) {
	Device_SetEntries(self, kb_gameDefaults, sizeof(kb_gameDefaults) / sizeof(kb_gameDefaults[0]));
}

/* the key states; a lost device is acquired again */
int32_t Keyboard_Poll(void) {
	exe_IDirectInputDevice8 *d = g_KeyboardDev;
	int32_t hr;

	if (!d) {
		return 0;
	}
	do {
		hr = d->lpVtbl->GetDeviceState(d, 0x100, g_KeyState);
		if (hr == DIERR_INPUTLOST || hr == DIERR_NOTACQUIRED) {
			d = g_KeyboardDev;
			hr = d->lpVtbl->Acquire(d);
			if (hr < 0) {
				return hr;
			}
		}
		d = g_KeyboardDev;
	} while (hr == DIERR_INPUTLOST);
	if (hr < 0) {
		return hr;
	}
	/* differs from the original on purpose (no blocking waits): the keys of a video are ignored here, not waited out
	 * in Video_WaitKeysUp */
	Video_MaskHeldKeys(g_KeyState);
	return 0;
}

/* ESCAPE, GRAVE, F1 and APPS */
byte cPcKeyboard_IsReserved(cPcKeyboard *self, int entry) {
	(void)self;
	return entry == 0x01 || entry == 0x29 || entry == 0x3b || entry == 0xdd;
}
