/*
 * World/FX/FXMisc.cpp of the game: small effects of the world that ride on a debris of a debris system (the element
 * at cDebris + 0x54): the blink, fade and take effects of the items, the fade and noise of models, particle systems,
 * sparks with trails and trails. Also here: the visibility test of the effects, random vectors and the start of the
 * item and Sputnik effects. The element vtable: 0 deleting destructor, 1 Update (each frame, the debris and dt),
 * 2 Alive (the debris step, 0 ends the debris), 3 Attach (to the model of the debris), 4 Reset.
 * The constructors and destructors of the FX elements of the FX scenes and of the character effects (the ones after
 * World/CollisionGrid.cpp in the exe) are here too.
 */
#ifndef SCRAP_FXMISC_H
#define SCRAP_FXMISC_H

#include "scrap/crt.h"
#include "scrap/entity.h"
#include "scrap/scrap.h"
#include "scrap/scrapcamera.h"
#include "scrap/scrapparticles.h"
#include "scrap/scraptrails.h"

/* the debris an element rides on: its model first (CGameModel), the element at 0x54 */
#define Debris_Model(d) ((CGameModel *)(d))
#define Debris_Life(d) ((d)->speed)		/* 0x4c counts down by 0.05 a step, ends at 0 */
#define Debris_Element(d) ((void *)((cDebris *)(d))->fx)

/* Debris.cpp (rva 0x204940..) */
/* the destructor of the base class: the vtable of cObjElementAbstract */
void __thiscall cObjElement_dtor(void *self);
/* the debris moved by its velocities over dt */
void __thiscall cDebris_Move(cDebris *self, float dt);
/* the first idle debris of the system, else a new one */
cDebris *__thiscall cDebrisSys_GetIdle(cDebrisSys *self);
byte __thiscall cDebrisSys_IsIdle(cDebrisSys *self, cDebris *d);

/* the inline normalize of the effects: the length of (y y + x x) + z z, 1 / len in float; d as it is for length 0 */
static inline vec3_t FX_Unit(vec3_t d) {
	float len = (float)__builtin_sqrt((double)(d.y * d.y + d.x * d.x + d.z * d.z));

	if (len != 0.0f) {
		float k = 1.0f / len;
		d.x = d.x * k;
		d.y = d.y * k;
		d.z = d.z * k;
	}
	return d;
}

/* rand() & 0x7fff of the exe as 0..1 times range, plus add (all in double), the way the effects pick values */
static inline float FX_Rand(double range, double add) {
	return (float)((double)(crt_rand() & 0x7fff) * (1.0 / 32767.0) * range + add);
}

/* an entity that is deleted (the int at 0x20) or off: the effect lets it go */
#define Entity_Removed(e) (((cEntity *)(e))->deadNode.target)

/* the engine variables (Gvars): the files of the debris systems of the effects */
#define g_FXItemBlinkFile GAME_GLOBAL_IN(const char *, GVar_FXItemBlinkFile, 16, 0x467240)
#define g_FXItemFadeFile GAME_GLOBAL_IN(const char *, GVar_FXItemFadeFile, 16, 0x467228)
#define g_FXItemTakeFile GAME_GLOBAL_IN(const char *, GVar_FXItemTakeFile, 16, 0x467210)
#define g_FXCharacterSputnikFile GAME_GLOBAL_IN(const char *, GVar_FXCharacterSputnikFile, 16, 0x466e38)
#define g_FXShipEditBuildFile GAME_GLOBAL_IN(const char *, GVar_FXShipEditBuildFile, 16, 0x467258)
#define g_FXSmokeDebrisFile GAME_GLOBAL_IN(const char *, GVar_FXSmokeDebrisFile, 16, 0x4673c0)
#define g_FXFiredDebrisFile GAME_GLOBAL_IN(const char *, GVar_FXFiredDebrisFile, 16, 0x4673a8)
#define g_FXShipExplosionFile GAME_GLOBAL_IN(const char *, GVar_FXShipExplosionFile, 16, 0x467348)

/* the color ramps (9 colors) of the particles */
#define FX_RAMP_BLINK 0x430b44
#define FX_RAMP_FADE 0x430ad8		/* [3] */
#define FX_RAMP_TAKE 0x430b68		/* [3] */

#define FXSPARKS_MAXSPARKS 32
#define FXITEMTAKE_LINES 4
#define FXITEMTAKE_POINTS 14

/* the elements */

typedef struct cObjElementFXItemFade {
	void *vftable;			/* 0x00 */
	cEntity *ent;			/* 0x04 */
	CGameParticles particles;	/* 0x08 */
	int mode;			/* 0xb8 1 fade out (disabled at the end), 2 fade in */
	float duration;			/* 0xbc */
	float t;			/* 0xc0 */
	float scale;			/* 0xc4 */
	byte done;			/* 0xc8 */
	byte started;			/* 0xc9 */
} cObjElementFXItemFade;
SD_OFFSET(cObjElementFXItemFade, mode, 0xb8);
SD_OFFSET(cObjElementFXItemFade, started, 0xc9);

typedef struct cObjElementFXItemTake {
	void *vftable;			/* 0x000 */
	cEntity *ent;			/* 0x004 the item */
	cEntity *target;		/* 0x008 the one that takes it */
	CGameParticles particles;	/* 0x00c */
	vec3_t dir[FXITEMTAKE_LINES];	/* 0x0bc */
	CGameTrail trail;		/* 0x0ec */
	float phase[FXITEMTAKE_LINES * FXITEMTAKE_POINTS];	/* 0x0f0 0..1 along the curve */
	float speed[FXITEMTAKE_LINES * FXITEMTAKE_POINTS];	/* 0x1d0 */
	vec3_t offset[FXITEMTAKE_LINES * FXITEMTAKE_POINTS];	/* 0x2b0 */
	uint32_t color[FXITEMTAKE_LINES * FXITEMTAKE_POINTS];	/* 0x550 */
	int mode;			/* 0x630 */
	float unused_634;			/* 0x634 */
	float t;			/* 0x638 */
	float emitT;			/* 0x63c */
	float scale;			/* 0x640 */
	byte done;			/* 0x644 */
	byte started;			/* 0x645 */
} cObjElementFXItemTake;
SD_OFFSET(cObjElementFXItemTake, trail, 0xec);
SD_OFFSET(cObjElementFXItemTake, color, 0x550);
SD_OFFSET(cObjElementFXItemTake, started, 0x645);

typedef struct cObjElementFXModelFade {
	void *vftable;			/* 0x00 */
	float t;			/* 0x04 */
	float duration;			/* 0x08 */
	float fadeOut;			/* 0x0c the life of the debris over which it fades out */
} cObjElementFXModelFade;

typedef struct cObjElementFXModelNoise {
	void *vftable;			/* 0x00 */
	cEntity *ent;			/* 0x04 */
	float t;			/* 0x08 */
	float duration;			/* 0x0c */
} cObjElementFXModelNoise;

typedef struct cObjElementFXParticleSystem {
	void *vftable;			/* 0x00 */
	CGameParticles particles;	/* 0x04 */
	byte move;			/* 0xb4 the debris moves */
	byte on;			/* 0xb5 */
} cObjElementFXParticleSystem;
SD_OFFSET(cObjElementFXParticleSystem, move, 0xb4);
/* the same with the particles on "Dummy01 (+dudv_pass)" (0xb8 bytes) */
typedef cObjElementFXParticleSystem cObjElementFXParticleSystemDuDv;

typedef struct cObjElementFXSparks {
	void *vftable;			/* 0x000 */
	float time;			/* 0x004 */
	float dt;			/* 0x008 the time of the frame */
	byte pad_0c[4];
	byte active[FXSPARKS_MAXSPARKS];	/* 0x010 */
	vec3_t pos[FXSPARKS_MAXSPARKS];		/* 0x030 */
	vec3_t vel[FXSPARKS_MAXSPARKS];		/* 0x1b0 */
	vec3_t acc[FXSPARKS_MAXSPARKS];		/* 0x330 */
	vec3_t jerk[FXSPARKS_MAXSPARKS];	/* 0x4b0 */
	float life[FXSPARKS_MAXSPARKS];		/* 0x630 */
	float lifeMax[FXSPARKS_MAXSPARKS];	/* 0x6b0 */
	float delay[FXSPARKS_MAXSPARKS];	/* 0x730 */
	float size[FXSPARKS_MAXSPARKS];		/* 0x7b0 */
	uint32_t color[FXSPARKS_MAXSPARKS];	/* 0x830 */
	float noise[FXSPARKS_MAXSPARKS];	/* 0x8b0 */
	int segments[FXSPARKS_MAXSPARKS];	/* 0x930 */
	float trailT;				/* 0x9b0 */
	int sprite[FXSPARKS_MAXSPARKS];		/* 0x9b4 -1 for none */
	int numActive;				/* 0xa34 */
	int activeList[FXSPARKS_MAXSPARKS];	/* 0xa38 */
	int numFree;				/* 0xab8 */
	int freeList[FXSPARKS_MAXSPARKS];	/* 0xabc */
	CGameTrail trails[FXSPARKS_MAXSPARKS];	/* 0xb3c */
	CGameParticles particles;		/* 0xbbc */
	byte on;				/* 0xc6c set by the hook laser that emits */
	byte pad_c6d[3];
} cObjElementFXSparks;
SD_SIZE(cObjElementFXSparks, 0xc70);
SD_OFFSET(cObjElementFXSparks, life, 0x630);
SD_OFFSET(cObjElementFXSparks, trailT, 0x9b0);
SD_OFFSET(cObjElementFXSparks, trails, 0xb3c);
SD_OFFSET(cObjElementFXSparks, particles, 0xbbc);

typedef struct cObjElementFXTrail {
	void *vftable;			/* 0x00 */
	CGameTrail trail;		/* 0x04 */
	vec3_t last;			/* 0x08 the last point added */
	float minDist;			/* 0x14 */
	byte stopped;			/* 0x18 */
	byte on;			/* 0x19 */
} cObjElementFXTrail;
SD_OFFSET(cObjElementFXTrail, on, 0x19);

typedef struct cObjElementFXFadeOutTrail {
	void *vftable;			/* 0x00 */
	CGameTrail trail;		/* 0x04 */
	float duration;			/* 0x08 */
	float t;			/* 0x0c */
	byte running;			/* 0x10 */
	byte on;			/* 0x11 */
} cObjElementFXFadeOutTrail;
SD_OFFSET(cObjElementFXFadeOutTrail, on, 0x11);

/* ScrapEngLensFlares.cpp: the lens flare holder of the scene elements */
void *__thiscall CGameLensFlare_ctor(void *self);
void __thiscall CGameLensFlare_dtor(void *self);

/* Math (rva 0x066ad0, 0x072340): the angle between two vectors (radians); in normalized into out */
double Vec3_Angle(const vec3_t *a, const vec3_t *b);
vec3_t *Vec3_NormalizeTo(vec3_t *out, const vec3_t *in);

/* the element methods; the same code of several classes is one function */
byte __thiscall FXMisc_Alive(void *self, cDebris *d, cDebrisSys *sys);
void __thiscall FXMisc_Nop(void *self, int arg);
void __thiscall FXMisc_Clear4(void *self, int arg);
void __thiscall FXMisc_Clear8(void *self, int arg);
void __thiscall cObjElementFXItemFade_Update(cObjElementFXItemFade *self, cDebris *d, float dt);
void __thiscall cObjElementFXItemFade_Attach(cObjElementFXItemFade *self, cDebris *d);
void __thiscall cObjElementFXItemFade_Reset(cObjElementFXItemFade *self, int arg);
/* the fade of the model of e starts with the blink particles around it */
void __thiscall cObjElementFXItemFade_Start(cObjElementFXItemFade *self, cEntity *e);
void __thiscall cObjElementFXItemTake_Update(cObjElementFXItemTake *self, cDebris *d, float dt);
void __thiscall cObjElementFXItemTake_Attach(cObjElementFXItemTake *self, cDebris *d);
void __thiscall cObjElementFXItemTake_Reset(cObjElementFXItemTake *self, int arg);
/* the random lines of particles from e (to the target set after it) */
void __thiscall cObjElementFXItemTake_Start(cObjElementFXItemTake *self, cEntity *e);
void __thiscall cObjElementFXModelFade_Update(cObjElementFXModelFade *self, cDebris *d, float dt);
byte __thiscall cObjElementFXModelFade_Alive(cObjElementFXModelFade *self, cDebris *d, cDebrisSys *sys);
void __thiscall cObjElementFXModelNoise_Update(cObjElementFXModelNoise *self, cDebris *d, float dt);
byte __thiscall cObjElementFXModelNoise_Alive(cObjElementFXModelNoise *self, cDebris *d, cDebrisSys *sys);
void __thiscall cObjElementFXParticleSystem_Update(cObjElementFXParticleSystem *self, cDebris *d, float dt);
void __thiscall cObjElementFXParticleSystem_Attach(cObjElementFXParticleSystem *self, cDebris *d);
void __thiscall cObjElementFXParticleSystemDuDv_Attach(cObjElementFXParticleSystemDuDv *self, cDebris *d);
void __thiscall cObjElementFXSparks_Update(cObjElementFXSparks *self, cDebris *d, float dt);
void __thiscall cObjElementFXSparks_Attach(cObjElementFXSparks *self, cDebris *d);
void __thiscall cObjElementFXSparks_Reset(cObjElementFXSparks *self, int arg);
/* n sparks at p along dir turned by up to the spread angles; the pairs are random ranges */
void __thiscall cObjElementFXSparks_Emit(cObjElementFXSparks *self, int n, float px, float py, float pz, float dx,
	float dy, float dz, float sx, float sy, float sz, float jx, float jy, float jz, float speedMin, float speedMax,
	float sizeMin, float sizeMax, float scaleHead, float scaleTail, float lifeMin, float lifeMax, float lmMin,
	float lmMax, float delayMin, float delayMax, uint32_t color, float noiseMin, float noiseMax, float segMin,
	float segMax, int sprite);
void __thiscall cObjElementFXTrail_Update(cObjElementFXTrail *self, cDebris *d, float dt);
byte __thiscall cObjElementFXTrail_Alive(cObjElementFXTrail *self, cDebris *d, cDebrisSys *sys);
void __thiscall cObjElementFXTrail_Reset(cObjElementFXTrail *self, int arg);
byte __thiscall cObjElementFXFadeOutTrail_Alive(cObjElementFXFadeOutTrail *self, cDebris *d, cDebrisSys *sys);
void __thiscall cObjElementFXFadeOutTrail_Reset(cObjElementFXFadeOutTrail *self, int arg);
void __thiscall cObjElementFXFadeOutTrail_Update(cObjElementFXFadeOutTrail *self, cDebris *d, float dt);
/* the constructors and destructors (rva 0x1da190..0x1da6c8, fastcall) and the deleting destructors (vtable slot
 * 0, rva 0x1da6d0..0x1da940) */
void *__fastcall cObjElementFXFadeOutTrail_ctor(void *self);
void *__fastcall cObjElementFXItemFade_ctor(void *self);
void *__fastcall cObjElementFXItemTake_ctor(void *self);
void *__fastcall cObjElementFXModelFade_ctor(void *self);
void *__fastcall cObjElementFXModelNoise_ctor(void *self);
void *__fastcall cObjElementFXParticleSystem_ctor(void *self);
cObjElementFXParticleSystemDuDv *__fastcall cObjElementFXParticleSystemDuDv_ctor(cObjElementFXParticleSystemDuDv *self);
void *__fastcall cObjElementFXSparks_ctor(void *self);
void *__fastcall cObjElementFXTrail_ctor(void *self);
void __fastcall cObjElementFXFadeOutTrail_dtor(void *self);
void __fastcall cObjElementFXItemFade_dtor(void *self);
void __fastcall cObjElementFXItemTake_dtor(void *self);
void __fastcall cObjElementFXModelFade_dtor(void *self);
void __fastcall cObjElementFXModelNoise_dtor(void *self);
void __fastcall cObjElementFXParticleSystem_dtor(void *self);
void __fastcall cObjElementFXParticleSystemDuDv_dtor(cObjElementFXParticleSystemDuDv *self);
void __fastcall cObjElementFXSparks_dtor(void *self);
void __fastcall cObjElementFXTrail_dtor(void *self);
void *__thiscall cObjElementFXFadeOutTrail_Delete(void *self, uint32_t flags);
void *__thiscall cObjElementFXItemFade_Delete(void *self, uint32_t flags);
void *__thiscall cObjElementFXItemTake_Delete(void *self, uint32_t flags);
void *__thiscall cObjElementFXModelFade_Delete(void *self, uint32_t flags);
void *__thiscall cObjElementFXModelNoise_Delete(void *self, uint32_t flags);
void *__thiscall cObjElementFXParticleSystem_Delete(void *self, uint32_t flags);
cObjElementFXParticleSystemDuDv *__thiscall cObjElementFXParticleSystemDuDv_Delete(
	cObjElementFXParticleSystemDuDv *self,
	uint32_t flags);
void *__thiscall cObjElementFXSparks_Delete(void *self, uint32_t flags);
void *__thiscall cObjElementFXTrail_Delete(void *self, uint32_t flags);
/* the elements of the effects started by name: the entity (its model a CHECK), fields at 0x8 and 0xc */
void __thiscall FXMisc_SetEntity(void *self, cEntity *e);
void __thiscall FXMisc_SetF8(void *self, float v);
void __thiscall FXMisc_SetFC(void *self, float v);
void __thiscall FXMisc_SetI8(void *self, int v);

/* the free functions */
/* the particles to "Dummy01" of the model, to the node index of the pointer itself without a model (as the
 * original does) */
void FX_AttachDummy(CGameParticles *sp, CGameModel *m);
/* a new debris of the system of file, life set, dropped; its element */
void *FX_DropNew(const char *file, float life);
/* the sphere x y z (radius) seen by the camera (the actual one for NULL) nearer than maxDist (0 any), in its view
 * cone and with sector in a visible sector; 1 always in split screen */
byte FX_IsVisible(float x, float y, float z, float radius, float maxDist, const CGameCamera *cam, byte sector);
/* the blink of an item */
void FX_ItemBlink(cEntity *e);
/* the fade of an item (mode 1 out, 2 in) and the take effect towards target */
void FX_ItemFade(cEntity *e, int mode, float scale, cEntity *target);
/* the Sputnik and ShipEditBuild effects of an entity by its name */
void FX_Sputnik(cEntity *e, const char *name);
/* from <= x <= to; the place of x in it (0..1) into frac */
byte FX_InRange(float x, float from, float to, float *frac);
vec3_t *FX_RandomVec(vec3_t *out, const vec3_t *lo, const vec3_t *hi);
/* (1, 1, 1) turned by three random angles */
vec3_t *FX_RandomDir(vec3_t *out);

#endif
