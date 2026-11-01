/*
 * Entities/Vehicle/VehicleEntity.cpp of the game: cVehicleEntity ("Car"), the ships and the traffic, a
 * cWithLifeEntity (0x3e8 bytes) with the flight model, the network controller, the pilot model at the node
 * DC_Player_01, the weapons (cWeaponManager at 0x5f0), the motors (cVehicleMotors at 0x720) and the effects
 * (cVehicleEffects at 0x7f0, include/scrap/vehicleeffects.h). src/game/Entities/Vehicle/VehicleEntity.c.
 */
#ifndef SCRAP_VEHICLEENTITY_H
#define SCRAP_VEHICLEENTITY_H

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

#define CVEHICLEENTITY_VFTABLE GAME_VFTABLE(cVehicleEntity_vftable)
#define CVEHICLEENTITY_SIZE 0x814
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
SD_OFFSET(cNetVController, vehicle, 0x5c);
SD_OFFSET(cNetVController, hit, 0x64);
SD_OFFSET(cNetVController, keys, 0x74);
SD_SIZE(cNetVController, CNETVCONTROLLER_SIZE);

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
	byte o648[0x650 - 0x648];	/* 0x648 updated for player ships */
	char target[0x80];		/* 0x650 "Target" */
	cAutoAim autoAim;		/* 0x6d0 */
	cVehicleMotors motors;		/* 0x720 */
	float vertBalance;		/* 0x7e8 "VerticalBalance" */
	int money;			/* 0x7ec "Money" */
	cVehicleEffects effects;	/* 0x7f0 */
};
SD_OFFSET(cVehicleEntity, collision, 0x3f0);
SD_OFFSET(cVehicleEntity, slot, 0x410);
SD_OFFSET(cVehicleEntity, controls, 0x414);
SD_OFFSET(cVehicleEntity, wRadius, 0x458);
SD_OFFSET(cVehicleEntity, speed, 0x46c);
SD_OFFSET(cVehicleEntity, acMass, 0x4a8);
SD_OFFSET(cVehicleEntity, localAcc, 0x4f0);
SD_OFFSET(cVehicleEntity, pilot, 0x500);
SD_OFFSET(cVehicleEntity, pilotAnimLen, 0x554);
SD_OFFSET(cVehicleEntity, typeName, 0x568);
SD_OFFSET(cVehicleEntity, emi, 0x58c);
SD_OFFSET(cVehicleEntity, hitTime, 0x5c0);
SD_OFFSET(cVehicleEntity, weapons, 0x5f0);
SD_OFFSET(cVehicleEntity, fixedModel, 0x638);
SD_OFFSET(cVehicleEntity, target, 0x650);
SD_OFFSET(cVehicleEntity, autoAim, 0x6d0);
SD_OFFSET(cVehicleEntity, motors, 0x720);
SD_OFFSET(cVehicleEntity, effects, 0x7f0);
SD_SIZE(cVehicleEntity, CVEHICLEENTITY_SIZE);

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
/* cVehicleEntity::vftable: the slots of the vehicle after the ones of cWithLifeEntity */
typedef struct cVehicleEntityVtbl {
	cWithLifeEntityVtbl base;
	void(*UpdateSounds)(cVehicleEntity *self, float dt);			/* 0xe0 vf56 */
	void(*UpdateHistory)(cVehicleEntity *self);				/* 0xe4 vf57 */
	void(*EnableAI)(cEntity *self, byte enable, void *p1, void *p2, void *p3);	/* 0xe8 vf58 */
	byte(*GetLaunchTaunts)(cVehicleEntity *self);				/* 0xec vf59 */
	byte(*SetPilot)(cVehicleEntity *self, int slot, const char *name, float sx, float sy, float sz);
												/* 0xf0 vf60 */
	const char *(*GetPilot)(cVehicleEntity *self, int slot);			/* 0xf4 vf61 */
	byte(*SetPilotAnim)(cVehicleEntity *self, int slot, int anim);		/* 0xf8 vf62 */
	void(*UpdatePilot)(cVehicleEntity *self, int slot, float dt);		/* 0xfc vf63 */
	byte(*GetPilotPos)(cVehicleEntity *self, int slot, vec3_t *pos, vec3_t *ang);	/* 0x100 vf64 */
	void(*Crash)(cVehicleEntity *self, const vec3_t *acc, float force);	/* 0x104 vf65 */
} cVehicleEntityVtbl;
SD_OFFSET(cVehicleEntityVtbl, UpdateSounds, 0xe0);
SD_OFFSET(cVehicleEntityVtbl, EnableAI, 0xe8);
SD_OFFSET(cVehicleEntityVtbl, SetPilot, 0xf0);
SD_OFFSET(cVehicleEntityVtbl, Crash, 0x104);

static inline const cVehicleEntityVtbl *Veh_Vtbl(cVehicleEntity *v) {
	return (const cVehicleEntityVtbl *)v->wl.obj.base.vftable;
}

/* the motors (rva 0x10d270.., not decompiled yet): their variables through the vtable at 0x720 */
typedef struct cVehicleMotorsVtbl {
	byte(*VarI)(void *self, const char *name, int *i, int set);			/* 0x00 */
	byte(*VarF)(void *self, const char *name, double *d, int set);		/* 0x04 */
	byte(*VarS)(void *self, const char *name, char **s, int set);		/* 0x08 */
	byte(*VarV)(void *self, const char *name, double *x, double *y, double *z, int set); /* 0x0c */
} cVehicleMotorsVtbl;

static inline const cVehicleMotorsVtbl *Veh_MotorsVtbl(cVehicleEntity *v) {
	return (const cVehicleMotorsVtbl *)v->motors.vftable;
}

/* the engine variables */
#define g_AtmosphereLimit GAME_GLOBAL_IN(float, GVar_AtmosphereLimit, 16)	/* above it the ships fall back */
#define g_SpaceLimit GAME_GLOBAL_IN(float, GVar_TopLimit, 16)

/* the parts of other files (not decompiled yet; VehicleMotors, VehicleTraffic and WeaponManager in their headers);
 * the objects inside the vehicle by their offset */
/* the flight of dt: thrust, turn, boost, friction, the fall above the atmosphere and the free space magnet */
void cVehicleEntity_Step(cVehicleEntity *self, float dt);
/* vf24: the model of the slot (the wobble above the atmosphere, the floating, the tilt), the player camera, the
 * pilot, the motors and the effects */
void cVehicleEntity_UpdateModel(cVehicleEntity *self);
void *cVehicleMotors_ctor(void *motors);
void cVehicleMotors_dtor(void *motors);
void cVehicleControls_Tick(cVehicleControls *c, cEntity *v, float dt);

/*
 * The target selection of a player ship (o648 at 0x648 and the "Target" after it; src/game/Entities/Vehicle/
 * Controls.c): a list of the 16 nearest enemies (the attacker of the last TgtRegenerationTime first) the controls
 * cycle through, made new when the selected one is gone or the list is older than TgtRegenerationTime.
 */
#define VEH_TARGETS 16

typedef struct cVehicleTargetList {
	int count;			/* 0x000 */
	float wait;			/* 0x004 3 s after a choice, then a new choice when the target is gone */
	char names[VEH_TARGETS][0x80];	/* 0x008 the nearest first */
	float time;			/* 0x808 the world time of the last choice */
	int cur;			/* 0x80c */
} cVehicleTargetList;
SD_OFFSET(cVehicleTargetList, time, 0x808);
SD_SIZE(cVehicleTargetList, 0x810);

typedef struct cVehicleTargets {
	cEntity *owner;			/* 0x00 */
	cVehicleTargetList *list;	/* 0x04 only for the player slots */
	char target[0x80];		/* 0x08 the "Target" of the vehicle */
} cVehicleTargets;
SD_SIZE(cVehicleTargets, 0x88);

#define g_TgtRegenerationTime GAME_GLOBAL_IN(float, GVar_TgtRegenerationTime, 16)

/* the functions take the o648 bytes of the vehicle */
void *cVehicleO648_ctor(void *o);
void cVehicleO648_dtor(void *o);
/* 1 while the target can not change: an EMIed ship (not a player one with VehicleCenterPadLock) */
byte cVehicleO648_IsBlocked(void *o);
/* the list made new from the attacker and the enemies by distance, the target its first */
void cVehicleO648_Rebuild(void *o);
/* the list for a player slot, none for -1 */
void cVehicleO648_SetSlot(void *o, int slot);
/* the next (or previous) of the list */
void cVehicleO648_Cycle(void *o, byte next);
/* the entity in the view of the owner (cos > 0.95 within 50000) */
void cVehicleO648_PickInView(void *o);
/* the target dropped when it is gone, a new one after the wait */
void cVehicleO648_Update(void *o, float dt);
/* 1 when the list is too old or the target is not active in it */
byte cVehicleO648_IsStale(void *o);
/* the time of the life sound i set to -t */
void cWithLifeEntity_SetSndTime(cEntity *self, float t, int i);
void cWithLifeEntity_Tick(cEntity *self, float dt);
void cWithLifeEntity_RenderNow(cEntity *self);
void *cAIVehicle_ctor(void *mem, cEntity *v, void *p1, void *p2, void *p3);
cNetVController *cNetVController_ctor(void *mem, cEntity *v);
/* NetVController.c */
byte cNetVController_Changed(cNetVController *self, byte flags, cEntity *e);
void cNetVController_Reset(cNetVController *self);
void cNetVController_Read(cNetVController *self, void *msg);
void cNetVController_Write(cNetVController *self, byte flags, cEntity *e, void *msg, byte force);

/* VehicleEntity.c; the constructor is declared in world.h */
void cVehicleEntity_dtor(cVehicleEntity *self);
cVehicleEntity *cVehicleEntity_Delete(cVehicleEntity *self, byte flags);
/* the push of two vehicles that touch: bounced by their masses, pushed apart when they are inside each other */
void cVehicleEntity_Touch(cVehicleEntity *self, cEntity *e, const vec3_t *point, const vec3_t *normal);
/* the destructor of the cAutoAim at 0x6d0 (only its target link) */
void cAutoAim_dtor(void *self);
/* MaxSpeed, Fric, AcMass and the trusts from the mass, life, engine and style */
void cVehicleEntity_UpdateTrust(cVehicleEntity *self);
/* the speed history restarted from the velocity (AvoidCrash) */
void cVehicleEntity_UpdateSpeed(cEntity *self);
void cVehicleEntity_BoostTick(cVehicleEntity *self, float dt);
byte cVehicleEntity_GetLaunchTaunts(cVehicleEntity *self);
/* the boost sound and the speed history restarted */
void cVehicleEntity_StartBoost(cVehicleEntity *self);
void cVehicleEntity_EMITick(cVehicleEntity *self, float dt);
void cVehicleEntity_CloudTick(cVehicleEntity *self, float dt);
void cVehicleEntity_NoFireTick(cVehicleEntity *self, float dt);
void cVehicleEntity_PushTick(cVehicleEntity *self, float dt);
void cVehicleEntity_SetMass(cVehicleEntity *self, float mass);
/* the debris cloud on a vehicle for time */
void cVehicleEntity_SetCloud(cEntity *self, float time, byte on);
/* a state flag for the network game */
void cVehicleEntity_SetNetFlag(cEntity *self, byte on);
byte cVehicleEntity_IsBoostReady(cVehicleEntity *self);
int cVehicleEntity_IsEMIed(cVehicleEntity *self);
int cVehicleEntity_CanFire(cVehicleEntity *self);
void cVehicleEntity_UpdateHistory(cVehicleEntity *self);
void cVehicleEntity_Crash(cVehicleEntity *self, const vec3_t *acc, float force);
/* the smooth values of the flight for the effects, the camera and the sounds */
void cVehicleEntity_Smooth(cVehicleEntity *self, float dt);
/* from (the position before the step) to the position: collisions with the world and the entities */
void cVehicleEntity_Collide(cVehicleEntity *self, vec3_t *from, float dt);
/* the moves of a ship of a client player, of a client copy, on the server and of the others */
void cVehicleEntity_NetMove(cVehicleEntity *self, float dt);
void cVehicleEntity_ClientMove(cVehicleEntity *self, float dt);
void cVehicleEntity_ServerMove(cVehicleEntity *self, float dt);
void cVehicleEntity_Move(cVehicleEntity *self, float dt);
void cVehicleEntity_Tick(cVehicleEntity *self, float dt);
void cVehicleEntity_Update(cVehicleEntity *self);
void cVehicleEntity_SetVel(cVehicleEntity *self, const vec3_t *vel);
/* the cross fade of the motor sound a into b */
void Vehicle_SndCrossFade(cSndItem *a, cSndItem *b);
void cVehicleEntity_UpdateSounds(cVehicleEntity *self, float dt);
void cVehicleEntity_NetCreate(cVehicleEntity *self);
void cVehicleEntity_NetDelete(cVehicleEntity *self);
void cVehicleEntity_NetRead(cVehicleEntity *self, void *msg, int unused);
void cVehicleEntity_NetWrite(cVehicleEntity *self, float dist2, void *msg, int full);
void cVehicleEntity_SetUsrControl(cVehicleEntity *self, int slot);
void cVehicleEntity_EnableAI(cVehicleEntity *self, byte enable, void *p1, void *p2, void *p3);
const char *cVehicleEntity_GetPilot(cVehicleEntity *self, int slot);
byte cVehicleEntity_GetPilotPos(cVehicleEntity *self, int slot, vec3_t *pos, vec3_t *ang);
byte cVehicleEntity_SetPilot(cVehicleEntity *self, int slot, const char *name, float sx, float sy,
	float sz);
byte cVehicleEntity_SetPilotAnim(cVehicleEntity *self, int slot, int anim);
void cVehicleEntity_UpdatePilot(cVehicleEntity *self, int slot, float dt);
void cVehicleEntity_UpdatePilotPos(cVehicleEntity *self, byte on);
void cVehicleEntity_SetModel(cVehicleEntity *self, const char *file, float sx, float sy, float sz,
	byte inDir);
void cVehicleEntity_SetEmbeddedFX(cVehicleEntity *self, const char *fxType);
void cVehicleEntity_StartFade(cVehicleEntity *self);
void cVehicleEntity_SetFX(cVehicleEntity *self, int fx);
void cVehicleEntity_SetFXParam(cVehicleEntity *self, int i, float v);
void cVehicleEntity_SetActive(cVehicleEntity *self, byte on);
void cVehicleEntity_RenderNow(cVehicleEntity *self);
void cVehicleEntity_GetCamPoint(cVehicleEntity *self, vec3_t *pos, vec3_t *ang);
const char *cVehicleEntity_GetClassName(cVehicleEntity *self);
/* < 0 when no player controls the entity (assumed from SAI.EnableAI*) */
int cVehicleEntity_GetOwnerSlot(cEntity *e);
float cVehicleEntity_GetWRadius(cVehicleEntity *self);
void cVehicleEntity_UpdateOff(cVehicleEntity *self);
byte cVehicleEntity_IsType(cVehicleEntity *self, const char *type);
/* the turn input curve: |x| <= 1 kept, else sqrt(|x| - 0.75) + 0.5 with the sign of x */
float Vehicle_TurnCurve(float x);
byte cVehicleEntity_VarF(cVehicleEntity *self, const char *name, double *d, int set);
byte cVehicleEntity_VarI(cVehicleEntity *self, const char *name, int *i, int set);
byte cVehicleEntity_VarS(cVehicleEntity *self, const char *name, char **s, int set);
byte cVehicleEntity_VarV(cVehicleEntity *self, const char *name, double *x, double *y, double *z,
	int set);

#endif
