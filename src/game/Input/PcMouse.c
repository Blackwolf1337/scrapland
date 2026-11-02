/*
 * Input/PcMouse.cpp of the game: the mouse, held by the game as DirectInput's exclusive mouse. Entries 0..3 the buttons, 4..7 the two axes (right,
 * left, down, up), 8 and 9 the wheel. See include/scrap/input.h.
 */
#include <stdio.h>
#include <string.h>

#include "scrap/app.h"
#include "scrap/console.h"
#include "scrap/global.h"
#include "scrap/input.h"
#include "scrap/platform.h"
#include "scrap/scrap.h"
#include "scrap/scrapmain.h"
#include "scrap/weapons.h"
#include "scrap/staticinit.h"

#define MOUSE_NAME "Mouse"

static const char *const mouse_names[6] = { "Right", "Left", "Down", "Up", "WheelUp", "WheelDown" };
static const char *const mouse_labels[6] = { "PC_Mouse_Right", "PC_Mouse_Left", "PC_Mouse_Down", "PC_Mouse_Up",
	"PC_Mouse_WheelUp", "PC_Mouse_WheelDown" };

/* menu (set 0) and debug (2) */
static const byte mouse_defaults[][3] = {
	{ 0x04, 0, 5 }, { 0x05, 0, 4 }, { 0x06, 0, 3 }, { 0x07, 0, 2 }, { 0x08, 0, 11 }, { 0x09, 0, 12 },
	{ 0x00, 0, 10 }, { 0x01, 0, 0 }, { 0x00, 2, 6 },
};

/* character (set 1) and vehicle (3) */
static const byte mouse_gameDefaults[][3] = {
	{ 0x07, 1, 2 }, { 0x06, 1, 3 }, { 0x05, 1, 4 }, { 0x04, 1, 5 }, { 0x00, 1, 16 }, { 0x01, 1, 9 },
	{ 0x02, 1, 17 }, { 0x08, 1, 14 }, { 0x09, 1, 15 }, { 0x07, 3, 2 }, { 0x06, 3, 3 }, { 0x05, 3, 4 },
	{ 0x04, 3, 5 }, { 0x00, 3, 17 }, { 0x01, 3, 18 }, { 0x02, 3, 19 }, { 0x08, 3, 21 }, { 0x09, 3, 22 },
};

cPcMouse *cPcMouse_ctor(cPcMouse *self) {
	cAbstractIDevice_ctor(&self->base);
	self->base.vftable = CPCMOUSE_VFTABLE;
	cAbstractIDevice_InitEntries(&self->base, 10);
	self->move[0] = 0.0f;
	self->move[1] = 0.0f;
	self->time = 0.0f;
	self->speed[0] = 0.0f;
	self->speed[1] = 0.0f;
	cFuncGvarDevices *g = operator_new(sizeof(cFuncGvarDevices));
	self->base.gvar = g ? cFuncGvarDevices_ctor(g, &self->base) : NULL;
	cPcMouse_SetGameDefaults(&self->base);
	return self;
}

cAbstractIDevice *cPcMouse_Delete(cAbstractIDevice *elem, byte flags) {
	cPcMouse *self = (cPcMouse *)elem;
	cAbstractIDevice_dtor(&self->base);
	if (flags & 1) {
		operator_delete_sized(self, sizeof(cPcMouse));
	}
	return elem;
}

void Mouse_Acquire(byte on) {
	Plat_GrabMouse(on);
}

byte cPcMouse_SetEntry(cAbstractIDevice *self, int entry, int set, int player, int action, byte check) {
	if (self->vftable->IsReserved(self, entry) && check) {
		return 0;
	}
	return cAbstractIDevice_SetEntry(self, entry, set, player, action, 0);
}

const char *cPcMouse_GetName(cAbstractIDevice *self) {
	(void)self;
	return MOUSE_NAME;
}

char g_MouseNameBuf[32];

/* "Right" .. "WheelDown", "But1" .. "But4" for the buttons, "Unknow" else */
const char *cPcMouse_GetEntryName(cAbstractIDevice *self, int entry) {
	(void)self;
	CHECK(entry >= 0);
	if ((uint32_t)(entry - 4) <= 5) {
		return mouse_names[entry - 4];
	}
	if (entry >= 4) {
		memcpy(g_MouseNameBuf, "Unknow", 7);
		return g_MouseNameBuf;
	}
	snprintf(g_MouseNameBuf, 0x20, "But%d", entry + 1);
	return g_MouseNameBuf;
}

char g_MouseLabelBuf[32];

const char *cPcMouse_GetEntryLabel(cAbstractIDevice *self, int entry) {
	(void)self;
	CHECK(entry >= 0);
	if ((uint32_t)(entry - 4) <= 5) {
		return Lang_GetStr(mouse_labels[entry - 4]);
	}
	if (entry >= 4) {
		memcpy(g_MouseLabelBuf, "Unknow", 7);
		return Lang_GetStr(g_MouseLabelBuf);
	}
	snprintf(g_MouseLabelBuf, 0x20, "PC_Mouse_Button%d", entry + 1);
	return Lang_GetStr(g_MouseLabelBuf);
}

void cPcMouse_SetDefaults(cAbstractIDevice *self) {
	Device_SetEntries(self, mouse_defaults, sizeof(mouse_defaults) / sizeof(mouse_defaults[0]));
}

cPcMouse g_Mouse;

void Mouse_Init(void) {
	Con_Printf("Input device: %s initialized.\n", MOUSE_NAME);
	cPcMouse_SetDefaults(&g_Mouse.base);
	cAbstractIDevice *d = &g_Mouse.base;
	InputDevices_Add(&g_InputDevices, &d);
}

exe_mouse_state g_MouseState;

/*
 * The speed is the movement of the last 1/30 second (every frame in the menu); the sensitivity counts in the
 * character and vehicle sets only. The text cursor of player 0 moves twice per frame, as the original.
 */
void cPcMouse_Update(cAbstractIDevice *elem, float dt) {
	cPcMouse *self = (cPcMouse *)elem;
	g_MouseState.x = 0;
	Mouse_Poll();
	uint32_t set = Input_GetActionSet();
	float y;
	if (set == 0 || set == 2) {
		self->move[0] = (float)g_MouseState.x / 400.0f + self->move[0];
		y = (float)g_MouseState.y / 400.0f + self->move[1];
	} else {
		self->move[0] = (float)g_MouseState.x / 400.0f * g_MouseSensH + self->move[0];
		if (g_InvertMouse) {
			y = self->move[1] - (float)g_MouseState.y / 400.0f * g_MouseSensV;
		} else {
			y = (float)g_MouseState.y / 400.0f * g_MouseSensV + self->move[1];
		}
	}
	self->move[1] = y;
	float t = dt + self->time;
	self->time = t;
	if (t > 0.0333333351f || set == 0) {
		float mx = self->move[0];
		self->move[0] = 0.0f;
		self->time = 0.0f;
		self->speed[0] = mx / t;
		float my = self->move[1];
		self->move[1] = 0.0f;
		self->speed[1] = my / t;
	}
	float wheel = 0.0f;
	float z = (float)g_MouseState.z;
	if (z != 0.0f) {
		wheel = 0.0f > z ? -1.0f : 1.0f;
	}
	if (g_PlayerInput[0].active) {
		if (g_MouseState.buttons[0] & 0x80) {
			cPlayerInput_PressKey(&g_PlayerInput[0], 0);
		}
		cPlayerInput_MoveCursor(&g_PlayerInput[0], self->speed[0] * dt * 4.0f, self->speed[1] * dt * 4.0f);
		cPlayerInput_MoveCursor(&g_PlayerInput[0], self->speed[0] * dt * 4.0f, self->speed[1] * dt * 4.0f);
	}
	cAbstractIDevice_SetAxis(&self->base, 4, self->speed[0]);
	cAbstractIDevice_SetAxis(&self->base, 6, self->speed[1]);
	cAbstractIDevice_SetAxis(&self->base, 8, wheel);
	for (int i = 0; i < 4; i++) {
		cAbstractIDevice_SetValue(&self->base, i, (int8_t)g_MouseState.buttons[i] < 0 ? 1.0f : 0.0f);
	}
}

void cPcMouse_SetGameDefaults(cAbstractIDevice *self) {
	Device_SetEntries(self, mouse_gameDefaults, sizeof(mouse_gameDefaults) / sizeof(mouse_gameDefaults[0]));
}

/* the state; a mouse let go is held again (DirectInput acquired it again); a moved mouse switches the pad helpers
 * of the cameras off */
int32_t Mouse_Poll(void) {
	if (!g_InputOpen) {
		return 0;
	}
	if (!Plat_MouseGrabbed()) {
		Plat_GrabMouse(1);
	}
	Plat_MouseState(&g_MouseState.x, &g_MouseState.y, &g_MouseState.z, (byte *)g_MouseState.buttons);
	if (g_MouseState.x || g_MouseState.y || g_MouseState.z) {
		g_IsUsingMouse = 1;
		g_VehicleCenterPadLock = 0.0f;
		g_WalkCameraCenter = 0.0f;
	}
	return 0;
}

/* the axes */
byte cPcMouse_IsReserved(cAbstractIDevice *self, int entry) {
	(void)self;
	return (uint32_t)(entry - 4) <= 3;
}

/* the vtable of cPcMouse */
const cAbstractIDeviceVtbl cPcMouse_vftable = {
	.Delete = cPcMouse_Delete,
	.SetDefaults = cPcMouse_SetDefaults,
	.SetGameDefaults = cPcMouse_SetGameDefaults,
	.GetName = cPcMouse_GetName,
	.GetEntryLabel = cPcMouse_GetEntryLabel,
	.GetEntryName = cPcMouse_GetEntryName,
	.Update = cPcMouse_Update,
	.IsReserved = cPcMouse_IsReserved,
	.SetEntry = cPcMouse_SetEntry,
};

cGVar GVar_MouseSensitivityV;

cGVar GVar_MouseSensitivityH;

cGVar GVar_InvertMouse;

/* the dynamic initializers of the globals here, run in the order of src/standalone/crtxc.inc */
INIT_GVAR_FLOAT(Init_GVar_MouseSensitivityH, &GVar_MouseSensitivityH, "MouseSensitivityH", 2, "Horizontal Mouse Sensitivity", 1.0f)
INIT_GVAR_FLOAT(Init_GVar_MouseSensitivityV, &GVar_MouseSensitivityV, "MouseSensitivityV", 2, "Vertical Mouse Sensitivity", 1.0f)
INIT_GVAR_INT(Init_GVar_InvertMouse, &GVar_InvertMouse, "InvertMouse", 2, "0 = Mouse normal... 1 = Mouse inverted.", 0)
FINI_CALL(Fini_cPcMouse_Mouse, &g_Mouse.base, cAbstractIDevice_dtor)
INIT_OBJECT(Init_cPcMouse_Mouse, Fini_cPcMouse_Mouse, &g_Mouse, cPcMouse_ctor)
