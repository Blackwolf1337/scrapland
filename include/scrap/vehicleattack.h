/*
 * cVehicleAttack ("Hit_" variables of the entities with life, at 0x298; Entities/Vehicle, src/game/Entities/Vehicle/
 * VehicleAttack.c): the shots of a ship on an entity or a point since the last frame and the attacker that hit it.
 * Update calls the OnHit function of the current weapon for each shot, the earlier ones spread on the line from the
 * last point to the new one. A server sends the hits as a byte (count, sequence and a target bit).
 */
#ifndef SCRAP_VEHICLEATTACK_H
#define SCRAP_VEHICLEATTACK_H

#include "scrap/gvar.h"
#include "scrap/entity.h"

typedef struct cVehicleAttack {
	Nto1Node target;		/* the entity shot at, data the attack */
	Nto1Node sent;			/* the target of the last network flags */
	Nto1Node last;			/* the target of the last OnHit */
	vec3_t point;			/* the point shot at */
	cEntity *owner;
	int count;			/* the shots since the last Update */
	vec3_t lastPoint;		/* the point of the last OnHit */
	byte hasLast;
	byte pad_51[3];
	int netCount;			/* the shots since the last network flags */
	int netSeq;
	byte netFlags;			/* count (4 bits), sequence (3 bits), a target (bit 7) */
	byte pad_5d[3];
	Nto1Node hitBy;			/* the attacker, data the attack */
	float hitTime;			/* the time of the owner of the last hit */
	byte hitCount;
	byte pad_75[3];
	float resetTime;		/* the world time the shots on the target are dropped */
} cVehicleAttack;

cVehicleAttack *cVehicleAttack_ctor(cVehicleAttack *self);
/* drops the target after its reset time */
void cVehicleAttack_Expire(cVehicleAttack *self);
/* the entity of the hit info, "" for none */
const char *cHitInfo_GetName(cVehicleAttack *info);
/* the network flags when there are new shots or a new target (1), into *out (may be NULL) */
byte cVehicleAttack_NetChanged(cVehicleAttack *self, byte *out);
vec3_t *cVehicleAttack_GetPoint(cVehicleAttack *self);
byte cVehicleAttack_VarI(cVehicleAttack *self, const char *name, int *i, int set);
/* the attacker of the owner by name */
void cVehicleAttack_HitBy(cVehicleAttack *self, const char *name);
/* the shots: on the entity of that name or a point, n more */
void cVehicleAttack_ShotAt(cVehicleAttack *self, const char *name, int n);
void cVehicleAttack_ShotAtPoint(cVehicleAttack *self, const vec3_t *p, int n);
/* every frame after the weapons */
void cVehicleAttack_Update(cVehicleAttack *self);
/* not in the original: the hits of a ship of the instance on its proxy of a client (include/scrap/play.h, Rep_Shot):
 * count shots of weapon from the point from to the point to (at target, NULL: a point) through the weapon's OnHit */
void cVehicleAttack_Replay(cVehicleAttack *self, int weapon, int count, const vec3_t *from, const vec3_t *to,
	cEntity *target);
byte cVehicleAttack_VarS(cVehicleAttack *self, const char *name, char **s, int set);
byte cVehicleAttack_VarV(cVehicleAttack *self, const char *name, double *x, double *y, double *z, int set);
/* the network flags of a message: *changed if they differ, the count from the low 4 bits; the target bit */
int cVehicleAttack_NetFlags(cVehicleAttack *self, byte flags, byte *changed);

#endif
