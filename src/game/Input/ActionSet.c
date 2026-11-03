/*
 * Input/ActionSet.cpp of the game and the Global/Table.h instances it uses: the action sets and their actions, the
 * text input of the players, the virtual keyboard and the rumble of the pads. See include/scrap/input.h.
 */
#include <string.h>

#include "scrap/app.h"
#include "scrap/console.h"
#include "scrap/global.h"
#include "scrap/input.h"
#include "scrap/scorer.h"
#include "scrap/world.h"
#include "scrap/staticinit.h"

#define g_LogRumble (GVar_LogRumble.value.i)		/* LogRumble */
static cGVar GVar_LogRumble;
#define INPUTACTION_CTOR 0x139240
#define INPUTACTION_DTOR 0x139430
#define ACTIONSET_CTOR 0x1392b0
#define ACTIONSET_DTOR 0x139480

#define CPYFUNCTION_CTOR 0x192170
#define CPYFUNCTION_DTOR 0x192200

/* the static constructor of g_ActionSets: the four sets and the names of their actions */
static void ActionSetTable_Init(cActionSetTable *t, int num);
static void ActionTable_Init(cInputActionTable *t, int num);
static void cInputAction_Init(cInputAction *a, const char *name, byte latch);
cActionSetTable *ActionSets_ctor(cActionSetTable *sets) {
	cActionSet *s;

	sets->items = NULL;
	sets->count = 0;
	ActionSetTable_Init(sets, NUM_SETS);
	s = &sets->items[ACTSET_MENU];
	cString_Set(&s->name, "Menu");
	ActionTable_Init(&s->actions, NUM_MENU_ACTIONS);
	cInputAction_Init(TABLE_AT(s->actions, MENU_CURSOR_UP), "CursorUp", 1);
	cInputAction_Init(TABLE_AT(s->actions, MENU_CURSOR_DOWN), "CursorDown", 1);
	cInputAction_Init(TABLE_AT(s->actions, MENU_CURSOR_LEFT), "CursorLeft", 1);
	cInputAction_Init(TABLE_AT(s->actions, MENU_CURSOR_RIGHT), "CursorRight", 1);
	cInputAction_Init(TABLE_AT(s->actions, MENU_ITEM_UP), "ItemUp", 1);
	cInputAction_Init(TABLE_AT(s->actions, MENU_ITEM_DOWN), "ItemDown", 1);
	cInputAction_Init(TABLE_AT(s->actions, MENU_ITEM_LEFT), "ItemLeft", 1);
	cInputAction_Init(TABLE_AT(s->actions, MENU_ITEM_RIGHT), "ItemRight", 1);
	cInputAction_Init(TABLE_AT(s->actions, MENU_ACCEPT), "Accept", 1);
	cInputAction_Init(TABLE_AT(s->actions, MENU_CANCEL), "Cancel", 1);
	cInputAction_Init(TABLE_AT(s->actions, MENU_PG_UP), "PgUp", 1);
	cInputAction_Init(TABLE_AT(s->actions, MENU_PG_DOWN), "PgDown", 1);
	cInputAction_Init(TABLE_AT(s->actions, MENU_DELETE), "Delete", 1);
	cInputAction_Init(TABLE_AT(s->actions, MENU_OPEN_CONSOLE), "OpenConsole", 1);
	s = &sets->items[ACTSET_RACER];
	cString_Set(&s->name, "Racer");
	ActionTable_Init(&s->actions, NUM_RACER_ACTIONS);
	cInputAction_Init(TABLE_AT(s->actions, RACER_UP), "Up", 0);
	cInputAction_Init(TABLE_AT(s->actions, RACER_DOWN), "Down", 0);
	cInputAction_Init(TABLE_AT(s->actions, RACER_LEFT), "Left", 0);
	cInputAction_Init(TABLE_AT(s->actions, RACER_RIGHT), "Right", 0);
	cInputAction_Init(TABLE_AT(s->actions, RACER_FORWARD), "Forward", 0);
	cInputAction_Init(TABLE_AT(s->actions, RACER_BACKWARD), "Backward", 0);
	cInputAction_Init(TABLE_AT(s->actions, RACER_THRUST_UP), "TrustUp", 0);
	cInputAction_Init(TABLE_AT(s->actions, RACER_BOOST), "Boost", 0);
	cInputAction_Init(TABLE_AT(s->actions, RACER_STRAFE_LEFT), "StrafeLeft", 0);
	cInputAction_Init(TABLE_AT(s->actions, RACER_STRAFE_RIGHT), "StrafeRight", 0);
	cInputAction_Init(TABLE_AT(s->actions, RACER_STRAFE_UP), "StrafeUp", 0);
	cInputAction_Init(TABLE_AT(s->actions, RACER_STRAFE_DOWN), "StrafeDown", 0);
	cInputAction_Init(TABLE_AT(s->actions, RACER_NEXT_WEAPON), "NextWeapon", 1);
	cInputAction_Init(TABLE_AT(s->actions, RACER_PREV_WEAPON), "PrevWeapon", 1);
	cInputAction_Init(TABLE_AT(s->actions, RACER_FIRE), "Fire", 1);
	cInputAction_Init(TABLE_AT(s->actions, RACER_CM), "CM", 1);
	cInputAction_Init(TABLE_AT(s->actions, RACER_BLOCK_STRAFE), "BockStrafe", 1);
	cInputAction_Init(TABLE_AT(s->actions, RACER_TARGET_NEXT), "TargetNext", 1);
	cInputAction_Init(TABLE_AT(s->actions, RACER_TARGET_PREV), "TargetPrev", 1);
	cInputAction_Init(TABLE_AT(s->actions, RACER_TARGET_RETICLE), "TargetReticle", 1);
	cInputAction_Init(TABLE_AT(s->actions, RACER_MAIN_MENU), "MainMenu", 1);
	cInputAction_Init(TABLE_AT(s->actions, RACER_OPEN_CONSOLE), "OpenConsole", 1);
	cInputAction_Init(TABLE_AT(s->actions, RACER_CENTER_VIEW), "CenterView", 1);
	cInputAction_Init(TABLE_AT(s->actions, RACER_SCORER), "Scorer", 1);
	cInputAction_Init(TABLE_AT(s->actions, RACER_LASER), "Laser", 1);
	cInputAction_Init(TABLE_AT(s->actions, RACER_VULCAN), "Vulcan", 1);
	cInputAction_Init(TABLE_AT(s->actions, RACER_DEVASTATOR), "Devastator", 1);
	cInputAction_Init(TABLE_AT(s->actions, RACER_TESLA), "Tesla", 1);
	cInputAction_Init(TABLE_AT(s->actions, RACER_ATPC), "ATPC", 1);
	cInputAction_Init(TABLE_AT(s->actions, RACER_SWARM), "Swarm", 1);
	cInputAction_Init(TABLE_AT(s->actions, RACER_INFERNO), "Inferno", 1);
	cInputAction_Init(TABLE_AT(s->actions, RACER_SWITCH_VIEW), "SwitchView", 1);
	cInputAction_Init(TABLE_AT(s->actions, RACER_CHAT), "Chat", 1);
	cInputAction_Init(TABLE_AT(s->actions, RACER_QMSG1), "QMsg1", 1);
	cInputAction_Init(TABLE_AT(s->actions, RACER_QMSG2), "QMsg2", 1);
	cInputAction_Init(TABLE_AT(s->actions, RACER_QMSG3), "QMsg3", 1);
	cInputAction_Init(TABLE_AT(s->actions, RACER_QMSG4), "QMsg4", 1);
	cInputAction_Init(TABLE_AT(s->actions, RACER_QMSG5), "QMsg5", 1);
	cInputAction_Init(TABLE_AT(s->actions, RACER_QMSG6), "QMsg6", 1);
	cInputAction_Init(TABLE_AT(s->actions, RACER_MISSION_INFO), "MissionInfo", 1);
	s = &sets->items[ACTSET_INACTIVE];
	cString_Set(&s->name, "Inactive");
	ActionTable_Init(&s->actions, NUM_INACTIVE_ACTIONS);
	cInputAction_Init(TABLE_AT(s->actions, INACTIVE_ABORT), "Abort", 1);
	cInputAction_Init(TABLE_AT(s->actions, INACTIVE_OPEN_CONSOLE), "OpenConsole", 1);
	cInputAction_Init(TABLE_AT(s->actions, INACTIVE_NEXT_CONSOLE), "NextConsole", 1);
	cInputAction_Init(TABLE_AT(s->actions, INACTIVE_PREV_CONSOLE), "PrevConsole", 1);
	cInputAction_Init(TABLE_AT(s->actions, INACTIVE_NEXT_SHIP), "NextShip", 1);
	cInputAction_Init(TABLE_AT(s->actions, INACTIVE_PREV_SHIP), "PrevShip", 1);
	cInputAction_Init(TABLE_AT(s->actions, INACTIVE_SELECT_SHIP), "SeleShip", 1);
	cInputAction_Init(TABLE_AT(s->actions, INACTIVE_EXIT_SHIP), "ExitShip", 1);
	cInputAction_Init(TABLE_AT(s->actions, INACTIVE_NO), "No", 1);
	s = &sets->items[ACTSET_WALK];
	cString_Set(&s->name, "Walk");
	ActionTable_Init(&s->actions, NUM_WALK_ACTIONS);
	cInputAction_Init(TABLE_AT(s->actions, WALK_MAIN_MENU), "MainMenu", 1);
	cInputAction_Init(TABLE_AT(s->actions, WALK_OPEN_CONSOLE), "OpenConsole", 1);
	cInputAction_Init(TABLE_AT(s->actions, WALK_TURN_UP), "TurnUp", 0);
	cInputAction_Init(TABLE_AT(s->actions, WALK_TURN_DOWN), "TurnDown", 0);
	cInputAction_Init(TABLE_AT(s->actions, WALK_TURN_LEFT), "TurnLeft", 0);
	cInputAction_Init(TABLE_AT(s->actions, WALK_TURN_RIGHT), "TurnRight", 0);
	cInputAction_Init(TABLE_AT(s->actions, WALK_FORWARD), "Forward", 0);
	cInputAction_Init(TABLE_AT(s->actions, WALK_BACKWARD), "Backward", 0);
	cInputAction_Init(TABLE_AT(s->actions, WALK_STRAFE_LEFT), "StrafeLeft", 0);
	cInputAction_Init(TABLE_AT(s->actions, WALK_STRAFE_RIGHT), "StrafeRight", 0);
	cInputAction_Init(TABLE_AT(s->actions, WALK_STRAFE_UP), "StrafeUp", 0);
	cInputAction_Init(TABLE_AT(s->actions, WALK_STRAFE_DOWN), "StrafeDown", 0);
	cInputAction_Init(TABLE_AT(s->actions, WALK_STRAFE_TURN_LEFT), "StrafeTurnLeft", 0);
	cInputAction_Init(TABLE_AT(s->actions, WALK_STRAFE_TURN_RIGHT), "StrafeTurnRight", 0);
	cInputAction_Init(TABLE_AT(s->actions, WALK_BLOCK_STRAFE_TURN), "BlockStrafeTurn", 1);
	cInputAction_Init(TABLE_AT(s->actions, WALK_BLOCK_STRAFE), "BlockStrafe", 1);
	cInputAction_Init(TABLE_AT(s->actions, WALK_SWITCH_RUN_WALK), "SwitchRunWalk", 1);
	cInputAction_Init(TABLE_AT(s->actions, WALK_USE), "Use", 1);
	cInputAction_Init(TABLE_AT(s->actions, WALK_ACTION), "Action", 1);
	cInputAction_Init(TABLE_AT(s->actions, WALK_POSSESSION), "Possession", 1);
	cInputAction_Init(TABLE_AT(s->actions, WALK_CAMERA_CENTER), "CameraCenter", 1);
	cInputAction_Init(TABLE_AT(s->actions, WALK_NEXT_ACTION), "NextAction", 1);
	cInputAction_Init(TABLE_AT(s->actions, WALK_PREV_ACTION), "PrevAction", 1);
	cInputAction_Init(TABLE_AT(s->actions, WALK_MISSION_INFO), "MissionInfo", 1);
	return sets;
}

static cInputAction *cInputAction_ctor(cInputAction *a) {
	cString_ctor(&a->name);
	crt_eh_vector_ctor(a->func, sizeof(cPy_Function), MAX_LOCAL_PLAYERS, (void *)cPy_Function_ctor,
		(void *)cPy_Function_dtor);
	return a;
}

static cActionSet *cActionSet_ctor(cActionSet *s) {
	cString_ctor(&s->name);
	s->actions.items = NULL;
	s->actions.count = 0;
	return s;
}

cPlayerInput *cPlayerInput_ctor(cPlayerInput *in) {
	cPy_Function_ctor(&in->func);
	memset(in->text, 0, 0x80);
	in->maxLen = 0x80;
	in->cursorX = 0.0f;
	in->cursorY = 0.0f;
	in->len = 0;
	in->keyDown = 0;
	in->keyWasDown = 0;
	in->onDone = NULL;
	in->active = 0;
	return in;
}

/* the atexit destructor of g_ActionSets (rva 0x38d950, the Free inlined) */
static void ActionSetTable_Free(cActionSetTable *t);
void ActionSet_FiniActionSets(void) {
	ActionSetTable_Free(&g_ActionSets);
}

/* the Global/Table.h Free of the sets */
static void cActionSet_dtor(cActionSet *s);
static void ActionSetTable_Free(cActionSetTable *t) {
	if (t->items) {
		Delete_Objects(t->items, sizeof(cActionSet), (void *)cActionSet_dtor);
		t->items = NULL;
		t->count = 0;
	}
}

static void cInputAction_dtor(cInputAction *a) {
	crt_eh_vector_dtor(a->func, sizeof(cPy_Function), MAX_LOCAL_PLAYERS, (void *)cPy_Function_dtor);
	cString_dtor(&a->name);
}

static void cActionSet_dtor(cActionSet *s) {
	if (s->actions.items) {
		Delete_Objects(s->actions.items, sizeof(cInputAction), (void *)cInputAction_dtor);
		s->actions.items = NULL;
		s->actions.count = 0;
	}
	cString_dtor(&s->name);
}

void cPlayerInput_dtor(cPlayerInput *in) {
	cPy_Function_dtor(&in->func);
}

cVirtualKeyboard g_VirtualKeyboard;

/* the accept key on the virtual keyboard: the character under the cursor typed, once per press */
void cPlayerInput_PressKey(cPlayerInput *in, int player) {
	if (in->keyDown) {
		return;
	}
	in->keyDown = 1;
	if (in->keyWasDown) {
		return;
	}
	int i = (int)in->cursorY * g_VirtualKeyboard.a + (int)in->cursorX;
	if (i > g_VirtualKeyboard.len) {
		i = g_VirtualKeyboard.len - 1;
	}
	char c = g_VirtualKeyboard.text[i];
	in->text[in->len] = c;
	in->text[in->len + 1] = 0;
	if (c == '\b') {
		if (in->len > 0) {
			in->len--;
		}
	} else if (c == '\n') {
		in->text[in->len] = 0;
		void (*f)(int, const char *) = in->onDone;
		if (f) {
			in->onDone = NULL;
			f(player, in->text);
		}
		return;
	} else if (in->maxLen > in->len) {
		in->len++;
		return;
	}
	in->text[in->len] = 0;
}

/* the presses kept over the set change end when the value falls to 0.5 */
void cActionSet_HoldPressed(cActionSet *s) {
	for (uint32_t i = 0; (int)i < s->actions.count; i++) {
		cInputAction *a = TABLE_AT(s->actions, i);
		for (int p = 0; p < MAX_LOCAL_PLAYERS; p++) {
			if (a->pressed[p] && a->value[p] > 0.5f) {
				a->value[p] = 0.0f;
				a->prev[p] = 0.0f;
			} else {
				a->pressed[p] = 0;
			}
		}
	}
}

static uint32_t g_CurActionSet = 0xffffffff;

uint32_t Input_GetActionSet(void) {
	return g_CurActionSet;
}

char cPlayerInput_GetCursorChar(cPlayerInput *in) {
	int i = (int)in->cursorY * g_VirtualKeyboard.a + (int)in->cursorX;
	if (i > g_VirtualKeyboard.len) {
		return g_VirtualKeyboard.text[g_VirtualKeyboard.len - 1];
	}
	return g_VirtualKeyboard.text[i];
}

/* the Global/Table.h Init of the two instances: new T[num] with the constructors */
static void ActionTable_Init(cInputActionTable *t, int num) {
	CHECK(!t->items && num > 0);
	t->count = num;
	t->items = New_Objects(num, sizeof(cInputAction), (void *)cInputAction_ctor, (void *)cInputAction_dtor);
	CHECK(t->items);
}

static void ActionSetTable_Init(cActionSetTable *t, int num) {
	CHECK(!t->items && num > 0);
	t->count = num;
	t->items = New_Objects(num, sizeof(cActionSet), (void *)cActionSet_ctor, (void *)cActionSet_dtor);
	CHECK(t->items);
}

static void cInputAction_Init(cInputAction *a, const char *name, byte latch) {
	a->latch = latch;
	cString_Set(&a->name, name);
	memset(a->pressed, 0, sizeof(a->pressed));
}

static void cPad_Init(cPad *pad, int index) {
	pad->motor[0] = 0.0f;
	pad->motor[1] = 0.0f;
	pad->prev[0] = 0.0f;
	pad->prev[1] = 0.0f;
	pad->time = 0.0f;
	pad->duration = 0.0f;
	pad->onRumble = NULL;
	pad->index = index;
}

static void Input_ClearActions(void);
void InputMaps_Init(void) {
	g_CurActionSet = 0;
	Input_ClearActions();
	for (int i = 0; i < MAX_LOCAL_PLAYERS; i++) {
		cPad_Init(&g_Pads[i], i);
	}
}

static void cPad_Stop(cPad *pad);
void InputMaps_Clear(void) {
	for (int i = 0; i < MAX_LOCAL_PLAYERS; i++) {
		cPad_Stop(&g_Pads[i]);
	}
}

void InputMaps_Done(void) {
}

/* every 0.05 seconds: counts the duration down and passes the motors to the callback while one of them runs */
static void cPad_Update(cPad *pad) {
	double next = (double)(pad->time + 0.05f);

	if (!(App_GetGameTime(g_pApp) > next)) {
		return;
	}
	float d = (float)App_GetGameTime(g_pApp) - pad->time;
	pad->time = (float)App_GetGameTime(g_pApp);
	float rem = pad->duration - d;
	pad->duration = rem;
	if (0.0f >= rem || g_CurActionSet == 0 || (g_SplitScreenGame && g_CurActionSet == 2)) {
		pad->motor[1] = 0.0f;
		pad->motor[0] = 0.0f;
		pad->duration = 0.0f;
	}
	if (pad->motor[1] != 0.0f || pad->motor[0] != 0.0f || pad->prev[0] != 0.0f || pad->prev[1] != 0.0f) {
		if (g_LogRumble) {
			char l[11], r[11];
			memset(r, ' ', 10);
			memset(l, ' ', 10);
			r[10] = 0;
			l[10] = 0;
			/* the original has no bound; Rumble keeps the motors in 0..1 */
			int n = (int)(pad->motor[0] * 10.0f);
			memset(l, '#', n < 0 ? 0 : n > 10 ? 10 : n);
			n = (int)(pad->motor[1] * 10.0f);
			memset(r, '#', n < 0 ? 0 : n > 10 ? 10 : n);
			Con_Verbosef("L:%s   R:%s\n", l, r);
		}
		if (pad->onRumble) {
			pad->onRumble(pad->index, pad->motor[0], pad->motor[1]);
		}
	}
	pad->prev[0] = pad->motor[0];
	pad->prev[1] = pad->motor[1];
}

/* the cursor wraps around the grid of the virtual keyboard */
void cPlayerInput_MoveCursor(cPlayerInput *in, float dx, float dy) {
	float x = dx + in->cursorX;
	float y = dy + in->cursorY;

	in->cursorX = x;
	in->cursorY = y;
	float w = (float)g_VirtualKeyboard.a;
	if (x > w - 0.001f) {
		in->cursorX = x - w;
	}
	float h = (float)g_VirtualKeyboard.b;
	if (y > h - 0.001f) {
		in->cursorY = y - h;
	}
	float cx = in->cursorX;
	if (0.0f > cx) {
		in->cursorX = (float)g_VirtualKeyboard.a + cx;
	}
	float cy = in->cursorY;
	if (0.0f > cy) {
		in->cursorY = (float)g_VirtualKeyboard.b + cy;
	}
}

/* puts the cursor on the cell of c; 0 if the keyboard has no such character */
byte cPlayerInput_SetCursorChar(cPlayerInput *in, char c) {
	char s[2] = { c, 0 };
	char *p = strstr(g_VirtualKeyboard.text, s);

	if (!p) {
		return 0;
	}
	int off = (int)(p - g_VirtualKeyboard.text);
	in->cursorX = (float)(off % g_VirtualKeyboard.a) + 0.5f;
	in->cursorY = (float)(off / g_VirtualKeyboard.a) + 0.5f;
	return 1;
}

/* the start of a frame: the values of the last frame become prev, the rumble goes on */
static void cActionSet_Shift(cActionSet *s);
void Input_UpdateActions(void) {
	for (int i = 0; i < MAX_LOCAL_PLAYERS; i++) {
		g_PlayerInput[i].keyWasDown = g_PlayerInput[i].keyDown;
	}
	for (int i = 0; i < MAX_LOCAL_PLAYERS; i++) {
		g_PlayerInput[i].keyDown = 0;
	}
	for (uint32_t i = 0; (int)i < g_ActionSets.count; i++) {
		cActionSet_Shift(TABLE_AT(g_ActionSets, i));
	}
	for (int i = 0; i < MAX_LOCAL_PLAYERS; i++) {
		cPad_Update(&g_Pads[i]);
	}
}

static void cInputAction_Shift(cInputAction *a) {
	for (int p = 0; p < MAX_LOCAL_PLAYERS; p++) {
		a->prev[p] = a->value[p];
		a->value[p] = 0.0f;
	}
}

static void cActionSet_Shift(cActionSet *s) {
	for (uint32_t i = 0; (int)i < s->actions.count; i++) {
		cInputAction_Shift(TABLE_AT(s->actions, i));
	}
}

byte cInputAction_Pressed(cInputAction *a, int player) {
	return 0.5f > a->prev[player] && a->value[player] > 0.5f;
}

/* the bound functions of the players that pressed the action in this frame */
static void cInputAction_Fire(cInputAction *a) {
	for (int p = 0; p < MAX_LOCAL_PLAYERS; p++) {
		if (0.5f > a->prev[p] && a->value[p] > 0.5f) {
			cPy_Function_AddParamI(&a->func[p], p);
			cPy_Function_Execute(&a->func[p], 0);
		}
	}
}

void cActionSet_Fire(cActionSet *s) {
	for (uint32_t i = 0; (int)i < s->actions.count; i++) {
		cInputAction_Fire(TABLE_AT(s->actions, i));
	}
}

byte cInputAction_Released(cInputAction *a, int player) {
	return a->prev[player] > 0.5f && 0.5f > a->value[player];
}

static void cPad_Stop(cPad *pad) {
	if (pad->onRumble) {
		pad->onRumble(pad->index, 0.0f, 0.0f);
	}
	pad->motor[1] = 0.0f;
	pad->motor[0] = 0.0f;
	pad->duration = 0.0f;
}

static void cActionSet_Clear(cActionSet *s);
static void Input_ClearActions(void) {
	for (uint32_t i = 0; (int)i < g_ActionSets.count; i++) {
		cActionSet_Clear(TABLE_AT(g_ActionSets, i));
	}
}

void Input_ResetActions(void) {
	for (int i = 0; i < MAX_LOCAL_PLAYERS; i++) {
		g_PlayerInput[i].onDone = NULL;
		cPy_Function_Clear(&g_PlayerInput[i].func);
	}
	Input_ResetInactivity();
}

static void cInputAction_Clear(cInputAction *a) {
	for (int p = 0; p < MAX_LOCAL_PLAYERS; p++) {
		a->value[p] = 0.0f;
		a->prev[p] = 0.0f;
	}
}

static void cActionSet_Clear(cActionSet *s) {
	for (uint32_t i = 0; (int)i < s->actions.count; i++) {
		cInputAction_Clear(TABLE_AT(s->actions, i));
	}
}

static void cActionSet_Keep(cActionSet *s);
void Input_ResetStates(void) {
	for (uint32_t i = 0; (int)i < g_ActionSets.count; i++) {
		cActionSet_Keep(TABLE_AT(g_ActionSets, i));
	}
}

static void cInputAction_Keep(cInputAction *a) {
	for (int p = 0; p < MAX_LOCAL_PLAYERS; p++) {
		a->prev[p] = a->value[p];
	}
}

static void cActionSet_Keep(cActionSet *s) {
	for (uint32_t i = 0; (int)i < s->actions.count; i++) {
		cInputAction_Keep(TABLE_AT(s->actions, i));
	}
}

/* clears the old and the new set and reads the devices once, so that a held key does not fire in the new set */
static void cActionSet_Latch(cActionSet *s);
void Input_SelectActionSet(int index) {
	Scorer_SetConsole(0);
	CHECK(NUM_SETS > index);
	cActionSet_Clear(TABLE_AT(g_ActionSets, g_CurActionSet));
	g_CurActionSet = index;
	cActionSet_Clear(TABLE_AT(g_ActionSets, index));
	Input_UpdateDevices(0.1f);
	cActionSet_Latch(TABLE_AT(g_ActionSets, g_CurActionSet));
	Input_Update(0.1f);
}

static void cInputAction_Latch(cInputAction *a) {
	for (int p = 0; p < MAX_LOCAL_PLAYERS; p++) {
		if (!a->latch) {
			return;
		}
		a->pressed[p] = a->value[p] > 0.5f;
	}
}

static void cActionSet_Latch(cActionSet *s) {
	for (uint32_t i = 0; (int)i < s->actions.count; i++) {
		cInputAction_Latch(TABLE_AT(s->actions, i));
	}
}

/* the constructor of a static keyboard (rva 0x1392e0, from its initializer): the standard grid */
cVirtualKeyboard *cVirtualKeyboard_ctor(cVirtualKeyboard *vk) {
	cVirtualKeyboard_Set(vk, "Standard");
	return vk;
}

static const char VK_STANDARD[80] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz 0123456789.:,;-!\"&/()=?\010\n";

static const char VK_CONSOLE[108] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz\\#@|$%=':+*~{_}[]<>^\t`ñÑ¿¡ 0123456789.:,;-!\"&/()=?\010\n";

/* "Standard", "Numbers" or "Console"; 0 for another name (the grid stays) */
byte cVirtualKeyboard_Set(cVirtualKeyboard *vk, const char *text) {
	byte ok = 1;

	if (!crt_stricmp(text, "Standard")) {
		Str_Copy(vk->text, VK_STANDARD, sizeof(vk->text));
		vk->a = 13;
	} else if (!crt_stricmp(text, "Numbers")) {
		Str_Copy(vk->text, "123456789\b0\n", sizeof(vk->text));
		vk->a = 3;
	} else if (!crt_stricmp(text, "Console")) {
		Str_Copy(vk->text, VK_CONSOLE, sizeof(vk->text));
		vk->a = 13;
	} else {
		ok = 0;
	}
	vk->len = (int)strlen(vk->text);
	vk->b = vk->len / vk->a;
	CHECK((vk->len % vk->a) == 0);
	return ok;
}

/* the function gets player and text when the player ends the line (Input_OnTextDone of the exe) */
static void Input_OnTextDone(int player, const char *text);
void cPlayerInput_SetInputFunc(cPlayerInput *in, const char *func) {
	in->onDone = (void (*)(int, const char *))(void *)Input_OnTextDone;
	cPy_Function_SetFunc(&in->func, func);
}

/* the motors in 0..1 (0.05 or less is off), the duration up to 30 seconds */
void cPad_Rumble(cPad *pad, float a, float b, float c) {
	if (0.05f >= a) {
		a = 0.0f;
	}
	if (0.05f >= b) {
		b = 0.0f;
	}
	pad->motor[0] = !(0.0f >= a) ? Float_MinSS(1.0f, a) : 0.0f;
	pad->motor[1] = !(0.0f >= b) ? Float_MinSS(1.0f, b) : 0.0f;
	pad->duration = !(0.0f >= c) ? Float_MinSS(30.0f, c) : 0.0f;
}

void cPlayerInput_SetString(cPlayerInput *in, const char *text) {
	size_t n = strlen(text);

	CHECK(n <= 0x80);
	/* the original copies without a bound after the CHECK */
	if (n >= sizeof(in->text)) {
		n = sizeof(in->text) - 1;
	}
	memcpy(in->text, text, n);
	in->text[n] = 0;
	in->maxLen = 0x80;
	in->cursorX = 0.0f;
	in->cursorY = 0.0f;
	in->len = (int)strlen(in->text);
}

static void Input_OnTextDone(int player, const char *text) {
	cPy_Function *f = &g_PlayerInput[player].func;

	cPy_Function_AddParamI(f, player);
	cPy_Function_AddParamS(f, text);
	cPy_Function_Execute(f, 0);
}

/* whether any action of the set is held by any player (the inactivity timer) */
byte cActionSet_AnyActive(cActionSet *s) {
	for (uint32_t i = 0; (int)i < s->actions.count; i++) {
		for (int p = 0; p < MAX_LOCAL_PLAYERS; p++) {
			if (TABLE_AT(s->actions, i)->value[p] > 0.5f) {
				return 1;
			}
		}
	}
	return 0;
}

static cGVar GVar_LogRumble;

/* the dynamic initializers of the globals here, run in the order of src/standalone/crtxc.inc */
INIT_GVAR_INT(Init_GVar_LogRumble, &GVar_LogRumble, "LogRumble", 0, "Log the input and output to the rumble interface.", 0)
INIT_CALL_ARGS(Init_cVirtualKeyboard_VirtualKeyboard, &g_VirtualKeyboard, cVirtualKeyboard_Set, "Standard")
