/*
 * cAutoAim, the aim help of a vehicle (at 0x6d0, "Can_" variables; Entities/Vehicle, src/game/Entities/Vehicle/
 * AutoAim.c): every slot it looks for a target in the cone of its angle (AngLimit, between LoAng and HiAng, DeltaAng
 * per second) up to DistLimit and turns its aim angles (x, y) to it: at once ("Select"), at Speed ("Search") or
 * drawn in by the distance ("Magnet"); "Keep" keeps the target. The aim point of the weapons is lerped from the
 * angles of the last slot.
 */
#ifndef SCRAP_AUTOAIM_H
#define SCRAP_AUTOAIM_H

#include "scrap/entity.h"

#define AUTOAIM_OFF 0
#define AUTOAIM_SEARCH 1
#define AUTOAIM_SELECT 2
#define AUTOAIM_KEEP 3
#define AUTOAIM_MAGNET 4

typedef struct cAutoAim {
	float dist;			/* 0x00 to the target, 10 without one */
	float cosAng;			/* 0x04 */
	float ang;			/* 0x08 "AngLimit" */
	float ax;			/* 0x0c "X" */
	float ay;			/* 0x10 "Y" */
	Nto1Node target;		/* 0x14 "Target", data the aim */
	float speed;			/* 0x24 "Speed" */
	float distLimit;		/* 0x28 "DistLimit" */
	float hiAng;			/* 0x2c */
	float loAng;			/* 0x30 */
	float deltaAng;			/* 0x34 */
	float prevAx;			/* 0x38 the angles of the last slot */
	float prevAy;			/* 0x3c */
	float interval;			/* 0x40 of the slot */
	float time;			/* 0x44 the world time of the slot */
	int status;			/* 0x48 AUTOAIM_ */
	byte on;			/* 0x4c */
	byte pad_4d[3];
} cAutoAim;

cAutoAim *cAutoAim_ctor(cAutoAim *self);
byte cAutoAim_VarF(cAutoAim *self, const char *name, double *d, int set);
/* a slot of interval with the angles ang (NULL keeps them) */
void cAutoAim_SetSlot(cAutoAim *self, float interval, const float *ang);
/* the aim point of owner between the slots; returns out */
vec3_t *cAutoAim_GetAimPos(cAutoAim *self, vec3_t *out, cEntity *owner);
/* the slot of the vehicle owner: dt its interval */
void cAutoAim_Update(cAutoAim *self, cEntity *owner, float dt);
void cAutoAim_SetAngle(cAutoAim *self, float ang);
void cAutoAim_SetRange(cAutoAim *self, float range);
byte cAutoAim_VarS(cAutoAim *self, const char *name, char **s, int set);

#endif
