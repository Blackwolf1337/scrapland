/*
 * Slot controllers (Global/SlotController.cpp): a countdown that fires every interval. cSlotController uses a
 * fixed interval, cRndSlotController a random one between minDiff and maxDiff. The first slot comes at a random
 * time within the first interval.
 */
#include "scrap/game.h"
#include "scrap/mathlib.h"
#include "scrap/slotcontroller.h"

#define cSlotController_vftable ((const cSlotControllerVtbl *)Game_Ptr(0x3bbaac))
#define cRndSlotController_vftable ((const cSlotControllerVtbl *)Game_Ptr(0x3bbab4))
#define g_RandScale GAME_DATA(double, 0x3a3d60)	/* 1/32767 */
#define g_SlotEpsilon GAME_DATA(float, 0x3a3d58)	/* 0.001 */

/* rand() of the exe as a double in [0, 1] times range */
static double RandRange(double range) {
	return (double)(crt_rand() & 0x7fff) * g_RandScale * range;
}

/* time to the first slot: interval minus a random part of it (the original adds 0.0 in double first) */
static void cSlotController_Start(cSlotController *self, float interval) {
	float t = (float)(RandRange(interval) + 0.0);
	self->time = -t + self->interval;
	self->interval = self->minDiff;
}

cSlotController *__thiscall cSlotController_ctor(cSlotController *self, float interval) {
	self->vftable = cSlotController_vftable;
	self->minDiff = interval;
	self->interval = interval;
	cSlotController_Start(self, interval);
	return self;
}

cSlotController *__thiscall cRndSlotController_ctor(cSlotController *self, float minDiff, float maxDiff) {
	cSlotController_ctor(self, minDiff);
	self->vftable = cRndSlotController_vftable;
	self->maxDiff = maxDiff;
	CHECK(!(maxDiff < self->minDiff));
	return self;
}

void __thiscall cSlotController_Next(cSlotController *self) {
	float t = self->interval;
	self->interval = self->minDiff;
	self->time += t;
}

void __thiscall cRndSlotController_Next(cSlotController *self) {
	self->time += self->interval;
	float lo = self->minDiff, hi = self->maxDiff;
	self->interval = (float)(RandRange(hi - lo) + (double)lo);
}

/* counts dt down; 1 when the slot came */
int __thiscall cSlotController_Tick(cSlotController *self, float dt) {
	self->time -= dt;
	return self->time <= 0.0f;
}

/* a slot that is still more than 1 ms ahead starts now, its interval shortened by the time left */
void __fastcall cSlotController_Restart(cSlotController *self) {
	float t = self->time;
	if (t > g_SlotEpsilon) {
		self->time = 0.0f;
		self->interval -= t;
	}
}

/* when the slot came: its interval into *out and on to the next; 1 then */
byte __thiscall cSlotController_Fire(cSlotController *self, float *out) {
	if (self->time <= 0.0f) {
		*out = self->interval;
		self->vftable->Next(self);
		return 1;
	}
	return 0;
}

/* the same, the time left set to 0 first */
byte __thiscall cSlotController_FireReset(cSlotController *self, float *out) {
	if (self->time <= 0.0f) {
		*out = self->interval;
		self->time = 0.0f;
		self->vftable->Next(self);
		return 1;
	}
	return 0;
}

void __thiscall cSlotController_CopyTime(cSlotController *self, const cSlotController *other) {
	self->time = other->time;
}
