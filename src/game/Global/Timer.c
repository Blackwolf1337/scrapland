/*
 * The game timer (Global/Timer.cpp): real time in, scaled game time out. A speed change can blend over a time from
 * the old speed (speedFrom) to the new one (speedTo).
 */
#include "scrap/global.h"
#include "scrap/timer.h"


cTimer *__fastcall cTimer_ctor(cTimer *self) {
	self->time = -1.0f;
	self->prevTime = 0.0f;
	self->gameTime = 0.0f;
	self->delta = 0.0f;
	self->paused = 0;
	self->speed = 1.0f;
	self->speedFrom = 1.0f;
	self->speedTo = 1.0f;
	self->blendLeft = 0.0f;
	self->blendTime = 1.0f;
	return self;
}

/* the speed blend after a step of dt game time */
static void cTimer_StepBlend(cTimer *self, float dt) {
	if (self->blendLeft != 0.0f) {
		self->blendLeft -= dt;
		if (self->blendLeft < 0.0f) {
			self->blendLeft = 0.0f;
			self->speedFrom = self->speedTo;
			self->speed = self->speedTo;
			return;
		}
		float q = self->blendLeft / self->blendTime;
		self->speed = (1.0f - q) * self->speedTo + self->speedFrom * q;
	}
}

/* t: the real time now */
void __thiscall cTimer_Update(cTimer *self, float t) {
	if (self->time == -1.0f) {
		self->time = t;
	}
	self->prevTime = self->time;
	self->time = t;
	self->delta = (t - self->prevTime) * self->speed;
	if (!self->paused) {
		self->gameTime += self->delta;
		cTimer_StepBlend(self, self->delta);
	}
}

/* dt: real time since the last step */
void __thiscall cTimer_Advance(cTimer *self, float dt) {
	if (self->time == -1.0f) {
		self->time = 0.0f;
	}
	self->prevTime = self->time;
	self->time += dt;
	self->delta = self->speed * dt;
	if (!self->paused) {
		self->gameTime = self->delta + self->gameTime;
		cTimer_StepBlend(self, self->delta);
	}
}

/* only the blend, by dt */
void __thiscall cTimer_Blend(cTimer *self, float dt) {
	cTimer_StepBlend(self, dt);
}

float __fastcall cTimer_GetSpeedTo(cTimer *self) {
	return self->speedTo;
}

float __fastcall cTimer_GetBlendTime(cTimer *self) {
	return self->blendTime;
}

float __fastcall cTimer_GetSpeed(cTimer *self) {
	return self->paused ? 0.0f : self->speed;
}

float __thiscall Timer_GetTime(cTimer *self) {
	return self->gameTime;
}

float __fastcall cTimer_GetDelta(cTimer *self) {
	return self->delta;
}

void __thiscall cTimer_SetBlend(cTimer *self, float time) {
	self->blendTime = time;
	self->blendLeft = time;
}

void __thiscall cTimer_SetSpeedTo(cTimer *self, float speed) {
	self->speedTo = speed;
}

void __thiscall cTimer_SetSpeedChecked(cTimer *self, float speed) {
	if (!self->paused) {
		CHECK(speed >= 0.0f);
		self->speed = speed;
		self->speedFrom = speed;
	}
}

void __thiscall Timer_SetSpeed(cTimer *self, float speed) {
	self->speed = speed;
	self->speedFrom = speed;
}

void __thiscall Timer_SetTime(cTimer *self, float t) {
	self->gameTime = t;
}
