/*
 * Actions/Events/CharEvents.cpp of the game: the C functions the events of the actions call on the controller of a
 * character. See include/scrap/actions.h.
 */
#include <string.h>

#include "scrap/actions.h"
#include "scrap/ai.h"
#include "scrap/walkcharentity.h"
#include "scrap/staticinit.h"

#define EVENT_STR 0x80

/* the entity name and the value string of the event */
static void Event_Read(cAbstractFile *params, char *name, char *str) {
	cAbstractFile_ReadString(params, name, EVENT_STR);
	cAbstractFile_ReadString(params, str, EVENT_STR);
}

/* the character of the event with its CHECKs */
static void *Event_Ctrl(cEntity *ent) {
	CHECK(Entity_IsType(ent, TYEN_WITH_LIFE));
	return WithLife_GetActCtrl(ent);
}

/* Actions/Events/CharEvents.cpp and SoundEvents.cpp (src/game/Actions/Events/): the C functions ("!name") the
 * events call, named like the registry; the parameters (entity name, value string, time) come in a memory file */
static void Event_AnmChangeRandomAnm(cAbstractFile *params) {
	char name[EVENT_STR], str[EVENT_STR], anim[EVENT_STR];
	float t;

	Event_Read(params, name, str);
	VFile_Read(params, &t, sizeof(t));
	cEntity *ent = World_FindEntity(name);
	CHECK(ent);
	CHECK(Entity_IsType(ent, TYEN_WITH_LIFE));
	char *strRandomizer = strchr(str, ':');
	CHECK(strRandomizer);
	*strRandomizer = 0;
	float mx = (float)crt_atof(strRandomizer + 1);
	/* from 1 for the characters of the AI, else from 0 */
	float mn = 1.0f;
	if (!(mx >= 1.0f) || !Entity_IsType(ent, TYEN_WALKCHAR) || cWalkCharEntity_GetOwnerSlot(ent) != -1) {
		mn = 0.0f;
	}
	int r = crt_rand() & 0x7fff;
	int n = (int)((float)((double)r * RAND_SCALE * (double)(mx - mn) + (double)mn) - 0.001f);
	Str_PrintfQuiet(anim, sizeof(anim), "%s%d", str, n);
	cActCtrl_Play(WithLife_GetActCtrl(ent), anim, 0, 1.0f);
}

static void Event_AnmCommand(cAbstractFile *params) {
	char name[EVENT_STR], str[EVENT_STR];

	Event_Read(params, name, str);
	cEntity *ent = World_FindEntity(name);
	CHECK(ent);
	cActCtrl_AnimCmd(Event_Ctrl(ent), str);
}

static void Event_AnmSetAction(cAbstractFile *params) {
	char name[EVENT_STR], str[EVENT_STR];

	Event_Read(params, name, str);
	cEntity *ent = World_FindEntity(name);
	CHECK(ent);
	cActCtrl_Play(Event_Ctrl(ent), str, 0, 1.0f);
}

static void Event_AnmSetDefaultLoop(cAbstractFile *params) {
	char name[EVENT_STR], str[EVENT_STR];

	Event_Read(params, name, str);
	cEntity *ent = World_FindEntity(name);
	CHECK(ent);
	cActCtrl_SetDefLoop(Event_Ctrl(ent), str, 0);
}

static void Event_AnmSetDisplAng(cAbstractFile *params) {
	char name[EVENT_STR], str[EVENT_STR];

	Event_Read(params, name, str);
	cEntity *ent = World_FindEntity(name);
	CHECK(ent);
	CHECK(Entity_IsType(ent, TYEN_WALKCHAR));
	WChar(ent)->displAng = (float)crt_atof(str);
}

static void Event_AnmStopChan(cAbstractFile *params) {
	char name[EVENT_STR], str[EVENT_STR];

	Event_Read(params, name, str);
	cEntity *ent = World_FindEntity(name);
	CHECK(ent);
	CHECK(Entity_IsType(ent, TYEN_WITH_LIFE));
	int i = crt_atoi(str);
	cActCtrl_StopChan(WithLife_GetActCtrl(ent), i);
}

static cPy_NativeFunc NativeFunc_AnmChangeRandomAnm;
static cPy_NativeFunc NativeFunc_AnmSetDefaultLoop;
static cPy_NativeFunc NativeFunc_AnmSetAction;
static cPy_NativeFunc NativeFunc_AnmStopChan;
static cPy_NativeFunc NativeFunc_AnmCommand;
static cPy_NativeFunc NativeFunc_AnmSetDisplAng;

/* the dynamic initializers of the globals here, run in the order of src/standalone/crtxc.inc */
INIT_NATIVE_FUNC(Init_NativeFunc_AnmChangeRandomAnm, &NativeFunc_AnmChangeRandomAnm, "AnmChangeRandomAnm", Event_AnmChangeRandomAnm)
INIT_NATIVE_FUNC(Init_NativeFunc_AnmSetDefaultLoop, &NativeFunc_AnmSetDefaultLoop, "AnmSetDefaultLoop", Event_AnmSetDefaultLoop)
INIT_NATIVE_FUNC(Init_NativeFunc_AnmSetAction, &NativeFunc_AnmSetAction, "AnmSetAction", Event_AnmSetAction)
INIT_NATIVE_FUNC(Init_NativeFunc_AnmStopChan, &NativeFunc_AnmStopChan, "AnmStopChan", Event_AnmStopChan)
INIT_NATIVE_FUNC(Init_NativeFunc_AnmCommand, &NativeFunc_AnmCommand, "AnmCommand", Event_AnmCommand)
INIT_NATIVE_FUNC(Init_NativeFunc_AnmSetDisplAng, &NativeFunc_AnmSetDisplAng, "AnmSetDisplAng", Event_AnmSetDisplAng)
