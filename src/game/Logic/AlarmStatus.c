/*
 * Logic/AlarmStatus.cpp of the game: the alarm of the world (cAlarm at g_pWorld->alarm, include/scrap/logic.h). It
 * grows or falls by its rate every frame; at 1 it goes on (the AlarmCallback, the gears and sentinels of an indoor
 * map after the player), at 0 off. Nothing changes while it is locked or the action set is not the one of the
 * game (1 or 3). The type of the player character is kept with the tests of UsrDefChar and GuaranteedChars.
 */
#include <string.h>

#include "scrap/chartype.h"
#include "scrap/console.h"
#include "scrap/cstring.h"
#include "scrap/input.h"
#include "scrap/vehicleentity.h"

/* the alarm takes changes */
static int Alarm_Open(const cAlarm *self) {
	if (self->locked) {
		return 0;
	}
	return Input_GetActionSet() == 1 || Input_GetActionSet() == 3;
}

/* the strcpy of the original between the 0x200 buffers (both are written by Str_Copy) */
static void Alarm_CopyChars(char *dst, const char *src) {
	size_t n = strnlen(src, 0x1ff);

	memcpy(dst, src, n);
	dst[n] = 0;
}

void *__thiscall Alarm_ctor(void *alarm) {
	Alarm_Reset(alarm);
	cPy_Function_SetFunc(g_AlarmCallback, LOGIC_EMPTY);
	return alarm;
}

/* the sign of the grow rate (-1, 0, 1), not of the level: the exe reads the float at +0 */
int __thiscall Alarm_GetValue(void *alarm) {
	float v = ((cAlarm *)alarm)->grow;

	if (v == 0.0f) {
		return 0;
	}
	return !(0.0f > v) * 2 - 1;
}

byte __thiscall Alarm_IsLocked(void *alarm) {
	cAlarm *self = alarm;

	if (self->locked) {
		return 1;
	}
	if (Input_GetActionSet() != 1 && Input_GetActionSet() != 3) {
		return 1;
	}
	return 0;
}

void __fastcall Alarm_Reset(void *alarm) {
	cAlarm *self = alarm;

	self->value = 0.0f;
	self->on = 0;
	self->chars[0] = 0;
	self->grow = -0.1f;
	self->lastChars[0] = 0;
	self->sameChar = 0;
	self->notDefChar = 0;
	self->hasGuaranteed = 0;
	self->locked = 0;
	self->notGuaranteed = 0;
	self->tauntReady = 1;
	cString_Set(&g_GuaranteedChars, LOGIC_EMPTY);
	cString_Assign(g_PoliceTauntRace, g_PoliceTauntRace + 1);
}

/* on: the callback and the attack; while on the type is kept and the gears and a sentinel go for the player */
void __thiscall Alarm_SetOn(void *alarm, byte on) {
	cAlarm *self = alarm;
	vec3_t tmp;

	if (!Alarm_Open(self)) {
		return;
	}
	if (self->on != on) {
		self->on = on;
		Con_Debugf("Alarm status changed to %d\n", on);
		cPy_Function_AddParamI(g_AlarmCallback, self->on != 0);
		cPy_Function_Execute(g_AlarmCallback, 0);
		if (self->on) {
			g_GenCharAttackActive = 1;
		}
	}
	if (!self->on) {
		return;
	}
	Alarm_CopyChars(self->lastChars, self->chars);
	if (g_IsMapOutDoor == 1) {
		return;
	}
	cEntity *usr = cWorld_GetUsrEntity(g_pWorld, 0);
	if (!usr || !usr->vftable->IsType(usr, TYEN_WITH_LIFE)) {
		return;
	}
	AICtrl_GearAttack(g_pWorld->aiCtrl, usr, 1);
	if (AICtrl_SendAccuser(g_pWorld->aiCtrl, 1)) {
		return;
	}
	AICtrl_SendSentinelToWatch(g_pWorld->aiCtrl, usr->vftable->GetPos(usr, &tmp));
}

void __thiscall Alarm_SetGrowRate(void *alarm, float grow) {
	cAlarm *self = alarm;

	if (Alarm_Open(self)) {
		self->grow = grow;
	}
}

/* 1 grow, 0 stop, -1 fall (slower in the mission map of an outdoor map and in an easy single game) */
void __thiscall Alarm_SetGrow(void *alarm, int grow) {
	cAlarm *self = alarm;

	if (!Alarm_Open(self)) {
		return;
	}
	if (grow != -1) {
		if (grow == 0) {
			self->grow = 0.0f;
			return;
		}
		if (grow == 1) {
			self->grow = g_AlarmGrowDelta;
			return;
		}
		CHECK(0);
		return;
	}
	self->grow = g_AlarmFallDelta;
	if (g_IsMapOutDoor) {
		const char *map = SaveVar_Get("Mission.Map");
		if (map) {
			const char *level = g_pApp->levelPath;
			if (strlen(level) > 7 && !crt_stricmp(map, level + 7)) {
				self->grow *= g_AlarmFallMission;
			}
		}
	}
	if (World_IsEasySingle()) {
		self->grow *= g_EasyAlarmFallFactor;
	}
}

void __thiscall Alarm_SetLock(void *alarm, int lock) {
	((cAlarm *)alarm)->locked = (byte)lock;
}

/* the level clamped to 0 .. 1, the alarm off at 0 and on at 1 (the first full alarm of an outdoor map taunts) */
void __thiscall Alarm_Set(void *alarm, float level) {
	cAlarm *self = alarm;

	if (!Alarm_Open(self)) {
		return;
	}
	self->value = level;
	if (level <= 0.0f) {
		self->value = 0.0f;
		Alarm_SetOn(self, 0);
		self->tauntReady = 1;
		return;
	}
	if (level >= 1.0f) {
		self->value = 1.0f;
		Alarm_SetOn(self, 1);
		if (self->tauntReady && g_IsMapOutDoor == 1) {
			Taunts_Play(LOGIC_EMPTY, *g_PoliceTauntRace, crt_rand() % 2 + 1);
			self->tauntReady = 0;
		}
	}
}

void __thiscall Alarm_SetChar(void *alarm, const char *name) {
	cAlarm *self = alarm;

	if (Alarm_Open(self)) {
		Str_Copy(self->lastChars, name, 0x200);
		Str_Copy(self->chars, name, 0x200);
	}
}

/* the level by the rate, the type of the player (the class of a character, the pilot of a ship) and its tests */
void __thiscall Alarm_Update(void *alarm, float dt) {
	cAlarm *self = alarm;
	char buf[0x80];

	if (!Alarm_Open(self)) {
		return;
	}
	Alarm_Set(self, self->grow * dt + self->value);
	cEntity *usr = ChType_Usr0();
	if (!usr) {
		return;
	}
	if (usr->vftable->IsType(usr, TYEN_WALKCHAR)) {
		Str_Copy(self->chars, *(char **)(ChType_ActClass(ChType_ActCtrl(usr)) + 4), 0x200);
	} else if (usr->vftable->IsType(usr, TYEN_CAR)) {
		const char *pilot = Veh_Vtbl((cVehicleEntity *)usr)->GetPilot((cVehicleEntity *)usr, 0);
		if (*pilot) {
			Str_Copy(self->chars, pilot + 2, 0x200);
		}
	}
	const char *chars = self->chars;
	self->notDefChar = cString_CmpStr(g_UsrDefChar, chars) != 0;
	self->hasGuaranteed = g_GuaranteedChars && *g_GuaranteedChars;
	if (!self->hasGuaranteed) {
		self->notGuaranteed = 0;
		self->sameChar = self->on && !strcmp(self->lastChars, chars);
		return;
	}
	Str_PrintfQuiet(buf, 0x80, ",%s,", chars);
	byte notGuaranteed = strstr(g_GuaranteedChars, buf) == NULL;
	self->notGuaranteed = notGuaranteed;
	self->sameChar = self->on && (!strcmp(self->lastChars, chars) || notGuaranteed);
	self->notDefChar = self->notDefChar || notGuaranteed;
}
