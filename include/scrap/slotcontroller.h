/*
 * Slot controllers (Global/SlotController.cpp, src/game/Global/SlotController.c): a countdown that fires every
 * interval. cSlotController uses a fixed interval, cRndSlotController a random one between minDiff and maxDiff.
 */
#ifndef SCRAP_SLOTCONTROLLER_H
#define SCRAP_SLOTCONTROLLER_H

#include "scrap/shared.h"

typedef struct cSlotController cSlotController;

typedef struct cSlotControllerVtbl {
	void(__thiscall *Next)(cSlotController *self);	/* 0x0 the next interval */
} cSlotControllerVtbl;

struct cSlotController {
	const cSlotControllerVtbl *vftable;	/* 0x00 */
	float time;				/* 0x04 left until the slot */
	float interval;				/* 0x08 the interval running */
	float minDiff;				/* 0x0c the fixed interval, the minimum of cRndSlotController */
	float maxDiff;				/* 0x10 cRndSlotController only */
};
/* new allocates 0x10 bytes for a cSlotController, 0x14 for a cRndSlotController */
#define CSLOTCONTROLLER_SIZE 0x10

cSlotController *__thiscall cSlotController_ctor(cSlotController *self, float interval);
/* counts dt down; 1 when the slot came */
int __thiscall cSlotController_Tick(cSlotController *self, float dt);
/* when the slot came: its interval into *out and on to the next; 1 then */
byte __thiscall cSlotController_Fire(cSlotController *self, float *out);
/* the same, the time left set to 0 first */
byte __thiscall cSlotController_FireReset(cSlotController *self, float *out);
/* a slot still more than 1 ms ahead starts now, its interval shortened by the time left */
void __fastcall cSlotController_Restart(cSlotController *self);

#endif
