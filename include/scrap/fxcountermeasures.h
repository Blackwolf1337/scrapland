/*
 * World/FX/FXCountermeasures.cpp of the game: the effects of the countermeasures and the hook laser, objects
 * (cObjElementFX) the debris systems keep on their debris (cDebris: its CGameModel first, the effect at +0x54).
 * The debris system calls the vtable: Start when a debris is dropped, Update and IsAlive every frame, Reset when the
 * debris goes back to the pool.
 */
#ifndef SCRAP_FXCOUNTERMEASURES_H
#define SCRAP_FXCOUNTERMEASURES_H

#include "scrap/gvar.h"
#include "scrap/entity.h"
#include "scrap/scrap.h"
#include "scrap/scrapparticles.h"
#include "scrap/scraptrails.h"

typedef struct cObjElementFX cObjElementFX;

/* cObjElementAbstract::vftable and the ones of the effects */
typedef struct cObjElementFXVtbl {
	void (*Free)(cObjElementFX *self);
	void(*Update)(cObjElementFX *self, cDebris *debris, float dt);
	/* 0: the debris goes back to the pool */
	byte(*IsAlive)(cObjElementFX *self, cDebris *debris, cDebrisSys *sys);
	void(*Start)(cObjElementFX *self, cDebris *debris);
	void(*Reset)(cObjElementFX *self, cDebris *debris);
} cObjElementFXVtbl;

/* calls through the slots of cObjElementFXVtbl */
static inline void FXObj_Free(void *self) {
	VT(cObjElementFXVtbl, self)->Free(self);
}

static inline byte FXObj_IsAlive(void *self, cDebris *debris, cDebrisSys *sys) {
	return VT(cObjElementFXVtbl, self)->IsAlive(self, debris, sys);
}

static inline void FXObj_Reset(void *self, cDebris *debris) {
	VT(cObjElementFXVtbl, self)->Reset(self, debris);
}

static inline void FXObj_Start(void *self, cDebris *debris) {
	VT(cObjElementFXVtbl, self)->Start(self, debris);
}

static inline void FXObj_Update(void *self, cDebris *debris, float dt) {
	VT(cObjElementFXVtbl, self)->Update(self, debris, dt);
}

struct cObjElementFX {
	const cObjElementFXVtbl *vftable;
};
/* the base class of the effects, 4 bytes (cObjElement_dtor sets its vtable) */
typedef cObjElementFX cObjElementAbstract;

/* the vtable of an effect element set by its constructor or destructor */
static inline void FXObj_SetVtbl(void *self, const cObjElementFXVtbl *vt) {
	((cObjElementFX *)self)->vftable = vt;
}

#define FXHOOK_POINTS 10
#define FXEMI_TRAILS 8
#define FXEMI_CTRL 10
#define FXEMI_PTS 20
#define FXSONIC_RINGS 8
#define FXDEBRISCLOUD_RANDS 0x48

/* the laser hook between the ship and its target */
typedef struct cObjElementFXHook {
	cObjElementFX base;
	cEntity *emitter;		/* the ship that shoots */
	cEntity *target;		/* the entity hooked, NULL for a point */
	int slot;			/* the weapon node of the emitter (Entity_WeapNodes) */
	byte active;
	byte pad_11[3];
	float fade;			/* the release fade time */
	CGameTrail trail[2];
	vec3_t points[FXHOOK_POINTS];	/* from the weapon node to points[numPoints] */
	int numPoints;			/* index of the last point */
	CGameParticles particles;
	float flash;
	byte withTarget;		/* started by FXHook_StartTarget */
	byte pad_151[3];
	float sparkTime;		/* since the last sparks debris */
	byte on;
	byte pad_159[3];
} cObjElementFXHook;

/* the electromagnetic impulse: up to 8 lightning bolts from the missile to
 * the world or to entities near it, each a trail along the Bezier curve of its control points that follow its points,
 * and glows at the center */
typedef struct cObjElementFXEMI {
	cObjElementFX base;
	cEntity *entity;		/* the EMI missile */
	int node;			/* Dummy01, -1 for none */
	CGameParticles particles;	/* 0 the center, 2 i + 1 and 2 i + 2 the end of bolt i */
	float hideTime;
	float c0;			/* 0.6 */
	float time;			/* since the start */
	float trailTime[FXEMI_TRAILS];	/* the time left of bolt i, -1 for a trail not running */
	CGameTrail trails[FXEMI_TRAILS];
	vec3_t ctrl[FXEMI_TRAILS][FXEMI_CTRL];	/* the control points of the curve */
	vec3_t pts[FXEMI_TRAILS][FXEMI_PTS];	/* the points, from FXEMI_CTRL the ones of the last step */
	float stepTime[FXEMI_TRAILS];	/* since the last new points (FXEMIStepTime) */
	float boltScale[FXEMI_TRAILS];	/* the width and the segment length of the trail */
	Nto1Node links[FXEMI_TRAILS];	/* target the entity the bolt hit */
	int numPoints;			/* 6 */
	float glowTime;			/* since the last glow */
	byte on;			/* set by the countermeasure that drops it */
	byte pad_d1d[3];
} cObjElementFXEMI;

/* the sonic mine explosion: rings that grow and fade */
typedef struct cObjElementFXSonicExplosion {
	cObjElementFX base;
	CGameParticles particles;	/* the emitted ones */
	CGameParticles rings;		/* one particle per ring */
	float time;
	float time2;
	vec3_t pos;
	float scale;
	float ringVel[FXSONIC_RINGS];
	float ringAcc[FXSONIC_RINGS];
	float ringSize[FXSONIC_RINGS];
	float unused_1dc[FXSONIC_RINGS];
	float ringFade[FXSONIC_RINGS];	/* 1: gone */
	byte on;			/* set by the mine that drops it */
	byte pad_21d[3];
} cObjElementFXSonicExplosion;

/* the debris cloud countermeasure */
typedef struct cObjElementFXDebrisCloud {
	cObjElementFX base;
	cEntity *entity;
	int node;			/* Dummy01, -1 for none */
	CGameParticles particles;
	float emitT;			/* the time since the last puffs */
	float time;			/* since the start */
	float rands[FXDEBRISCLOUD_RANDS]; /* random numbers in [0, 2]: the phases of the cloud particles */
	byte on;			/* set by the countermeasure that drops it */
	byte pad_1f1[3];
} cObjElementFXDebrisCloud;

/* the fields of the ships the hook reads */
#define WEAPON_LASER 0x18						/* the slot of the laser in Entity_Weapons */
#define Entity_PlayerCam(e) (Obj(e)->cam)	/* the camera data of the ship of a player, NULL else */

/* a weapon of the world in a cTargetLock: the mounted slots (bit per slot), the upgrade and its ammo */
typedef struct cWeaponSlot {
	uint32_t slots;
	int upgrade;
	float uammo;			/* the ammo of the upgrade, 0..200 */
} cWeaponSlot;

/* the render nodes of one ammo kind, per slot (a pointer to it, see tri16_t in meshd3d.h) */
typedef int ammoNodes_t[4];

/* the weapons of a ship and its lock on an entity or a point: the cWeaponManager of the RTTI (0x48 bytes,
 * vftable), src/game/Entities/Vehicle/WeaponManager.c */
typedef struct cTargetLock {
	cEntity *owner;			/* the vehicle */
	int prevWeap;
	int curWeap;
	float fireStamp;		/* the weapon fires again from 0 on */
	float cmStamp;			/* the same for the ammo (countermeasures) */
	cWeaponSlot *weaps;		/* per weapon of the world */
	ammoNodes_t *nodes;		/* render node per ammo kind and slot, -1 none */
	float *ammo;			/* per ammo kind */
	int weapSnd;			/* the sound of the owner the weapon plays, -1 none */
	Nto1Node lock;			/* target the locked entity */
	vec3_t point;			/* the locked point without entity */
	byte on;
	byte wasOn;			/* on of the last Update */
	byte teslaFiring;	/* a Tesla ray of a net shot is on (Tesla_CreateNative), cleared when it ends */
	byte pad_47;
} cTargetLock;

/* 0x11a720: the point of a lock without entity, the name of the locked entity (NULL when off) */
vec3_t *cTargetLock_GetPoint(cTargetLock *self);
char *cTargetLock_GetEntity(cTargetLock *self);
/* item i of an array of pointers (count at +4, items at +8), NULL out of range */
void *PtrArray_Get(void *self, int i);

extern cGVar GVar_HookRange;
#define g_HookRange (GVar_HookRange.value.f)
extern cGVar GVar_LaserIDAmmo;
#define g_LaserIDAmmo (GVar_LaserIDAmmo.value.i)
extern cGVar GVar_FXSonicRadius;
#define g_FXSonicRadius (GVar_FXSonicRadius.value.f)
extern cGVar GVar_R_SceneDuDvRTarget;
#define g_SceneDuDvRTarget (GVar_R_SceneDuDvRTarget.value.i)		/* R_SceneDuDvRTarget */

void cObjElementFXHook_StartTarget(cObjElementFXHook *self, cEntity *emitter, cEntity *target, int slot);
void cObjElementFXHook_StartPoint(cObjElementFXHook *self, cEntity *emitter, int slot);

void cObjElementFXEMI_SetEntity(cObjElementFXEMI *self, cEntity *e);

/* the rings and the particles of an explosion at x y z */
void cObjElementFXSonicExplosion_Explode(cObjElementFXSonicExplosion *self, float x, float y, float z);

void cObjElementFXDebrisCloud_SetEntity(cObjElementFXDebrisCloud *self, cEntity *e);
/* four Updates with dt 0 */
void cObjElementFXDebrisCloud_Warm(cObjElementFXDebrisCloud *self, cDebris *debris);

void *cObjElementFXDebrisCloud_ctor(void *self);
void *cObjElementFXEMI_ctor(void *self);
void *cObjElementFXHook_ctor(void *self);
void *cObjElementFXSonicExplosion_ctor(void *self);

#endif
