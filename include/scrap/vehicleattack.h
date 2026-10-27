/*
 * cVehicleAttack ("Hit_" variables of the entities with life, at 0x298; Entities/Vehicle, src/game/Entities/Vehicle/
 * VehicleAttack.c): the shots of a ship on an entity or a point since the last frame and the attacker that hit it.
 * Update calls the OnHit function of the current weapon for each shot, the earlier ones spread on the line from the
 * last point to the new one. A server sends the hits as a byte (count, sequence and a target bit).
 */
#ifndef SCRAP_VEHICLEATTACK_H
#define SCRAP_VEHICLEATTACK_H

#include "scrap/entity.h"

typedef struct cVehicleAttack {
	Nto1Node target;		/* 0x00 the entity shot at, data the attack */
	Nto1Node sent;			/* 0x10 the target of the last network flags */
	Nto1Node last;			/* 0x20 the target of the last OnHit */
	vec3_t point;			/* 0x30 the point shot at */
	cEntity *owner;			/* 0x3c */
	int count;			/* 0x40 the shots since the last Update */
	vec3_t lastPoint;		/* 0x44 the point of the last OnHit */
	byte hasLast;			/* 0x50 */
	byte pad_51[3];
	int netCount;			/* 0x54 the shots since the last network flags */
	int netSeq;			/* 0x58 */
	byte netFlags;			/* 0x5c count (4 bits), sequence (3 bits), a target (bit 7) */
	byte pad_5d[3];
	Nto1Node hitBy;			/* 0x60 the attacker, data the attack */
	float hitTime;			/* 0x70 the time of the owner of the last hit */
	byte hitCount;			/* 0x74 */
	byte pad_75[3];
	float resetTime;		/* 0x78 the world time the shots on the target are dropped */
} cVehicleAttack;
SD_OFFSET(cVehicleAttack, point, 0x30);
SD_OFFSET(cVehicleAttack, owner, 0x3c);
SD_OFFSET(cVehicleAttack, netFlags, 0x5c);
SD_OFFSET(cVehicleAttack, hitBy, 0x60);
SD_SIZE(cVehicleAttack, 0x7c);

#define g_VehicleAttackReset GAME_DATA(float, 0x4479c0)	/* the time the shots on a target are kept */

cVehicleAttack *__fastcall cVehicleAttack_ctor(cVehicleAttack *self);
/* drops the target after its reset time */
void __fastcall cVehicleAttack_Expire(cVehicleAttack *self);
/* the entity of the hit info, "" for none */
const char *__thiscall cHitInfo_GetName(cVehicleAttack *info);
/* the network flags when there are new shots or a new target (1), into *out (may be NULL) */
byte __thiscall cVehicleAttack_NetChanged(cVehicleAttack *self, byte *out);
vec3_t *__thiscall cVehicleAttack_GetPoint(cVehicleAttack *self);
byte __thiscall cVehicleAttack_VarI(cVehicleAttack *self, const char *name, int *i, int set);
/* the attacker of the owner by name */
void __thiscall cVehicleAttack_HitBy(cVehicleAttack *self, const char *name);
/* the shots: on the entity of that name or a point, n more */
void __thiscall cVehicleAttack_ShotAt(cVehicleAttack *self, const char *name, int n);
void __thiscall cVehicleAttack_ShotAtPoint(cVehicleAttack *self, const vec3_t *p, int n);
/* every frame after the weapons */
void __thiscall cVehicleAttack_Update(cVehicleAttack *self);
byte __thiscall cVehicleAttack_VarS(cVehicleAttack *self, const char *name, char **s, int set);
byte __thiscall cVehicleAttack_VarV(cVehicleAttack *self, const char *name, double *x, double *y, double *z, int set);
/* the network flags of a message: *changed if they differ, the count from the low 4 bits; the target bit */
int __thiscall cVehicleAttack_NetFlags(cVehicleAttack *self, byte flags, byte *changed);

#endif
