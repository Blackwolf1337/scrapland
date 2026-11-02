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
		cEntity base;		/* 0x000 */
		cObjEntity obj;		/* 0x000 the flags bit 0 of obj.net: the item is there */
	};
	float respawn;			/* 0x228 "rTime": > 0 while taken (hidden) */
	int syncTicks;			/* 0x22c sent while > 0 */
	char owner[0x80];		/* 0x230 "Owner": not taken while that entity is active */
	vec3_t origin;			/* 0x2b0 */
	float backTime;			/* 0x2bc "BackTime": the time left to go back to origin */
	byte appearsIfFree;		/* 0x2c0 */
	byte autoFace;			/* 0x2c1 turned to the camera */
	byte onlyUser;			/* 0x2c2 only player 0 takes it */
	byte pad_2c3;
	float takeSoundVol;		/* 0x2c4 */
	char takeSound[0x80];		/* 0x2c8 */
	byte noFx;			/* 0x348 until the first tick: no fade effects */
	byte pad_349[3];
	Nto1Node taker;			/* 0x34c link to the entity that took it */
	uint16_t typeMask;		/* 0x35c "TgTypeMarker": the teams that take it (bit per team), 0xffff all */
	uint16_t pad_35e;
	float radius;			/* 0x360 */
	byte blink;			/* 0x364 */
	byte pad_365[3];
	int fadeIn;			/* 0x368 "FadeInEffect": 1 fade, 2 blink */
	int fadeOut;			/* 0x36c "FadeOutEffect" */
	cPy_Function onTake;		/* 0x370 (item, taker) */
	cPy_Function onReset;		/* 0x384 (item) */
} cItemEntity;

/* t > 0: taken, hidden for t (fade out, take sound); else back (fade in) */
void cItemEntity_SetRespawn(cItemEntity *self, float t);

#endif
