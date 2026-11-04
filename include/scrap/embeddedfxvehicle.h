/*
 * Entities/EmbeddedFX/EmbeddedFXVehicle.cpp of the game: the effects embedded in the vehicles (a cVehicleEntity
 * keeps one at +0x224, SetEmbeddedFX "Em..."). cEmbeddedFXVehicle follows the model of the vehicle with its own
 * model (VehicleEmbeddedFX.M3D, TrafficEmbeddedFX.M3D for traffic) and two particle systems of it, fades the
 * vehicle in and out with the distance to the camera, and owns the police sirens ("SPoli" vehicles, a model and
 * particle system shared by all of them) and the effects of the traffic types TProbe (cEmbeddedFXVehicleProbe) and
 * TVoteMayor / TVoteBishop (cEmbeddedFXVehicleVote).
 * The vtable of the three classes (cEmbeddedFX): 0 Free, 1 Init, 2 Done, 3 Configure (the argument
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
	void (*Free)(cEmbeddedFXVehicleSub *self);
	byte(*Init)(cEmbeddedFXVehicleSub *self);
	void(*Done)(cEmbeddedFXVehicleSub *self);
	void(*Configure)(cEmbeddedFXVehicleSub *self, float unused);	/* cEmbeddedFX Update, called with the frame time */
	void(*Update)(cEmbeddedFXVehicleSub *self);
	void(*SetActive)(cEmbeddedFXVehicleSub *self, byte on);
} cEmbeddedFXVehicleVtbl;

/* calls through the slots of cEmbeddedFXVehicleVtbl */
static inline void EFXVeh_Free(void *self) {
	VT(cEmbeddedFXVehicleVtbl, self)->Free(self);
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
		const cEmbeddedFXVehicleVtbl *vftable;
	};
	cEntity *owner;			/* the vehicle */
	float camDist;			/* to the camera of the view */
	CGameModel model;
	CGameParticles follow;		/* node Dummy01 */
	CGameParticles follow2;		/* node Dummy02 (not traffic) */
	byte visible;			/* the vehicle model is visible */
	byte followed;			/* the car of player 0 follows this vehicle (its name at +0x650) */
	byte traffic;
	byte fading;			/* fades with the AI distances (StartFade) */
	byte aiRange;			/* the distances of the AI of the vehicle, else MinDistReposAI.. */
	byte pad_18d[3];
	float alpha;			/* 1 opaque */
	float time;			/* since the visibility changed */
	vec3_t rootPos;			/* DC_Root of the vehicle model */
	void *attached;			/* an FXVehicle element drawn with the followed vehicle once */
	cFXVehicleSirens *sirens;
	cEmbeddedFXVehicleSub *other;	/* Probe or Vote */
} cEmbeddedFXVehicle;

/* two flashes per period at the DF_SpeedTrail_01/02 dummies of the vehicle */
struct cFXVehicleSirens {
	cEmbeddedFXVehicle *owner;
	vec3_t pos[2];			/* the dummies relative to DC_Root */
	float time;			/* 0..0.9 */
	float lastTime;
};

#define PROBE_LIGHTS 4
typedef struct cEmbeddedFXVehicleProbe {
	union {
		cEmbeddedFXVehicleSub sub;
		const cEmbeddedFXVehicleVtbl *vftable;
	};
	cEmbeddedFXVehicle *fx;
	byte visible;
	byte pad_009[3];
	CGameParticles lights[PROBE_LIGHTS];	/* DF_Sonda_Glows_A..D */
	CGameParticles trans;		/* DF_Sonda_Transmision */
	int transNode;
	float lightTime;		/* 0..1.8 */
	float transTime;		/* 0..1 */
	CGameTrail trail;		/* the rings */
	float ringTime;			/* 0..1 */
} cEmbeddedFXVehicleProbe;

#define VOTE_MOTORS 3
#define VOTE_MAYOR 1
#define VOTE_BISHOP 2
typedef struct cEmbeddedFXVehicleVote {
	union {
		cEmbeddedFXVehicleSub sub;
		const cEmbeddedFXVehicleVtbl *vftable;
	};
	cEmbeddedFXVehicle *fx;
	byte visible;
	byte pad_009[3];
	int glowNode;			/* DS_Electoral_ArzobispoGlow */
	CGameParticles glow;
	int rootNode;			/* DC_Root */
	CGameTrail trails[2];
	CGameParticles motor[VOTE_MOTORS];	/* DF_MotorTrust_01..03 */
	int type;			/* VOTE_MAYOR, VOTE_BISHOP */
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
