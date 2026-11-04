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
	cNetController base;
	cEntity *vehicle;
	byte seq;			/* low nibble: hit counter, high nibble: force counter; a change sets hit/force on the client */
	byte pad_61[3];
	int hit;			/* 2 after a hit */
	int force;			/* 2 for "ForceAll" */
	int bumpSends;		/* writes left that carry only the position and velocity after a vehicle bump (3) */
	float height;			/* of the camera point */
	uint16_t keys;			/* the controls of the client */
	byte pad_76[2];
};

#define VEH_TARGETS 16

typedef struct cVehicleTargetList {
	int count;
	float wait;			/* 3 s after a choice, then a new choice when the target is gone */
	char names[VEH_TARGETS][0x80];	/* the nearest first */
	float time;			/* the world time of the last choice */
	int cur;
} cVehicleTargetList;

typedef struct cVehicleTargets {
	cEntity *owner;
	cVehicleTargetList *list;	/* only for the player slots */
	char target[0x80];		/* the "Target" of the vehicle */
} cVehicleTargets;

struct cVehicleEntity {
	cWithLifeEntity wl;
	/* cVehicleEntity */
	float timeOfs;			/* random, for the hover wobble */
	byte launchTaunts;		/* "LaunchTaunts" */
	byte pad_3ed[3];
	byte collision[0x410 - 0x3f0];	/* the state of the world collision */
	int slot;			/* "UsrControl": the player slot 0..3, -1 for the AI */
	cVehicleControls controls;
	float wRadius;			/* "WRadius" */
	float thrust;
	float turnSpeedX;
	float turnSpeedY;
	float roll;
	float speed;			/* "Speed" */
	float pushTime;
	vec3_t push;
	float trustTime;		/* "TrustTime" */
	float frwTrust;			/* "FrwTrust" */
	float backTrust;		/* "BackTrust" */
	float strTrust;			/* "StrTrust" */
	float boost;			/* the boost left ("isBoost": 0) */
	float boostTime;		/* "BoostTime" */
	float maxSpeed;			/* "MaxSpeed" */
	float friction;			/* "Friction" */
	float style;			/* "Style" */
	float engine;			/* "Engine" */
	float acMass;			/* "AcMass" */
	float smoothTurnX;
	float smoothThrust;
	float fxEngineTime;		/* "FxEngineTime" */
	float fxBoostEngineTime;	/* "FxBoostEngineTime" */
	float inAtmosphere;		/* approaches 1 under the atmosphere limit */
	float active;			/* approaches 1 while active */
	float smoothSpeed;
	float shake;			/* 1 at a boost, goes down */
	vec3_t smoothAng;
	vec3_t smoothRotVel;
	vec3_t localVel;		/* the velocity in the frame of smoothAng */
	vec3_t localAcc;
	cNetVController *netCtrl;	/* the server side controller of a player ship */
	CGameModel pilot;
	int pilotNode;			/* DC_Player_01 of the vehicle model, -1 none */
	char pilotName[0x20];
	int pilotAnims[VEH_PILOT_ANIMS];	/* -1 none */
	float pilotAnimLen[VEH_PILOT_ANIMS];
	char typeName[0x20];		/* "Model": the model name, "SMosquito", "T..." for traffic */
	byte rendering;			/* inside the extra render of */
	byte pad_589[3];
	float emi;			/* "EMICounter" */
	float cloud;			/* "MotorDCnt" */
	byte cloudOn;			/* "BoostDeact" */
	byte pad_595[3];
	float noFire;
	float histTime;
	float histSpeed;
	vec3_t histVel;
	vec3_t histAcc;
	float histAccLen;
	float hitTime;			/* of the last collision */
	cVehicleTraffic traffic;	/* "Traf_" variables */
	cTargetLock weapons;		/* cWeaponManager */
	byte fixedModel;		/* "FixedModel" */
	byte pad_639[3];
	int minLife;			/* "MinLife" */
	int maxLife;			/* "MaxLife" */
	byte isInit;			/* "isInit" */
	byte pad_645[3];
	union {
		cVehicleTargets o648;	/* updated for player ships, with the target */
		struct {
			byte pad_648[offsetof(cVehicleTargets, target)];
			char target[0x80];	/* "Target" */
		};
	};
	cAutoAim autoAim;
	cVehicleMotors motors;
	float vertBalance;		/* "VerticalBalance" */
	int money;			/* "Money" */
	cVehicleEffects effects;
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
/* the motors (.., not decompiled yet): their variables through the vtable at 0x720 */

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
