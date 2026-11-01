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

#include "scrap/embeddedfxchar.h"
#include "scrap/entity.h"
#include "scrap/scrapparticles.h"
#include "scrap/scraptrails.h"

#define CEMBEDDEDFXVEHICLE_VFTABLE GAME_VFTABLE(cEmbeddedFXVehicle_vftable)
#define CEMBEDDEDFXVEHICLEPROBE_VFTABLE GAME_VFTABLE(cEmbeddedFXVehicleProbe_vftable)
#define CEMBEDDEDFXVEHICLEVOTE_VFTABLE GAME_VFTABLE(cEmbeddedFXVehicleVote_vftable)

typedef struct cEmbeddedFXVehicleVtbl {
	void *(*Delete)(void *self, byte flags);	/* 0x00 */
	byte(*Init)(void *self);			/* 0x04 */
	void(*Done)(void *self);			/* 0x08 */
	void(*Configure)(void *self, int unused);	/* 0x0c */
	void(*Update)(void *self);			/* 0x10 */
	void(*SetActive)(void *self, byte on);	/* 0x14 */
} cEmbeddedFXVehicleVtbl;

/* Probe or Vote: the vtable first, the cEmbeddedFXVehicle after it */
typedef struct cEmbeddedFXVehicleSub {
	const cEmbeddedFXVehicleVtbl *vftable;
} cEmbeddedFXVehicleSub;

typedef struct cFXVehicleSirens cFXVehicleSirens;

typedef struct cEmbeddedFXVehicle {
	const cEmbeddedFXVehicleVtbl *vftable;	/* 0x000 */
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
SD_OFFSET(cEmbeddedFXVehicle, follow, 0x28);
SD_OFFSET(cEmbeddedFXVehicle, visible, 0x188);
SD_OFFSET(cEmbeddedFXVehicle, alpha, 0x190);
SD_OFFSET(cEmbeddedFXVehicle, attached, 0x1a4);
SD_SIZE(cEmbeddedFXVehicle, 0x1b0);

/* two flashes per period at the DF_SpeedTrail_01/02 dummies of the vehicle */
struct cFXVehicleSirens {
	cEmbeddedFXVehicle *owner;	/* 0x00 */
	vec3_t pos[2];			/* 0x04 the dummies relative to DC_Root */
	float time;			/* 0x1c 0..0.9 */
	float lastTime;			/* 0x20 */
};
SD_SIZE(cFXVehicleSirens, 0x24);

#define PROBE_LIGHTS 4
typedef struct cEmbeddedFXVehicleProbe {
	const cEmbeddedFXVehicleVtbl *vftable;	/* 0x000 */
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
SD_OFFSET(cEmbeddedFXVehicleProbe, trans, 0x2cc);
SD_OFFSET(cEmbeddedFXVehicleProbe, trail, 0x388);
SD_SIZE(cEmbeddedFXVehicleProbe, 0x390);

#define VOTE_MOTORS 3
#define VOTE_MAYOR 1
#define VOTE_BISHOP 2
typedef struct cEmbeddedFXVehicleVote {
	const cEmbeddedFXVehicleVtbl *vftable;	/* 0x000 */
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
SD_OFFSET(cEmbeddedFXVehicleVote, glow, 0x10);
SD_OFFSET(cEmbeddedFXVehicleVote, trails, 0xc4);
SD_OFFSET(cEmbeddedFXVehicleVote, type, 0x2dc);
SD_SIZE(cEmbeddedFXVehicleVote, 0x2e0);

/* the vehicle fields used here */

/* the shared siren model and particles */
#define g_SirenModel (GAME_ARRAY(CGameModel, g_SirenModel))
extern int g_SirenRefs;
#define g_SirenParticles (GAME_ARRAY(CGameParticles, g_SirenParticles))

/* engine variables */
#define g_FXVehicleEmbeddedFile GAME_GLOBAL_IN(const char *, GVar_FXVehicleEmbeddedFile, 16)	/* the string of the cString */
#define g_FXTrafficEmbeddedFile GAME_GLOBAL_IN(const char *, GVar_FXTrafficEmbeddedFile, 16)
#define g_FXVehicleSirenDistGrow GAME_GLOBAL_IN(float, GVar_FXVehicleSiren_DistGrow, 16)
#define g_FXVehicleSirenSizeMin GAME_GLOBAL_IN(float, GVar_FXVehicleSiren_SizeMin, 16)
#define g_FXVehicleSirenSizeMax GAME_GLOBAL_IN(float, GVar_FXVehicleSiren_SizeMax, 16)
#define g_FXVehicleProbeLightDistGrow GAME_GLOBAL_IN(float, GVar_FXVehicleProbeLight_DistGrow, 16)
#define g_FXVehicleProbeLightSizeMin GAME_GLOBAL_IN(float, GVar_FXVehicleProbeLight_SizeMin, 16)
#define g_FXVehicleProbeLightSizeMax GAME_GLOBAL_IN(float, GVar_FXVehicleProbeLight_SizeMax, 16)
#define g_MinDistReposAI GAME_GLOBAL_IN(float, GVar_MinDistReposAI, 16)
#define g_MaxDistReposAI GAME_GLOBAL_IN(float, GVar_MaxDistReposAI, 16)

/* World/FX/FXVehicle.cpp: an element drawn with the followed vehicle */
void FXVehicle_RenderAttached(void *element);
/* the near and far distance of the AI of a vehicle */
void cAIVehicle_GetFadeDists(void *ai, float *nearDist, float *farDist);

cEmbeddedFXVehicle *cEmbeddedFXVehicle_ctor(cEmbeddedFXVehicle *self);
void cEmbeddedFXVehicle_dtor(cEmbeddedFXVehicle *self);
cEmbeddedFXVehicle *cEmbeddedFXVehicle_Delete(cEmbeddedFXVehicle *self, byte flags);
byte cEmbeddedFXVehicle_Init(cEmbeddedFXVehicle *self);
void cEmbeddedFXVehicle_Done(cEmbeddedFXVehicle *self);
void cEmbeddedFXVehicle_Configure(cEmbeddedFXVehicle *self, int unused);
void cEmbeddedFXVehicle_Update(cEmbeddedFXVehicle *self);
void cEmbeddedFXVehicle_SetActive(cEmbeddedFXVehicle *self, byte on);
void cEmbeddedFXVehicle_Hide(cEmbeddedFXVehicle *self);
void cEmbeddedFXVehicle_Render(cEmbeddedFXVehicle *self);
void cEmbeddedFXVehicle_SetAttached(cEmbeddedFXVehicle *self, void *element);
/* the model, sirens and Probe / Vote of the vehicle */
void cEmbeddedFXVehicle_SetOwner(cEmbeddedFXVehicle *self, cEntity *owner);
void cEmbeddedFXVehicle_SetTraffic(cEmbeddedFXVehicle *self, byte on);
void cEmbeddedFXVehicle_StartFade(cEmbeddedFXVehicle *self);
void cEmbeddedFXVehicle_SetAIRange(cEmbeddedFXVehicle *self, byte on);

cFXVehicleSirens *cFXVehicleSirens_ctor(cFXVehicleSirens *self, cEmbeddedFXVehicle *owner);
/* the shared model and particles released with the last one (self is not used) */
void cFXVehicleSirens_dtor(cFXVehicleSirens *self);
byte cFXVehicleSirens_Init(cFXVehicleSirens *self);
void cFXVehicleSirens_Update(cFXVehicleSirens *self);

cEmbeddedFXVehicleProbe *cEmbeddedFXVehicleProbe_ctor(cEmbeddedFXVehicleProbe *self,
	cEmbeddedFXVehicle *fx);
void cEmbeddedFXVehicleProbe_dtor(cEmbeddedFXVehicleProbe *self);
cEmbeddedFXVehicleProbe *cEmbeddedFXVehicleProbe_Delete(cEmbeddedFXVehicleProbe *self, byte flags);
byte cEmbeddedFXVehicleProbe_Init(cEmbeddedFXVehicleProbe *self);
void cEmbeddedFXVehicleProbe_Done(cEmbeddedFXVehicleProbe *self);
void cEmbeddedFXVehicleProbe_Configure(cEmbeddedFXVehicleProbe *self, int unused);
void cEmbeddedFXVehicleProbe_Update(cEmbeddedFXVehicleProbe *self);
void cEmbeddedFXVehicleProbe_SetActive(cEmbeddedFXVehicleProbe *self, byte on);
void cEmbeddedFXVehicleProbe_ClearTrail(cEmbeddedFXVehicleProbe *self);

cEmbeddedFXVehicleVote *cEmbeddedFXVehicleVote_ctor(cEmbeddedFXVehicleVote *self,
	cEmbeddedFXVehicle *fx);
void cEmbeddedFXVehicleVote_dtor(cEmbeddedFXVehicleVote *self);
cEmbeddedFXVehicleVote *cEmbeddedFXVehicleVote_Delete(cEmbeddedFXVehicleVote *self, byte flags);
byte cEmbeddedFXVehicleVote_Init(cEmbeddedFXVehicleVote *self);
void cEmbeddedFXVehicleVote_Done(cEmbeddedFXVehicleVote *self);
void cEmbeddedFXVehicleVote_Configure(cEmbeddedFXVehicleVote *self, int unused);
void cEmbeddedFXVehicleVote_Update(cEmbeddedFXVehicleVote *self);
void cEmbeddedFXVehicleVote_SetActive(cEmbeddedFXVehicleVote *self, byte on);
void cEmbeddedFXVehicleVote_ClearTrails(cEmbeddedFXVehicleVote *self);
void cEmbeddedFXVehicleVote_SetType(cEmbeddedFXVehicleVote *self, int type);

#endif
