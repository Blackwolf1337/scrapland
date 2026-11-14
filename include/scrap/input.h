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
/* the action sets of g_ActionSets (ActionSets_ctor): the names of Controls.cfg */
enum { ACTSET_MENU, ACTSET_RACER, ACTSET_INACTIVE, ACTSET_WALK, NUM_SETS };

/* the actions of the set Menu */
enum {
	MENU_CANCEL, MENU_OPEN_CONSOLE, MENU_CURSOR_UP, MENU_CURSOR_DOWN, MENU_CURSOR_LEFT, MENU_CURSOR_RIGHT,
	MENU_ITEM_UP, MENU_ITEM_DOWN, MENU_ITEM_LEFT, MENU_ITEM_RIGHT, MENU_ACCEPT, MENU_PG_UP, MENU_PG_DOWN,
	MENU_DELETE, NUM_MENU_ACTIONS
};

/* the actions of the set Racer (the vehicles) */
enum {
	RACER_MAIN_MENU, RACER_OPEN_CONSOLE, RACER_UP, RACER_DOWN, RACER_LEFT, RACER_RIGHT, RACER_FORWARD,
	RACER_BACKWARD, RACER_THRUST_UP, RACER_BOOST, RACER_STRAFE_LEFT, RACER_STRAFE_RIGHT, RACER_STRAFE_UP,
	RACER_STRAFE_DOWN, RACER_NEXT_WEAPON, RACER_PREV_WEAPON, RACER_FIRE, RACER_CM, RACER_BLOCK_STRAFE,
	RACER_TARGET_NEXT, RACER_TARGET_PREV, RACER_TARGET_RETICLE, RACER_CENTER_VIEW, RACER_SCORER, RACER_LASER,
	RACER_VULCAN, RACER_DEVASTATOR, RACER_TESLA, RACER_ATPC, RACER_SWARM, RACER_INFERNO, RACER_SWITCH_VIEW,
	RACER_CHAT, RACER_QMSG1, RACER_QMSG2, RACER_QMSG3, RACER_QMSG4, RACER_QMSG5, RACER_QMSG6, RACER_MISSION_INFO,
	NUM_RACER_ACTIONS
};

/* the actions of the set Inactive */
enum {
	INACTIVE_ABORT, INACTIVE_OPEN_CONSOLE, INACTIVE_NEXT_CONSOLE, INACTIVE_PREV_CONSOLE, INACTIVE_NEXT_SHIP,
	INACTIVE_PREV_SHIP, INACTIVE_SELECT_SHIP, INACTIVE_EXIT_SHIP, INACTIVE_NO, NUM_INACTIVE_ACTIONS
};

/* the actions of the set Walk (the characters) */
enum {
	WALK_MAIN_MENU, WALK_OPEN_CONSOLE, WALK_TURN_UP, WALK_TURN_DOWN, WALK_TURN_LEFT, WALK_TURN_RIGHT,
	WALK_FORWARD, WALK_BACKWARD, WALK_STRAFE_LEFT, WALK_STRAFE_RIGHT, WALK_STRAFE_UP, WALK_STRAFE_DOWN,
	WALK_STRAFE_TURN_LEFT, WALK_STRAFE_TURN_RIGHT, WALK_BLOCK_STRAFE_TURN, WALK_BLOCK_STRAFE,
	WALK_SWITCH_RUN_WALK, WALK_USE, WALK_POSSESSION, WALK_ACTION, WALK_CAMERA_CENTER, WALK_NEXT_ACTION,
	WALK_PREV_ACTION, WALK_MISSION_INFO, WALK_COOP_LEADER, NUM_WALK_ACTIONS
};

/* an input action of an action set; value[player] is its current state (SScorer.GetMenuAccept) */
typedef struct cInputAction {
	byte pressed[MAX_LOCAL_PLAYERS];	/* held over a set change while the value stays above 0.5 */
	char *name;			/* cString */
	float prev[MAX_LOCAL_PLAYERS];	/* value of the last frame */
	float value[MAX_LOCAL_PLAYERS];
	cPy_Function func[MAX_LOCAL_PLAYERS];	/* SInput.Bind, called with the player on the press */
	byte latch;			/* keeps pressed over a set change */
	byte pad_79[3];
} cInputAction;

typedef TABLE(cInputAction) cInputActionTable;

typedef struct cActionSet {
	char *name;
	cInputActionTable actions;
} cActionSet;
typedef TABLE(cActionSet) cActionSetTable;

/* the text input of a player: typed through the virtual keyboard grid, the cursor is a cell of it */
typedef struct cPlayerInput {
	int maxLen;			/* 0x80 */
	int len;
	char text[0x8c - 0x08];		/* SInput.GetString */
	float cursorX;			/* SInput.GetCursorChar */
	float cursorY;
	byte keyDown;			/* the accept key of this frame */
	byte keyWasDown;
	byte pad_96[2];
	void (*onDone)(int player, const char *text);	/* once on the new line */
	cPy_Function func;		/* SInput.SetInputFunc */
	byte active;			/* the cursor follows mouse and pad */
	byte pad_b1[3];
} cPlayerInput;

/* the rumble state of a player */
typedef struct cPad {
	int index;
	float prev[2];			/* left and right motor of the last update */
	float motor[2];
	float time;			/* game time of the last update */
	float duration;			/* seconds left */
	void (*onRumble)(int index, float left, float right);
} cPad;

typedef struct cVirtualKeyboard {
	char text[0x100];		/* the characters of the grid, row by row */
	int a;				/* columns (SInput.GetVirtualKeyboard) */
	int b;				/* rows */
	int len;
} cVirtualKeyboard;

extern cActionSetTable g_ActionSets;
extern cPlayerInput g_PlayerInput[4];	/* [MAX_LOCAL_PLAYERS] */
extern cVirtualKeyboard g_VirtualKeyboard;
extern cPad g_Pads[4];		/* [MAX_LOCAL_PLAYERS] */

/* is cString_EqStr on the name */
static inline byte ActionSet_IsNamed(cActionSet *set, const char *name) {
	return cString_EqStr(&set->name, name);
}
void Input_SelectActionSet(int index);
/* the action set of that name (Menu, Racer, Walk, Inactive); 0 when there is none */
byte Input_SelectActionSetNamed(const char *name);
/* the accept action of the menu set is held by the player (SScorer.GetMenuAccept) */
byte Input_MenuAccept(int player);
uint32_t Input_GetActionSet(void);
int Input_AssignEntry(const char *a, const char *b, int c, const char *d, const char *e);
/* the largest value of the device entries bound to the action of the set for the player, whatever set is on */
float Input_BoundValue(int set, int action, int player);
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

/* whether the action is pressed for the player */
byte cInputAction_Pressed(cInputAction *a, int player);

/* action of the set set (ACTSET_) pressed by the player in this frame */
static inline byte Input_ActionPressed(int set, int action, int player) {
	return cInputAction_Pressed(TABLE_AT(TABLE_AT(g_ActionSets, set)->actions, action), player);
}

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
	float prev;
	float value;
	struct {
		int player;
		int action;
	} sets[NUM_SETS];
} cDevEntry;

typedef struct cAbstractIDevice cAbstractIDevice;
typedef struct cAbstractIDeviceVtbl {
	void (*Free)(cAbstractIDevice *self);
	void(*SetDefaults)(cAbstractIDevice *self);				/* menu and debug sets */
	void(*SetGameDefaults)(cAbstractIDevice *self);			/* character and vehicle */
	const char *(*GetName)(cAbstractIDevice *self);
	const char *(*GetEntryLabel)(cAbstractIDevice *self, int entry);	/* for the menus */
	const char *(*GetEntryName)(cAbstractIDevice *self, int entry);	/* Scrap.cfg */
	void(*Update)(cAbstractIDevice *self, float dt);
	byte(*IsReserved)(cAbstractIDevice *self, int entry);
	byte(*SetEntry)(cAbstractIDevice *self, int entry, int set, int player, int action, byte check);
} cAbstractIDeviceVtbl;

/* calls through the slots of cAbstractIDeviceVtbl */
static inline const char *InputDev_GetEntryLabel(void *self, int entry) {
	return VT(cAbstractIDeviceVtbl, self)->GetEntryLabel(self, entry);
}

static inline const char *InputDev_GetEntryName(void *self, int entry) {
	return VT(cAbstractIDeviceVtbl, self)->GetEntryName(self, entry);
}

static inline const char *InputDev_GetName(void *self) {
	return VT(cAbstractIDeviceVtbl, self)->GetName(self);
}

static inline byte InputDev_IsReserved(void *self, int entry) {
	return VT(cAbstractIDeviceVtbl, self)->IsReserved(self, entry);
}

static inline void InputDev_SetDefaults(void *self) {
	VT(cAbstractIDeviceVtbl, self)->SetDefaults(self);
}

static inline byte InputDev_SetEntry(void *self, int entry, int set, int player, int action, byte check) {
	return VT(cAbstractIDeviceVtbl, self)->SetEntry(self, entry, set, player, action, check);
}

static inline void InputDev_SetGameDefaults(void *self) {
	VT(cAbstractIDeviceVtbl, self)->SetGameDefaults(self);
}

static inline void InputDev_Update(void *self, float dt) {
	VT(cAbstractIDeviceVtbl, self)->Update(self, dt);
}

/* the engine variable of a device: its bindings as lines of Scrap.cfg */
typedef struct cFuncGvarDevices {
	cFuncGvar base;			/* the variable is named after the device */
	cAbstractIDevice *device;
} cFuncGvarDevices;

struct cAbstractIDevice {
	const cAbstractIDeviceVtbl *vftable;
	cFuncGvarDevices *gvar;
	int nEntries;
	cDevEntry *entries;		/* Mem_Alloc */
};

/* PcKeyboard.cpp: the keyboard adds no members ((&g_Keyboard)) */
typedef cAbstractIDevice cPcKeyboard;

typedef struct cPcMouse {
	cAbstractIDevice base;
	float speed[2];			/* x and y per second of the last period */
	float move[2];			/* moved in this period */
	float time;
} cPcMouse;

/* XINPUT_STATE */
typedef struct exe_xinput_state {
	uint32_t packet;
	uint16_t buttons;
	byte leftTrigger;
	byte rightTrigger;
	int16_t thumbLX, thumbLY;
	int16_t thumbRX, thumbRY;
} exe_xinput_state;

typedef struct cPcPad {
	cAbstractIDevice base;
	int index;
	exe_xinput_state state;
	byte pad_24;
	byte f25[4];			/* cleared by the constructor, unaligned */
	byte pad_29[3];
} cPcPad;

/* the node data of the device list */
typedef struct cInputDevNode {
	struct cInputDevNode *next;
	struct cInputDevNode *prev;
	cAbstractIDevice *dev;
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
	int32_t x, y, z;
	char buttons[4];
} exe_mouse_state;
extern cGVar GVar_IsUsingMouse;
#define g_IsUsingMouse (GVar_IsUsingMouse.value.i)		/* IsUsingMouse */
void Mouse_Acquire(byte on);
void Mouse_Init(void);

extern cGVar GVar_WalkCameraCenter;
#define g_WalkCameraCenter (GVar_WalkCameraCenter.value.f)	/* WalkCameraCenter */
int Pad_Init(void);

#endif
