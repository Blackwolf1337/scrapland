/*
 * Slot controllers (Global/SlotController.cpp): a countdown that fires every interval. cSlotController uses a
 * fixed interval, cRndSlotController a random one between minDiff and maxDiff. The first slot comes at a random
 * time within the first interval.
 */
#include "scrap/game.h"
#include "scrap/mathlib.h"
#include "scrap/slotcontroller.h"

static const cSlotControllerVtbl cSlotController_vftable;
#define CSLOTCONTROLLER_VFTABLE (&cSlotController_vftable)

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

static void cSlotController_Next(cSlotController *self) {
	float t = self->interval;
	self->interval = self->minDiff;
	self->time += t;
}

/* counts dt down; 1 when the slot came */
int cSlotController_Tick(cSlotController *self, float dt) {
	self->time -= dt;
	return self->time <= 0.0f;
}

static const float g_SlotEpsilon = 0.001f;

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
		SlotCtl_Next(self);
		return 1;
	}
	return 0;
}

/* the same, the time left set to 0 first */
byte cSlotController_FireReset(cSlotController *self, float *out) {
	if (self->time <= 0.0f) {
		*out = self->interval;
		self->time = 0.0f;
		SlotCtl_Next(self);
		return 1;
	}
	return 0;
}

/* the vtable of cSlotController */
static const cSlotControllerVtbl cSlotController_vftable = {
	.Next = cSlotController_Next,
};

