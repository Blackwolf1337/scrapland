/*
 * Slot controllers (Global/SlotController.cpp): a countdown that fires every interval. cSlotController uses a
 * fixed interval, cRndSlotController a random one between minDiff and maxDiff. The first slot comes at a random
 * time within the first interval.
 */
#include "scrap/game.h"
#include "scrap/mathlib.h"
#include "scrap/slotcontroller.h"

extern const cSlotControllerVtbl cSlotController_vftable;
#define CSLOTCONTROLLER_VFTABLE (&cSlotController_vftable)
extern const float g_SlotEpsilon;	/* 0.001 */

/* rand() of the exe as a double in [0, 1] times range */
static double RandRange(double range) {
	return (double)(crt_rand() & 0x7fff) * RAND_SCALE * range;
}

/* time to the first slot: interval minus a random part of it (the original adds 0.0 in double first) */
static void cSlotController_Start(cSlotController *self, float interval) {
	float t = (float)(RandRange(interval) + 0.0);
	self->time = -t + self->interval;
	self->interval = self->minDiff;
}

cSlotController *cSlotController_ctor(cSlotController *self, float interval) {
	self->vftable = CSLOTCONTROLLER_VFTABLE;
	self->minDiff = interval;
	self->interval = interval;
	cSlotController_Start(self, interval);
	return self;
}

void cSlotController_Next(cSlotController *self) {
	float t = self->interval;
	self->interval = self->minDiff;
	self->time += t;
}

void cRndSlotController_Next(cSlotController *elem) {
	cRndSlotController *self = (cRndSlotController *)elem;
	self->base.time += self->base.interval;
	float lo = self->base.minDiff, hi = self->maxDiff;
	self->base.interval = (float)(RandRange(hi - lo) + (double)lo);
}

/* counts dt down; 1 when the slot came */
int cSlotController_Tick(cSlotController *self, float dt) {
	self->time -= dt;
	return self->time <= 0.0f;
}

/* a slot that is still more than 1 ms ahead starts now, its interval shortened by the time left */
void cSlotController_Restart(cSlotController *self) {
	float t = self->time;
	if (t > g_SlotEpsilon) {
		self->time = 0.0f;
		self->interval -= t;
	}
}

/* when the slot came: its interval into *out and on to the next; 1 then */
byte cSlotController_Fire(cSlotController *self, float *out) {
	if (self->time <= 0.0f) {
		*out = self->interval;
		self->vftable->Next(self);
		return 1;
	}
	return 0;
}

/* the same, the time left set to 0 first */
byte cSlotController_FireReset(cSlotController *self, float *out) {
	if (self->time <= 0.0f) {
		*out = self->interval;
		self->time = 0.0f;
		self->vftable->Next(self);
		return 1;
	}
	return 0;
}

/* the vtable of cSlotController */
const cSlotControllerVtbl cSlotController_vftable = {
	.Next = cSlotController_Next,
};

/* the vtable of cRndSlotController */
const cSlotControllerVtbl cRndSlotController_vftable = {
	.Next = cRndSlotController_Next,
};
