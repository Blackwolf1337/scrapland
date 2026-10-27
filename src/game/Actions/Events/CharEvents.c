/*
 * Actions/Events/CharEvents.cpp of the game: the C functions the events of the actions call on the controller of a
 * character. See include/scrap/actions.h.
 */
#include <string.h>

#include "scrap/actions.h"
#include "scrap/ai.h"
#include "scrap/walkcharentity.h"

#define EVENT_STR 0x80
#define RAND_SCALE 0x1.0002000400080p-15	/* 1 / 32767 (rva 0x3a3d60) */

/* the entity name and the value string of the event */
static void Event_Read(cAbstractFile *params, char *name, char *str) {
	cAbstractFile_ReadString(params, name, EVENT_STR);
	cAbstractFile_ReadString(params, str, EVENT_STR);
}

/* the character of the event with its CHECKs */
static void *Event_Ctrl(cEntity *ent) {
	CHECK(ent->vftable->IsType(ent, TYEN_WITH_LIFE));
	return WithLife_Vtbl(ent)->GetActCtrl(ent);
}

void Event_AnmChangeRandomAnm(cAbstractFile *params) {
	char name[EVENT_STR], str[EVENT_STR], anim[EVENT_STR];
	float t;

	Event_Read(params, name, str);
	params->vftable->Read(params, &t, sizeof(t));
	cEntity *ent = World_FindEntity(name);
	CHECK(ent);
	CHECK(ent->vftable->IsType(ent, TYEN_WITH_LIFE));
	char *strRandomizer = strchr(str, ':');
	CHECK(strRandomizer);
	*strRandomizer = 0;
	float mx = (float)crt_atof(strRandomizer + 1);
	/* from 1 for the characters of the AI, else from 0 */
	float mn = 1.0f;
	if (!(mx >= 1.0f) || !ent->vftable->IsType(ent, TYEN_WALKCHAR) || cWalkCharEntity_GetOwnerSlot(ent) != -1) {
		mn = 0.0f;
	}
	int r = crt_rand() & 0x7fff;
	int n = (int)((float)((double)r * RAND_SCALE * (double)(mx - mn) + (double)mn) - 0.001f);
	Str_PrintfQuiet(anim, sizeof(anim), "%s%d", str, n);
	cActCtrl_Play(WithLife_Vtbl(ent)->GetActCtrl(ent), anim, 0, 1.0f);
}

void Event_AnmCommand(cAbstractFile *params) {
	char name[EVENT_STR], str[EVENT_STR];

	Event_Read(params, name, str);
	cEntity *ent = World_FindEntity(name);
	CHECK(ent);
	cActCtrl_AnimCmd(Event_Ctrl(ent), str);
}

void Event_AnmSetAction(cAbstractFile *params) {
	char name[EVENT_STR], str[EVENT_STR];

	Event_Read(params, name, str);
	cEntity *ent = World_FindEntity(name);
	CHECK(ent);
	cActCtrl_Play(Event_Ctrl(ent), str, 0, 1.0f);
}

void Event_AnmSetDefaultLoop(cAbstractFile *params) {
	char name[EVENT_STR], str[EVENT_STR];

	Event_Read(params, name, str);
	cEntity *ent = World_FindEntity(name);
	CHECK(ent);
	cActCtrl_SetDefLoop(Event_Ctrl(ent), str, 0);
}

void Event_AnmSetDisplAng(cAbstractFile *params) {
	char name[EVENT_STR], str[EVENT_STR];

	Event_Read(params, name, str);
	cEntity *ent = World_FindEntity(name);
	CHECK(ent);
	CHECK(ent->vftable->IsType(ent, TYEN_WALKCHAR));
	WChar(ent)->displAng = (float)crt_atof(str);
}

void Event_AnmStopChan(cAbstractFile *params) {
	char name[EVENT_STR], str[EVENT_STR];

	Event_Read(params, name, str);
	cEntity *ent = World_FindEntity(name);
	CHECK(ent);
	CHECK(ent->vftable->IsType(ent, TYEN_WITH_LIFE));
	int i = crt_atoi(str);
	cActCtrl_StopChan(WithLife_Vtbl(ent)->GetActCtrl(ent), i);
}
