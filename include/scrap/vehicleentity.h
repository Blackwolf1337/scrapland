/*
 * Entities/Vehicle/VehicleEntity.cpp of the game: cVehicleEntity ("Car"), the ships and the traffic, a
 * cWithLifeEntity (0x3e8 bytes) with the flight model, the network controller, the pilot model at the node
 * DC_Player_01, the weapons (cWeaponManager at 0x5f0), the motors (cVehicleMotors at 0x720) and the effects
 * (cVehicleEffects at 0x7f0, include/scrap/vehicleeffects.h). src/game/Entities/Vehicle/VehicleEntity.c.
 */
#ifndef SCRAP_VEHICLEENTITY_H
#define SCRAP_VEHICLEENTITY_H

#include "scrap/gvar.h"
#include "scrap/autoaim.h"
#include "scrap/controls.h"
#include "scrap/fxcountermeasures.h"
#include "scrap/scrapmodel.h"
#include "scrap/sound.h"
#include "scrap/timer.h"
#include "scrap/vehicleeffects.h"
#include "scrap/vehiclemotors.h"
#include "scrap/vehicletraffic.h"
#include "scrap/withlifeentity.h"

#define CVEHICLEENTITY_SIZE sizeof(cVehicleEntity)
#define CNETVCONTROLLER_SIZE 0x78	/* allocated with this size, freed with 0x5c (the base class) */

#define VEH_PILOT_ANIMS 5

typedef struct cNetVController cNetVController;

/* the network controller of a vehicle (cNetVController of the RTTI, a cNetController; see cObjNet of
 * include/scrap/itementity.h): at 0xf4 for every vehicle, a second one at 0x4fc for the ships of the players on
 * the server. base.age is 0.5 until the first state, then counts down; Apply moves the vehicle to the state */
struct cNetVController {
	cNetController base;		/* 0x00 */
	cEntity *vehicle;		/* 0x5c */
	byte seq;			/* 0x60 low nibble: hit counter, high nibble: force counter; a change sets hit/force on the client */
	byte pad_61[3];
	int hit;			/* 0x64 2 after a hit */
	int force;			/* 0x68 2 for "ForceAll" */
	int bumpSends;		/* 0x6c writes left that carry only the position and velocity after a vehicle bump (3) */
	float height;			/* 0x70 of the camera point */
	uint16_t keys;			/* 0x74 the controls of the client */
	byte pad_76[2];
};

#define VEH_TARGETS 16

typedef struct cVehicleTargetList {
	int count;			/* 0x000 */
	float wait;			/* 0x004 3 s after a choice, then a new choice when the target is gone */
	char names[VEH_TARGETS][0x80];	/* 0x008 the nearest first */
	float time;			/* 0x808 the world time of the last choice */
	int cur;			/* 0x80c */
} cVehicleTargetList;

typedef struct cVehicleTargets {
	cEntity *owner;			/* 0x00 */
	cVehicleTargetList *list;	/* 0x04 only for the player slots */
	char target[0x80];		/* 0x08 the "Target" of the vehicle */
} cVehicleTargets;

struct cVehicleEntity {
	cWithLifeEntity wl;		/* 0x000 */
	/* cVehicleEntity */
	float timeOfs;			/* 0x3e8 random, for the hover wobble */
	byte launchTaunts;		/* 0x3ec "LaunchTaunts" */
	byte pad_3ed[3];
	byte collision[0x410 - 0x3f0];	/* 0x3f0 the state of the world collision */
	int slot;			/* 0x410 "UsrControl": the player slot 0..3, -1 for the AI */
	cVehicleControls controls;	/* 0x414 */
	float wRadius;			/* 0x458 "WRadius" */
	float thrust;			/* 0x45c */
	float turnSpeedX;		/* 0x460 */
	float turnSpeedY;		/* 0x464 */
	float roll;			/* 0x468 */
	float speed;			/* 0x46c "Speed" */
	float pushTime;			/* 0x470 */
	vec3_t push;			/* 0x474 */
	float trustTime;		/* 0x480 "TrustTime" */
	float frwTrust;			/* 0x484 "FrwTrust" */
	float backTrust;		/* 0x488 "BackTrust" */
	float strTrust;			/* 0x48c "StrTrust" */
	float boost;			/* 0x490 the boost left ("isBoost": 0) */
	float boostTime;		/* 0x494 "BoostTime" */
	float maxSpeed;			/* 0x498 "MaxSpeed" */
	float friction;			/* 0x49c "Friction" */
	float style;			/* 0x4a0 "Style" */
	float engine;			/* 0x4a4 "Engine" */
	float acMass;			/* 0x4a8 "AcMass" */
	float smoothTurnX;		/* 0x4ac */
	float smoothThrust;		/* 0x4b0 */
	float fxEngineTime;		/* 0x4b4 "FxEngineTime" */
	float fxBoostEngineTime;	/* 0x4b8 "FxBoostEngineTime" */
	float inAtmosphere;		/* 0x4bc approaches 1 under the atmosphere limit */
	float active;			/* 0x4c0 approaches 1 while active */
	float smoothSpeed;		/* 0x4c4 */
	float shake;			/* 0x4c8 1 at a boost, goes down */
	vec3_t smoothAng;		/* 0x4cc */
	vec3_t smoothRotVel;		/* 0x4d8 */
	vec3_t localVel;		/* 0x4e4 the velocity in the frame of smoothAng */
	vec3_t localAcc;		/* 0x4f0 */
	cNetVController *netCtrl;	/* 0x4fc the server side controller of a player ship */
	CGameModel pilot;		/* 0x500 */
	int pilotNode;			/* 0x51c DC_Player_01 of the vehicle model, -1 none */
	char pilotName[0x20];		/* 0x520 */
	int pilotAnims[VEH_PILOT_ANIMS];	/* 0x540 -1 none */
	float pilotAnimLen[VEH_PILOT_ANIMS];	/* 0x554 */
	char typeName[0x20];		/* 0x568 "Model": the model name, "SMosquito", "T..." for traffic */
	byte rendering;			/* 0x588 inside the extra render of vf46 */
	byte pad_589[3];
	float emi;			/* 0x58c "EMICounter" */
	float cloud;			/* 0x590 "MotorDCnt" */
	byte cloudOn;			/* 0x594 "BoostDeact" */
	byte pad_595[3];
	float noFire;			/* 0x598 */
	float histTime;			/* 0x59c */
	float histSpeed;		/* 0x5a0 */
	vec3_t histVel;			/* 0x5a4 */
	vec3_t histAcc;			/* 0x5b0 */
	float histAccLen;		/* 0x5bc */
	float hitTime;			/* 0x5c0 of the last collision */
	cVehicleTraffic traffic;	/* 0x5c4 "Traf_" variables */
	cTargetLock weapons;		/* 0x5f0 cWeaponManager */
	byte fixedModel;		/* 0x638 "FixedModel" */
	byte pad_639[3];
	int minLife;			/* 0x63c "MinLife" */
	int maxLife;			/* 0x640 "MaxLife" */
	byte isInit;			/* 0x644 "isInit" */
	byte pad_645[3];
	union {
		cVehicleTargets o648;	/* 0x648 updated for player ships, with the target */
		struct {
			byte pad_648[offsetof(cVehicleTargets, target)];
			char target[0x80];	/* 0x650 "Target" */
		};
	};
	cAutoAim autoAim;		/* 0x6d0 */
	cVehicleMotors motors;		/* 0x720 */
	float vertBalance;		/* 0x7e8 "VerticalBalance" */
	int money;			/* 0x7ec "Money" */
	cVehicleEffects effects;	/* 0x7f0 */
};

static inline cVehicleEntity *Veh(cEntity *e) {
	return (cVehicleEntity *)e;
}

static inline cEntity *Veh_Ent(cVehicleEntity *v) {
	return &v->wl.obj.base;
}

/* the net controller of a vehicle is a cNetVController */
static inline cNetVController *Veh_Net(const cVehicleEntity *v) {
	return (cNetVController *)v->wl.obj.net;
}

static inline struct cEmbeddedFXVehicle *Veh_FX(const cVehicleEntity *v) {
	return (struct cEmbeddedFXVehicle *)v->wl.obj.fx;
}

/* the fields of a vehicle the embedded effects read */
#define Vehicle_Player(e) (Veh(e)->wl.obj.cam)		/* the follow camera: driven by a player */

/* the cVehicleEntity vtable slots called here */
/* the motors (rva 0x10d270.., not decompiled yet): their variables through the vtable at 0x720 */

/* the engine variables */
extern cGVar GVar_AtmosphereLimit;
#define g_AtmosphereLimit (GVar_AtmosphereLimit.value.f)	/* above it the ships fall back */
extern cGVar GVar_TopLimit;
#define g_SpaceLimit (GVar_TopLimit.value.f)

/* the parts of other files (not decompiled yet; VehicleMotors, VehicleTraffic and WeaponManager in their headers);
 * the objects inside the vehicle by their offset */
void *cVehicleMotors_ctor(void *motors);
void cVehicleMotors_dtor(void *motors);
void cVehicleControls_Tick(cVehicleControls *c, cEntity *v, float dt);

/*
 * The target selection of a player ship (o648 at 0x648 and the "Target" after it; src/game/Entities/Vehicle/
 * Controls.c): a list of the 16 nearest enemies (the attacker of the last TgtRegenerationTime first) the controls
 * cycle through, made new when the selected one is gone or the list is older than TgtRegenerationTime.
 */

/* the functions take the o648 bytes of the vehicle */
void *cVehicleO648_ctor(void *o);
void cVehicleO648_dtor(void *o);
/* the list for a player slot, none for -1 */
void cVehicleO648_SetSlot(void *o, int slot);
/* the target dropped when it is gone, a new one after the wait */
void cVehicleO648_Update(void *o, float dt);
/* 1 when the list is too old or the target is not active in it */
byte cVehicleO648_IsStale(void *o);
/* the time of the life sound i set to -t */
void cWithLifeEntity_SetSndTime(cEntity *self, float t, int i);
void cWithLifeEntity_Tick(cEntity *self, float dt);
void cWithLifeEntity_RenderNow(cEntity *self);
byte cWithLifeEntity_TestLine(cEntity *self, const vec3_t *from, vec3_t *hit);
byte cWithLifeEntity_TestSphere(cEntity *self, cEntity *exclude, const vec3_t *center, float radius, const vec3_t *prev);
void cWithLifeEntity_SetMass(cEntity *self, float mass);
cActCtrl *cWithLifeEntity_GetActCtrl(cEntity *self);
void *cAIVehicle_ctor(void *mem, cEntity *v, void *p1, void *p2, void *p3);
cNetVController *cNetVController_ctor(void *mem, cEntity *v);

/* the speed history restarted from the velocity (AvoidCrash) */
void cVehicleEntity_UpdateSpeed(cEntity *self);
/* the debris cloud on a vehicle for time */
void cVehicleEntity_SetCloud(cEntity *self, float time, byte on);
/* a state flag for the network game */
void cVehicleEntity_SetNetFlag(cEntity *self, byte on);
int cVehicleEntity_CanFire(cVehicleEntity *self);
void cVehicleEntity_NetCreate(cVehicleEntity *self);
void cVehicleEntity_UpdatePilotPos(cVehicleEntity *self, byte on);
void cVehicleEntity_StartFade(cVehicleEntity *self);
/* < 0 when no player controls the entity (assumed from SAI.EnableAI*) */
int cVehicleEntity_GetOwnerSlot(cEntity *e);
const char *cVehicleEntity_GetPilot(cVehicleEntity *self, int slot);
byte cVehicleEntity_GetLaunchTaunts(cVehicleEntity *self);
void cVehicleEntity_EnableAI(cVehicleEntity *self, byte enable, void *p1, void *p2, void *p3);
byte cVehicleEntity_SetPilot(cVehicleEntity *self, int slot, const char *name, float sx, float sy, float sz);

#endif
