/*
 * Logic/AlarmStatus.cpp of the game (src/game/Logic/AlarmStatus.c): the alarm of the world (cAlarm, 0x410 bytes,
 * embedded in cWorld at 0x1c68, g_pWorld->alarm).
 */
#ifndef SCRAP_ALARM_H
#define SCRAP_ALARM_H

#include "scrap/global.h"

typedef struct cAlarm {
	float grow;			/* 0x000 change per second (Scrap.SetAlarmGrow) */
	float value;			/* 0x004 0 .. 1 */
	byte on;			/* 0x008 set at 1, cleared at 0 */
	char chars[0x200];		/* 0x009 the type of the player character (Scrap.GetAlarmChars) */
	char lastChars[0x200];		/* 0x209 the type when the alarm went on */
	byte sameChar;			/* 0x409 the alarm is on and lastChars is the type of now (or not guaranteed) */
	byte notDefChar;		/* 0x40a the type is not UsrDefChar (or not guaranteed) */
	byte notGuaranteed;		/* 0x40b the type is not in GuaranteedChars */
	byte hasGuaranteed;		/* 0x40c GuaranteedChars is set */
	byte locked;			/* 0x40d Scrap.SetLockAlarm: no changes */
	byte tauntReady;		/* 0x40e the police taunt of the next full alarm */
	byte pad_40f;
} cAlarm;
SD_OFFSET(cAlarm, lastChars, 0x209);
SD_OFFSET(cAlarm, locked, 0x40d);
SD_SIZE(cAlarm, 0x410);

cAlarm *__thiscall Alarm_ctor(cAlarm *self);
void __fastcall Alarm_Reset(cAlarm *self);
void __thiscall Alarm_Update(cAlarm *self, float dt);
/* the sign of the grow rate (-1, 0, 1) */
int __thiscall Alarm_GetValue(cAlarm *self);
byte __thiscall Alarm_IsLocked(cAlarm *self);
void __thiscall Alarm_SetOn(cAlarm *self, byte on);
void __thiscall Alarm_SetGrowRate(cAlarm *self, float grow);
void __thiscall Alarm_Set(cAlarm *self, float level);
void __thiscall Alarm_SetChar(cAlarm *self, const char *name);
void __thiscall Alarm_SetGrow(cAlarm *self, int grow);
void __thiscall Alarm_SetLock(cAlarm *self, int lock);

#endif
