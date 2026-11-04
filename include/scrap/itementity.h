/*
 * cItemEntity ("item"): the pickups of the levels, an "Obj" (cObjEntity, 0x228 bytes) with a sphere that calls
 * OnTake for the entity that touches it, then hides until the respawn time ("rTime") ran out. It can be pushed
 * ("Vel") and comes back to its origin after "BackTime". src/game/Entities/ItemEntity.c; the original code sits
 * between Doors.cpp and the missiles, its source file has no CHECK.
 */
#ifndef SCRAP_ITEMENTITY_H
#define SCRAP_ITEMENTITY_H

#include "scrap/gvar.h"
#include "scrap/objentity.h"
#include "scrap/pyfunction.h"
#include "scrap/scrapmodel.h"
#include "scrap/world.h"

#define CITEMENTITY_SIZE sizeof(cItemEntity)

typedef struct cItemEntity {
	union {
		cEntity base;
		cObjEntity obj;		/* the flags bit 0 of obj.net: the item is there */
	};
	float respawn;			/* "rTime": > 0 while taken (hidden) */
	int syncTicks;			/* sent while > 0 */
	char owner[0x80];		/* "Owner": not taken while that entity is active */
	vec3_t origin;
	float backTime;			/* "BackTime": the time left to go back to origin */
	byte appearsIfFree;
	byte autoFace;			/* turned to the camera */
	byte onlyUser;			/* only player 0 takes it */
	byte pad_2c3;
	float takeSoundVol;
	char takeSound[0x80];
	byte noFx;			/* until the first tick: no fade effects */
	byte pad_349[3];
	Nto1Node taker;			/* link to the entity that took it */
	uint16_t typeMask;		/* "TgTypeMarker": the teams that take it (bit per team), 0xffff all */
	uint16_t pad_35e;
	float radius;
	byte blink;
	byte pad_365[3];
	int fadeIn;			/* "FadeInEffect": 1 fade, 2 blink */
	int fadeOut;			/* "FadeOutEffect" */
	cPy_Function onTake;		/* (item, taker) */
	cPy_Function onReset;		/* (item) */
} cItemEntity;

/* t > 0: taken, hidden for t (fade out, take sound); else back (fade in) */
void cItemEntity_SetRespawn(cItemEntity *self, float t);

#endif
