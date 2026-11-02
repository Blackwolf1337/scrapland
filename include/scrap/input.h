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

typedef TABLE(cInputAction) cInputActionTable;

typedef struct cActionSet {
	char *name;			/* 0x0 */
	cInputActionTable actions;	/* 0x4 */
} cActionSet;
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

/* the rumble state of a player */
typedef struct cPad {
	int index;			/* 0x00 */
	float prev[2];			/* 0x04 left and right motor of the last update */
	float motor[2];			/* 0x0c */
	float time;			/* 0x14 game time of the last update */
	float duration;			/* 0x18 seconds left */
	void (*onRumble)(int index, float left, float right);	/* 0x1c */
} cPad;

typedef struct cVirtualKeyboard {
	char text[0x100];		/* 0x000 the characters of the grid, row by row */
	int a;				/* 0x100 columns (SInput.GetVirtualKeyboard) */
	int b;				/* 0x104 rows */
	int len;			/* 0x108 */
} cVirtualKeyboard;

extern cActionSetTable g_ActionSets;
extern cPlayerInput g_PlayerInput[4];	/* [MAX_LOCAL_PLAYERS] */
extern cVirtualKeyboard g_VirtualKeyboard;
extern cPad g_Pads[4];		/* [MAX_LOCAL_PLAYERS] */

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

void cPlayerInput_MoveCursor(cPlayerInput *in, float dx, float dy);
byte cPlayerInput_SetCursorChar(cPlayerInput *in, char c);
void Input_UpdateActions(void);
byte cInputAction_Released(cInputAction *a, int player);
void cActionSet_Fire(cActionSet *s);
byte cActionSet_AnyActive(cActionSet *s);

void cPlayerInput_PressKey(cPlayerInput *in, int player);
void cActionSet_HoldPressed(cActionSet *s);
cActionSetTable *ActionSets_ctor(cActionSetTable *sets);
cPlayerInput *cPlayerInput_ctor(cPlayerInput *in);
void cPlayerInput_dtor(cPlayerInput *in);

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

/* the engine variable of a device: its bindings as lines of Scrap.cfg */
typedef struct cFuncGvarDevices {
	cFuncGvar base;			/* 0x00 the variable is named after the device */
	cAbstractIDevice *device;	/* 0x14 */
} cFuncGvarDevices;

struct cAbstractIDevice {
	const cAbstractIDeviceVtbl *vftable;	/* 0x00 */
	cFuncGvarDevices *gvar;		/* 0x04 */
	int nEntries;			/* 0x08 */
	cDevEntry *entries;		/* 0x0c Mem_Alloc */
};

/* PcKeyboard.cpp: the keyboard adds no members ((&g_Keyboard), 0x10 bytes) */
typedef cAbstractIDevice cPcKeyboard;

typedef struct cPcMouse {
	cAbstractIDevice base;		/* 0x00 */
	float speed[2];			/* 0x10 x and y per second of the last period */
	float move[2];			/* 0x18 moved in this period */
	float time;			/* 0x20 */
} cPcMouse;

/* XINPUT_STATE */
typedef struct exe_xinput_state {
	uint32_t packet;		/* 0x00 */
	uint16_t buttons;		/* 0x04 */
	byte leftTrigger;		/* 0x06 */
	byte rightTrigger;		/* 0x07 */
	int16_t thumbLX, thumbLY;	/* 0x08 */
	int16_t thumbRX, thumbRY;	/* 0x0c */
} exe_xinput_state;

typedef struct cPcPad {
	cAbstractIDevice base;		/* 0x00 */
	int index;			/* 0x10 */
	exe_xinput_state state;		/* 0x14 */
	byte pad_24;
	byte f25[4];			/* 0x25 cleared by the constructor, unaligned */
	byte pad_29[3];
} cPcPad;

/* the node data of the device list */
typedef struct cInputDevNode {
	struct cInputDevNode *next;	/* 0x0 */
	struct cInputDevNode *prev;	/* 0x4 */
	cAbstractIDevice *dev;		/* 0x8 */
} cInputDevNode;
typedef DLIST(cInputDevNode) cInputDevList;

extern cInputDevList g_InputDevices;
typedef void (*input_cb_t)(void);

cAbstractIDevice *cAbstractIDevice_ctor(cAbstractIDevice *self);
cFuncGvarDevices *cFuncGvarDevices_ctor(cFuncGvarDevices *self, cAbstractIDevice *dev);
void cAbstractIDevice_dtor(cAbstractIDevice *self);
byte cAbstractIDevice_SetEntry(cAbstractIDevice *self, int entry, int set, int player, int action, byte check);
void cAbstractIDevice_InitEntries(cAbstractIDevice *self, int n);
void cAbstractIDevice_SetValue(cAbstractIDevice *self, int entry, float value);
void cAbstractIDevice_SetAxis(cAbstractIDevice *self, int entry, float value);
byte cAbstractIDevice_IsReservedTrue(cAbstractIDevice *self, int entry);
void cAbstractIDevice_SetGameDefaultsNop(cAbstractIDevice *self);
void InputDevices_Add(cInputDevList *list, cAbstractIDevice **dev);
void Input_UpdateDevices(float dt);
void Input_ResetInactivity(void);
void Device_SetEntries(cAbstractIDevice *d, const byte (*t)[3], int n);

extern byte g_InputOpen;		/* the devices are open (Input_OpenDevices, not on a dedicated server) */

void Keyboard_Init(void);

/* DIMOUSESTATE */
typedef struct exe_mouse_state {
	int32_t x, y, z;		/* 0x0 */
	char buttons[4];		/* 0xc */
} exe_mouse_state;
extern cGVar GVar_IsUsingMouse;
#define g_IsUsingMouse (GVar_IsUsingMouse.value.i)		/* IsUsingMouse */
void Mouse_Acquire(byte on);
void Mouse_Init(void);

extern cGVar GVar_WalkCameraCenter;
#define g_WalkCameraCenter (GVar_WalkCameraCenter.value.f)	/* WalkCameraCenter */
int Pad_Init(void);

#endif
