/*
 * Slot controllers (Global/SlotController.cpp, src/game/Global/SlotController.c): a countdown that fires every
 * interval. cSlotController uses a fixed interval, cRndSlotController a random one between minDiff and maxDiff.
 */
#ifndef SCRAP_SLOTCONTROLLER_H
#define SCRAP_SLOTCONTROLLER_H

#include "scrap/shared.h"

typedef struct cSlotController cSlotController;

struct cSlotController {
	float time;				/* left until the slot */
	float interval;				/* the interval running */
	float minDiff;				/* the fixed interval, the minimum of cRndSlotController */
};
/* new allocates for a cSlotController, 0x14 for a cRndSlotController */
#define CSLOTCONTROLLER_SIZE sizeof(cSlotController)

typedef struct cRndSlotController {
	cSlotController base;
	float maxDiff;
} cRndSlotController;

cSlotController *cSlotController_ctor(cSlotController *self, float interval);
/* counts dt down; 1 when the slot came */
int cSlotController_Tick(cSlotController *self, float dt);
/* when the slot came: its interval into *out and on to the next; 1 then */
byte cSlotController_Fire(cSlotController *self, float *out);
/* the same, the time left set to 0 first */
byte cSlotController_FireReset(cSlotController *self, float *out);
/* a slot still more than 1 ms ahead starts now, its interval shortened by the time left */
void cSlotController_Restart(cSlotController *self);

#endif
