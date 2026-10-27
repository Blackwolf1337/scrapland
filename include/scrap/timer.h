/*
 * The game timer of Global/Timer.cpp (src/game/Global/Timer.c): real time in, scaled game time out. A speed
 * change can blend from speedFrom to speedTo over blendTime.
 */
#ifndef SCRAP_TIMER_H
#define SCRAP_TIMER_H

#include "scrap/shared.h"

typedef struct cTimer {
	float time;			/* 0x00 last real time, -1 before the first update */
	float prevTime;			/* 0x04 */
	float gameTime;			/* 0x08 */
	float delta;			/* 0x0c game time of the last step */
	byte paused;			/* 0x10 */
	byte pad_11[3];
	float speed;			/* 0x14 */
	float speedFrom;		/* 0x18 */
	float speedTo;			/* 0x1c */
	float blendLeft;		/* 0x20 */
	float blendTime;		/* 0x24 */
} cTimer;
SD_SIZE(cTimer, 0x28);

cTimer *__fastcall cTimer_ctor(cTimer *self);
/* dt: real time since the last step */
void __thiscall cTimer_Advance(cTimer *self, float dt);
/* 0 while paused */
float __fastcall cTimer_GetSpeed(cTimer *self);
/* the speed a blend goes to (rva 0x136890) */
float __fastcall cTimer_GetSpeedTo(cTimer *self);
float __fastcall cTimer_GetDelta(cTimer *self);
float __thiscall Timer_GetTime(cTimer *self);
void __thiscall Timer_SetTime(cTimer *self, float t);
void __thiscall Timer_SetSpeed(cTimer *self, float speed);

#endif
