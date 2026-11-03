/*
 * Input/PcPad.cpp of the game: four pads in the layout of XInput (from the platform layer), 0x34 entries each. Entries 0..11 the buttons and triggers,
 * 0x20..0x2f the half axes of the sticks, 0x30..0x33 the cross. The default bindings use the entry codes of
 * g_PadCodes and the XB* variables of the pad options. See include/scrap/input.h.
 */
#include <stdio.h>
#include <string.h>

#include "scrap/app.h"
#include "scrap/console.h"
#include "scrap/crt.h"
#include "scrap/global.h"
#include "scrap/input.h"
#include "scrap/mathlib.h"
#include "scrap/platform.h"
#include "scrap/scrapmain.h"
#include "scrap/weapons.h"
#include "scrap/platform.h"
#include "scrap/staticinit.h"

#define CPCPAD_VFTABLE (&cPcPad_vftable)
#define g_MainPcPad (GVar_MainPcPad.value.i)		/* MainPcPad: the pad of player 0, -1 all */
#define g_PadDeadZone (GVar_PCPadDeadZone.value.f)	/* PCPadDeadZone */
#define g_PCPadConnected (GVar_PCPadConnected.value.i)	/* PCPadConnected */
#define g_PadForeground (GVar_PCPadForeground.value.i)	/* PCPadForeground: read only while the window is in front */
#define g_XBCDigitalAsAnalog (GVar_XBCDigitalAsAnalog.value.i)
#define g_XBCameraAutoPad (GVar_XBCameraAutoPad.value.i)
#define g_XBInvertCharYPad (GVar_XBInvertCharYPad.value.i)
#define g_XBInvertLRChar (GVar_XBInvertLRChar.value.i)
#define g_XBInvertLRVehicle (GVar_XBInvertLRVehicle.value.i)
#define g_XBInvertVehicleYPad (GVar_XBInvertVehicleYPad.value.i)
#define g_XBVDigitalAsAnalog (GVar_XBVDigitalAsAnalog.value.i)
#define g_XBVehicleAsFPS (GVar_XBVehicleAsFPS.value.i)
static const cAbstractIDeviceVtbl cPcPad_vftable;
static cGVar GVar_XBVehicleAsFPS;
static cGVar GVar_XBVDigitalAsAnalog;
static cGVar GVar_XBInvertVehicleYPad;
static cGVar GVar_XBInvertLRVehicle;
static cGVar GVar_XBInvertLRChar;
static cGVar GVar_XBInvertCharYPad;
static cGVar GVar_XBCameraAutoPad;
static cGVar GVar_XBCDigitalAsAnalog;
static cGVar GVar_PCPadForeground;
static cGVar GVar_PCPadDeadZone;
static cGVar GVar_PCPadConnected;
static cGVar GVar_MainPcPad;
#define PAD_ERROR "ERROR"

static const char *const pad_labels[0x34] = {
	[0x00] = "A", [0x01] = "B", [0x02] = "X", [0x03] = "Y", [0x04] = "BLACK", [0x05] = "WHITE", [0x06] = "BACK",
	[0x07] = "START", [0x08] = "L_TUMB", [0x09] = "R_TUMB", [0x0a] = "L", [0x0b] = "R", [0x20] = "LXRIGHT",
	[0x21] = "LXLEFT", [0x22] = "LXDOWN", [0x23] = "LXUP", [0x24] = "ZPos", [0x25] = "ZNeg", [0x26] = "RXRIGHT",
	[0x27] = "RXLEFT", [0x28] = "RXDOWN", [0x29] = "RXUP", [0x2a] = "RotZPos", [0x2b] = "RotZNeg",
	[0x2c] = "Sly0Pos", [0x2d] = "Sly0Neg", [0x2e] = "Sly1Pos", [0x2f] = "Sly1Neg", [0x30] = "RIGHT",
	[0x31] = "LEFT", [0x32] = "UP", [0x33] = "DOWN",
};

/* menu (set 0) and debug (2), and the reserved entries of every set: g_PadCodes index, set, action */
static const byte pad_defaults[][3] = {
	{ 3, 0, 5 }, { 2, 0, 4 }, { 1, 0, 3 }, { 0, 0, 2 }, { 8, 0, 5 }, { 7, 0, 4 }, { 6, 0, 3 }, { 5, 0, 2 },
	{ 23, 0, 9 }, { 22, 0, 8 }, { 21, 0, 7 }, { 20, 0, 6 }, { 10, 0, 10 }, { 18, 0, 10 }, { 11, 0, 0 },
	{ 19, 0, 0 }, { 17, 0, 12 }, { 15, 0, 11 }, { 13, 0, 13 }, { 18, 1, 0 }, { 18, 3, 0 }, { 18, 2, 0 },
	{ 19, 3, 23 }, { 19, 1, 39 }, { 19, 2, 0 }, { 3, 2, 4 }, { 8, 2, 4 }, { 23, 2, 4 }, { 2, 2, 5 },
	{ 7, 2, 5 }, { 22, 2, 5 }, { 10, 2, 6 }, { 13, 2, 7 }, { 11, 2, 8 },
};

/* the XInput entries of the pad buttons and half axes in the order of the defaults */
static int g_PadCodes[24] = { 35, 34, 33, 32, 8, 41, 40, 39, 38, 9, 0, 1, 2, 3, 4, 10, 5, 11, 7, 6, 50, 51, 49, 48 };

static void Pad_Bind(cPcPad *pad, int code, int set, int action) {
	cAbstractIDevice_SetEntry(&pad->base, g_PadCodes[code], set, pad->index, action, 0);
}

static cPcPad *cPcPad_ctor(cPcPad *self, int index) {
	cAbstractIDevice_ctor(&self->base);
	self->base.vftable = CPCPAD_VFTABLE;
	self->index = index;
	cAbstractIDevice_InitEntries(&self->base, 0x34);
	cFuncGvarDevices *g = operator_new(sizeof(cFuncGvarDevices));
	self->base.gvar = g ? cFuncGvarDevices_ctor(g, &self->base) : NULL;
	memset(self->f25, 0, sizeof(self->f25));
	return self;
}

static cPcPad g_PcPads[4];

/* the atexit destructor and the static initializer of the four pads; the original has
 * the constructor inlined */
static void PcPad_FiniPads(void) {
	crt_eh_vector_dtor(g_PcPads, sizeof(cPcPad), 4, (void *)cAbstractIDevice_dtor);
}

void PcPad_InitPads(void) {
	for (int i = 0; i < 4; i++) {
		cPcPad_ctor(&g_PcPads[i], i);
	}
	crt_atexit(PcPad_FiniPads);
}

static cAbstractIDevice *cPcPad_Delete(cAbstractIDevice *elem, byte flags) {
	cPcPad *self = (cPcPad *)elem;
	cAbstractIDevice_dtor(&self->base);
	if (flags & 1) {
		operator_delete_sized(self, sizeof(cPcPad));
	}
	return elem;
}

/* the stick inside the dead zone is 0; outside, the length from the dead zone to 1 scaled to 0..1 */
static void Pad_DeadZone(float *x, float *y, float dz) {
	float len = (float)__builtin_sqrt((double)(*x * *x + *y * *y));

	if (dz > len) {
		*x = 0.0f;
		*y = 0.0f;
		return;
	}
	float a = (float)__builtin_fabs((double)len);
	float c = !(dz >= a) ? Float_MinSS(1.0f, a) : dz;
	float k = (c - dz) / (1.0f - dz);
	double ang = (double)(float)atan2(*x, *y);
	*x = (float)sin(ang) * k;
	*y = (float)cos(ang) * k;
}

/* only the bindings of its own player (or none) */
static byte cPcPad_SetEntry(cAbstractIDevice *elem, int entry, int set, int player, int action, byte check) {
	cPcPad *self = (cPcPad *)elem;
	if (player != self->index && player != -1) {
		return 0;
	}
	return cAbstractIDevice_SetEntry(&self->base, entry, set, player, action, check);
}

static void cPcPad_SetVehicleDefaults(cPcPad *self) {
	int a, b;

	if (g_XBCameraAutoPad) {
		a = 13;
		b = 12;
	} else {
		a = 9;
		b = 8;
	}
	Pad_Bind(self, 3, 3, a);
	Pad_Bind(self, 2, 3, b);
	Pad_Bind(self, 1, 3, 7);
	Pad_Bind(self, 0, 3, 6);
	if (g_XBCDigitalAsAnalog) {
		Pad_Bind(self, 23, 3, a);
		Pad_Bind(self, 22, 3, b);
		Pad_Bind(self, 21, 3, 7);
		Pad_Bind(self, 20, 3, 6);
	} else {
		Pad_Bind(self, 21, 3, 22);
		Pad_Bind(self, 20, 3, 21);
	}
	Pad_Bind(self, 8, 3, 5);
	Pad_Bind(self, 7, 3, 4);
	Pad_Bind(self, 6, 3, g_XBInvertCharYPad ? 2 : 3);
	Pad_Bind(self, 5, 3, (g_XBInvertCharYPad != 0) + 2);
	Pad_Bind(self, 11, 3, 19);
	Pad_Bind(self, 13, 3, 18);
	Pad_Bind(self, 10, 3, 17);
	Pad_Bind(self, 12, 3, 21);
	int l = g_PadCodes[15], r = g_PadCodes[17];
	if (g_XBInvertLRChar) {
		l = g_PadCodes[17];
		r = g_PadCodes[15];
	}
	cAbstractIDevice_SetEntry(&self->base, l, 3, self->index, 20, 0);
	cAbstractIDevice_SetEntry(&self->base, r, 3, self->index, g_XBCameraAutoPad ? 14 : 19, 0);
}

static void cPcPad_SetCharDefaults(cPcPad *self) {
	int inv = g_XBInvertVehicleYPad != 0;
	int right, left, down, up, rright, rleft, rdown, rup;

	if (g_XBVehicleAsFPS) {
		right = 11;
		left = 10;
		down = 7;
		up = 6;
		rright = 5;
		rleft = 4;
		rdown = 3 - inv;
		rup = inv + 2;
	} else {
		right = 5;
		left = 4;
		down = 3 - inv;
		up = inv + 2;
		rright = 11;
		rleft = 10;
		rdown = 13;
		rup = 12;
	}
	Pad_Bind(self, 3, 1, right);
	Pad_Bind(self, 2, 1, left);
	Pad_Bind(self, 1, 1, down);
	Pad_Bind(self, 0, 1, up);
	if (g_XBVDigitalAsAnalog) {
		Pad_Bind(self, 23, 1, right);
		Pad_Bind(self, 22, 1, left);
		Pad_Bind(self, 21, 1, down);
		Pad_Bind(self, 20, 1, up);
	} else {
		Pad_Bind(self, 23, 1, 14);
		Pad_Bind(self, 22, 1, 15);
		Pad_Bind(self, 21, 1, 19);
		Pad_Bind(self, 20, 1, 20);
	}
	Pad_Bind(self, 8, 1, rright);
	Pad_Bind(self, 7, 1, rleft);
	Pad_Bind(self, 6, 1, rdown);
	Pad_Bind(self, 5, 1, rup);
	/* the shoulder buttons and triggers (g_PadCodes 14..17), swapped by XBInvertLRVehicle */
	int e = g_PadCodes[17], d = g_PadCodes[15], lb = g_PadCodes[16], c = g_PadCodes[14];
	if (g_XBInvertLRVehicle) {
		e = g_PadCodes[15];
		d = g_PadCodes[17];
		lb = g_PadCodes[14];
		c = g_PadCodes[16];
	}
	if (g_XBVehicleAsFPS) {
		cAbstractIDevice_SetEntry(&self->base, e, 1, self->index, 16, 0);
		cAbstractIDevice_SetEntry(&self->base, d, 1, self->index, 22, 0);
		Pad_Bind(self, 10, 1, 21);
		Pad_Bind(self, 11, 1, 18);
		Pad_Bind(self, 12, 1, 14);
		Pad_Bind(self, 13, 1, 19);
		cAbstractIDevice_SetEntry(&self->base, lb, 1, self->index, 17, 0);
		cAbstractIDevice_SetEntry(&self->base, c, 1, self->index, 9, 0);
	} else {
		cAbstractIDevice_SetEntry(&self->base, e, 1, self->index, 6, 0);
		cAbstractIDevice_SetEntry(&self->base, d, 1, self->index, 22, 0);
		Pad_Bind(self, 10, 1, 16);
		Pad_Bind(self, 11, 1, 9);
		Pad_Bind(self, 12, 1, 14);
		Pad_Bind(self, 13, 1, 17);
		cAbstractIDevice_SetEntry(&self->base, c, 1, self->index, 19, 0);
		cAbstractIDevice_SetEntry(&self->base, lb, 1, self->index, 7, 0);
	}
	Pad_Bind(self, 9, 1, 31);
}

static const char *cPcPad_GetName(cAbstractIDevice *base) {
	cPcPad *self = (cPcPad *)base;
	switch (self->index) {
	case 0:
		return "Pad1";	/* "Pad1" .. "Pad4", 8 bytes apart */
	case 1:
		return "Pad2";
	case 2:
		return "Pad3";
	case 3:
		return "Pad4";
	default:
		CHECK(0);
		return PAD_ERROR;
	}
}

static const char *cPcPad_GetEntryName(cAbstractIDevice *elem, int entry) {
	cPcPad *self = (cPcPad *)elem;
	return self->base.vftable->GetEntryLabel(&self->base, entry);
}

static char g_PadLabelBuf[32];

/* "A" .. "DOWN"; "But13" .. "But32" for the buttons without a name, "Unknow" above */
static const char *cPcPad_GetEntryLabel(cAbstractIDevice *base, int entry) {
	cPcPad *self = (cPcPad *)base;
	(void)self;
	CHECK(entry >= 0);
	if ((uint32_t)entry <= 0x33 && pad_labels[entry]) {
		return pad_labels[entry];
	}
	if (entry < 0x20) {
		snprintf(g_PadLabelBuf, 0x20, "But%d", entry + 1);
		return g_PadLabelBuf;
	}
	memcpy(g_PadLabelBuf, "Unknow", 7);
	return g_PadLabelBuf;
}

static int g_NumPcPads;

/* the four pads into the device list (the static objects, no device check) */
static void cPcPad_SetDefaults(cAbstractIDevice *base);
int Pad_Init(void) {
	g_NumPcPads = 4;
	for (int i = 0; i < g_NumPcPads; i++) {
		cPcPad *p = &g_PcPads[i];
		memset(&p->state, 0, sizeof(p->state));
		p->index = i;
		cInputDevNode *n = DList_NewNode(&g_InputDevices, sizeof(cInputDevNode));
		n->dev = &p->base;
		DList_LinkFirst(&g_InputDevices, n);
		Con_Printf("Input device: %s initialized.\n", cPcPad_GetName(&p->base));
		cPcPad_SetDefaults(&p->base);
	}
	return 0;
}

static void cPcPad_SetDefaults(cAbstractIDevice *base) {
	cPcPad *self = (cPcPad *)base;
	for (uint32_t i = 0; i < sizeof(pad_defaults) / sizeof(pad_defaults[0]); i++) {
		Pad_Bind(self, pad_defaults[i][0], pad_defaults[i][1], pad_defaults[i][2]);
	}
	cPcPad_SetVehicleDefaults(self);
	cPcPad_SetCharDefaults(self);
}

/*
 * A frame of the pad: the sticks, A types the character under the cursor, X deletes, Y ends the line. Pad input
 * switches the mouse camera helpers back on.
 */
static uint32_t Pad_Poll(int index);
static void cPcPad_Update(cAbstractIDevice *elem, float dt) {
	cPcPad *self = (cPcPad *)elem;
	Pad_Poll(self->index);
	float lx = (float)self->state.thumbLX / 32767.0f;
	float ly = (float)self->state.thumbLY / -32767.0f;
	Pad_DeadZone(&lx, &ly, g_PadDeadZone);
	if (lx != 0.0f || ly != 0.0f) {
		g_PlayerInput[self->index].active = 1;
	}
	if (self->state.buttons & 0x1000) {
		cPlayerInput_PressKey(&g_PlayerInput[self->index], self->index);
	}
	if (self->state.buttons & 0x4000) {
		char c = cPlayerInput_GetCursorChar(&g_PlayerInput[self->index]);
		cPlayerInput_SetCursorChar(&g_PlayerInput[self->index], '\b');
		cPlayerInput_PressKey(&g_PlayerInput[self->index], self->index);
		cPlayerInput_SetCursorChar(&g_PlayerInput[self->index], c);
	}
	if ((int16_t)self->state.buttons < 0) {
		cPlayerInput_SetCursorChar(&g_PlayerInput[self->index], '\n');
		cPlayerInput_PressKey(&g_PlayerInput[self->index], self->index);
	}
	float mx = lx * dt * 4.0f;
	float my = ly * dt * 4.0f;
	cPlayerInput_MoveCursor(&g_PlayerInput[self->index], mx, my);
	cPlayerInput_MoveCursor(&g_PlayerInput[self->index], mx, my);
	cAbstractIDevice_SetAxis(&self->base, 0x20, lx);
	cAbstractIDevice_SetAxis(&self->base, 0x22, ly);
	cAbstractIDevice_SetAxis(&self->base, 0x24, 0.0f);
	float rx = (float)self->state.thumbRX / 32767.0f;
	float ry = (float)self->state.thumbRY / -32767.0f;
	Pad_DeadZone(&rx, &ry, g_PadDeadZone);
	cAbstractIDevice_SetAxis(&self->base, 0x26, rx);
	cAbstractIDevice_SetAxis(&self->base, 0x28, ry);
	cAbstractIDevice_SetAxis(&self->base, 0x2a, 0.0f);
	cAbstractIDevice_SetAxis(&self->base, 0x2c, 0.0f);
	cAbstractIDevice_SetAxis(&self->base, 0x2e, 0.0f);
	static const uint16_t bits[10] = { 0x1000, 0x2000, 0x4000, 0x8000, 0x100, 0x200, 0x20, 0x10, 0x40, 0x80 };
	for (int i = 0; i < 10; i++) {
		cAbstractIDevice_SetValue(&self->base, i, (self->state.buttons & bits[i]) ? 1.0f : 0.0f);
	}
	cAbstractIDevice_SetValue(&self->base, 10, (float)self->state.leftTrigger / 255.0f);
	cAbstractIDevice_SetValue(&self->base, 11, (float)self->state.rightTrigger / 255.0f);
	cAbstractIDevice_SetValue(&self->base, 0x32, (self->state.buttons & 1) ? 1.0f : 0.0f);
	cAbstractIDevice_SetValue(&self->base, 0x33, (self->state.buttons & 2) ? 1.0f : 0.0f);
	cAbstractIDevice_SetValue(&self->base, 0x31, (self->state.buttons & 4) ? 1.0f : 0.0f);
	cAbstractIDevice_SetValue(&self->base, 0x30, (self->state.buttons & 8) ? 1.0f : 0.0f);
	if (self->state.buttons) {
		g_PCPadConnected = 1;
	}
	for (int i = 12; i < 0x20; i++) {
		cAbstractIDevice_SetValue(&self->base, i, 0.0f);
	}
	if (self->state.buttons & 0x1000) {
		g_PlayerInput[self->index].active = 1;
	}
	if (!g_IsUsingMouse) {
		return;
	}
	const double lim = (double)0.2f;
	if ((Input_GetActionSet() != 3 && (__builtin_fabs((double)lx) > lim || __builtin_fabs((double)ly) > lim)) ||
		__builtin_fabs((double)rx) > lim || __builtin_fabs((double)ry) > lim || self->state.buttons ||
		self->state.leftTrigger || self->state.rightTrigger) {
		g_IsUsingMouse = 0;
		g_VehicleCenterPadLock = 0.5f;
		g_WalkCameraCenter = 30.0f;
	}
}

/* the state of the pad (MainPcPad >= 0: only that one, as pad 0); 0, else not connected */
static uint32_t Pad_Poll(int index) {
	int i = index;

	if (g_MainPcPad >= 0) {
		if (index != g_MainPcPad) {
			return 0;
		}
		i = 0;
	}
	exe_xinput_state *st = &g_PcPads[i].state;
	memset(st, 0, sizeof(*st));
	if (g_PadForeground && !Plat_HasFocus()) {
		return 0;
	}
	uint32_t r = Plat_PadState(index, st);
	if (i == 0 && r) {
		g_PCPadConnected = 0;
	}
	return r;
}

/* the vtable of cPcPad */
static const cAbstractIDeviceVtbl cPcPad_vftable = {
	.Delete = cPcPad_Delete,
	.SetDefaults = cPcPad_SetDefaults,
	.SetGameDefaults = cAbstractIDevice_SetGameDefaultsNop,
	.GetName = cPcPad_GetName,
	.GetEntryLabel = cPcPad_GetEntryLabel,
	.GetEntryName = cPcPad_GetEntryName,
	.Update = cPcPad_Update,
	.IsReserved = cAbstractIDevice_IsReservedTrue,
	.SetEntry = cPcPad_SetEntry,
};

static cGVar GVar_XBInvertCharYPad;
static cGVar GVar_XBInvertLRChar;
static cGVar GVar_XBCDigitalAsAnalog;
static cGVar GVar_XBVDigitalAsAnalog;
static cGVar GVar_XBInvertLRVehicle;
static cGVar GVar_XBInvertVehicleYPad;
static cGVar GVar_XBCameraAutoPad;
static cGVar GVar_XBVehicleAsFPS;
cGVar GVar_IsUsingMouse;
/* Input/PcPad.cpp: XInput pads */
static cGVar GVar_MainPcPad;
static cGVar GVar_PCPadDeadZone;
static cGVar GVar_PCPadConnected;
static cGVar GVar_PCPadForeground;

/* the dynamic initializers of the globals here, run in the order of src/standalone/crtxc.inc */
INIT_GVAR_INT(Init_GVar_PCPadConnected, &GVar_PCPadConnected, "PCPadConnected", 0, "", 0)
INIT_GVAR_INT(Init_GVar_MainPcPad, &GVar_MainPcPad, "MainPcPad", 0, "-1= all", -1)
INIT_GVAR_INT(Init_GVar_IsUsingMouse, &GVar_IsUsingMouse, "IsUsingMouse", 0, "", 1)
INIT_GVAR_INT(Init_GVar_PCPadForeground, &GVar_PCPadForeground, "PCPadForeground", 4, "", 1)
INIT_GVAR_FLOAT(Init_GVar_PCPadDeadZone, &GVar_PCPadDeadZone, "PCPadDeadZone", 2, "Dead zone for analog controllers.", 0.2f)
INIT_GVAR_INT(Init_GVar_XBInvertCharYPad, &GVar_XBInvertCharYPad, "XBInvertCharYPad", 10, "Invert Y Axis in pad (character)", 0)
INIT_GVAR_INT(Init_GVar_XBCDigitalAsAnalog, &GVar_XBCDigitalAsAnalog, "XBCDigitalAsAnalog", 10, "if true, the cross is digital (character)", 0)
INIT_GVAR_INT(Init_GVar_XBCameraAutoPad, &GVar_XBCameraAutoPad, "XBCameraAutoPad", 10, "if true, the camera automaticaly turns (character)", 0)
INIT_GVAR_INT(Init_GVar_XBInvertLRChar, &GVar_XBInvertLRChar, "XBInvertLRChar", 10, "if true, invert the L&R buttons (character)", 0)
INIT_GVAR_INT(Init_GVar_XBInvertVehicleYPad, &GVar_XBInvertVehicleYPad, "XBInvertVehicleYPad", 10, "Invert Y Axis in pad (vehicle)", 1)
INIT_GVAR_INT(Init_GVar_XBVDigitalAsAnalog, &GVar_XBVDigitalAsAnalog, "XBVDigitalAsAnalog", 10, "if true, the cross is digital (vehicle)", 0)
INIT_GVAR_INT(Init_GVar_XBVehicleAsFPS, &GVar_XBVehicleAsFPS, "XBVehicleAsFPS", 10, "if true, controlled as FPS (vehicle)", 0)
INIT_GVAR_INT(Init_GVar_XBInvertLRVehicle, &GVar_XBInvertLRVehicle, "XBInvertLRVehicle", 10, "if true, invert the L&R buttons (vehicle)", 0)
