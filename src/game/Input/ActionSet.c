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

#define INPUTACTION_CTOR 0x139240
#define INPUTACTION_DTOR 0x139430
#define ACTIONSET_CTOR 0x1392b0
#define ACTIONSET_DTOR 0x139480
#define VK_STANDARD (GAME_ARRAY(const char, VK_STANDARD, 0x3bc0b0))	/* "ABC...xyz 0123456789.:,;-!\"&/()=?\b\n" */
#define VK_CONSOLE (GAME_ARRAY(const char, VK_CONSOLE, 0x3bc120))	/* the same with the console signs */

#define CPYFUNCTION_CTOR 0x192170
#define CPYFUNCTION_DTOR 0x192200

/* the static constructor of g_ActionSets: the four sets and the names of their actions */
cActionSetTable *__fastcall ActionSets_ctor(cActionSetTable *sets) {
	cActionSet *s;

	sets->items = NULL;
	sets->count = 0;
	ActionSetTable_Init(sets, NUM_SETS);
	s = &sets->items[0];
	cString_Set(&s->name, "Menu");
	ActionTable_Init(&s->actions, 14);
	cInputAction_Init(TABLE_AT(s->actions, 2), "CursorUp", 1);
	cInputAction_Init(TABLE_AT(s->actions, 3), "CursorDown", 1);
	cInputAction_Init(TABLE_AT(s->actions, 4), "CursorLeft", 1);
	cInputAction_Init(TABLE_AT(s->actions, 5), "CursorRight", 1);
	cInputAction_Init(TABLE_AT(s->actions, 6), "ItemUp", 1);
	cInputAction_Init(TABLE_AT(s->actions, 7), "ItemDown", 1);
	cInputAction_Init(TABLE_AT(s->actions, 8), "ItemLeft", 1);
	cInputAction_Init(TABLE_AT(s->actions, 9), "ItemRight", 1);
	cInputAction_Init(TABLE_AT(s->actions, 10), "Accept", 1);
	cInputAction_Init(TABLE_AT(s->actions, 0), "Cancel", 1);
	cInputAction_Init(TABLE_AT(s->actions, 11), "PgUp", 1);
	cInputAction_Init(TABLE_AT(s->actions, 12), "PgDown", 1);
	cInputAction_Init(TABLE_AT(s->actions, 13), "Delete", 1);
	cInputAction_Init(TABLE_AT(s->actions, 1), "OpenConsole", 1);
	s = &sets->items[1];
	cString_Set(&s->name, "Racer");
	ActionTable_Init(&s->actions, 40);
	cInputAction_Init(TABLE_AT(s->actions, 2), "Up", 0);
	cInputAction_Init(TABLE_AT(s->actions, 3), "Down", 0);
	cInputAction_Init(TABLE_AT(s->actions, 4), "Left", 0);
	cInputAction_Init(TABLE_AT(s->actions, 5), "Right", 0);
	cInputAction_Init(TABLE_AT(s->actions, 6), "Forward", 0);
	cInputAction_Init(TABLE_AT(s->actions, 7), "Backward", 0);
	cInputAction_Init(TABLE_AT(s->actions, 8), "TrustUp", 0);
	cInputAction_Init(TABLE_AT(s->actions, 9), "Boost", 0);
	cInputAction_Init(TABLE_AT(s->actions, 10), "StrafeLeft", 0);
	cInputAction_Init(TABLE_AT(s->actions, 11), "StrafeRight", 0);
	cInputAction_Init(TABLE_AT(s->actions, 12), "StrafeUp", 0);
	cInputAction_Init(TABLE_AT(s->actions, 13), "StrafeDown", 0);
	cInputAction_Init(TABLE_AT(s->actions, 14), "NextWeapon", 1);
	cInputAction_Init(TABLE_AT(s->actions, 15), "PrevWeapon", 1);
	cInputAction_Init(TABLE_AT(s->actions, 16), "Fire", 1);
	cInputAction_Init(TABLE_AT(s->actions, 17), "CM", 1);
	cInputAction_Init(TABLE_AT(s->actions, 18), "BockStrafe", 1);
	cInputAction_Init(TABLE_AT(s->actions, 19), "TargetNext", 1);
	cInputAction_Init(TABLE_AT(s->actions, 20), "TargetPrev", 1);
	cInputAction_Init(TABLE_AT(s->actions, 21), "TargetReticle", 1);
	cInputAction_Init(TABLE_AT(s->actions, 0), "MainMenu", 1);
	cInputAction_Init(TABLE_AT(s->actions, 1), "OpenConsole", 1);
	cInputAction_Init(TABLE_AT(s->actions, 22), "CenterView", 1);
	cInputAction_Init(TABLE_AT(s->actions, 23), "Scorer", 1);
	cInputAction_Init(TABLE_AT(s->actions, 24), "Laser", 1);
	cInputAction_Init(TABLE_AT(s->actions, 25), "Vulcan", 1);
	cInputAction_Init(TABLE_AT(s->actions, 26), "Devastator", 1);
	cInputAction_Init(TABLE_AT(s->actions, 27), "Tesla", 1);
	cInputAction_Init(TABLE_AT(s->actions, 28), "ATPC", 1);
	cInputAction_Init(TABLE_AT(s->actions, 29), "Swarm", 1);
	cInputAction_Init(TABLE_AT(s->actions, 30), "Inferno", 1);
	cInputAction_Init(TABLE_AT(s->actions, 31), "SwitchView", 1);
	cInputAction_Init(TABLE_AT(s->actions, 32), "Chat", 1);
	cInputAction_Init(TABLE_AT(s->actions, 33), "QMsg1", 1);
	cInputAction_Init(TABLE_AT(s->actions, 34), "QMsg2", 1);
	cInputAction_Init(TABLE_AT(s->actions, 35), "QMsg3", 1);
	cInputAction_Init(TABLE_AT(s->actions, 36), "QMsg4", 1);
	cInputAction_Init(TABLE_AT(s->actions, 37), "QMsg5", 1);
	cInputAction_Init(TABLE_AT(s->actions, 38), "QMsg6", 1);
	cInputAction_Init(TABLE_AT(s->actions, 39), "MissionInfo", 1);
	s = &sets->items[2];
	cString_Set(&s->name, "Inactive");
	ActionTable_Init(&s->actions, 9);
	cInputAction_Init(TABLE_AT(s->actions, 0), "Abort", 1);
	cInputAction_Init(TABLE_AT(s->actions, 1), "OpenConsole", 1);
	cInputAction_Init(TABLE_AT(s->actions, 2), "NextConsole", 1);
	cInputAction_Init(TABLE_AT(s->actions, 3), "PrevConsole", 1);
	cInputAction_Init(TABLE_AT(s->actions, 4), "NextShip", 1);
	cInputAction_Init(TABLE_AT(s->actions, 5), "PrevShip", 1);
	cInputAction_Init(TABLE_AT(s->actions, 6), "SeleShip", 1);
	cInputAction_Init(TABLE_AT(s->actions, 7), "ExitShip", 1);
	cInputAction_Init(TABLE_AT(s->actions, 8), "No", 1);
	s = &sets->items[3];
	cString_Set(&s->name, "Walk");
	ActionTable_Init(&s->actions, 24);
	cInputAction_Init(TABLE_AT(s->actions, 0), "MainMenu", 1);
	cInputAction_Init(TABLE_AT(s->actions, 1), "OpenConsole", 1);
	cInputAction_Init(TABLE_AT(s->actions, 2), "TurnUp", 0);
	cInputAction_Init(TABLE_AT(s->actions, 3), "TurnDown", 0);
	cInputAction_Init(TABLE_AT(s->actions, 4), "TurnLeft", 0);
	cInputAction_Init(TABLE_AT(s->actions, 5), "TurnRight", 0);
	cInputAction_Init(TABLE_AT(s->actions, 6), "Forward", 0);
	cInputAction_Init(TABLE_AT(s->actions, 7), "Backward", 0);
	cInputAction_Init(TABLE_AT(s->actions, 8), "StrafeLeft", 0);
	cInputAction_Init(TABLE_AT(s->actions, 9), "StrafeRight", 0);
	cInputAction_Init(TABLE_AT(s->actions, 10), "StrafeUp", 0);
	cInputAction_Init(TABLE_AT(s->actions, 11), "StrafeDown", 0);
	cInputAction_Init(TABLE_AT(s->actions, 12), "StrafeTurnLeft", 0);
	cInputAction_Init(TABLE_AT(s->actions, 13), "StrafeTurnRight", 0);
	cInputAction_Init(TABLE_AT(s->actions, 14), "BlockStrafeTurn", 1);
	cInputAction_Init(TABLE_AT(s->actions, 15), "BlockStrafe", 1);
	cInputAction_Init(TABLE_AT(s->actions, 16), "SwitchRunWalk", 1);
	cInputAction_Init(TABLE_AT(s->actions, 17), "Use", 1);
	cInputAction_Init(TABLE_AT(s->actions, 19), "Action", 1);
	cInputAction_Init(TABLE_AT(s->actions, 18), "Possession", 1);
	cInputAction_Init(TABLE_AT(s->actions, 20), "CameraCenter", 1);
	cInputAction_Init(TABLE_AT(s->actions, 21), "NextAction", 1);
	cInputAction_Init(TABLE_AT(s->actions, 22), "PrevAction", 1);
	cInputAction_Init(TABLE_AT(s->actions, 23), "MissionInfo", 1);
	return sets;
}

cInputAction *__fastcall cInputAction_ctor(cInputAction *a) {
	cString_ctor(&a->name);
	crt_eh_vector_ctor(a->func, sizeof(cPy_Function), MAX_LOCAL_PLAYERS, GAME_FUNC(cPy_Function_ctor, CPYFUNCTION_CTOR),
		GAME_FUNC(cPy_Function_dtor, CPYFUNCTION_DTOR));
	return a;
}

cActionSet *__fastcall cActionSet_ctor(cActionSet *s) {
	cString_ctor(&s->name);
	s->actions.items = NULL;
	s->actions.count = 0;
	return s;
}

cPlayerInput *__fastcall cPlayerInput_ctor(cPlayerInput *in) {
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
void ActionSet_FiniActionSets(void) {
	ActionSetTable_Free(&g_ActionSets);
}

/* the Global/Table.h Free of the sets */
void __fastcall ActionSetTable_Free(cActionSetTable *t) {
	if (t->items) {
		Delete_Objects(t->items, sizeof(cActionSet), GAME_FUNC(cActionSet_dtor, ACTIONSET_DTOR));
		t->items = NULL;
		t->count = 0;
	}
}

void __fastcall cInputAction_dtor(cInputAction *a) {
	crt_eh_vector_dtor(a->func, sizeof(cPy_Function), MAX_LOCAL_PLAYERS, GAME_FUNC(cPy_Function_dtor, CPYFUNCTION_DTOR));
	cString_dtor(&a->name);
}

void __fastcall cActionSet_dtor(cActionSet *s) {
	if (s->actions.items) {
		Delete_Objects(s->actions.items, sizeof(cInputAction), GAME_FUNC(cInputAction_dtor, INPUTACTION_DTOR));
		s->actions.items = NULL;
		s->actions.count = 0;
	}
	cString_dtor(&s->name);
}

void __fastcall cPlayerInput_dtor(cPlayerInput *in) {
	cPy_Function_dtor(&in->func);
}

/* the vector deleting destructors: flags 2 is an array with its count before it, 1 frees */
cInputAction *__thiscall cInputAction_Delete(cInputAction *a, uint32_t flags) {
	if (flags & 2) {
		return Delete_VectorPart(a, flags, sizeof(cInputAction), GAME_FUNC(cInputAction_dtor, INPUTACTION_DTOR));
	}
	cInputAction_dtor(a);
	if (flags & 1) {
		operator_delete_sized(a, sizeof(*a));
	}
	return a;
}

cActionSet *__thiscall cActionSet_Delete(cActionSet *s, uint32_t flags) {
	if (flags & 2) {
		return Delete_VectorPart(s, flags, sizeof(cActionSet), GAME_FUNC(cActionSet_dtor, ACTIONSET_DTOR));
	}
	cActionSet_dtor(s);
	if (flags & 1) {
		operator_delete_sized(s, sizeof(*s));
	}
	return s;
}

/* the accept key on the virtual keyboard: the character under the cursor typed, once per press */
void __thiscall cPlayerInput_PressKey(cPlayerInput *in, int player) {
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
void __thiscall cActionSet_HoldPressed(cActionSet *s) {
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

uint32_t Input_GetActionSet(void) {
	return g_CurActionSet;
}

char __thiscall cPlayerInput_GetCursorChar(cPlayerInput *in) {
	int i = (int)in->cursorY * g_VirtualKeyboard.a + (int)in->cursorX;
	if (i > g_VirtualKeyboard.len) {
		return g_VirtualKeyboard.text[g_VirtualKeyboard.len - 1];
	}
	return g_VirtualKeyboard.text[i];
}

/* the Global/Table.h Init of the two instances: new T[num] with the constructors */
void __thiscall ActionTable_Init(cInputActionTable *t, int num) {
	CHECK(!t->items && num > 0);
	t->count = num;
	t->items = New_Objects(num, sizeof(cInputAction), GAME_FUNC(cInputAction_ctor, INPUTACTION_CTOR), GAME_FUNC(cInputAction_dtor, INPUTACTION_DTOR));
	CHECK(t->items);
}

void __thiscall ActionSetTable_Init(cActionSetTable *t, int num) {
	CHECK(!t->items && num > 0);
	t->count = num;
	t->items = New_Objects(num, sizeof(cActionSet), GAME_FUNC(cActionSet_ctor, ACTIONSET_CTOR), GAME_FUNC(cActionSet_dtor, ACTIONSET_DTOR));
	CHECK(t->items);
}

void __thiscall cInputAction_Init(cInputAction *a, const char *name, byte latch) {
	a->latch = latch;
	cString_Set(&a->name, name);
	memset(a->pressed, 0, sizeof(a->pressed));
}

void __thiscall cActionSet_Init(cActionSet *s, int num, const char *name) {
	cString_Set(&s->name, name);
	ActionTable_Init(&s->actions, num);
}

void __thiscall cPad_Init(cPad *pad, int index) {
	pad->motor[0] = 0.0f;
	pad->motor[1] = 0.0f;
	pad->prev[0] = 0.0f;
	pad->prev[1] = 0.0f;
	pad->time = 0.0f;
	pad->duration = 0.0f;
	pad->onRumble = NULL;
	pad->index = index;
}

void InputMaps_Init(void) {
	g_CurActionSet = 0;
	Input_ClearActions();
	for (int i = 0; i < MAX_LOCAL_PLAYERS; i++) {
		cPad_Init(&g_Pads[i], i);
	}
}

void InputMaps_Clear(void) {
	for (int i = 0; i < MAX_LOCAL_PLAYERS; i++) {
		cPad_Stop(&g_Pads[i]);
	}
}

void InputMaps_Done(void) {
}

/* every 0.05 seconds: counts the duration down and passes the motors to the callback while one of them runs */
void __thiscall cPad_Update(cPad *pad) {
	double next = (double)(pad->time + 0.05f);

	if (!(g_pApp->vftable->GetGameTime(g_pApp) > next)) {
		return;
	}
	float d = (float)g_pApp->vftable->GetGameTime(g_pApp) - pad->time;
	pad->time = (float)g_pApp->vftable->GetGameTime(g_pApp);
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
void __thiscall cPlayerInput_MoveCursor(cPlayerInput *in, float dx, float dy) {
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
byte __thiscall cPlayerInput_SetCursorChar(cPlayerInput *in, char c) {
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

void __thiscall cInputAction_Shift(cInputAction *a) {
	for (int p = 0; p < MAX_LOCAL_PLAYERS; p++) {
		a->prev[p] = a->value[p];
		a->value[p] = 0.0f;
	}
}

void __thiscall cActionSet_Shift(cActionSet *s) {
	for (uint32_t i = 0; (int)i < s->actions.count; i++) {
		cInputAction_Shift(TABLE_AT(s->actions, i));
	}
}

byte __thiscall cInputAction_Pressed(cInputAction *a, int player) {
	return 0.5f > a->prev[player] && a->value[player] > 0.5f;
}

/* the bound functions of the players that pressed the action in this frame */
void __thiscall cInputAction_Fire(cInputAction *a) {
	for (int p = 0; p < MAX_LOCAL_PLAYERS; p++) {
		if (0.5f > a->prev[p] && a->value[p] > 0.5f) {
			cPy_Function_AddParamI(&a->func[p], p);
			cPy_Function_Execute(&a->func[p], 0);
		}
	}
}

void __thiscall cActionSet_Fire(cActionSet *s) {
	for (uint32_t i = 0; (int)i < s->actions.count; i++) {
		cInputAction_Fire(TABLE_AT(s->actions, i));
	}
}

byte __thiscall cInputAction_Released(cInputAction *a, int player) {
	return a->prev[player] > 0.5f && 0.5f > a->value[player];
}

void __thiscall cPad_Stop(cPad *pad) {
	if (pad->onRumble) {
		pad->onRumble(pad->index, 0.0f, 0.0f);
	}
	pad->motor[1] = 0.0f;
	pad->motor[0] = 0.0f;
	pad->duration = 0.0f;
}

void Input_ClearActions(void) {
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

void __thiscall cInputAction_Clear(cInputAction *a) {
	for (int p = 0; p < MAX_LOCAL_PLAYERS; p++) {
		a->value[p] = 0.0f;
		a->prev[p] = 0.0f;
	}
}

void __thiscall cActionSet_Clear(cActionSet *s) {
	for (uint32_t i = 0; (int)i < s->actions.count; i++) {
		cInputAction_Clear(TABLE_AT(s->actions, i));
	}
}

void __thiscall cPlayerInput_Clear(cPlayerInput *in) {
	memset(in->text, 0, 0x80);
	in->maxLen = 0x80;
	in->cursorX = 0.0f;
	in->cursorY = 0.0f;
	in->len = 0;
}

void Input_ResetStates(void) {
	for (uint32_t i = 0; (int)i < g_ActionSets.count; i++) {
		cActionSet_Keep(TABLE_AT(g_ActionSets, i));
	}
}

void __thiscall cInputAction_Keep(cInputAction *a) {
	for (int p = 0; p < MAX_LOCAL_PLAYERS; p++) {
		a->prev[p] = a->value[p];
	}
}

void __thiscall cActionSet_Keep(cActionSet *s) {
	for (uint32_t i = 0; (int)i < s->actions.count; i++) {
		cInputAction_Keep(TABLE_AT(s->actions, i));
	}
}

/* clears the old and the new set and reads the devices once, so that a held key does not fire in the new set */
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

void __thiscall cInputAction_Latch(cInputAction *a) {
	for (int p = 0; p < MAX_LOCAL_PLAYERS; p++) {
		if (!a->latch) {
			return;
		}
		a->pressed[p] = a->value[p] > 0.5f;
	}
}

void __thiscall cActionSet_Latch(cActionSet *s) {
	for (uint32_t i = 0; (int)i < s->actions.count; i++) {
		cInputAction_Latch(TABLE_AT(s->actions, i));
	}
}

/* the constructor of a static keyboard (rva 0x1392e0, from its initializer): the standard grid */
cVirtualKeyboard *__fastcall cVirtualKeyboard_ctor(cVirtualKeyboard *vk) {
	cVirtualKeyboard_Set(vk, "Standard");
	return vk;
}

/* "Standard", "Numbers" or "Console"; 0 for another name (the grid stays) */
byte __thiscall cVirtualKeyboard_Set(cVirtualKeyboard *vk, const char *text) {
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
void __thiscall cPlayerInput_SetInputFunc(cPlayerInput *in, const char *func) {
	in->onDone = (void (*)(int, const char *))GAME_FUNC(Input_OnTextDone, 0x13a930);
	cPy_Function_SetFunc(&in->func, func);
}

/* the motors in 0..1 (0.05 or less is off), the duration up to 30 seconds */
void __thiscall cPad_Rumble(cPad *pad, float a, float b, float c) {
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

void __thiscall cPlayerInput_SetString(cPlayerInput *in, const char *text) {
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

void Input_OnTextDone(int player, const char *text) {
	cPy_Function *f = &g_PlayerInput[player].func;

	cPy_Function_AddParamI(f, player);
	cPy_Function_AddParamS(f, text);
	cPy_Function_Execute(f, 0);
}

/* whether any action of the set is held by any player (the inactivity timer) */
byte __thiscall cActionSet_AnyActive(cActionSet *s) {
	for (uint32_t i = 0; (int)i < s->actions.count; i++) {
		for (int p = 0; p < MAX_LOCAL_PLAYERS; p++) {
			if (TABLE_AT(s->actions, i)->value[p] > 0.5f) {
				return 1;
			}
		}
	}
	return 0;
}
