/*
 * Logic/AlarmStatus.cpp of the game (src/game/Logic/AlarmStatus.c): the alarm of the world (cAlarm, 0x410 bytes,
 * embedded in cWorld at 0x1c68, g_pWorld->alarm).
 */
#ifndef SCRAP_ALARM_H
#define SCRAP_ALARM_H

#include "scrap/global.h"

typedef struct cAlarm {
	float grow;			/* change per second (Scrap.SetAlarmGrow) */
	float value;			/* 0 .. 1 */
	byte on;			/* set at 1, cleared at 0 */
	char chars[0x200];		/* the type of the player character (Scrap.GetAlarmChars) */
	char lastChars[0x200];		/* the type when the alarm went on */
	byte sameChar;			/* the alarm is on and lastChars is the type of now (or not guaranteed) */
	byte notDefChar;		/* the type is not UsrDefChar (or not guaranteed) */
	byte notGuaranteed;		/* the type is not in GuaranteedChars */
	byte hasGuaranteed;		/* GuaranteedChars is set */
	byte locked;			/* Scrap.SetLockAlarm: no changes */
	byte tauntReady;		/* the police taunt of the next full alarm */
	byte pad_40f;
} cAlarm;

cAlarm *Alarm_ctor(cAlarm *self);
void Alarm_Update(cAlarm *self, float dt);
/* the sign of the grow rate (-1, 0, 1) */
int Alarm_GetValue(cAlarm *self);
byte Alarm_IsLocked(cAlarm *self);
void Alarm_SetOn(cAlarm *self, byte on);
void Alarm_Set(cAlarm *self, float level);
void Alarm_SetChar(cAlarm *self, const char *name);
void Alarm_SetGrow(cAlarm *self, int grow);
void Alarm_SetLock(cAlarm *self, int lock);

#endif
