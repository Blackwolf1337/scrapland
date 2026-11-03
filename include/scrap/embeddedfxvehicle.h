/*
 * Entities/EmbeddedFX/EmbeddedFXVehicle.cpp of the game: the effects embedded in the vehicles (a cVehicleEntity
 * keeps one at +0x224, SetEmbeddedFX "Em..."). cEmbeddedFXVehicle follows the model of the vehicle with its own
 * model (VehicleEmbeddedFX.M3D, TrafficEmbeddedFX.M3D for traffic) and two particle systems of it, fades the
 * vehicle in and out with the distance to the camera, and owns the police sirens ("SPoli" vehicles, a model and
 * particle system shared by all of them) and the effects of the traffic types TProbe (cEmbeddedFXVehicleProbe) and
 * TVoteMayor / TVoteBishop (cEmbeddedFXVehicleVote).
 * The vtable of the three classes (cEmbeddedFX): 0 deleting destructor, 1 Init, 2 Done, 3 Configure (the argument
 * is not used), 4 Update (every frame), 5 SetActive.
 */
#ifndef SCRAP_EMBEDDEDFXVEHICLE_H
#define SCRAP_EMBEDDEDFXVEHICLE_H

#include "scrap/gvar.h"
#include "scrap/embeddedfxchar.h"
#include "scrap/entity.h"
#include "scrap/scrapparticles.h"
#include "scrap/scraptrails.h"

typedef struct cEmbeddedFXVehicleSub cEmbeddedFXVehicleSub;

typedef struct cEmbeddedFXVehicleVtbl {
	cEmbeddedFXVehicleSub *(*Delete)(cEmbeddedFXVehicleSub *self, byte flags);	/* 0x00 */
	byte(*Init)(cEmbeddedFXVehicleSub *self);			/* 0x04 */
	void(*Done)(cEmbeddedFXVehicleSub *self);			/* 0x08 */
	void(*Configure)(cEmbeddedFXVehicleSub *self, float unused);	/* 0x0c cEmbeddedFX Update, called with the frame time */
	void(*Update)(cEmbeddedFXVehicleSub *self);			/* 0x10 */
	void(*SetActive)(cEmbeddedFXVehicleSub *self, byte on);	/* 0x14 */
} cEmbeddedFXVehicleVtbl;

/* calls through the slots of cEmbeddedFXVehicleVtbl */
static inline cEmbeddedFXVehicleSub *EFXVeh_Delete(void *self, byte flags) {
	return VT(cEmbeddedFXVehicleVtbl, self)->Delete(self, flags);
}

static inline void EFXVeh_Done(void *self) {
	VT(cEmbeddedFXVehicleVtbl, self)->Done(self);
}

static inline byte EFXVeh_Init(void *self) {
	return VT(cEmbeddedFXVehicleVtbl, self)->Init(self);
}

static inline void EFXVeh_Update(void *self) {
	VT(cEmbeddedFXVehicleVtbl, self)->Update(self);
}

/* the common head of the three classes: the vtable */
struct cEmbeddedFXVehicleSub {
	const cEmbeddedFXVehicleVtbl *vftable;
};

typedef struct cFXVehicleSirens cFXVehicleSirens;

typedef struct cEmbeddedFXVehicle {
	union {
		cEmbeddedFXVehicleSub sub;
		const cEmbeddedFXVehicleVtbl *vftable;	/* 0x000 */
	};
	cEntity *owner;			/* 0x004 the vehicle */
	float camDist;			/* 0x008 to the camera of the view */
	CGameModel model;		/* 0x00c */
	CGameParticles follow;		/* 0x028 node Dummy01 */
	CGameParticles follow2;		/* 0x0d8 node Dummy02 (not traffic) */
	byte visible;			/* 0x188 the vehicle model is visible */
	byte followed;			/* 0x189 the car of player 0 follows this vehicle (its name at +0x650) */
	byte traffic;			/* 0x18a */
	byte fading;			/* 0x18b fades with the AI distances (StartFade) */
	byte aiRange;			/* 0x18c the distances of the AI of the vehicle, else MinDistReposAI.. */
	byte pad_18d[3];
	float alpha;			/* 0x190 1 opaque */
	float time;			/* 0x194 since the visibility changed */
	vec3_t rootPos;			/* 0x198 DC_Root of the vehicle model */
	void *attached;			/* 0x1a4 an FXVehicle element drawn with the followed vehicle once */
	cFXVehicleSirens *sirens;	/* 0x1a8 */
	cEmbeddedFXVehicleSub *other;	/* 0x1ac Probe or Vote */
} cEmbeddedFXVehicle;

/* two flashes per period at the DF_SpeedTrail_01/02 dummies of the vehicle */
struct cFXVehicleSirens {
	cEmbeddedFXVehicle *owner;	/* 0x00 */
	vec3_t pos[2];			/* 0x04 the dummies relative to DC_Root */
	float time;			/* 0x1c 0..0.9 */
	float lastTime;			/* 0x20 */
};

#define PROBE_LIGHTS 4
typedef struct cEmbeddedFXVehicleProbe {
	union {
		cEmbeddedFXVehicleSub sub;
		const cEmbeddedFXVehicleVtbl *vftable;	/* 0x000 */
	};
	cEmbeddedFXVehicle *fx;		/* 0x004 */
	byte visible;			/* 0x008 */
	byte pad_009[3];
	CGameParticles lights[PROBE_LIGHTS];	/* 0x00c DF_Sonda_Glows_A..D */
	CGameParticles trans;		/* 0x2cc DF_Sonda_Transmision */
	int transNode;			/* 0x37c */
	float lightTime;		/* 0x380 0..1.8 */
	float transTime;		/* 0x384 0..1 */
	CGameTrail trail;		/* 0x388 the rings */
	float ringTime;			/* 0x38c 0..1 */
} cEmbeddedFXVehicleProbe;

#define VOTE_MOTORS 3
#define VOTE_MAYOR 1
#define VOTE_BISHOP 2
typedef struct cEmbeddedFXVehicleVote {
	union {
		cEmbeddedFXVehicleSub sub;
		const cEmbeddedFXVehicleVtbl *vftable;	/* 0x000 */
	};
	cEmbeddedFXVehicle *fx;		/* 0x004 */
	byte visible;			/* 0x008 */
	byte pad_009[3];
	int glowNode;			/* 0x00c DS_Electoral_ArzobispoGlow */
	CGameParticles glow;		/* 0x010 */
	int rootNode;			/* 0x0c0 DC_Root */
	CGameTrail trails[2];		/* 0x0c4 */
	CGameParticles motor[VOTE_MOTORS];	/* 0x0cc DF_MotorTrust_01..03 */
	int type;			/* 0x2dc VOTE_MAYOR, VOTE_BISHOP */
} cEmbeddedFXVehicleVote;

/* the vehicle fields used here */

extern cGVar GVar_MinDistReposAI;
#define g_MinDistReposAI (GVar_MinDistReposAI.value.f)
extern cGVar GVar_MaxDistReposAI;
#define g_MaxDistReposAI (GVar_MaxDistReposAI.value.f)

/* World/FX/FXVehicle.cpp: an element drawn with the followed vehicle */
void FXVehicle_RenderAttached(void *element);
/* the near and far distance of the AI of a vehicle */
void cAIVehicle_GetFadeDists(void *ai, float *nearDist, float *farDist);

cEmbeddedFXVehicle *cEmbeddedFXVehicle_ctor(cEmbeddedFXVehicle *self);
void cEmbeddedFXVehicle_Render(cEmbeddedFXVehicle *self);
void cEmbeddedFXVehicle_SetAttached(cEmbeddedFXVehicle *self, void *element);
/* the model, sirens and Probe / Vote of the vehicle */
void cEmbeddedFXVehicle_SetOwner(cEmbeddedFXVehicle *self, cEntity *owner);
void cEmbeddedFXVehicle_SetTraffic(cEmbeddedFXVehicle *self, byte on);
void cEmbeddedFXVehicle_StartFade(cEmbeddedFXVehicle *self);

#endif
