/*
 * Entities/Char/Type/Sentinel.cpp of the game: the sentinel, who points at the character it watches and calls the
 * others on it (include/scrap/chartype.h).
 */
#include <string.h>

#include "scrap/chartype.h"

/* params: the sentinel: ActionInit unless it runs */
void ChType_SentinelActionPress(cAbstractFile *params) {
	char name[0x80];

	cAbstractFile_ReadString(params, name, 0x80);
	cEntity *v = ChType_CheckChar(name);
	if (!cActCtrl_IsActive(ChType_ActCtrl(v), "ActionInit")) {
		cActCtrl_Play(ChType_ActCtrl(v), "ActionInit", 0, 1.0f);
	}
}

/* params: the sentinel: Relax when ActionInit or Action runs */
void ChType_SentinelActionRelease(cAbstractFile *params) {
	char name[0x80];

	cAbstractFile_ReadString(params, name, 0x80);
	cEntity *v = ChType_CheckChar(name);
	if (cActCtrl_IsActive(ChType_ActCtrl(v), "ActionInit") || cActCtrl_IsActive(ChType_ActCtrl(v), "Action")) {
		cActCtrl_Play(ChType_ActCtrl(v), "Relax", 1, 1.0f);
	}
}

/* the character e watches (its victim, a WalkChar); none while the alarm runs in the chase mode */
cEntity *ChType_SentinelTarget(cEntity *e) {
	cWorld *w = g_pWorld;

	if (w->alarm.on && World_AlarmChase(w)) {
		return NULL;
	}
	if (!e) {
		return NULL;
	}
	cEntity *t = WChar_Victim(e);
	if (!t || !t->vftable->IsType(t, TYEN_WALKCHAR)) {
		return NULL;
	}
	return t;
}

byte ChType_SentinelCanCall(void) {
	cWorld *w = g_pWorld;

	if (w->alarm.on && World_AlarmChase(w)) {
		return 0;
	}
	return 1;
}

/* params: the sentinel of the player, an event, a float. The character it points at raises the alarm (a sentinel,
 * gear or police at once), else the others are called on it; the view of the player turns to it and its logic
 * hears of it. Gatekeepers and Sebastian do not count. */
void ChType_SentinelPoint(cAbstractFile *params) {
	char name[0x80], event[0x80];
	vec3_t tmp;

	ChType_ReadParams(params, name, event);
	cEntity *v = ChType_CheckChar(name);
	if (v != ChType_Usr0()) {
		return;
	}
	cEntity *t = WChar_Victim(v);
	if (!t || !t->vftable->IsType(t, TYEN_WALKCHAR) || !strcmp(ChType_ClassName(t), "GateKeeper") ||
		!strcmp(ChType_ClassName(t), "Sebastian")) {
		return;
	}
	if (!strcmp(ChType_ClassName(t), "Sentinel")) {
		Alarm_Set(&g_pWorld->alarm, 1.0f);
	} else {
		if (!g_pWorld->alarm.on && !strcmp(ChType_ClassName(t), "Gear")) {
			Alarm_Set(&g_pWorld->alarm, 1.0f);
		} else if (!g_pWorld->alarm.on && !strcmp(ChType_ClassName(t), "Police")) {
			Alarm_Set(&g_pWorld->alarm, 1.0f);
		} else {
			if (ChType_SentinelCanCall()) {
				AICtrl_GearsChase(&g_pWorld->logic.aiCtrl, t);
			}
			AICtrl_SendSentinelToWatch(&g_pWorld->logic.aiCtrl, v->vftable->GetPos(v, &tmp));
		}
		if (!WChar_Control(v)->target.target) {
			cWalkCharControl_SetTarget(WChar_Control(v), t);
		}
	}
	cLocalLogic *l = Logic_GetLocalLogic(t);
	if (l) {
		ChLogic_Vtbl(l)->OnSeen(l, v);
	}
}
