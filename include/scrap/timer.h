/*
 * The game timer of Global/Timer.cpp (src/game/Global/Timer.c): real time in, scaled game time out. A speed
 * change can blend from speedFrom to speedTo over blendTime.
 */
#ifndef SCRAP_TIMER_H
#define SCRAP_TIMER_H

#include "scrap/shared.h"

typedef struct cTimer {
	float time;			/* last real time, -1 before the first update */
	float prevTime;
	float gameTime;
	float delta;			/* game time of the last step */
	byte paused;
	byte pad_11[3];
	float speed;
	float speedFrom;
	float speedTo;
	float blendLeft;
	float blendTime;
} cTimer;

cTimer *cTimer_ctor(cTimer *self);
/* dt: real time since the last step */
void cTimer_Advance(cTimer *self, float dt);
/* 0 while paused */
float cTimer_GetSpeed(cTimer *self);
/* the speed a blend goes to */
float cTimer_GetSpeedTo(cTimer *self);
float cTimer_GetDelta(cTimer *self);
float Timer_GetTime(cTimer *self);
void Timer_SetTime(cTimer *self, float t);
void Timer_SetSpeed(cTimer *self, float speed);
float cTimer_GetBlendTime(cTimer *self);
void cTimer_SetBlend(cTimer *self, float time);
void cTimer_SetSpeedTo(cTimer *self, float speed);
/* the speed (CHECK >= 0) unless paused */
void cTimer_SetSpeedChecked(cTimer *self, float speed);

#endif
