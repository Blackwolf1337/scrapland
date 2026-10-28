/*
 * cItemEntity ("item"): the pickups of the levels, an "Obj" (cObjEntity, 0x228 bytes) with a sphere that calls
 * OnTake for the entity that touches it, then hides until the respawn time ("rTime") ran out. It can be pushed
 * ("Vel") and comes back to its origin after "BackTime". src/game/Entities/ItemEntity.c; the original code sits
 * between Doors.cpp and the missiles, its source file has no CHECK.
 */
#ifndef SCRAP_ITEMENTITY_H
#define SCRAP_ITEMENTITY_H

#include "scrap/objentity.h"
#include "scrap/pyfunction.h"
#include "scrap/scrapmodel.h"
#include "scrap/world.h"

#define CITEMENTITY_VFTABLE GAME_VFTABLE(cItemEntity_vftable, 0x3b294c)
#define CITEMENTITY_SIZE 0x398

typedef struct cItemEntity {
	cEntity base;			/* 0x000 */
	byte pad_7c[0xb8 - 0x7c];
	CGameModel model;		/* 0x0b8 of the cObjEntity */
	vec3_t vel;			/* 0x0d4 */
	byte pad_e0[0xf4 - 0xe0];
	cNetController *net;		/* 0x0f4 the flags bit 0: the item is there */
	byte pad_f8[0x140 - 0xf8];
	byte *obj140;			/* 0x140 a float at 0xc is set to 0.05 */
	byte pad_144[0x228 - 0x144];
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
SD_OFFSET(cItemEntity, model, 0xb8);
SD_OFFSET(cItemEntity, net, 0xf4);
SD_OFFSET(cItemEntity, respawn, 0x228);
SD_OFFSET(cItemEntity, origin, 0x2b0);
SD_OFFSET(cItemEntity, takeSound, 0x2c8);
SD_OFFSET(cItemEntity, taker, 0x34c);
SD_OFFSET(cItemEntity, radius, 0x360);
SD_OFFSET(cItemEntity, onTake, 0x370);
SD_SIZE(cItemEntity, CITEMENTITY_SIZE);

#define g_ItemFxDist GAME_DATA(float, 0x447694)		/* the fade effects only nearer (60000) */
#define g_ItemSyncDist GAME_DATA(float, 0x4476ac)
#define g_ItemSyncCount GAME_DATA(uint32_t, 0x4476b4)

void __fastcall cItemEntity_dtor(cItemEntity *self);
cItemEntity *__thiscall cItemEntity_Delete(cItemEntity *self, uint32_t flags);
void __thiscall cItemEntity_Touch(cItemEntity *self, cEntity *other, const vec3_t *point, const vec3_t *dir);
byte __thiscall cItemEntity_VarF(cItemEntity *self, const char *name, double *d, int set);
vec3_t *__thiscall cItemEntity_GetOrigin(cItemEntity *self, vec3_t *out);
char *__fastcall cItemEntity_GetOwner(cItemEntity *self);
float __fastcall cItemEntity_GetBackTime(cItemEntity *self);
const char *__thiscall cItemEntity_GetClassName(cItemEntity *self);
float __thiscall cItemEntity_GetRadius(cItemEntity *self);
byte __thiscall cItemEntity_VarI(cItemEntity *self, const char *name, int *i, int set);
byte __thiscall cItemEntity_IsType(cItemEntity *self, const char *type);
void __thiscall cItemEntity_NetRead(cItemEntity *self, void *msg, int unused);
void __thiscall cItemEntity_NetWrite(cItemEntity *self, int unused, void *msg, byte full);
void __thiscall cItemEntity_Tick(cItemEntity *self, float dt);
void __thiscall cItemEntity_SetOrigin(cItemEntity *self, const vec3_t *v);
void __thiscall cItemEntity_SetOwner(cItemEntity *self, const char *name);
void __thiscall cItemEntity_SetPos(cItemEntity *self, const vec3_t *pos);
void __thiscall cItemEntity_SetBackTime(cItemEntity *self, float t);
/* t > 0: taken, hidden for t (fade out, take sound); else back (fade in) */
void __thiscall cItemEntity_SetRespawn(cItemEntity *self, float t);
/* the teams of the letters R, B, G, N, K (either case); "" all */
void __thiscall cItemEntity_SetTypeMarker(cItemEntity *self, const char *types);
void __thiscall cItemEntity_Update(cItemEntity *self);
byte __thiscall cItemEntity_TestSphere(cItemEntity *self, cEntity *other, const vec3_t *center, float radius,
	int arg);
byte __thiscall cItemEntity_VarS(cItemEntity *self, const char *name, char **s, int set);
byte __thiscall cItemEntity_NetNeedsSend(cItemEntity *self, float dist2);
byte __thiscall cItemEntity_VarV(cItemEntity *self, const char *name, double *x, double *y, double *z, int set);

#endif
