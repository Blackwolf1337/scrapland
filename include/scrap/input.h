/*
 * Input: action sets, key definitions and the per player input state (original source: Input/). Up to four
 * local players (split screen), each with an input slot of 0xb4 bytes and a pad of 0x20 bytes.
 *
 * The devices (Input/IDevice.cpp and the PC classes) turn DirectInput and XInput states into entry values. Every
 * entry of a device has one binding per action set: the player and the action of that set it drives. The
 * actions of the current set take the largest value of their entries each frame.
 */
#ifndef SCRAP_INPUT_H
#define SCRAP_INPUT_H

#include "scrap/gvar.h"
#include "scrap/cstring.h"
#include "scrap/dlist.h"
#include "scrap/game.h"
#include "scrap/pyfunction.h"
#include "scrap/scrap.h"

extern cGVarFunction GVar_CheckInactivityFunc;

#define MAX_LOCAL_PLAYERS 4
#define NUM_SETS 4			/* action sets: menu, character, debug, vehicle */

/* an input action of an action set; value[player] is its current state (SScorer.GetMenuAccept) */
typedef struct cInputAction {
	byte pressed[MAX_LOCAL_PLAYERS];	/* 0x00 held over a set change while the value stays above 0.5 */
	char *name;			/* 0x04 cString */
	float prev[MAX_LOCAL_PLAYERS];	/* 0x08 value of the last frame */
	float value[MAX_LOCAL_PLAYERS];	/* 0x18 */
	cPy_Function func[MAX_LOCAL_PLAYERS];	/* 0x28 SInput.Bind, called with the player on the press */
	byte latch;			/* 0x78 keeps pressed over a set change */
	byte pad_79[3];
} cInputAction;
SD_SIZE(cInputAction, 0x7c);
SD_OFFSET(cInputAction, func, 0x28);
SD_OFFSET(cInputAction, latch, 0x78);

typedef TABLE(cInputAction) cInputActionTable;

typedef struct cActionSet {
	char *name;			/* 0x0 */
	cInputActionTable actions;	/* 0x4 */
} cActionSet;
SD_SIZE(cActionSet, 0xc);
typedef TABLE(cActionSet) cActionSetTable;

/* the text input of a player: typed through the virtual keyboard grid, the cursor is a cell of it */
typedef struct cPlayerInput {
	int maxLen;			/* 0x00 0x80 */
	int len;			/* 0x04 */
	char text[0x8c - 0x08];		/* 0x08 SInput.GetString */
	float cursorX;			/* 0x8c SInput.GetCursorChar */
	float cursorY;			/* 0x90 */
	byte keyDown;			/* 0x94 the accept key of this frame */
	byte keyWasDown;		/* 0x95 */
	byte pad_96[2];
	void (*onDone)(int player, const char *text);	/* 0x98 once on the new line */
	cPy_Function func;		/* 0x9c SInput.SetInputFunc */
	byte active;			/* 0xb0 the cursor follows mouse and pad */
	byte pad_b1[3];
} cPlayerInput;
SD_SIZE(cPlayerInput, 0xb4);
SD_OFFSET(cPlayerInput, cursorX, 0x8c);
SD_OFFSET(cPlayerInput, func, 0x9c);
SD_OFFSET(cPlayerInput, active, 0xb0);

/* the rumble state of a player */
typedef struct cPad {
	int index;			/* 0x00 */
	float prev[2];			/* 0x04 left and right motor of the last update */
	float motor[2];			/* 0x0c */
	float time;			/* 0x14 game time of the last update */
	float duration;			/* 0x18 seconds left */
	void (*onRumble)(int index, float left, float right);	/* 0x1c */
} cPad;
SD_SIZE(cPad, 0x20);

typedef struct cVirtualKeyboard {
	char text[0x100];		/* 0x000 the characters of the grid, row by row */
	int a;				/* 0x100 columns (SInput.GetVirtualKeyboard) */
	int b;				/* 0x104 rows */
	int len;			/* 0x108 */
} cVirtualKeyboard;
SD_OFFSET(cVirtualKeyboard, a, 0x100);
SD_OFFSET(cVirtualKeyboard, len, 0x108);

extern cActionSetTable g_ActionSets;
extern cPlayerInput g_PlayerInput[4];	/* [MAX_LOCAL_PLAYERS] */
extern cVirtualKeyboard g_VirtualKeyboard;
extern cPad g_Pads[4];		/* [MAX_LOCAL_PLAYERS] */
extern uint32_t g_CurActionSet;	/* Input_GetActionSet */

/* rva 0x133a30 is cString_EqStr on the name */
static inline byte ActionSet_IsNamed(cActionSet *set, const char *name) {
	return cString_EqStr(&set->name, name);
}
void Input_SelectActionSet(int index);
uint32_t Input_GetActionSet(void);
int Input_AssignEntry(const char *a, const char *b, int c, const char *d, const char *e);
int Input_GetEntry(const char *a, const char *b, const char *c, int *index, char **name);
int Input_ListenToDefine(int player, const char *a, const char *b, const char *c);
void Input_AbortListenToDefine(void);
char *Input_GetDefinedEntry(int player, const char *a, const char *b, const char *c, int flag);
int Input_GetDefinedList(int player, const char *a, const char *b, char *buf, int size, int flag);
int Input_ClearDefinedList(int player, const char *a, const char *b, int flag);
void Input_ResetToDefault(void);
int Input_Bind(int player, const char *a, const char *b, const char *c);
void cPlayerInput_SetString(cPlayerInput *in, const char *text);
void cPlayerInput_SetInputFunc(cPlayerInput *in, const char *func);
char cPlayerInput_GetCursorChar(cPlayerInput *in);
cVirtualKeyboard *cVirtualKeyboard_ctor(cVirtualKeyboard *vk);
byte cVirtualKeyboard_Set(cVirtualKeyboard *vk, const char *text);
void cPad_Rumble(cPad *pad, float a, float b, float c);

/* rva 0x13a1b0: whether the action is pressed for the player */
byte cInputAction_Pressed(cInputAction *a, int player);

/* ActionSet.cpp (src/game/Input/ActionSet.c); the cInputAction functions also exist as loops over a set */
void ActionTable_Init(cInputActionTable *t, int num);
void ActionSetTable_Init(cActionSetTable *t, int num);
void cInputAction_Init(cInputAction *a, const char *name, byte latch);
void cActionSet_Init(cActionSet *s, int num, const char *name);
void cPad_Init(cPad *pad, int index);
void cPad_Update(cPad *pad);
void cPad_Stop(cPad *pad);
void cPlayerInput_MoveCursor(cPlayerInput *in, float dx, float dy);
byte cPlayerInput_SetCursorChar(cPlayerInput *in, char c);
void cPlayerInput_Clear(cPlayerInput *in);
void Input_UpdateActions(void);
void Input_ClearActions(void);
void cInputAction_Shift(cInputAction *a);
void cInputAction_Fire(cInputAction *a);
byte cInputAction_Released(cInputAction *a, int player);
void cInputAction_Clear(cInputAction *a);
void cInputAction_Keep(cInputAction *a);
void cInputAction_Latch(cInputAction *a);
void cActionSet_Shift(cActionSet *s);
void cActionSet_Fire(cActionSet *s);
void cActionSet_Clear(cActionSet *s);
void cActionSet_Keep(cActionSet *s);
void cActionSet_Latch(cActionSet *s);
byte cActionSet_AnyActive(cActionSet *s);
void Input_OnTextDone(int player, const char *text);

void cPlayerInput_PressKey(cPlayerInput *in, int player);
void cActionSet_HoldPressed(cActionSet *s);
cActionSetTable *ActionSets_ctor(cActionSetTable *sets);
cInputAction *cInputAction_ctor(cInputAction *a);
cActionSet *cActionSet_ctor(cActionSet *s);
cPlayerInput *cPlayerInput_ctor(cPlayerInput *in);
void ActionSetTable_Free(cActionSetTable *t);
void cInputAction_dtor(cInputAction *a);
void cActionSet_dtor(cActionSet *s);
void cPlayerInput_dtor(cPlayerInput *in);
cInputAction *cInputAction_Delete(cInputAction *a, uint32_t flags);
cActionSet *cActionSet_Delete(cActionSet *s, uint32_t flags);

/*
 * Input/IDevice.cpp: the devices. An entry is a key, button or half axis; sets[set] is the binding of it in that
 * action set (-1, -1 for none).
 */
typedef struct cDevEntry {
	float prev;			/* 0x00 */
	float value;			/* 0x04 */
	struct {
		int player;
		int action;
	} sets[NUM_SETS];		/* 0x08 */
} cDevEntry;
SD_SIZE(cDevEntry, 0x28);

typedef struct cAbstractIDevice cAbstractIDevice;
typedef struct cAbstractIDeviceVtbl {
	cAbstractIDevice *(*Delete)(cAbstractIDevice *self, byte flags);	/* 0x00 */
	void(*SetDefaults)(cAbstractIDevice *self);				/* 0x04 menu and debug sets */
	void(*SetGameDefaults)(cAbstractIDevice *self);			/* 0x08 character and vehicle */
	const char *(*GetName)(cAbstractIDevice *self);			/* 0x0c */
	const char *(*GetEntryLabel)(cAbstractIDevice *self, int entry);	/* 0x10 for the menus */
	const char *(*GetEntryName)(cAbstractIDevice *self, int entry);	/* 0x14 Scrap.cfg */
	void(*Update)(cAbstractIDevice *self, float dt);			/* 0x18 */
	byte(*IsReserved)(cAbstractIDevice *self, int entry);		/* 0x1c */
	byte(*SetEntry)(cAbstractIDevice *self, int entry, int set, int player, int action, byte check);	/* 0x20 */
} cAbstractIDeviceVtbl;
SD_OFFSET(cAbstractIDeviceVtbl, SetEntry, 0x20);

/* the engine variable of a device: its bindings as lines of Scrap.cfg */
typedef struct cFuncGvarDevices {
	cFuncGvar base;			/* 0x00 the variable is named after the device */
	cAbstractIDevice *device;	/* 0x14 */
} cFuncGvarDevices;
SD_SIZE(cFuncGvarDevices, 0x18);

struct cAbstractIDevice {
	const cAbstractIDeviceVtbl *vftable;	/* 0x00 */
	cFuncGvarDevices *gvar;		/* 0x04 */
	int nEntries;			/* 0x08 */
	cDevEntry *entries;		/* 0x0c Mem_Alloc */
};
SD_SIZE(cAbstractIDevice, 0x10);

/* PcKeyboard.cpp: the keyboard adds no members ((&g_Keyboard), 0x10 bytes) */
typedef cAbstractIDevice cPcKeyboard;

typedef struct cPcMouse {
	cAbstractIDevice base;		/* 0x00 */
	float speed[2];			/* 0x10 x and y per second of the last period */
	float move[2];			/* 0x18 moved in this period */
	float time;			/* 0x20 */
} cPcMouse;
SD_SIZE(cPcMouse, 0x24);

/* XINPUT_STATE */
typedef struct exe_xinput_state {
	uint32_t packet;		/* 0x00 */
	uint16_t buttons;		/* 0x04 */
	byte leftTrigger;		/* 0x06 */
	byte rightTrigger;		/* 0x07 */
	int16_t thumbLX, thumbLY;	/* 0x08 */
	int16_t thumbRX, thumbRY;	/* 0x0c */
} exe_xinput_state;
SD_SIZE(exe_xinput_state, 0x10);

typedef struct cPcPad {
	cAbstractIDevice base;		/* 0x00 */
	int index;			/* 0x10 */
	exe_xinput_state state;		/* 0x14 */
	byte pad_24;
	byte f25[4];			/* 0x25 cleared by the constructor, unaligned */
	byte pad_29[3];
} cPcPad;
SD_SIZE(cPcPad, 0x2c);
SD_OFFSET(cPcPad, state, 0x14);

/* the node data of the device list */
typedef struct cInputDevNode {
	struct cInputDevNode *next;	/* 0x0 */
	struct cInputDevNode *prev;	/* 0x4 */
	cAbstractIDevice *dev;		/* 0x8 */
} cInputDevNode;
typedef DLIST(cInputDevNode) cInputDevList;

extern const cAbstractIDeviceVtbl cAbstractIDevice_vftable;
#define CABSTRACTIDEVICE_VFTABLE (&cAbstractIDevice_vftable)
extern const cGVarListVtbl cFuncGvarDevices_vftable;
#define CFUNCGVARDEVICES_VFTABLE (&cFuncGvarDevices_vftable)
extern const cAbstractIDeviceVtbl cPcKeyboard_vftable;
#define CPCKEYBOARD_VFTABLE (&cPcKeyboard_vftable)
extern const cAbstractIDeviceVtbl cPcMouse_vftable;
#define CPCMOUSE_VFTABLE (&cPcMouse_vftable)
extern const cAbstractIDeviceVtbl cPcPad_vftable;
#define CPCPAD_VFTABLE (&cPcPad_vftable)

/* the player, action and set waiting for the next key (SInput.ListenToDefine), -1 for none */
extern int g_ToDefinePlayer;
extern int g_ToDefineAction;
extern uint32_t g_ToDefineSet;
/* the XInput entries of the pad buttons and half axes in the order of the defaults */
extern int g_PadCodes[24];	/* [24] */
extern cGVar GVar_LogRumble;
#define g_LogRumble (GVar_LogRumble.value.i)		/* LogRumble */
extern cGVar GVar_CheckInactivityTime;
#define g_InactivityTime (GVar_CheckInactivityTime.value.f)	/* CheckInactivityTime */
#define g_InactivityTimeDef (GVar_CheckInactivityTime.def.f)
#define g_InactivityFunc (((cPy_Function *)&GVar_CheckInactivityFunc.func))
extern cPy_Function g_DefineFunc;	/* the function of ListenToDefine */
extern cInputDevList g_InputDevices;
extern cGVar GVar_PauseInactivityTime;
#define g_PauseInactivity (GVar_PauseInactivityTime.value.i)	/* PauseInactivityTime */
typedef void (*input_cb_t)(void);
extern input_cb_t g_OnDefined;
extern float g_InactivityTimer;

cAbstractIDevice *cAbstractIDevice_ctor(cAbstractIDevice *self);
cFuncGvarDevices *cFuncGvarDevices_ctor(cFuncGvarDevices *self, cAbstractIDevice *dev);
void cAbstractIDevice_dtor(cAbstractIDevice *self);
cAbstractIDevice *cAbstractIDevice_Delete(cAbstractIDevice *self, byte flags);
byte cAbstractIDevice_SetEntry(cAbstractIDevice *self, int entry, int set, int player, int action, byte check);
byte cAbstractIDevice_IsReserved(cAbstractIDevice *self, int entry);
void cAbstractIDevice_ClearEntries(cAbstractIDevice *self);
void cAbstractIDevice_GetEntry(cAbstractIDevice *self, int entry, int set, int *player, int *action);
void cAbstractIDevice_InitEntries(cAbstractIDevice *self, int n);
void cAbstractIDevice_SetValue(cAbstractIDevice *self, int entry, float value);
void cAbstractIDevice_SetAxis(cAbstractIDevice *self, int entry, float value);
byte cAbstractIDevice_IsReservedTrue(cAbstractIDevice *self, int entry);
void cAbstractIDevice_SetGameDefaultsNop(cAbstractIDevice *self);
void cFuncGvarDevices_Save(struct cFuncGvar *elem, void *f);
void cFuncGvarDevices_Load(struct cFuncGvar *elem, void *f);
void InputDevices_Clear(cInputDevList *list);
void InputDevices_Add(cInputDevList *list, cAbstractIDevice **dev);
void Input_CheckInactivity(float dt);
int Input_ClearBinding(uint32_t player, int set, int action, byte check);
const char *Input_FindBinding(uint32_t player, int set, int action, const char *device, byte check);
int Input_ListBindings(uint32_t player, int set, int action, char *buf, int size, byte check);
void Input_SetToDefine(int player, int action, uint32_t set);
void Input_OnDefined(void);
void Input_UpdateDevices(float dt);
void Input_ResetInactivity(void);
void Input_SetDefineCallback(void (*f)(void));
void Input_SetDefineFunc(const char *func);
void Device_SetEntries(cAbstractIDevice *d, const byte (*t)[3], int n);

extern byte g_InputOpen;		/* the devices are open (Input_OpenDevices, not on a dedicated server) */

/* Input/PcKeyboard.cpp */
extern cPcKeyboard g_Keyboard;
extern char g_KeyState[256];		/* [256] DIK_ */
cPcKeyboard *cPcKeyboard_ctor(cPcKeyboard *self);
cPcKeyboard *cPcKeyboard_Delete(cPcKeyboard *self, byte flags);
byte cPcKeyboard_SetEntry(cPcKeyboard *self, int entry, int set, int player, int action, byte check);
const char *cPcKeyboard_GetName(cPcKeyboard *self);
const char *cPcKeyboard_GetEntryName(cPcKeyboard *self, int entry);
const char *cPcKeyboard_GetEntryLabel(cPcKeyboard *self, int entry);
void cPcKeyboard_SetDefaults(cPcKeyboard *self);
void Keyboard_Init(void);
void cPcKeyboard_Update(cPcKeyboard *self, float dt);
void cPcKeyboard_SetGameDefaults(cPcKeyboard *self);
int32_t Keyboard_Poll(void);
byte cPcKeyboard_IsReserved(cPcKeyboard *self, int entry);

/* Input/PcMouse.cpp */
extern cGVar GVar_MouseSensitivityH;
#define g_MouseSensH (GVar_MouseSensitivityH.value.f)		/* MouseSensitivityH */
extern cGVar GVar_MouseSensitivityV;
#define g_MouseSensV (GVar_MouseSensitivityV.value.f)		/* MouseSensitivityV */
extern cGVar GVar_InvertMouse;
#define g_InvertMouse (GVar_InvertMouse.value.i)		/* InvertMouse */
extern cPcMouse g_Mouse;
/* DIMOUSESTATE */
typedef struct exe_mouse_state {
	int32_t x, y, z;		/* 0x0 */
	char buttons[4];		/* 0xc */
} exe_mouse_state;
extern exe_mouse_state g_MouseState;
extern cGVar GVar_IsUsingMouse;
#define g_IsUsingMouse (GVar_IsUsingMouse.value.i)		/* IsUsingMouse */
cPcMouse *cPcMouse_ctor(cPcMouse *self);
cAbstractIDevice *cPcMouse_Delete(cAbstractIDevice *elem, byte flags);
void Mouse_Acquire(byte on);
byte cPcMouse_SetEntry(cAbstractIDevice *self, int entry, int set, int player, int action, byte check);
const char *cPcMouse_GetName(cAbstractIDevice *self);
const char *cPcMouse_GetEntryName(cAbstractIDevice *self, int entry);
const char *cPcMouse_GetEntryLabel(cAbstractIDevice *self, int entry);
void cPcMouse_SetDefaults(cAbstractIDevice *self);
void Mouse_Init(void);
void cPcMouse_Update(cAbstractIDevice *elem, float dt);
void cPcMouse_SetGameDefaults(cAbstractIDevice *self);
int32_t Mouse_Poll(void);
byte cPcMouse_IsReserved(cAbstractIDevice *self, int entry);

/* Input/PcPad.cpp: XInput pads */
extern cGVar GVar_MainPcPad;
#define g_MainPcPad (GVar_MainPcPad.value.i)		/* MainPcPad: the pad of player 0, -1 all */
extern cGVar GVar_PCPadDeadZone;
#define g_PadDeadZone (GVar_PCPadDeadZone.value.f)	/* PCPadDeadZone */
extern cGVar GVar_PCPadConnected;
#define g_PCPadConnected (GVar_PCPadConnected.value.i)	/* PCPadConnected */
extern cPcPad g_PcPads[4];		/* [4] */
extern cGVar GVar_PCPadForeground;
#define g_PadForeground (GVar_PCPadForeground.value.i)	/* PCPadForeground: read only while the window is in front */
extern int g_NumPcPads;
extern cGVar GVar_WalkCameraCenter;
#define g_WalkCameraCenter (GVar_WalkCameraCenter.value.f)	/* WalkCameraCenter */
extern cGVar GVar_XBCDigitalAsAnalog;
#define g_XBCDigitalAsAnalog (GVar_XBCDigitalAsAnalog.value.i)
extern cGVar GVar_XBCameraAutoPad;
#define g_XBCameraAutoPad (GVar_XBCameraAutoPad.value.i)
extern cGVar GVar_XBInvertCharYPad;
#define g_XBInvertCharYPad (GVar_XBInvertCharYPad.value.i)
extern cGVar GVar_XBInvertLRChar;
#define g_XBInvertLRChar (GVar_XBInvertLRChar.value.i)
extern cGVar GVar_XBInvertLRVehicle;
#define g_XBInvertLRVehicle (GVar_XBInvertLRVehicle.value.i)
extern cGVar GVar_XBInvertVehicleYPad;
#define g_XBInvertVehicleYPad (GVar_XBInvertVehicleYPad.value.i)
extern cGVar GVar_XBVDigitalAsAnalog;
#define g_XBVDigitalAsAnalog (GVar_XBVDigitalAsAnalog.value.i)
extern cGVar GVar_XBVehicleAsFPS;
#define g_XBVehicleAsFPS (GVar_XBVehicleAsFPS.value.i)
cPcPad *cPcPad_ctor(cPcPad *self, int index);
cAbstractIDevice *cPcPad_Delete(cAbstractIDevice *elem, byte flags);
void Pad_DeadZone(float *x, float *y, float dz);
byte cPcPad_SetEntry(cAbstractIDevice *elem, int entry, int set, int player, int action, byte check);
void cPcPad_SetVehicleDefaults(cPcPad *self);
void cPcPad_SetCharDefaults(cPcPad *self);
const char *cPcPad_GetName(cAbstractIDevice *base);
const char *cPcPad_GetEntryName(cAbstractIDevice *elem, int entry);
const char *cPcPad_GetEntryLabel(cAbstractIDevice *base, int entry);
int Pad_Init(void);
void cPcPad_SetDefaults(cAbstractIDevice *base);
void cPcPad_Update(cAbstractIDevice *elem, float dt);
void cPcPad_SetGameDefaults(cPcPad *self);
uint32_t Pad_Poll(int index);
byte cPcPad_IsReserved(cPcPad *self, int entry);

#endif
