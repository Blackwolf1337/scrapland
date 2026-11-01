/*
 * Input/IDevice.cpp of the game: cAbstractIDevice (the entries of a device and their bindings), the list of the
 * devices, the bindings by set and action name (SInput), the key definition by the next key (ListenToDefine),
 * the inactivity timer and the DirectInput object. See include/scrap/input.h.
 */
#include <string.h>

#include "scrap/app.h"
#include "scrap/console.h"
#include "scrap/global.h"
#include "scrap/input.h"
#include "scrap/scrap.h"
#include "scrap/scrapmain.h"
#include "scrap/win32.h"
#include "scrap/world.h"

#define EXE_EMPTY GAME_STR(str_Empty)		/* "" of the exe */
#define DEV_NODE_SIZE sizeof(cInputDevNode)
#define IID_IDIRECTINPUT8A ((const void *)GAME_PTR(IID_IDIRECTINPUT8A, 0))

/* the scorer state the definition of a key switches to (rva 0x1710f0, Scorer/) */
void Scorer_SetInputState(int state);

/*
 * The walk over the device ring with the iterator of the list, as the exe: the iterator is a global, every step
 * reads it back and compares with the head.
 */
static cInputDevNode *Devices_First(void) {
	return g_InputDevices.iter = g_InputDevices.head;
}

static cInputDevNode *Devices_Next(void) {
	cInputDevNode *n = g_InputDevices.iter = g_InputDevices.iter->next;
	return n == g_InputDevices.head ? NULL : n;
}

/* the set and the action by name: 0 found, 1 no such set, 2 no such action */
static int Input_FindAction(const char *set, const char *action, uint32_t *s, uint32_t *a) {
	uint32_t i;

	for (i = 0; (int)i < g_ActionSets.count; i++) {
		if (cString_EqStr(&TABLE_AT(g_ActionSets, i)->name, set)) {
			break;
		}
	}
	if ((int)i >= g_ActionSets.count) {
		return 1;
	}
	uint32_t j;
	for (j = 0;; j++) {
		cActionSet *as = TABLE_AT(g_ActionSets, i);
		if ((int)j >= as->actions.count) {
			return 2;
		}
		if (cString_EqStr(&TABLE_AT(TABLE_AT(g_ActionSets, i)->actions, j)->name, action)) {
			break;
		}
	}
	*s = i;
	*a = j;
	return 0;
}

/* the inlined accessor of the binding of an entry (the "iEntry<nEntries" CHECKs of line 0x11f) */
static cDevEntry *Dev_Entry(cAbstractIDevice *d, int entry, int set) {
	CHECK(entry < d->nEntries);
	CHECK(set < NUM_SETS);
	return &d->entries[entry];
}

cAbstractIDevice *__thiscall cAbstractIDevice_ctor(cAbstractIDevice *self) {
	self->vftable = CABSTRACTIDEVICE_VFTABLE;
	self->entries = NULL;
	self->nEntries = 0;
	self->gvar = NULL;
	return self;
}

cFuncGvarDevices *__thiscall cFuncGvarDevices_ctor(cFuncGvarDevices *self, cAbstractIDevice *dev) {
	cGVar *v = (cGVar *)&self->base.gvar;

	cGVar_ctor(v, dev->vftable->GetName(dev), 3, GAME_STR(str_3bc4b4));	/* "User Control definition" */
	self->device = dev;
	v->base.type = GVAR_FUNCTION;
	self->base.vftable = CFUNCGVARDEVICES_VFTABLE;
	return self;
}

/* the variable stays in g_GVars, as in the original */
void __thiscall cAbstractIDevice_dtor(cAbstractIDevice *self) {
	self->vftable = CABSTRACTIDEVICE_VFTABLE;
	if (self->entries) {
		Mem_Free(self->entries);
	}
	if (self->gvar) {
		operator_delete_sized(self->gvar, sizeof(cFuncGvarDevices));
	}
}

cAbstractIDevice *__thiscall cAbstractIDevice_Delete(cAbstractIDevice *self, byte flags) {
	cAbstractIDevice_dtor(self);
	if (flags & 1) {
		operator_delete_sized(self, sizeof(cAbstractIDevice));
	}
	return self;
}

void Input_AbortListenToDefine(void) {
	g_ToDefineSet = -1;
	g_ToDefineAction = -1;
	g_ToDefinePlayer = -1;
}

byte __thiscall cAbstractIDevice_SetEntry(cAbstractIDevice *self, int entry, int set, int player, int action, byte check) {
	(void)check;
	CHECK(entry < self->nEntries);
	CHECK(set < NUM_SETS);
	self->entries[entry].sets[set].player = player;
	self->entries[entry].sets[set].action = action;
	return 1;
}

/* AssingEntry(device, entry, player, set, action): 1 no device, 2 no entry, 3 bad player, 4 no set, 5 no action */
int Input_AssignEntry(const char *a, const char *b, int c, const char *d, const char *e) {
	uint32_t set, act;

	if ((uint32_t)c > 3) {
		return 3;
	}
	int r = Input_FindAction(d, e, &set, &act);
	if (r) {
		return r == 1 ? 4 : 5;
	}
	for (cInputDevNode *n = Devices_First(); n; n = Devices_Next()) {
		if (strcmp(n->dev->vftable->GetName(n->dev), a)) {
			continue;
		}
		for (int i = 0; i < n->dev->nEntries; i++) {
			if (!strcmp(n->dev->vftable->GetEntryName(n->dev, i), b)) {
				n->dev->vftable->SetEntry(n->dev, i, set, c, act, 0);
				return 0;
			}
		}
		return 2;
	}
	return 1;
}

/* Bind(player, set, action, function): the player is not checked */
int Input_Bind(int player, const char *a, const char *b, const char *c) {
	uint32_t set, act;

	int r = Input_FindAction(a, b, &set, &act);
	if (r) {
		return r == 1 ? 2 : 3;
	}
	cInputAction *ia = TABLE_AT(TABLE_AT(g_ActionSets, set)->actions, act);
	cPy_Function_SetFunc(&ia->func[player], c);
	return 0;
}

/* CheckInactivityTime seconds without an action held calls the function of the inactivity */
void Input_CheckInactivity(float dt) {
	float t;

	if (!g_PauseInactivity) {
		if (cActionSet_AnyActive(TABLE_AT(g_ActionSets, Input_GetActionSet()))) {
			t = 0.0f;
			g_InactivityTimer = t;
		} else {
			t = g_InactivityTimer + dt;
			g_InactivityTimer = t;
		}
	} else {
		t = g_InactivityTimer;
	}
	if (t > g_InactivityTime) {
		cPy_Function_Execute(g_InactivityFunc, 1);
		g_InactivityTimer = 0.0f;
	}
}

void __thiscall InputDevices_Clear(cInputDevList *list) {
	DList_Clear(list, DEV_NODE_SIZE);
}

/* the binding of player to action in set off every entry of every device; check spares the reserved entries */
int Input_ClearBinding(uint32_t player, int set, int action, byte check) {
	if (player > 3) {
		return 1;
	}
	for (cInputDevNode *n = Devices_First(); n; n = Devices_Next()) {
		for (int i = 0; i < n->dev->nEntries; i++) {
			cDevEntry *e = Dev_Entry(n->dev, i, set);
			if ((uint32_t)e->sets[set].player == player && e->sets[set].action == action &&
				(!n->dev->vftable->IsReserved(n->dev, i) || !check)) {
				n->dev->vftable->SetEntry(n->dev, i, set, -1, -1, 0);
			}
		}
	}
	return 0;
}

/* ClearDefinedList(player, set, action[, check]): 1 bad player, 2 no set, 3 no action */
int Input_ClearDefinedList(int player, const char *a, const char *b, int flag) {
	uint32_t set, act;

	int r = Input_FindAction(a, b, &set, &act);
	if (r) {
		return r + 1;
	}
	return Input_ClearBinding(player, set, act, (byte)flag);
}

void __thiscall cAbstractIDevice_ClearEntries(cAbstractIDevice *self) {
	for (int i = 0; i < self->nEntries; i++) {
		cDevEntry *e = &self->entries[i];
		for (int s = 0; s < NUM_SETS; s++) {
			e->sets[s].player = -1;
			e->sets[s].action = -1;
		}
		e->prev = 0.0f;
		e->value = 0.0f;
	}
}

/* the label of the entry bound to player and action, the one of the named device first; "" for none */
const char *Input_FindBinding(uint32_t player, int set, int action, const char *device, byte check) {
	const char *r = EXE_EMPTY;

	if (player > 3) {
		return r;
	}
	for (cInputDevNode *n = Devices_First(); n; n = Devices_Next()) {
		for (int i = 0; i < n->dev->nEntries; i++) {
			cDevEntry *e = Dev_Entry(n->dev, i, set);
			if ((uint32_t)e->sets[set].player != player || e->sets[set].action != action ||
				(n->dev->vftable->IsReserved(n->dev, i) && check)) {
				continue;
			}
			if (!strcmp(n->dev->vftable->GetName(n->dev), device)) {
				return n->dev->vftable->GetEntryLabel(n->dev, i);
			}
			r = n->dev->vftable->GetEntryLabel(n->dev, i);
		}
	}
	return r;
}

char *Input_GetDefinedEntry(int player, const char *a, const char *b, const char *c, int flag) {
	uint32_t set, act;

	if (Input_FindAction(a, b, &set, &act)) {
		return EXE_EMPTY;
	}
	return (char *)Input_FindBinding(player, set, act, c, (byte)flag);
}

/* "device:label, " for every entry bound to player and action, without the last ", " */
int Input_ListBindings(uint32_t player, int set, int action, char *buf, int size, byte check) {
	buf[0] = 0;
	if (player > 3) {
		return 1;
	}
	for (cInputDevNode *n = Devices_First(); n; n = Devices_Next()) {
		for (int i = 0; i < n->dev->nEntries; i++) {
			cDevEntry *e = Dev_Entry(n->dev, i, set);
			if ((uint32_t)e->sets[set].player != player || e->sets[set].action != action ||
				(n->dev->vftable->IsReserved(n->dev, i) && check)) {
				continue;
			}
			int len = (int)strlen(buf);
			cAbstractIDevice *d = n->dev;
			const char *label = d->vftable->GetEntryLabel(d, i);
			const char *name = d->vftable->GetName(d);
			Str_Printf(buf + len, size - len - 1, "%s:%s, ", name, label);
		}
	}
	if (buf[0]) {
		buf[strlen(buf) - 2] = 0;
	}
	return 0;
}

int Input_GetDefinedList(int player, const char *a, const char *b, char *buf, int size, int flag) {
	uint32_t set, act;

	buf[0] = 0;
	int r = Input_FindAction(a, b, &set, &act);
	if (r) {
		return r + 1;
	}
	return Input_ListBindings(player, set, act, buf, size, (byte)flag);
}

void __thiscall cAbstractIDevice_GetEntry(cAbstractIDevice *self, int entry, int set, int *player, int *action) {
	CHECK(entry < self->nEntries);
	CHECK(set < NUM_SETS);
	*player = self->entries[entry].sets[set].player;
	*action = self->entries[entry].sets[set].action;
}

/* GetEntry(device, entry, set): the player and the action name bound to it; 1 no device, 2 no entry, 3 no set */
int Input_GetEntry(const char *a, const char *b, const char *c, int *index, char **name) {
	uint32_t set;

	for (set = 0; (int)set < g_ActionSets.count; set++) {
		if (cString_EqStr(&TABLE_AT(g_ActionSets, set)->name, c)) {
			break;
		}
	}
	if ((int)set >= g_ActionSets.count) {
		return 3;
	}
	for (cInputDevNode *n = Devices_First(); n; n = Devices_Next()) {
		if (strcmp(n->dev->vftable->GetName(n->dev), a)) {
			continue;
		}
		for (int i = 0; i < n->dev->nEntries; i++) {
			if (strcmp(n->dev->vftable->GetEntryName(n->dev, i), b)) {
				continue;
			}
			cDevEntry *e = Dev_Entry(n->dev, i, set);
			*index = e->sets[set].player;
			uint32_t act = e->sets[set].action;
			if (act == 0xffffffffu) {
				*name = EXE_EMPTY;
				return 0;
			}
			*name = TABLE_AT(TABLE_AT(g_ActionSets, set)->actions, act)->name;
			return 0;
		}
		return 2;
	}
	return 1;
}

void __thiscall cAbstractIDevice_InitEntries(cAbstractIDevice *self, int n) {
	self->nEntries = n;
	self->entries = Mem_Alloc(n * sizeof(cDevEntry));
	cAbstractIDevice_ClearEntries(self);
}

/* ListenToDefine(player, set, action, function): the next key pressed is bound; 1 bad player, 2 no set, 3 no action */
int Input_ListenToDefine(int player, const char *a, const char *b, const char *c) {
	uint32_t set, act;

	if ((uint32_t)player > 3) {
		return 1;
	}
	int r = Input_FindAction(a, b, &set, &act);
	if (r) {
		return r + 1;
	}
	Input_SetDefineFunc(c);
	Input_SetToDefine(player, act, set);
	return 0;
}

void Input_SetToDefine(int player, int action, uint32_t set) {
	CHECK((int)set < NUM_SETS);
	CHECK(action < TABLE_AT(g_ActionSets, set)->actions.count);
	CHECK(player < MAX_LOCAL_PLAYERS);
	CHECK((int)set >= 0);
	CHECK(action >= 0);
	CHECK(player >= 0);
	g_ToDefinePlayer = player;
	g_ToDefineSet = set;
	g_ToDefineAction = action;
}

/* the lines "set.action.player = entry" of the device up to "name = <end>" */
void __thiscall cFuncGvarDevices_Load(cFuncGvarDevices *self, void *f) {
	char line[0x200];
	char *value;

	cAbstractIDevice_ClearEntries(self->device);
	while (crt_fgets(line, sizeof(line), f)) {
		if (!Cfg_ParseLine(line, &value)) {
			continue;
		}
		if (!crt_stricmp(line, self->base.gvar.name) && !crt_stricmp(value, "<end>")) {
			break;
		}
		char *dot = strstr(line, ".");
		if (!dot) {
			continue;
		}
		*dot = 0;
		uint32_t set;
		for (set = 0; set < NUM_SETS; set++) {
			if (!cString_ICmpStr(&TABLE_AT(g_ActionSets, set)->name, line)) {
				break;
			}
		}
		if (set == NUM_SETS) {
			continue;
		}
		char *action = dot + 1;
		char *dot2 = strstr(action, ".");
		if (!dot2) {
			continue;
		}
		*dot2 = 0;
		uint32_t a;
		for (a = 0; (int)a < TABLE_AT(g_ActionSets, set)->actions.count; a++) {
			if (!cString_ICmpStr(&TABLE_AT(TABLE_AT(g_ActionSets, set)->actions, a)->name, action)) {
				break;
			}
		}
		/* the original compares the count of the set with 4, not the index */
		if (TABLE_AT(g_ActionSets, set)->actions.count == 4) {
			continue;
		}
		uint32_t player = crt_atoi(dot2 + 1);
		if (player > 3) {
			continue;
		}
		int e;
		for (e = 0; e < self->device->nEntries; e++) {
			if (!crt_stricmp(value, self->device->vftable->GetEntryName(self->device, e))) {
				break;
			}
		}
		if (e == self->device->nEntries) {
			continue;
		}
		self->device->vftable->SetEntry(self->device, e, set, player, a, 0);
	}
}

/* a frame: actions shifted, devices read, the bound functions and the inactivity */
void Input_Update(float dt) {
	Input_UpdateActions();
	Input_UpdateDevices(dt);
	cActionSet_HoldPressed(TABLE_AT(g_ActionSets, Input_GetActionSet()));
	cActionSet_Fire(TABLE_AT(g_ActionSets, Input_GetActionSet()));
	Input_CheckInactivity(dt);
}

/* the callback of ListenToDefine with a function: called once */
void Input_OnDefined(void) {
	cPy_Function_Execute(g_DefineFunc, 0);
	cPy_Function_SetFunc(g_DefineFunc, NULL);
}

void Input_UpdateDevices(float dt) {
	for (cInputDevNode *n = Devices_First(); n; n = Devices_Next()) {
		n->dev->vftable->Update(n->dev, dt);
	}
}

void Input_ResetInactivity(void) {
	g_InactivityTimer = 0.0f;
	cPy_Function_Clear(g_InactivityFunc);
	g_InactivityTime = g_InactivityTimeDef;
}

void Input_ResetToDefault(void) {
	for (cInputDevNode *n = Devices_First(); n; n = Devices_Next()) {
		cAbstractIDevice_ClearEntries(n->dev);
		n->dev->vftable->SetDefaults(n->dev);
		n->dev->vftable->SetGameDefaults(n->dev);
	}
}

void __thiscall cFuncGvarDevices_Save(cFuncGvarDevices *self, void *f) {
	char buf[0x100];

	for (int set = 0; set < NUM_SETS; set++) {
		for (int i = 0; i < self->device->nEntries; i++) {
			cDevEntry *e = Dev_Entry(self->device, i, set);
			int player = e->sets[set].player;
			int action = e->sets[set].action;
			if (player == -1 || action == -1) {
				continue;
			}
			const char *aname = TABLE_AT(TABLE_AT(g_ActionSets, set)->actions, action)->name;
			int r = Str_Printf(buf, sizeof(buf), "%s.%s.%d", TABLE_AT(g_ActionSets, set)->name, aname, player);
			CHECK(r < 254);
			crt_fprintf(f, "\t\t%-40s = %s\n", buf, self->device->vftable->GetEntryName(self->device, i));
		}
	}
	crt_fprintf(f, "%s = <end>\n\n\n", self->base.gvar.name);
}

/* an axis as two entries: entry the positive half, entry + 1 the negative one */
void __thiscall cAbstractIDevice_SetAxis(cAbstractIDevice *self, int entry, float value) {
	cAbstractIDevice_SetValue(self, entry, value > 0.0f ? value : 0.0f);
	cAbstractIDevice_SetValue(self, entry + 1, 0.0f > value ? -value : 0.0f);
}

/* the new value of an entry: defines the waiting key on a press, else raises the action it is bound to */
void __thiscall cAbstractIDevice_SetValue(cAbstractIDevice *self, int entry, float value) {
	CHECK(self->nEntries > entry);
	cDevEntry *e = &self->entries[entry];
	e->prev = e->value;
	self->entries[entry].value = value;
	if (g_ToDefineAction != -1) {
		Scorer_SetInputState(2);
		CHECK((int)g_ToDefineSet < NUM_SETS);
		CHECK(g_ToDefineAction < TABLE_AT(g_ActionSets, g_ToDefineSet)->actions.count);
		CHECK(g_ToDefinePlayer < MAX_LOCAL_PLAYERS);
		CHECK((int)g_ToDefineSet >= 0);
		CHECK(g_ToDefineAction >= 0);
		CHECK(g_ToDefinePlayer >= 0);
		e = &self->entries[entry];
		if (!(0.5f >= e->prev) || !(e->value >= 0.5f)) {
			return;
		}
		if (self->vftable->SetEntry(self, entry, g_ToDefineSet, g_ToDefinePlayer, g_ToDefineAction, 1)) {
			void (*f)(void) = g_OnDefined;
			g_ToDefineSet = -1;
			g_ToDefineAction = -1;
			g_ToDefinePlayer = -1;
			if (f) {
				f();
				g_OnDefined = NULL;
			}
			return;
		}
	}
	uint32_t set = Input_GetActionSet();
	cDevEntry *b = &self->entries[entry];
	int player = b->sets[set].player;
	uint32_t action = b->sets[set].action;
	if (player == -1 && action == 0xffffffffu) {
		return;
	}
	/* player -1 with an action reads before value[], as the original */
	float *v = (float *)((byte *)TABLE_AT(TABLE_AT(g_ActionSets, set)->actions, action) + 0x18) + player;
	if (value > *v) {
		v = (float *)((byte *)TABLE_AT(TABLE_AT(g_ActionSets, set)->actions, action) + 0x18) + player;
		*v = value;
	}
}

void Input_SetDefineCallback(void (*f)(void)) {
	CHECK(f != NULL);
	g_OnDefined = f;
}

void Input_SetDefineFunc(const char *func) {
	CHECK(func != NULL);
	if (!*func) {
		g_OnDefined = NULL;
		return;
	}
	g_OnDefined = (void (*)(void))(void *)Input_OnDefined;	/* Input_OnDefined of the exe */
	cPy_Function_SetFunc(g_DefineFunc, func);
}

byte __thiscall cAbstractIDevice_IsReserved(cAbstractIDevice *self, int entry) {
	(void)self;
	(void)entry;
	return 0;
}

void Input_ReleaseAll(int arg) {
	if (g_DInput) {
		Keyboard_Acquire((byte)arg);
		Pad_Acquire((byte)arg);
		Mouse_Acquire((byte)arg);
	}
}

void DInput_Release(void) {
	exe_IDirectInput8 *di = g_DInput;

	if (di) {
		di->lpVtbl->Release(di);
		g_DInput = NULL;
	}
}

void Input_CloseDevices(void) {
	if (g_pApp->flags & APPF_DEDICATED) {
		g_DInput = NULL;
		return;
	}
	Keyboard_Close();
	Pad_Close();
	Mouse_Close();
	DInput_Release();
	InputDevices_Clear(&g_InputDevices);
}

void Input_OpenDevices(void) {
	if (g_pApp->flags & APPF_DEDICATED) {
		g_DInput = NULL;
		return;
	}
	if (!g_DInput) {
		exe_DirectInput8Create(App_GetHInstance(), 0x800, IID_IDIRECTINPUT8A, (void **)&g_DInput, NULL);
	}
	Mouse_Init();
	Pad_Init();
	Keyboard_Init();
	if (g_DInput) {
		Keyboard_Acquire(1);
		Pad_Acquire(1);
		Mouse_Acquire(1);
	}
}

void __thiscall InputDevices_Add(cInputDevList *list, cAbstractIDevice **dev) {
	cInputDevNode *n = DList_NewNode(list, DEV_NODE_SIZE);

	n->dev = *dev;
	DList_LinkFirst(list, n);
}

/* the default bindings of player 0 from a table of entry, set, action (not a function of the exe) */
void Device_SetEntries(cAbstractIDevice *d, const byte (*t)[3], int n) {
	for (int i = 0; i < n; i++) {
		cAbstractIDevice_SetEntry(d, t[i][0], t[i][1], 0, t[i][2], 0);
	}
}
