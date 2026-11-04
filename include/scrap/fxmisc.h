/*
 * World/FX/FXMisc.cpp of the game: small effects of the world that ride on a debris of a debris system (the element
 * at cDebris + 0x54): the blink, fade and take effects of the items, the fade and noise of models, particle systems,
 * sparks with trails and trails. Also here: the visibility test of the effects, random vectors and the start of the
 * item and Sputnik effects. The element vtable: 0 Free, 1 Update (each frame, the debris and dt),
 * 2 Alive (the debris step, 0 ends the debris), 3 Attach (to the model of the debris), 4 Reset.
 * The constructors and destructors of the FX elements of the FX scenes and of the character effects (the ones after
 * World/CollisionGrid.cpp in the exe) are here too.
 */
#ifndef SCRAP_FXMISC_H
#define SCRAP_FXMISC_H

#include "scrap/gvar.h"
#include "scrap/crt.h"
#include "scrap/entity.h"
#include "scrap/fxcountermeasures.h"
#include "scrap/scrap.h"
#include "scrap/scrapcamera.h"
#include "scrap/scrapparticles.h"
#include "scrap/scraptrails.h"

/* the debris an element rides on: its model first (CGameModel), the element at 0x54 */
#define Debris_Model(d) ((CGameModel *)(d))
#define Debris_Element(d) ((void *)((cDebris *)(d))->fx)

/* Debris.cpp (..) */
/* the destructor of the base class: the vtable of cObjElementAbstract */
void cObjElement_dtor(void *self);
void cObjElementAbstract_Free(cObjElementFX *base);
/* the debris moved by its velocities over dt */
void cDebris_Move(cDebris *self, float dt);
/* the first idle debris of the system, else a new one */
cDebris *cDebrisSys_GetIdle(cDebrisSys *self);
byte cDebrisSys_IsIdle(cDebrisSys *self, cDebris *d);

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

extern cGVar GVar_FXCharacterSputnikFile;
#define g_FXCharacterSputnikFile (GVar_FXCharacterSputnikFile.value.s)
extern cGVar GVar_FXSmokeDebrisFile;
#define g_FXSmokeDebrisFile (GVar_FXSmokeDebrisFile.value.s)

/* the color ramps (9 colors) of the particles */

#define FXSPARKS_MAXSPARKS 32
#define FXITEMTAKE_LINES 4
#define FXITEMTAKE_POINTS 14

/* the elements */

typedef struct cObjElementFXItemFade {
	const cObjElementFXVtbl *vftable;
	cEntity *ent;
	CGameParticles particles;
	int mode;			/* 1 fade out (disabled at the end), 2 fade in */
	float duration;
	float t;
	float scale;
	byte done;
	byte started;
} cObjElementFXItemFade;

typedef struct cObjElementFXItemTake {
	const cObjElementFXVtbl *vftable;
	cEntity *ent;			/* the item */
	cEntity *target;		/* the one that takes it */
	CGameParticles particles;
	vec3_t dir[FXITEMTAKE_LINES];
	CGameTrail trail;
	float phase[FXITEMTAKE_LINES * FXITEMTAKE_POINTS];	/* 0..1 along the curve */
	float speed[FXITEMTAKE_LINES * FXITEMTAKE_POINTS];
	vec3_t offset[FXITEMTAKE_LINES * FXITEMTAKE_POINTS];
	uint32_t color[FXITEMTAKE_LINES * FXITEMTAKE_POINTS];
	int mode;
	float unused_634;
	float t;
	float emitT;
	float scale;
	byte done;
	byte started;
} cObjElementFXItemTake;

/* the start of the effect classes that the setters by name reach (FXMisc_SetEntity, SetF8, SetFC, SetI8): the
 * vtable, the entity, two words and the started flag */
typedef struct FXElemHead {
	const cObjElementFXVtbl *vftable;
	cEntity *ent;
	union {
		float f;
		int i;
	} w8;
	float fC;
	byte started;
} FXElemHead;

typedef struct cObjElementFXModelFade {
	const cObjElementFXVtbl *vftable;
	float t;
	float duration;
	float fadeOut;			/* the life of the debris over which it fades out */
	byte unused_10;
	byte pad_11[3];
} cObjElementFXModelFade;

typedef struct cObjElementFXModelNoise {
	const cObjElementFXVtbl *vftable;
	cEntity *ent;
	float t;
	float duration;
	byte unused_10;
	byte pad_11[3];
} cObjElementFXModelNoise;

typedef struct cObjElementFXParticleSystem {
	const cObjElementFXVtbl *vftable;
	CGameParticles particles;
	byte move;			/* the debris moves */
	byte on;
} cObjElementFXParticleSystem;
/* the same with the particles on "Dummy01 (+dudv_pass)" */
typedef cObjElementFXParticleSystem cObjElementFXParticleSystemDuDv;

typedef struct cObjElementFXSparks {
	const cObjElementFXVtbl *vftable;
	float time;
	float dt;			/* the time of the frame */
	byte pad_0c[4];
	byte active[FXSPARKS_MAXSPARKS];
	vec3_t pos[FXSPARKS_MAXSPARKS];
	vec3_t vel[FXSPARKS_MAXSPARKS];
	vec3_t acc[FXSPARKS_MAXSPARKS];
	vec3_t jerk[FXSPARKS_MAXSPARKS];
	float life[FXSPARKS_MAXSPARKS];
	float lifeMax[FXSPARKS_MAXSPARKS];
	float delay[FXSPARKS_MAXSPARKS];
	float size[FXSPARKS_MAXSPARKS];
	uint32_t color[FXSPARKS_MAXSPARKS];
	float noise[FXSPARKS_MAXSPARKS];
	int segments[FXSPARKS_MAXSPARKS];
	float trailT;
	int sprite[FXSPARKS_MAXSPARKS];		/* -1 for none */
	int numActive;
	int activeList[FXSPARKS_MAXSPARKS];
	int numFree;
	int freeList[FXSPARKS_MAXSPARKS];
	CGameTrail trails[FXSPARKS_MAXSPARKS];
	CGameParticles particles;
	byte on;				/* set by the hook laser that emits */
	byte pad_c6d[3];
} cObjElementFXSparks;

typedef struct cObjElementFXTrail {
	const cObjElementFXVtbl *vftable;
	CGameTrail trail;
	vec3_t last;			/* the last point added */
	float minDist;
	byte stopped;
	byte on;
	byte unused_1a;			/* cleared with on by the constructor */
	byte pad_1b;
} cObjElementFXTrail;

typedef struct cObjElementFXFadeOutTrail {
	const cObjElementFXVtbl *vftable;
	CGameTrail trail;
	float duration;
	float t;
	byte running;
	byte on;
} cObjElementFXFadeOutTrail;

/* ScrapEngLensFlares.cpp: the lens flare holder of the scene elements */
void *CGameLensFlare_ctor(void *self);
void CGameLensFlare_dtor(void *self);

/* Math: the angle between two vectors (radians); in normalized into out */
double Vec3_Angle(const vec3_t *a, const vec3_t *b);
vec3_t *Vec3_NormalizeTo(vec3_t *out, const vec3_t *in);

/* the element methods; the same code of several classes is one function */
byte FXMisc_Alive(cObjElementFX *base, cDebris *d, cDebrisSys *sys);
/* n sparks at p along dir turned by up to the spread angles; the pairs are random ranges */
void cObjElementFXSparks_Emit(cObjElementFXSparks *self, int n, float px, float py, float pz, float dx,
	float dy, float dz, float sx, float sy, float sz, float jx, float jy, float jz, float speedMin, float speedMax,
	float sizeMin, float sizeMax, float scaleHead, float scaleTail, float lifeMin, float lifeMax, float lmMin,
	float lmMax, float delayMin, float delayMax, uint32_t color, float noiseMin, float noiseMax, float segMin,
	float segMax, int sprite);
/* the constructors, the destructors and the Free slots */
void *cObjElementFXFadeOutTrail_ctor(void *self);
void *cObjElementFXItemFade_ctor(void *self);
void *cObjElementFXItemTake_ctor(void *self);
void *cObjElementFXModelFade_ctor(void *self);
void *cObjElementFXModelNoise_ctor(void *self);
void *cObjElementFXParticleSystem_ctor(void *self);
cObjElementFXParticleSystemDuDv *cObjElementFXParticleSystemDuDv_ctor(cObjElementFXParticleSystemDuDv *self);
void *cObjElementFXSparks_ctor(void *self);
void *cObjElementFXTrail_ctor(void *self);
/* the elements of the effects started by name: the entity (its model a CHECK), fields at 0x8 and 0xc */
void FXMisc_SetEntity(void *self, cEntity *e);
void FXMisc_SetF8(void *self, float v);
void FXMisc_SetFC(void *self, float v);

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
