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
	cObjElementFX *(*Delete)(cObjElementFX *self, byte flags);			/* 0x00 */
	void(*Update)(cObjElementFX *self, cDebris *debris, float dt);		/* 0x04 vf01 */
	/* 0: the debris goes back to the pool */
	byte(*IsAlive)(cObjElementFX *self, cDebris *debris, cDebrisSys *sys);	/* 0x08 vf02 */
	void(*Start)(cObjElementFX *self, cDebris *debris);			/* 0x0c vf03 */
	void(*Reset)(cObjElementFX *self, cDebris *debris);			/* 0x10 vf04 */
} cObjElementFXVtbl;
SD_SIZE(cObjElementFXVtbl, 0x14);

struct cObjElementFX {
	const cObjElementFXVtbl *vftable;	/* 0x00 */
};
/* the base class of the effects, 4 bytes (cObjElement_dtor sets its vtable) */
typedef cObjElementFX cObjElementAbstract;

/* the vtable of an effect element set by its constructor or destructor */
static inline void FXObj_SetVtbl(void *self, const cObjElementFXVtbl *vt) {
	((cObjElementFX *)self)->vftable = vt;
}

extern const cObjElementFXVtbl cObjElementFXHook_vftable;
#define COBJELEMENTFXHOOK_VFTABLE (&cObjElementFXHook_vftable)
extern const cObjElementFXVtbl cObjElementFXEMI_vftable;
#define COBJELEMENTFXEMI_VFTABLE (&cObjElementFXEMI_vftable)
extern const cObjElementFXVtbl cObjElementFXSonicExplosion_vftable;
#define COBJELEMENTFXSONICEXPLOSION_VFTABLE (&cObjElementFXSonicExplosion_vftable)
extern const cObjElementFXVtbl cObjElementFXDebrisCloud_vftable;
#define COBJELEMENTFXDEBRISCLOUD_VFTABLE (&cObjElementFXDebrisCloud_vftable)

#define FXHOOK_POINTS 10
#define FXEMI_TRAILS 8
#define FXEMI_CTRL 10
#define FXEMI_PTS 20
#define FXSONIC_RINGS 8
#define FXDEBRISCLOUD_RANDS 0x48

/* the laser hook between the ship and its target (0x15c bytes, destructor rva 0x1c9ab0) */
typedef struct cObjElementFXHook {
	cObjElementFX base;
	cEntity *emitter;		/* 0x004 the ship that shoots */
	cEntity *target;		/* 0x008 the entity hooked, NULL for a point */
	int slot;			/* 0x00c the weapon node of the emitter (Entity_WeapNodes) */
	byte active;			/* 0x010 */
	byte pad_11[3];
	float fade;			/* 0x014 the release fade time */
	CGameTrail trail[2];		/* 0x018 */
	vec3_t points[FXHOOK_POINTS];	/* 0x020 from the weapon node to points[numPoints] */
	int numPoints;			/* 0x098 index of the last point */
	CGameParticles particles;	/* 0x09c */
	float flash;			/* 0x14c */
	byte withTarget;		/* 0x150 started by FXHook_StartTarget */
	byte pad_151[3];
	float sparkTime;		/* 0x154 since the last sparks debris */
	byte on;			/* 0x158 */
	byte pad_159[3];
} cObjElementFXHook;
SD_OFFSET(cObjElementFXHook, trail, 0x18);
SD_OFFSET(cObjElementFXHook, numPoints, 0x98);
SD_OFFSET(cObjElementFXHook, particles, 0x9c);
SD_OFFSET(cObjElementFXHook, withTarget, 0x150);
SD_SIZE(cObjElementFXHook, 0x15c);

/* the electromagnetic impulse (0xd20 bytes, destructor rva 0x1c9a10): up to 8 lightning bolts from the missile to
 * the world or to entities near it, each a trail along the Bezier curve of its control points that follow its points,
 * and glows at the center */
typedef struct cObjElementFXEMI {
	cObjElementFX base;
	cEntity *entity;		/* 0x004 the EMI missile */
	int node;			/* 0x008 Dummy01, -1 for none */
	CGameParticles particles;	/* 0x00c 0 the center, 2 i + 1 and 2 i + 2 the end of bolt i */
	float hideTime;			/* 0x0bc */
	float c0;			/* 0x0c0 0.6 */
	float time;			/* 0x0c4 since the start */
	byte pad_c8[0xd4 - 0xc8];
	float trailTime[FXEMI_TRAILS];	/* 0x0d4 the time left of bolt i, -1 for a trail not running */
	CGameTrail trails[FXEMI_TRAILS]; /* 0x0f4 */
	vec3_t ctrl[FXEMI_TRAILS][FXEMI_CTRL];	/* 0x114 the control points of the curve */
	vec3_t pts[FXEMI_TRAILS][FXEMI_PTS];	/* 0x4d4 the points, from FXEMI_CTRL the ones of the last step */
	float stepTime[FXEMI_TRAILS];	/* 0xc54 since the last new points (FXEMIStepTime) */
	float boltScale[FXEMI_TRAILS];	/* 0xc74 the width and the segment length of the trail */
	Nto1Node links[FXEMI_TRAILS];	/* 0xc94 target the entity the bolt hit */
	int numPoints;			/* 0xd14 6 */
	float glowTime;			/* 0xd18 since the last glow */
	byte on;			/* 0xd1c set by the countermeasure that drops it */
	byte pad_d1d[3];
} cObjElementFXEMI;
SD_OFFSET(cObjElementFXEMI, particles, 0xc);
SD_OFFSET(cObjElementFXEMI, trails, 0xf4);
SD_OFFSET(cObjElementFXEMI, ctrl, 0x114);
SD_OFFSET(cObjElementFXEMI, pts, 0x4d4);
SD_OFFSET(cObjElementFXEMI, stepTime, 0xc54);
SD_OFFSET(cObjElementFXEMI, links, 0xc94);
SD_OFFSET(cObjElementFXEMI, on, 0xd1c);
SD_SIZE(cObjElementFXEMI, 0xd20);

/* the sonic mine explosion: rings that grow and fade (0x220 bytes, destructor rva 0x1c9b00) */
typedef struct cObjElementFXSonicExplosion {
	cObjElementFX base;
	CGameParticles particles;	/* 0x004 the emitted ones */
	CGameParticles rings;		/* 0x0b4 one particle per ring */
	float time;			/* 0x164 */
	float time2;			/* 0x168 */
	vec3_t pos;			/* 0x16c */
	float scale;			/* 0x178 */
	float ringVel[FXSONIC_RINGS];	/* 0x17c */
	float ringAcc[FXSONIC_RINGS];	/* 0x19c */
	float ringSize[FXSONIC_RINGS];	/* 0x1bc */
	float unused_1dc[FXSONIC_RINGS];	/* 0x1dc */
	float ringFade[FXSONIC_RINGS];	/* 0x1fc 1: gone */
	byte on;			/* 0x21c set by the mine that drops it */
	byte pad_21d[3];
} cObjElementFXSonicExplosion;
SD_OFFSET(cObjElementFXSonicExplosion, rings, 0xb4);
SD_OFFSET(cObjElementFXSonicExplosion, pos, 0x16c);
SD_OFFSET(cObjElementFXSonicExplosion, ringFade, 0x1fc);
SD_SIZE(cObjElementFXSonicExplosion, 0x220);

/* the debris cloud countermeasure (0x1f4 bytes, destructor rva 0x1c99d0) */
typedef struct cObjElementFXDebrisCloud {
	cObjElementFX base;
	cEntity *entity;		/* 0x004 */
	int node;			/* 0x008 Dummy01, -1 for none */
	CGameParticles particles;	/* 0x00c */
	float emitT;			/* 0x0bc the time since the last puffs */
	float time;			/* 0x0c0 since the start */
	byte pad_c4[0xd0 - 0xc4];
	float rands[FXDEBRISCLOUD_RANDS]; /* 0x0d0 random numbers in [0, 2]: the phases of the cloud particles */
	byte on;			/* 0x1f0 set by the countermeasure that drops it */
	byte pad_1f1[3];
} cObjElementFXDebrisCloud;
SD_OFFSET(cObjElementFXDebrisCloud, rands, 0xd0);
SD_OFFSET(cObjElementFXDebrisCloud, on, 0x1f0);
SD_SIZE(cObjElementFXDebrisCloud, 0x1f4);

/* the fields of the ships the hook reads */
#define WEAPON_LASER 0x18						/* the slot of the laser in Entity_Weapons */
#define Entity_PlayerCam(e) (Obj(e)->cam)	/* the camera data of the ship of a player, NULL else */

/* a weapon of the world in a cTargetLock: the mounted slots (bit per slot), the upgrade and its ammo */
typedef struct cWeaponSlot {
	uint32_t slots;			/* 0x0 */
	int upgrade;			/* 0x4 */
	float uammo;			/* 0x8 the ammo of the upgrade, 0..200 */
} cWeaponSlot;
SD_SIZE(cWeaponSlot, 0xc);

/* the render nodes of one ammo kind, per slot (a pointer to it, see tri16_t in meshd3d.h) */
typedef int ammoNodes_t[4];

/* the weapons of a ship and its lock on an entity or a point: the cWeaponManager of the RTTI (0x48 bytes,
 * vftable rva 0x3b664c), src/game/Entities/Vehicle/WeaponManager.c */
typedef struct cTargetLock {
	const void *vftable;		/* 0x00 */
	cEntity *owner;			/* 0x04 the vehicle */
	int prevWeap;			/* 0x08 */
	int curWeap;			/* 0x0c */
	float fireStamp;		/* 0x10 the weapon fires again from 0 on */
	float cmStamp;			/* 0x14 the same for the ammo (countermeasures) */
	cWeaponSlot *weaps;		/* 0x18 per weapon of the world */
	ammoNodes_t *nodes;		/* 0x1c render node per ammo kind and slot, -1 none */
	float *ammo;			/* 0x20 per ammo kind */
	int weapSnd;			/* 0x24 the sound of the owner the weapon plays, -1 none */
	Nto1Node lock;			/* 0x28 target the locked entity */
	vec3_t point;			/* 0x38 the locked point without entity */
	byte on;			/* 0x44 */
	byte wasOn;			/* 0x45 on of the last Update */
	byte teslaFiring;	/* 0x46 a Tesla ray of a net shot is on (Tesla_CreateNative), cleared when it ends */
	byte pad_47;
} cTargetLock;
SD_OFFSET(cTargetLock, weaps, 0x18);
SD_OFFSET(cTargetLock, lock, 0x28);
SD_OFFSET(cTargetLock, point, 0x38);
SD_OFFSET(cTargetLock, on, 0x44);
SD_SIZE(cTargetLock, 0x48);

/* rva 0x11a740, 0x11a720: the point of a lock without entity, the name of the locked entity (NULL when off) */
vec3_t *cTargetLock_GetPoint(cTargetLock *self);
char *cTargetLock_GetEntity(cTargetLock *self);
/* rva 0x1a26e0: item i of an array of pointers (count at +4, items at +8), NULL out of range */
void *PtrArray_Get(void *self, int i);

/* the engine variables */
extern cGVar GVar_FXHookFadeTime;
#define g_FXHookFadeTime (GVar_FXHookFadeTime.value.f)
extern cGVar GVar_HookRange;
#define g_HookRange (GVar_HookRange.value.f)
extern cGVar GVar_LaserIDAmmo;
#define g_LaserIDAmmo (GVar_LaserIDAmmo.value.i)
extern cGVar GVar_FXHookSize;
#define g_FXHookSize (GVar_FXHookSize.value.f)
extern cGVar GVar_FXHookTensor;
#define g_FXHookTensor (GVar_FXHookTensor.value.f)
extern cGVar GVar_LaserHookSparksFile;
#define g_LaserHookSparksFile (GVar_LaserHookSparksFile.value.s)
extern cGVar GVar_FXSonicRadius;
#define g_FXSonicRadius (GVar_FXSonicRadius.value.f)
extern cGVar GVar_R_SceneDuDvRTarget;
#define g_SceneDuDvRTarget (GVar_R_SceneDuDvRTarget.value.i)		/* R_SceneDuDvRTarget */

/* cWorld: the position of the camera of the current view into out */
vec3_t *cWorld_GetCamPos(cWorld *self, vec3_t *out);

/* the hook laser: with target a curve from the weapon node over the points pulled to the line, glows at both ends and
 * sparks for the player; without target a straight line out of the weapon node */
void cObjElementFXHook_Update(cObjElementFX *base, cDebris *debris, float dt);
void cObjElementFXHook_StopTrails(cObjElementFXHook *self);
void cObjElementFXHook_StartTarget(cObjElementFXHook *self, cEntity *emitter, cEntity *target, int slot);
void cObjElementFXHook_StartPoint(cObjElementFXHook *self, cEntity *emitter, int slot);
byte cObjElementFXHook_IsAlive(cObjElementFX *base, cDebris *debris, cDebrisSys *sys);
void cObjElementFXHook_Start(cObjElementFX *base, cDebris *debris);
void cObjElementFXHook_Reset(cObjElementFX *base, cDebris *debris);

/* the bolts (while the missile is seen), new ones from a life of 0.8 on and the glows at the center */
void cObjElementFXEMI_Update(cObjElementFX *base, cDebris *debris, float dt);
byte cObjElementFXEMI_IsAlive(cObjElementFX *base, cDebris *debris, cDebrisSys *sys);
void cObjElementFXEMI_Start(cObjElementFX *base, cDebris *debris);
void cObjElementFXEMI_Reset(cObjElementFX *base, cDebris *debris);
void cObjElementFXEMI_SetEntity(cObjElementFXEMI *self, cEntity *e);

void cObjElementFXSonicExplosion_Update(cObjElementFX *base, cDebris *debris, float dt);
byte cObjElementFXSonicExplosion_IsAlive(cObjElementFX *base, cDebris *debris, cDebrisSys *sys);
void cObjElementFXSonicExplosion_Start(cObjElementFX *base, cDebris *debris);
void cObjElementFXSonicExplosion_Reset(cObjElementFX *base, cDebris *debris);
/* the rings and the particles of an explosion at x y z */
void cObjElementFXSonicExplosion_Explode(cObjElementFXSonicExplosion *self, float x, float y, float z);

/* the cloud around the missile (the countermeasure entity): a ring of particles that pulses, puffs of debris */
void cObjElementFXDebrisCloud_Update(cObjElementFX *base, cDebris *debris, float dt);
byte cObjElementFXDebrisCloud_IsAlive(cObjElementFX *base, cDebris *debris, cDebrisSys *sys);
void cObjElementFXDebrisCloud_Start(cObjElementFX *base, cDebris *debris);
void cObjElementFXDebrisCloud_Reset(cObjElementFX *base, cDebris *debris);
void cObjElementFXDebrisCloud_SetEntity(cObjElementFXDebrisCloud *self, cEntity *e);
/* four Updates with dt 0 */
void cObjElementFXDebrisCloud_Warm(cObjElementFXDebrisCloud *self, cDebris *debris);

/* the constructors and destructors (rva 0x1c9610..0x1c99c4, after World/FX/FXCharActions.cpp); the node of the
 * links of the EMI as array member */
Nto1Node *FXCountermeasures_NodeCtor(Nto1Node *self);
void *cObjElementFXDebrisCloud_ctor(void *self);
void *cObjElementFXEMI_ctor(void *self);
void *cObjElementFXHook_ctor(void *self);
void *cObjElementFXSonicExplosion_ctor(void *self);
void cObjElementFXDebrisCloud_dtor(void *self);
void cObjElementFXEMI_dtor(void *self);
void cObjElementFXHook_dtor(void *self);
void cObjElementFXSonicExplosion_dtor(void *self);
/* the deleting destructors */
cObjElementFX *cObjElementFXDebrisCloud_Delete(cObjElementFX *self, byte flags);
cObjElementFX *cObjElementFXEMI_Delete(cObjElementFX *self, byte flags);
cObjElementFX *cObjElementFXHook_Delete(cObjElementFX *self, byte flags);
cObjElementFX *cObjElementFXSonicExplosion_Delete(cObjElementFX *self, byte flags);

#endif
