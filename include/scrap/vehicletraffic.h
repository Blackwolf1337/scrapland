/*
 * Entities/Vehicle/VehicleTraffic.cpp of the game: cVehicleTraffic, the traffic lane of a ship: the segment from
 * Begin to End it keeps to while Active (the capsule of its Type: Light, Medium, Heavy), the terminal Speed, and
 * the collision slide of its moves. At +0x5c4 of the vehicle.
 */
#ifndef SCRAP_VEHICLETRAFFIC_H
#define SCRAP_VEHICLETRAFFIC_H

#include "scrap/entity.h"
#include "scrap/scrapcollision.h"

typedef struct cVehicleTraffic {
	cEntity *parent;		/* 0x00 the ship */
	vec3_t begin;			/* 0x04 */
	vec3_t end;			/* 0x10 */
	byte active;			/* 0x1c */
	byte outside;			/* 0x1d the last move left the capsule (or the lane is off) */
	byte pad_1e[2];
	float speed;			/* 0x20 the longest move */
	int type;			/* 0x24 0 Light, 1 Medium, 2 Heavy */
	byte off;			/* 0x28 moves go through without slide */
	byte pad_29[3];
} cVehicleTraffic;
SD_OFFSET(cVehicleTraffic, active, 0x1c);
SD_OFFSET(cVehicleTraffic, speed, 0x20);
SD_OFFSET(cVehicleTraffic, off, 0x28);
SD_SIZE(cVehicleTraffic, 0x2c);

/* the engine variables */
#define g_UseTrafficCollisionSkip GAME_DATA(int, 0x4486ec)
#define g_TrafficSpeed GAME_DATA(float, 0x448704)
#define g_HTrafficCapsule GAME_DATA(float, 0x448734)
#define g_NTrafficCapsule GAME_DATA(float, 0x448764)
#define g_MTrafficCapsule GAME_DATA(float, 0x448794)

/* AI of another module (rva 0x8b0e0): the width of the traffic path of an AI in the traffic states 4 and 9, else 0;
 * the value of the x87 stack, not rounded */
long double __thiscall cAIVehicle_GetPathWidth(void *ai);

cVehicleTraffic *__fastcall cVehicleTraffic_ctor(cVehicleTraffic *self);
/* the ship tests its collisions with the entities: not Active, not in a traffic state (4, 9) of its AI, or
 * UseTrafficCollisionSkip 0 */
byte __fastcall cVehicleTraffic_CollideEntities(cVehicleTraffic *self);
byte __thiscall cVehicleTraffic_VarF(cVehicleTraffic *self, const char *name, double *v, byte set);
void __thiscall cVehicleTraffic_SetOff(cVehicleTraffic *self, byte off);
void __thiscall cVehicleTraffic_SetParent(cVehicleTraffic *self, cEntity *parent);
byte __thiscall cVehicleTraffic_VarI(cVehicleTraffic *self, const char *name, int *v, byte set);
/* the move from .. to (at most speed of len) kept in the capsule of the lane, else slid; out the end point */
byte __thiscall cVehicleTraffic_Move(cVehicleTraffic *self, const vec3_t *from, const vec3_t *to, float radius,
	vec3_t *out, float len, CSphereBox *cache);
byte __thiscall cVehicleTraffic_VarS(cVehicleTraffic *self, const char *name, char **v, byte set);
byte __thiscall cVehicleTraffic_VarV(cVehicleTraffic *self, const char *name, double *x, double *y, double *z,
	byte set);

#endif
