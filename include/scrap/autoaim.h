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
	float dist;			/* to the target, 10 without one */
	float cosAng;
	float ang;			/* "AngLimit" */
	float ax;			/* "X" */
	float ay;			/* "Y" */
	Nto1Node target;		/* "Target", data the aim */
	float speed;			/* "Speed" */
	float distLimit;		/* "DistLimit" */
	float hiAng;
	float loAng;
	float deltaAng;
	float prevAx;			/* the angles of the last slot */
	float prevAy;
	float interval;			/* of the slot */
	float time;			/* the world time of the slot */
	int status;			/* AUTOAIM_ */
	byte on;
	byte pad_4d[3];
} cAutoAim;

cAutoAim *cAutoAim_ctor(cAutoAim *self);
byte cAutoAim_VarF(cAutoAim *self, const char *name, double *d, int set);
/* the aim point of owner between the slots; returns out */
vec3_t *cAutoAim_GetAimPos(cAutoAim *self, vec3_t *out, cEntity *owner);
/* the slot of the vehicle owner: dt its interval */
void cAutoAim_Update(cAutoAim *self, cEntity *owner, float dt);
void cAutoAim_SetAngle(cAutoAim *self, float ang);
void cAutoAim_SetRange(cAutoAim *self, float range);
byte cAutoAim_VarS(cAutoAim *self, const char *name, char **s, int set);

#endif
