/*
 * cVehicleEntity, the ships and the traffic, see include/scrap/vehicleentity.h.
 */
#include <string.h>

#include "scrap/ai.h"
#include "scrap/autoaim.h"
#include "scrap/camcontrol.h"
#include "scrap/console.h"
#include "scrap/crt.h"
#include "scrap/doors.h"
#include "scrap/embeddedfxvehicle.h"
#include "scrap/global.h"
#include "scrap/mathlib.h"
#include "scrap/primitive.h"
#include "scrap/scoreritems.h"
#include "scrap/missileentity.h"
#include "scrap/vehiclemotors.h"
#include "scrap/vehicletraffic.h"
#include "scrap/vehicleentity.h"
#include "scrap/weapons.h"
#include "scrap/weaps2.h"
#include "scrap/world.h"

#define VEH_CLASSNAME GAME_STR(str_3a77b8, 0x3a77b8)	/* "Car" */
#define VEH_EMPTY GAME_STR(str_Empty, 0x3a3228)		/* "" */
#define CGAMEMODEL_CTOR 0x105e60	/* CGameModel_ctor(self, NULL) */
#define CGAMEMODEL_DTOR 0x20ec10
#define VEH_RAND_SCALE 3.051850947599719e-05	/* 1 / 32767 (the double at rva 0x3a3d60) */
#define CAIVEHICLE_SIZE 0x3b0
#define CEMBEDDEDFXVEHICLE_SIZE 0x1b0

#define VEH_DEG_PER_RAD_NUM 180.0				/* the doubles at rva 0x3a3718, 0x3a3700 */
#define VEH_DEG_PER_RAD_DEN 3.141592653589793

CGameModel *__fastcall CGameModel_ctorDefault(CGameModel *self) {
	return CGameModel_ctor(self, NULL);
}

/* the names of the pilot animations by index ("Left", "Right", "Go", "Back", "Relax") */
#define g_PilotAnimNames (GAME_ARRAY(const char *, g_PilotAnimNames, 0x4287e0))

/* the engine variables */
#define g_ShipPathFmt GAME_GLOBAL_IN(const char *, GVar_ShipPathFmt, 16, 0x448220)
#define g_VeCharsPathFmt GAME_GLOBAL_IN(const char *, GVar_VeCharsPathFmt, 16, 0x448058)
#define g_AmorPieceWeight GAME_GLOBAL_IN(float, GVar_AmorPieceWeight, 16, 0x4480e8)
#define g_MotorDownFactor GAME_GLOBAL_IN(float, GVar_MotorDownFactor, 16, 0x448100)
#define g_VehicleCrashMinTime GAME_GLOBAL_IN(float, GVar_VehicleCrashMinTime, 16, 0x4481a8)
#define g_VehicleCrashAccLo GAME_GLOBAL_IN(float, GVar_VehicleCrashAccLo, 16, 0x448118)
#define g_VehicleCrashAccMed GAME_GLOBAL_IN(float, GVar_VehicleCrashAccMed, 16, 0x4481f0)
#define g_VehicleCrashAccHi GAME_GLOBAL_IN(float, GVar_VehicleCrashAccHi, 16, 0x447e60)
#define g_VehicleCrashAccHev GAME_GLOBAL_IN(float, GVar_VehicleCrashAccHev, 16, 0x447ec0)
#define g_FxSpeedVariation GAME_GLOBAL_IN(float, GVar_FxSpeedVariation, 16, 0x448208)
#define g_FxAngVariation GAME_GLOBAL_IN(float, GVar_FxAngVariation, 16, 0x448028)
#define g_FxVelVariation GAME_GLOBAL_IN(float, GVar_FxVelVariation, 16, 0x447fe0)
#define g_FxAccVariation GAME_GLOBAL_IN(float, GVar_FxAccVariation, 16, 0x447e48)
#define g_FxBoostTimeDuration GAME_GLOBAL_IN(float, GVar_FxBoostTimeDuration, 16, 0x448088)
#define g_VehicleEditingMode GAME_GLOBAL_IN(int, GVar_VehicleEditingMode, 16, 0x447ea8)
#define g_NPCSndMotorVolFactor GAME_GLOBAL_IN(float, GVar_NPCSndMotorVolFactor, 16, 0x448268)

/* the camera data of a player ship (vehicle + 0xf0) */

/* the pilot slots: only slot 0 has storage, the original indexes by slot anyway */
#define VEH_PILOT_AT(v, field, off) ((byte *)(v) + offsetof(cVehicleEntity, field) + (off))

static inline CGameModel *Veh_PilotModel(cVehicleEntity *v, int slot) {
	return (CGameModel *)VEH_PILOT_AT(v, pilot, slot * (int)sizeof(CGameModel));
}

static inline int *Veh_PilotNode(cVehicleEntity *v, int slot) {
	return (int *)VEH_PILOT_AT(v, pilotNode, slot * (int)sizeof(int));
}

static inline char *Veh_PilotName(cVehicleEntity *v, int slot) {
	return (char *)VEH_PILOT_AT(v, pilotName, slot * (int)sizeof(v->pilotName));
}

static inline int *Veh_PilotAnim(cVehicleEntity *v, int slot, int i) {
	return (int *)VEH_PILOT_AT(v, pilotAnims, (slot * VEH_PILOT_ANIMS + i) * (int)sizeof(int));
}

static inline float *Veh_PilotAnimLen(cVehicleEntity *v, int slot, int i) {
	return (float *)VEH_PILOT_AT(v, pilotAnimLen, (slot * VEH_PILOT_ANIMS + i) * (int)sizeof(float));
}

static inline byte Veh_IsServer(void) {
	return (g_pApp->flags >> 1) & 1;
}

static inline byte Veh_IsPlayerSlot(int slot) {
	return slot >= 0 && slot < 4;
}

static inline float Veh_Len(const vec3_t *v) {
	return (float)__builtin_sqrt((double)(v->x * v->x + v->y * v->y + v->z * v->z));
}

/* the speed and the speed history from the velocity */
static void Veh_ResetHistory(cVehicleEntity *self) {
	self->histSpeed = Veh_Len(&self->wl.obj.vel);
	self->histVel = self->wl.obj.vel;
	self->histAcc.x = 0.0f;
	self->histAcc.y = 0.0f;
	self->histAcc.z = 0.0f;
	self->histAccLen = 0.0f;
}

/* ---- construction and destruction ---- */

cEntity *__thiscall cVehicleEntity_ctor(void *mem, const char *name, float x, float y, float z) {
	cVehicleEntity *self = mem;

	cWithLifeEntity_ctor(mem, name, x, y, z);
	self->wl.obj.base.vftable = CVEHICLEENTITY_VFTABLE;
	*(int *)(self->collision + 0) = 0;
	*(int *)(self->collision + 4) = 0;
	Controls_ctor(&self->controls);
	crt_eh_vector_ctor(&self->pilot, sizeof(CGameModel), 1, GAME_FUNC(CGameModel_ctorDefault, CGAMEMODEL_CTOR), GAME_FUNC(CGameModel_Reset, CGAMEMODEL_DTOR));
	cVehicleTraffic_ctor(&self->traffic);
	cWeaponManager_ctor(&self->weapons);
	cVehicleO648_ctor(self->o648);
	cAutoAim_ctor(&self->autoAim);
	cVehicleMotors_ctor(&self->motors);
	cVehicleEffects_ctor(&self->effects);
	self->wl.obj.base.mask |= 1;
	self->thrust = 0.0f;
	self->turnSpeedX = 0.0f;
	self->turnSpeedY = 0.0f;
	self->roll = 0.0f;
	self->speed = 0.0f;
	self->money = 0;
	self->trustTime = 0.5f;
	self->wl.mass = 2800.0f;
	self->style = 2800.0f;
	self->engine = 2800.0f;
	cVehicleEntity_UpdateTrust(self);
	self->wRadius = 150.0f;
	cPrimitive_SetSize(&self->wl.prim, 130.0f);
	self->slot = -1;
	cVehicleMotors_SetOwner(&self->motors, Veh_Ent(self));
	cWeaponManager_Init(&self->weapons, Veh_Ent(self));
	self->netCtrl = NULL;
	if (Veh_Net(self)) {
		operator_delete_sized(self->wl.obj.net, 0x5c);
		void *n = operator_new(CNETVCONTROLLER_SIZE);
		self->wl.obj.net = n ? (cNetController *)cNetVController_ctor(n, Veh_Ent(self)) : NULL;
	}
	self->pilotNode = -1;
	/* the loop of the original runs over the animations of all slots with the stride of a slot (0x14): it also
	 * sets the first length, typeName + 0x0 and + 0x14 and cloud */
	for (int i = 0; i < VEH_PILOT_ANIMS; i++) {
		*Veh_PilotAnim(self, i, 0) = -1;
	}
	*(cVehicleEntity **)self->o648 = self;
	self->minLife = 100;
	self->maxLife = 150;
	self->acMass = self->wl.mass;
	self->noFire = 0.0f;
	self->boostTime = 10.0f;
	self->boost = 0.0f;
	self->cloud = 0.0f;
	self->cloudOn = 0;
	self->launchTaunts = 1;
	self->smoothTurnX = 0.0f;
	self->smoothThrust = 0.0f;
	self->fxEngineTime = 0.5f;
	self->fxBoostEngineTime = 0.25f;
	self->smoothSpeed = 0.0f;
	self->localVel.x = 0.0f;
	self->localVel.y = 0.0f;
	self->localVel.z = 0.0f;
	self->localAcc.x = 0.0f;
	self->localAcc.y = 0.0f;
	self->localAcc.z = 0.0f;
	self->smoothAng.x = 0.0f;
	self->smoothAng.y = 0.0f;
	self->smoothAng.z = 0.0f;
	self->smoothRotVel.x = 0.0f;
	self->smoothRotVel.y = 0.0f;
	self->smoothRotVel.z = 0.0f;
	self->inAtmosphere = 1.0f;
	self->active = 0.0f;
	self->shake = 0.0f;
	self->pushTime = 0.0f;
	self->push.x = 0.0f;
	self->push.y = 0.0f;
	self->push.z = 0.0f;
	self->rendering = 0;
	self->isInit = 0;
	self->emi = 0.0f;
	self->vertBalance = 1.0f;
	self->histTime = 0.0f;
	self->histSpeed = 0.0f;
	self->histVel.x = 0.0f;
	self->histVel.y = 0.0f;
	self->histVel.z = 0.0f;
	self->histAcc.x = 0.0f;
	self->histAcc.y = 0.0f;
	self->histAcc.z = 0.0f;
	self->histAccLen = 0.0f;
	self->hitTime = 0.0f;
	self->timeOfs = (float)((double)(crt_rand() & 0x7fff) * VEH_RAND_SCALE * 100.0 + 0.0);
	cVehicleEffects_SetOwner(&self->effects, Veh_Ent(self));
	cVehicleTraffic_SetParent(&self->traffic, Veh_Ent(self));
	self->wl.arrow.alwaysUse = 1;
	self->fixedModel = 0;
	self->typeName[0] = 0;
	return Veh_Ent(self);
}

void __fastcall cAutoAim_dtor(void *self) {
	Nto1Node_Done(&((cAutoAim *)self)->target);
}

void __fastcall cVehicleEntity_dtor(cVehicleEntity *self) {
	self->wl.obj.base.vftable = CVEHICLEENTITY_VFTABLE;
	if (self->netCtrl) {
		operator_delete_sized(self->netCtrl, 0x5c);
		self->netCtrl = NULL;
	}
	if (self->wl.obj.base.ai) {
		(*(void(__thiscall **)(void *, int))*(void **)self->wl.obj.base.ai)(self->wl.obj.base.ai, 1);
	}
	self->wl.obj.base.ai = NULL;
	cVehicleEffects_dtor(&self->effects);
	cVehicleMotors_dtor(&self->motors);
	cAutoAim_dtor(&self->autoAim);
	cVehicleO648_dtor(self->o648);
	cWeaponManager_dtor(&self->weapons);
	crt_eh_vector_dtor(&self->pilot, sizeof(CGameModel), 1, GAME_FUNC(CGameModel_Reset, CGAMEMODEL_DTOR));
	cWithLifeEntity_dtor(&self->wl.obj.base);
}

cVehicleEntity *__thiscall cVehicleEntity_Delete(cVehicleEntity *self, byte flags) {
	cVehicleEntity_dtor(self);
	if (flags & 1) {
		operator_delete_sized(self, sizeof(*self));
	}
	return self;
}

/* ---- the collision ---- */

void __thiscall cVehicleEntity_Touch(cVehicleEntity *self, cEntity *e, const vec3_t *point, const vec3_t *normal) {
	vec3_t tmp1, tmp2, out;

	(void)point;
	if (!(e->mask & 1)) {
		return;
	}
	self->hitTime = Timer_GetTime(&self->wl.obj.timer);
	CHECK(Entity_IsType(e, TYEN_VEHICLE));
	cVehicleEntity *o = Veh(e);
	vec3_t selfVel = self->wl.obj.vel;
	vec3_t otherVel = o->wl.obj.vel;
	float dvx = self->wl.obj.vel.x - o->wl.obj.vel.x;
	float dvy = self->wl.obj.vel.y - o->wl.obj.vel.y;
	float dvz = self->wl.obj.vel.z - o->wl.obj.vel.z;
	vec3_t *po = e->vftable->GetPos(e, &tmp1);
	vec3_t *ps = self->wl.obj.base.vftable->GetPos(Veh_Ent(self), &tmp2);
	float py = ps->y - po->y, px = ps->x - po->x, pz = ps->z - po->z;
	float dist = (float)__builtin_sqrt((double)(py * py + px * px + pz * pz));
	float dot = py * dvy + px * dvx + pz * dvz;
	if ((dot > 0.0f || 1.0f > dvy * dvy + dvx * dvx + dvz * dvz || 10.0f > dist) && !self->wl.obj.move.active && !o->wl.obj.move.active) {
		/* inside each other: pushed apart along the line of the centers */
		float nx, ny, nz;
		Con_Debugf("LOW LEVEL WARNING : Problematic Collision between %s %s\n", e->name, self->wl.obj.base.name);
		if (!(0.0f >= dist)) {
			vec3_t *s = self->wl.obj.base.vftable->GetPos(Veh_Ent(self), &tmp2);
			vec3_t *p = e->vftable->GetPos(e, &out);
			float inv = 1.0f / dist;
			nz = (p->z - s->z) * inv;
			nx = (p->x - s->x) * inv;
			ny = (p->y - s->y) * inv;
		} else {
			nx = 1.0f;
			ny = 0.0f;
			nz = 0.0f;
		}
		float pen = (o->wl.prim.radius + self->wl.prim.radius) - dist;
		float k = pen / self->wl.obj.slot->interval + 10.0f;
		self->wl.obj.vel.x = self->wl.obj.vel.x + nx * k;
		self->wl.obj.vel.y = self->wl.obj.vel.y + ny * k;
		self->wl.obj.vel.z = self->wl.obj.vel.z + nz * k;
		k = pen / self->wl.obj.slot->interval + 10.0f;
		out.x = o->wl.obj.vel.x - nx * k;
		out.y = o->wl.obj.vel.y - ny * k;
		out.z = o->wl.obj.vel.z - nz * k;
	} else {
		out = o->wl.obj.vel;
		Vec3_Bounce(normal, self->wl.mass, o->wl.mass, &self->wl.obj.vel, &out);
	}
	Obj_Vtbl(Veh_Ent(o))->SetVel(Veh_Ent(o), &out);
	if (Net_IsNetEntity(Veh_Ent(self))) {
		if (o->netCtrl) {
			o->netCtrl->i6c = 3;
		} else {
			o->wl.obj.vel = otherVel;
		}
		if (self->netCtrl) {
			self->netCtrl->i6c = 3;
		} else {
			self->wl.obj.vel = selfVel;
		}
	}
	if (Veh_IsServer()) {
		if (self->netCtrl) {
			self->netCtrl->hit = 2;
		}
		if (o->netCtrl) {
			o->netCtrl->hit = 2;
		}
	}
}

/* ---- the flight values ---- */

void __fastcall cVehicleEntity_UpdateTrust(cVehicleEntity *self) {
	self->acMass = self->wl.mass;
	if (self->wl.life > self->minLife) {
		self->acMass = (float)(self->wl.life - self->minLife) * g_AmorPieceWeight + self->wl.mass;
	}
	float engine = self->engine;
	if (self->cloud > 0.0f) {
		engine = engine * g_MotorDownFactor;
	}
	float acMass = self->acMass;
	self->maxSpeed = self->style + engine;
	float frw = engine * 14000.0f / acMass;
	self->friction = 2800.0f / acMass;
	self->frwTrust = frw;
	if (g_VehicleCenterPadLock != 0.0f && self->slot != -1) {
		self->strTrust = frw * 0.5f;
		self->backTrust = frw * 0.35714287f;
		return;
	}
	self->strTrust = frw * 0.35714287f;
	self->backTrust = frw * 0.21428572f;
}

void __thiscall cVehicleEntity_UpdateSpeed(cEntity *self) {
	Veh_ResetHistory(Veh(self));
}

void __thiscall cVehicleEntity_BoostTick(cVehicleEntity *self, float dt) {
	self->boost = self->boost - dt;
	if (0.0f >= self->boost) {
		self->boost = 0.0f;
	}
}

byte __thiscall cVehicleEntity_GetLaunchTaunts(cVehicleEntity *self) {
	return self->launchTaunts;
}

void __fastcall cVehicleEntity_StartBoost(cVehicleEntity *self) {
	Vehicle_StartSnd(Veh_Ent(self), 4);
	Veh_ResetHistory(self);
	self->shake = 1.0f;
}

void __thiscall cVehicleEntity_EMITick(cVehicleEntity *self, float dt) {
	self->emi = self->emi - dt;
	if (0.0f >= self->emi) {
		self->emi = 0.0f;
		return;
	}
	cWithLifeEntity_SetSndTime(Veh_Ent(self), 0.25f, 3);
}

void __thiscall cVehicleEntity_CloudTick(cVehicleEntity *self, float dt) {
	self->cloud = self->cloud - dt;
	if (0.0f >= self->cloud) {
		self->cloud = 0.0f;
		self->cloudOn = 0;
	}
}

void __thiscall cVehicleEntity_NoFireTick(cVehicleEntity *self, float dt) {
	if (0.0f > self->weapons.fireStamp || self->speed > 400.0f) {
		self->noFire = 1.0f;
		return;
	}
	if (self->noFire > 0.0f) {
		self->noFire = self->noFire - dt;
		return;
	}
	self->noFire = 0.0f;
}

void __thiscall cVehicleEntity_PushTick(cVehicleEntity *self, float dt) {
	if (self->pushTime > 0.0f) {
		float k = dt * 1000.0f / self->wl.mass;
		self->pushTime = self->pushTime - dt;
		self->wl.obj.vel.x = self->wl.obj.vel.x + self->push.x * k;
		self->wl.obj.vel.y = self->wl.obj.vel.y + self->push.y * k;
		self->wl.obj.vel.z = self->wl.obj.vel.z + self->push.z * k;
	}
}

void __thiscall cVehicleEntity_SetMass(cVehicleEntity *self, float mass) {
	CHECK(mass > 0.0f);
	self->wl.mass = mass;
	cVehicleEntity_UpdateTrust(self);
}

void __thiscall cVehicleEntity_SetCloud(cEntity *self, float time, byte on) {
	Veh(self)->cloud = time;
	Veh(self)->cloudOn = on;
}

void __thiscall cVehicleEntity_SetNetFlag(cEntity *self, byte on) {
	cVehicleEntity *v = Veh(self);
	if (Veh_IsServer() && v->netCtrl) {
		v->netCtrl->hit = on ? 2 : 0;
	}
}

byte __fastcall cVehicleEntity_IsBoostReady(cVehicleEntity *self) {
	return self->boost == 0.0f;
}

int __thiscall cVehicleEntity_IsEMIed(cVehicleEntity *self) {
	return self->emi > 0.0f;
}

int __fastcall cVehicleEntity_CanFire(cVehicleEntity *self) {
	if (Net_IsNetEntity(Veh_Ent(self))) {
		return (Veh_Net(self)->keys >> 2) & 1;
	}
	return 0.0f >= self->noFire;
}

/* the speed and the acceleration from the history, the crash sounds at a sudden change */
void __thiscall cVehicleEntity_UpdateHistory(cVehicleEntity *self) {
	vec3_t acc, jerk;

	float dt = Weap_X87Sub(Timer_GetTime(&self->wl.obj.timer), self->histTime);
	if (0.001 >= (double)dt) {
		return;
	}
	self->speed = Veh_Len(&self->wl.obj.vel);
	float inv = 1.0f / dt;
	acc.x = (self->histVel.x - self->wl.obj.vel.x) * inv;
	acc.y = (self->histVel.y - self->wl.obj.vel.y) * inv;
	acc.z = (self->histVel.z - self->wl.obj.vel.z) * inv;
	jerk.x = self->histAcc.x + acc.x;
	jerk.y = self->histAcc.y + acc.y;
	jerk.z = self->histAcc.z + acc.z;
	float accLen = (float)__builtin_sqrt((double)(acc.y * acc.y + acc.x * acc.x + acc.z * acc.z));
	float jerkLen = (float)__builtin_sqrt((double)(jerk.y * jerk.y + jerk.x * jerk.x + jerk.z * jerk.z));
	float since = Weap_X87Sub(Timer_GetTime(&self->wl.obj.timer), self->hitTime);
	if (g_VehicleCrashMinTime > since && self->histAccLen > g_VehicleCrashAccLo) {
		Veh_Vtbl(self)->Crash(self, &jerk, jerkLen);
		self->histAcc.x = 0.0f;
		accLen = 0.0f;
		self->histAcc.y = 0.0f;
		self->histAcc.z = 0.0f;
	}
	self->histTime = self->histTime + dt;
	self->histVel = self->wl.obj.vel;
	self->histSpeed = self->speed;
	self->histAcc = acc;
	self->histAccLen = accLen;
}

void __thiscall cVehicleEntity_Crash(cVehicleEntity *self, const vec3_t *acc, float force) {
	(void)acc;
	cFollowCamEntity *cam = (cFollowCamEntity *)self->wl.obj.cam;
	int snd;
	float shake;

	if (cam) {
		cam->quakeTime = 0.2f;
	}
	if (g_VehicleCrashAccMed > force) {
		snd = 0x1b;
		shake = 0x1.1df46ap-7f;	/* 0.5 degrees */
	} else if (g_VehicleCrashAccHi > force) {
		snd = 0x1c;
		shake = 0x1.7d45e2p-7f;	/* 2/3 degrees */
	} else if (g_VehicleCrashAccHev > force) {
		snd = 0x1d;
		shake = 0x1.1df46ap-6f;	/* 1 degree */
	} else {
		snd = 0x1e;
		shake = 0x1.7d45e2p-6f;	/* 4/3 degrees */
	}
	if (cam) {
		cam->quakeFactor = shake;
	}
	Vehicle_StartSnd(Veh_Ent(self), snd);
}

/* the smooth values the effects, the camera and the sounds read */
void __thiscall cVehicleEntity_Smooth(cVehicleEntity *self, float dt) {
	vec3_t d, n, ang, local;

	float thrust = self->controls.acc;
	if (!(thrust > 0.0f)) {
		thrust = thrust * 0.5f;
	}
	float f = Float_MinSS(1.0f, self->speed / 16800.0f);
	float k = dt / ((1.0f - f) * self->fxEngineTime + self->fxBoostEngineTime * f);
	Float_Approach(&self->smoothTurnX, self->controls.str * 0.5f, k);
	Float_Approach(&self->smoothThrust, thrust, k);
	Float_Approach(&self->smoothSpeed, self->speed,
		(float)__builtin_fabs((double)(self->speed - self->smoothSpeed)) * dt * g_FxSpeedVariation);
	float step = dt * 12.566371f;
	Float_Approach(&self->smoothRotVel.x, self->wl.obj.rotVel.x, step);
	Float_Approach(&self->smoothRotVel.y, self->wl.obj.rotVel.y, step);
	Float_Approach(&self->smoothRotVel.z, self->wl.obj.rotVel.z, step);
	d.x = self->wl.obj.ang.x - self->smoothAng.x;
	d.y = self->wl.obj.ang.y - self->smoothAng.y;
	d.z = self->wl.obj.ang.z - self->smoothAng.z;
	vec3_t *na = Vec3_NormAng(&n, &d);
	step = g_FxAngVariation * dt;
	float ty = self->smoothAng.y + na->y;
	float tz = self->smoothAng.z + na->z;
	float tx = na->x + self->smoothAng.x;
	Float_Approach(&self->smoothAng.x, tx, (float)__builtin_fabs((double)(tx - self->smoothAng.x)) * step);
	Float_Approach(&self->smoothAng.y, ty, (float)__builtin_fabs((double)(ty - self->smoothAng.y)) * step);
	Float_Approach(&self->smoothAng.z, tz, (float)__builtin_fabs((double)(tz - self->smoothAng.z)) * step);
	ang.x = self->smoothAng.x;
	ang.y = self->smoothAng.y;
	ang.z = 0.0f;
	Vec3_RotateInv(&local, &self->wl.obj.vel, &ang);
	vec3_t last = self->localVel;
	step = g_FxVelVariation * dt;
	Float_Approach(&self->localVel.x, local.x, (float)__builtin_fabs((double)(local.x - self->localVel.x)) * step);
	Float_Approach(&self->localVel.y, local.y, (float)__builtin_fabs((double)(local.y - self->localVel.y)) * step);
	Float_Approach(&self->localVel.z, local.z, (float)__builtin_fabs((double)(local.z - self->localVel.z)) * step);
	if ((double)dt > 0.001) {
		float inv = 1.0f / dt;
		float ax = (self->localVel.x - last.x) * inv;
		float ay = (self->localVel.y - last.y) * inv;
		float az = (self->localVel.z - last.z) * inv;
		step = g_FxAccVariation * dt;
		Float_Approach(&self->localAcc.x, ax, (float)__builtin_fabs((double)(ax - self->localAcc.x)) * step);
		Float_Approach(&self->localAcc.y, ay, (float)__builtin_fabs((double)(ay - self->localAcc.y)) * step);
		Float_Approach(&self->localAcc.z, az, (float)__builtin_fabs((double)(az - self->localAcc.z)) * step);
	}
	Float_Approach(&self->inAtmosphere, self->wl.obj.base.pos.y > g_AtmosphereLimit ? 0.0f : 1.0f, dt + dt);
	float on = 1.0f;
	if (!self->wl.obj.base.flag_4c ||
		(!(g_pApp->flags & APPF_CLIENT) && self->wl.obj.move.active && self->wl.obj.move.time >= self->wl.obj.move.moveTime)) {
		on = 0.0f;
	}
	Float_Approach(&self->active, on, dt);
	self->shake = self->shake - dt / g_FxBoostTimeDuration;
	if (0.0f > self->shake) {
		self->shake = 0.0f;
	}
}

/* ---- the moves ---- */

void __thiscall cVehicleEntity_Collide(cVehicleEntity *self, vec3_t *from, float dt) {
	vec3_t *pos = &self->wl.obj.base.pos;
	vec3_t to = *pos;

	float speed = Veh_Len(&self->wl.obj.vel);
	self->speed = speed;
	if (to.y > g_SpaceLimit) {
		to.y = g_SpaceLimit;
	}
	if (from->x != to.x || from->y != to.y || from->z != to.z) {
		if (cVehicleTraffic_Move(&self->traffic, from, &to, self->wRadius, pos, speed, (CSphereBox *)self->collision)) {
			self->hitTime = Timer_GetTime(&self->wl.obj.timer);
			float inv = 1.0f / dt;
			vec3_t v;
			v.x = (pos->x - from->x) * inv;
			v.y = (pos->y - from->y) * inv;
			v.z = (pos->z - from->z) * inv;
			float len = (float)__builtin_sqrt((double)(v.y * v.y + v.x * v.x + v.z * v.z));
			if (self->speed > len) {
				self->wl.obj.vel = v;
				self->speed = len;
			}
		}
	}
	if (cVehicleTraffic_CollideEntities(&self->traffic) &&
		cWorld_TestSphereEntities(g_pWorld, Veh_Ent(self), (void *)1, from, self->wl.prim.radius, NULL, 0,
			(int)(intptr_t)pos)) {
		self->hitTime = Timer_GetTime(&self->wl.obj.timer);
		to.x = from->x + self->wl.obj.vel.x * dt;
		to.y = from->y + self->wl.obj.vel.y * dt;
		to.z = from->z + self->wl.obj.vel.z * dt;
		cVehicleTraffic_Move(&self->traffic, from, &to, self->wRadius, pos, self->speed, (CSphereBox *)self->collision);
		if (cWorld_TestSphereEntities(g_pWorld, Veh_Ent(self), (void *)1, pos, self->wl.prim.radius, NULL, 1, 0)) {
			*pos = *from;
		}
	}
	if (self->wl.obj.base.pos.y > g_SpaceLimit) {
		*pos = *from;
	}
}

void __thiscall cVehicleEntity_NetMove(cVehicleEntity *self, float dt) {
	vec3_t from = self->wl.obj.base.pos;

	cVehicleEntity_Step(self, dt);
	cVehicleEntity_Collide(self, &from, dt);
	cNetVController *net = Veh_Net(self);
	if (net->force) {
		net->force = 0;
		Veh_Net(self)->hit = 0;
		self->netCtrl->i6c = 0;
		self->wl.obj.vel = Veh_Net(self)->vel;
		self->wl.obj.base.pos = Veh_Net(self)->pos;
		self->wl.obj.ang.x = Veh_Net(self)->ang.x;
		self->wl.obj.ang.y = Veh_Net(self)->ang.y;
		if (self->wl.obj.cam) {
			((cFollowCamEntity *)self->wl.obj.cam)->slowFrames = 10;
		}
	} else if (net->hit) {
		net->hit = 0;
		self->netCtrl->i6c = 0;
		self->wl.obj.vel = Veh_Net(self)->vel;
	}
}

void __thiscall cVehicleEntity_ClientMove(cVehicleEntity *self, float dt) {
	cNetVController *net = Veh_Net(self);

	if (net->wait == 0.5f) {
		net->vftable->Move(net, Veh_Ent(self), dt, 1);
		return;
	}
	vec3_t from = self->wl.obj.base.pos;
	net->vftable->Move(net, Veh_Ent(self), dt, 1);
	if (!(Veh_Net(self)->flags & 0x40)) {
		self->wl.obj.rotVel.z = 0.0f;
	}
	cVehicleEntity_Collide(self, &from, dt);
}

void __thiscall cVehicleEntity_ServerMove(cVehicleEntity *self, float dt) {
	vec3_t *pos = &self->wl.obj.base.pos;
	vec3_t start = *pos;
	cNetVController *net = Veh_Net(self);

	if (net->wait == 0.5f) {
		if (cWorld_TestSphereEntities(g_pWorld, Veh_Ent(self), (void *)1, &net->pos, self->wl.prim.radius, NULL, 1, 0)) {
			self->netCtrl->force = 2;
		}
		cNetVController *nc = self->netCtrl;
		if (!nc->force) {
			if (nc->hit) {
				Veh_Net(self)->flags &= 0xef;
			}
			Veh_Net(self)->vftable->Move(Veh_Net(self), Veh_Ent(self), dt, 1);
			cAutoAim_Update(&self->autoAim, Veh_Ent(self), dt);
		} else {
			Veh_Net(self)->wait = Veh_Net(self)->wait - dt;
		}
	} else {
		pos->x = pos->x + self->wl.obj.vel.x * dt;
		pos->y = self->wl.obj.vel.y * dt + pos->y;
		pos->z = self->wl.obj.vel.z * dt + pos->z;
		float wait = net->wait;
		if (wait > 0.0f) {
			net->wait = wait - dt;
		} else if (wait == -10000.0f) {
			float fr = self->friction;
			self->wl.obj.vel.x = self->wl.obj.vel.x - self->wl.obj.vel.x * dt * fr;
			self->wl.obj.vel.y = self->wl.obj.vel.y - self->wl.obj.vel.y * dt * fr;
			self->wl.obj.vel.z = self->wl.obj.vel.z - self->wl.obj.vel.z * dt * fr;
		} else {
			net->wait = -10000.0f;
			self->wl.obj.vel.x = 0.0f;
			self->wl.obj.vel.y = 0.0f;
			self->wl.obj.vel.z = 0.0f;
			self->wl.obj.rotVel.x = 0.0f;
			self->wl.obj.rotVel.y = 0.0f;
			self->wl.obj.rotVel.z = 0.0f;
		}
	}
	if (start.x != pos->x || start.y != pos->y || start.z != pos->z) {
		vec3_t moved = *pos;
		cVehicleEntity_Collide(self, &start, dt);
		if (pos->x == start.x && pos->y == start.y && pos->z == start.z) {
			float dx = moved.x - start.x, dy = moved.y - start.y, dz = moved.z - start.z;
			float d = (float)__builtin_sqrt((double)(dy * dy + dx * dx + dz * dz));
			if (d > 20000.0f) {
				*pos = moved;
			}
		}
	}
}

void __thiscall cVehicleEntity_Move(cVehicleEntity *self, float dt) {
	vec3_t from = self->wl.obj.base.pos;

	cVehicleEntity_Step(self, dt);
	cVehicleEntity_Collide(self, &from, dt);
}

void __thiscall cVehicleEntity_Tick(cVehicleEntity *self, float dt) {
	if ((Net_IsNetEntity(Veh_Ent(self)) && !Veh_IsPlayerSlot(self->slot)) || (Veh_IsServer() && self->netCtrl)) {
		Controls_FromNet(&self->controls, Veh_Ent(self), dt);
		if (self->boost == 0.0f && !(Veh_Net(self)->keys & 0x1000)) {
			cVehicleEntity_StartBoost(self);
			self->boost = self->boostTime + 0.1f;
		}
	}
	if (Net_IsNetEntity(Veh_Ent(self))) {
		if (Veh_IsPlayerSlot(self->slot)) {
			cVehicleEntity_NetMove(self, dt);
		} else {
			cVehicleEntity_ClientMove(self, dt);
		}
	} else if (Veh_IsServer() && self->netCtrl) {
		cVehicleEntity_ServerMove(self, dt);
	} else {
		vec3_t from = self->wl.obj.base.pos;
		cVehicleEntity_Step(self, dt);
		cVehicleEntity_Collide(self, &from, dt);
	}
	self->cloud = self->cloud - dt;
	if (0.0f >= self->cloud) {
		self->cloud = 0.0f;
		self->cloudOn = 0;
	}
	self->emi = self->emi - dt;
	if (0.0f >= self->emi) {
		self->emi = 0.0f;
	} else {
		cWithLifeEntity_SetSndTime(Veh_Ent(self), 0.25f, 3);
	}
	cVehicleEntity_NoFireTick(self, dt);
	cVehicleEntity_UpdateTrust(self);
	cWeaponManager_Update(&self->weapons, dt);
	if (self->slot != -1) {
		cVehicleO648_Update(self->o648, dt);
	}
	self->boost = self->boost - dt;
	if (0.0f >= self->boost) {
		self->boost = 0.0f;
	}
	self->wl.fireAtt = self->smoothSpeed / 16800.0f;
	cWithLifeEntity_Tick(Veh_Ent(self), dt);
	Veh_Vtbl(self)->UpdateHistory(self);
}

void __thiscall cVehicleEntity_Update(cVehicleEntity *self) {
	cObjEntity_UpdateTimer(Veh_Ent(self));
	float dt = cTimer_GetDelta(&self->wl.obj.timer);
	if (1e-06f >= dt) {
		return;
	}
	cVehicleEffects_UpdateState(&self->effects, dt);
	if (Veh_IsPlayerSlot(self->slot)) {
		cVehicleControls_Tick(&self->controls, Veh_Ent(self), dt);
	}
	cWithLifeEntity_Update(Veh_Ent(self));
	Veh_Vtbl(self)->UpdateSounds(self, dt);
	cVehicleEntity_Smooth(self, dt);
}

void __thiscall cVehicleEntity_SetVel(cVehicleEntity *self, const vec3_t *vel) {
	if (self->wl.obj.move.active) {
		return;
	}
	cObjEntity_SetVel(Veh_Ent(self), vel);
	float speed = Veh_Len(&self->wl.obj.vel);
	self->speed = speed;
	if (speed > 42000.0f) {
		float k = 42000.0f / speed;
		self->wl.obj.vel.x = self->wl.obj.vel.x * k;
		self->wl.obj.vel.y = self->wl.obj.vel.y * k;
		self->wl.obj.vel.z = self->wl.obj.vel.z * k;
		self->speed = 42000.0f;
	}
}

/* ---- the sounds of the motors ---- */

void Vehicle_SndCrossFade(cSndItem *a, cSndItem *b) {
	if (!b) {
		return;
	}
	float f = 1.0f;
	if (a->times[0] > 0.0f) {
		f = Float_MinSS(1.0f, a->time / a->times[0]);
	}
	float g = 1.0f - f;
	if (0.0f > a->time) {
		b->vols[4] = b->volume;
		a->time = 0.0f;
	}
	b->volume = b->vols[4] * g;
	a->volume = f * a->volume;
	b->time = -1.0f;
}

/* pitch and volume from the low and high values by t, the high ones blended to the top ones by f above MaxSpeed */
static void Veh_MotorSnd(cSndItem *s, float t, float f, byte fast) {
	float u = 1.0f - t;

	if (fast) {
		float g = 1.0f - f;
		s->pitches[3] = s->pitches[1] * g + s->pitches[2] * f;
		s->vols[3] = s->vols[1] * g + s->vols[2] * f;
		s->pitch = s->pitches[0] * u + s->pitches[3] * t;
		s->volume = s->vols[0] * u + s->vols[3] * t;
		return;
	}
	s->pitch = s->pitches[0] * u + s->pitches[1] * t;
	s->volume = s->vols[0] * u + s->vols[1] * t;
}

void __thiscall cVehicleEntity_UpdateSounds(cVehicleEntity *self, float dt) {
	cSndInterface *snd = &self->wl.obj.snd;
	cSndItem *a, *b, *c, *d;
	byte alt;

	alt = PtrArray_Get(snd, 8) == NULL;
	if (!alt) {
		alt = self->wl.obj.cam || (self->wl.obj.move.active && self->wl.obj.move.time >= self->wl.obj.move.moveTime);
		((cSndItem *)PtrArray_Get(snd, alt ? 8 : 0))->volume = 0.0f;
	}
	if (!self->cloudOn && !(self->cloud > 0.0f)) {
		a = PtrArray_Get(snd, 2);
		b = PtrArray_Get(snd, alt ? 0 : 8);
		c = PtrArray_Get(snd, 3);
		d = PtrArray_Get(snd, 1);
	} else {
		a = PtrArray_Get(snd, alt ? 0 : 8);
		b = PtrArray_Get(snd, 2);
		c = PtrArray_Get(snd, 1);
		d = PtrArray_Get(snd, 3);
	}
	if (b && d && a && c) {
		if (g_VehicleEditingMode) {
			b->volume = 0.0f;
			d->volume = 0.0f;
			a->volume = 0.0f;
			c->volume = 0.0f;
			return;
		}
		cEntity *cam = self->wl.obj.cam;
		if (!cam) {
			d->volume = 0.0f;
		}
		float t = Float_MinSS(1.0f, self->smoothTurnX * self->smoothTurnX + self->smoothThrust * self->smoothThrust);
		float max = self->maxSpeed;
		byte fast = !(max >= self->speed);
		float f = 0.0f;
		if (fast) {
			f = (self->speed - max) / (28000.0f - max);
		}
		Veh_MotorSnd(b, t, f, fast);
		if (cam) {
			Veh_MotorSnd(d, t, f, fast);
		}
		Vehicle_SndCrossFade(b, a);
		Vehicle_SndCrossFade(d, c);
		if (self->wl.obj.base.vftable->GetAI(Veh_Ent(self)) || Net_IsNetEntity(Veh_Ent(self))) {
			b->volume = b->volume * g_NPCSndMotorVolFactor;
			d->volume = d->volume * g_NPCSndMotorVolFactor;
			a->volume = a->volume * g_NPCSndMotorVolFactor;
			c->volume = c->volume * g_NPCSndMotorVolFactor;
		}
	}
	int bits;
	memcpy(&bits, &dt, sizeof(bits));
	cWeaponManager_NextWeapSnd(&self->weapons, bits);
}

/* ---- the network ---- */

void __fastcall cVehicleEntity_NetCreate(cVehicleEntity *self) {
	if (!self->netCtrl) {
		void *mem = operator_new(CNETVCONTROLLER_SIZE);
		self->netCtrl = mem ? cNetVController_ctor(mem, Veh_Ent(self)) : NULL;
	}
}

void __fastcall cVehicleEntity_NetDelete(cVehicleEntity *self) {
	if (self->netCtrl) {
		operator_delete_sized(self->netCtrl, 0x5c);
		self->netCtrl = NULL;
	}
}

void __thiscall cVehicleEntity_NetRead(cVehicleEntity *self, void *msg, int unused) {
	(void)unused;
	Veh_Net(self)->vftable->Read(Veh_Net(self), msg);
}

void __thiscall cVehicleEntity_NetWrite(cVehicleEntity *self, float dist2, void *msg, int full) {
	byte kind = 6;
	if (12099999744.0f >= dist2) {
		kind = 0x7e;
	}
	if (Veh_IsServer() && dist2 == 0.0f) {
		kind |= 0x80;
	}
	if (100000000.0f >= dist2) {
		kind |= 1;
	}
	/* the kind replaces the low byte of the argument slot, the rest keeps the bits of dist2 */
	uint32_t bits;
	memcpy(&bits, &dist2, sizeof(bits));
	bits = (bits & ~0xffu) | kind;
	cNetVController *net = self->netCtrl ? self->netCtrl : Veh_Net(self);
	net->vftable->Write(net, (int)bits, Veh_Ent(self), msg, full);
}

void __thiscall cVehicleEntity_SetUsrControl(cVehicleEntity *self, int slot) {
	if (slot >= 0) {
		self->money = -1;
	}
	if (Veh_IsPlayerSlot(self->slot)) {
		Controls_Reset(&self->controls);
	}
	int old = self->slot;
	cSlotController *sc = self->wl.obj.slot;
	self->slot = slot;
	if (slot == -1) {
		sc->minDiff = 0.05f;
		if ((uint32_t)old <= 3 && cWorld_GetUsrEntity(g_pWorld, old) == Veh_Ent(self)) {
			g_pWorld->owners[old] = NULL;
		}
	} else {
		sc->minDiff = 0x1.111112p-6f;	/* 1 / 60 */
		g_pWorld->owners[self->slot] = Veh_Ent(self);
	}
	cVehicleO648_SetSlot(self->o648, slot);
	if (Net_IsNetEntity(Veh_Ent(self)) && Veh_IsPlayerSlot(self->slot)) {
		cVehicleEntity_NetCreate(self);
		return;
	}
	cVehicleEntity_NetDelete(self);
}

void __thiscall cVehicleEntity_EnableAI(cVehicleEntity *self, byte enable, void *p1, void *p2, void *p3) {
	if (!enable) {
		if (self->wl.obj.base.ai) {
			(*(void(__thiscall **)(void *, int))*(void **)self->wl.obj.base.ai)(self->wl.obj.base.ai, 1);
		}
		self->wl.obj.base.ai = NULL;
		return;
	}
	if (!self->wl.obj.base.ai) {
		void *mem = operator_new(CAIVEHICLE_SIZE);
		self->wl.obj.base.ai = mem ? cAIVehicle_ctor(mem, Veh_Ent(self), p1, p2, p3) : NULL;
	}
}

/* ---- the pilot ---- */

const char *__thiscall cVehicleEntity_GetPilot(cVehicleEntity *self, int slot) {
	if (slot == 0 && *Veh_PilotNode(self, slot) != -1) {
		return Veh_PilotName(self, 0);
	}
	return VEH_EMPTY;
}

byte __thiscall cVehicleEntity_GetPilotPos(cVehicleEntity *self, int slot, vec3_t *pos, vec3_t *ang) {
	matrix_t m;
	vec3_t e;

	if (slot != 0 || *Veh_PilotNode(self, slot) == -1) {
		return 0;
	}
	CGameModel_NodeMatrix(&self->wl.obj.model, &m, (uint32_t)*Veh_PilotNode(self, slot), 0, NULL, 0);
	if (pos) {
		pos->x = m.m[12];
		pos->y = m.m[13];
		pos->z = m.m[14];
	}
	if (ang) {
		*ang = *Matrix_GetEuler(&m, &e);
	}
	return 1;
}

byte __thiscall cVehicleEntity_SetPilot(cVehicleEntity *self, int slot, const char *name, float sx, float sy,
	float sz) {
	char node[16];
	char path[0x200];
	vec3_t scale;

	if (slot != 0) {
		return 0;
	}
	if (!*name) {
		CGameModel *m = Veh_PilotModel(self, slot);
		if (CGameModel_IsLoaded(m)) {
			CGameModel_Disable(m);
		}
		*Veh_PilotNode(self, slot) = -1;
		return 1;
	}
	memcpy(node, "DC_Player_01", 13);
	node[11] = (char)(node[11] + slot);
	int n = CGameModel_FindRenderNode(&self->wl.obj.model, node);
	*Veh_PilotNode(self, slot) = n;
	if (n == -1) {
		return 0;
	}
	Str_Copy(Veh_PilotName(self, slot), name, 0x20);
	*Veh_PilotAnim(self, slot, 0) = -1;
	*Veh_PilotAnim(self, slot, 1) = -1;
	*Veh_PilotAnim(self, slot, 2) = -1;
	*Veh_PilotAnim(self, slot, 3) = -1;
	*Veh_PilotAnim(self, slot, 4) = -1;
	Str_Printf(path, 0x200, g_VeCharsPathFmt, name, name);
	scale.x = sx;
	scale.y = sy;
	scale.z = sz;
	CGameModel *m = Veh_PilotModel(self, slot);
	CGameModel_LoadScaled(m, path, &scale);
	CGameModel_SetPivotNode(m, "DC_Ship_Link");
	CGameModel_SetPos(m, &self->wl.obj.base.pos);
	CGameModel_SetRot(m, &self->wl.obj.ang);
	if (!self->wl.obj.base.flag_4c) {
		CGameModel_Disable(m);
	}
	return 1;
}

byte __thiscall cVehicleEntity_SetPilotAnim(cVehicleEntity *self, int slot, int anim) {
	char path[0x200];

	CGameModel *m = Veh_PilotModel(self, slot);
	if (!CGameModel_IsLoaded(m)) {
		Con_Warnf("Warning : Animation '%s' cannot be added because no pilot found", g_PilotAnimNames[anim]);
		return 0;
	}
	int *a = Veh_PilotAnim(self, slot, anim);
	if (*a != -1) {
		Con_Warnf("Warning : Animation '%s' cannot be added because already exist", g_PilotAnimNames[anim]);
		return 0;
	}
	Str_Printf(path, 0x200, g_VeCharsPathFmt, Veh_PilotName(self, slot), g_PilotAnimNames[anim]);
	*a = CGameModel_AddAnim(m, path);
	CGameModel_AnimInfo(m, *a, Veh_PilotAnimLen(self, slot, anim), NULL);
	return 1;
}

/* the lean of the pilot from the turn (anims 0, 1) and the vertical acceleration (2, 3), else the relax loop */
void __thiscall cVehicleEntity_UpdatePilot(cVehicleEntity *self, int slot, float dt) {
	(void)dt;
	CGameModel *m = Veh_PilotModel(self, slot);
	float roll = self->wl.obj.renderAng.z;
	float t;

	int a2 = *Veh_PilotAnim(self, slot, 2), a3 = *Veh_PilotAnim(self, slot, 3);
	if (a2 != -1 && a3 != -1 && __builtin_fabs((double)self->localAcc.z) > 1000.0 &&
		0.13089969754219055 > __builtin_fabs((double)roll)) {
		float f = self->localAcc.z;
		if (f > 0.0f) {
			f = f - 1000.0f;
		} else {
			f = f + 1000.0f;
		}
		f = f / 30000.0f;
		float k = 1.0f;
		if (!(f > 1.0f)) {
			k = Float_MaxSS(-1.0f, f);
			if (!(k > 0.0f)) {
				CGameModel_SetAnim(m, a2, 0, 0);
				t = -(*Veh_PilotAnimLen(self, slot, 2) * k);
				CGameModel_AnimDelta(m, 0, t, NULL);
				return;
			}
		}
		CGameModel_SetAnim(m, a3, 0, 0);
		t = *Veh_PilotAnimLen(self, slot, 3) * k;
		CGameModel_AnimDelta(m, 0, t, NULL);
		return;
	}
	int a0 = *Veh_PilotAnim(self, slot, 0), a1 = *Veh_PilotAnim(self, slot, 1);
	if (a0 != -1 && a1 != -1 && __builtin_fabs((double)roll) > 0.13089969754219055) {
		float f = roll;
		if (f > 0.0f) {
			f = f - 0.1308997f;
		} else {
			f = f + 0.1308997f;
		}
		f = f * 1.5707964f;
		float k = 1.0f;
		if (!(f > 1.0f)) {
			k = Float_MaxSS(-1.0f, f);
			if (!(k > 0.0f)) {
				CGameModel_SetAnim(m, a1, 0, 0);
				t = -(*Veh_PilotAnimLen(self, slot, 0) * k);
				CGameModel_AnimDelta(m, 0, t, NULL);
				return;
			}
		}
		CGameModel_SetAnim(m, a0, 0, 0);
		t = *Veh_PilotAnimLen(self, slot, 0) * k;
		CGameModel_AnimDelta(m, 0, t, NULL);
		return;
	}
	int *relax = Veh_PilotAnim(self, slot, 4);
	if (*relax != -1) {
		float len = *Veh_PilotAnimLen(self, slot, 4);
		float loops = Weap_X87Div(Timer_GetTime(&self->wl.obj.timer), len);
		/* the relax animation plays in 5 of every 10 loops (the jump table of the original) */
		switch ((int)loops % 10) {
		case 0:
		case 2:
		case 4:
		case 7:
		case 9:
			t = (float)Game_Fmod((double)Timer_GetTime(&self->wl.obj.timer), (double)len);
			CGameModel_SetAnim(m, *relax, 0, 0);
			CGameModel_AnimDelta(m, 0, t, NULL);
			return;
		default:
			break;
		}
	}
	CGameModel_StopAnim(m, 0);
}

void __thiscall cVehicleEntity_UpdatePilotPos(cVehicleEntity *self, byte on) {
	matrix_t m;
	vec3_t e, pos;

	float dt = cTimer_GetDelta(&self->wl.obj.timer);
	if (self->pilotNode == -1) {
		return;
	}
	CGameModel *pm = &self->pilot;
	if (on) {
		CGameModel_Enable(pm);
	} else {
		CGameModel_Disable(pm);
	}
	CGameModel_NodeMatrix(&self->wl.obj.model, &m, (uint32_t)self->pilotNode, 0, NULL, 0);
	CGameModel_SetRot(pm, Matrix_GetEuler(&m, &e));
	pos.x = m.m[12];
	pos.y = m.m[13];
	pos.z = m.m[14];
	CGameModel_SetPos(pm, &pos);
	Veh_Vtbl(self)->UpdatePilot(self, 0, dt);
}

/* ---- the model ---- */

void __thiscall cVehicleEntity_SetModel(cVehicleEntity *self, const char *file, float sx, float sy, float sz,
	byte inDir) {
	char path[0x200];
	char node[256];
	vec3_t tmp;

	cVehicleEffects_Reset(&self->effects);
	cVehicleMotors_ClearEffects(&self->motors);
	if (CGameModel_IsLoaded(&self->pilot)) {
		CGameModel_Disable(&self->pilot);
	}
	self->pilotNode = -1;
	cVehicleMotors_Reset(&self->motors);
	if (!file || !*file) {
		cObjEntity_SetModel(Veh_Ent(self), NULL, sx, sy, sz, 1);
		return;
	}
	if (inDir) {
		Str_Copy(self->typeName, file, 0x20);
		Str_Printf(path, 0x200, g_ShipPathFmt, self->typeName, self->typeName);
	} else {
		Path_Split(file, self->typeName, NULL, NULL);
		Str_Copy(path, file, 0x200);
	}
	cObjEntity_SetModel(Veh_Ent(self), path, sx, sy, sz, 1);
	CGameModel *model = &self->wl.obj.model;
	vec3_t root = *CGameModel_NodePos(model, &tmp, (uint32_t)CGameModel_FindRenderNode(model, "DC_Root"), 0);
	vec3_t w = *CGameModel_NodePos(model, &tmp, (uint32_t)CGameModel_FindRenderNode(model, "DC_WRadius"), 0);
	vec3_t v = *CGameModel_NodePos(model, &tmp, (uint32_t)CGameModel_FindRenderNode(model, "DC_VRadius"), 0);
	float dx = root.x - w.x, dy = root.y - w.y, dz = root.z - w.z;
	self->wRadius = (float)__builtin_sqrt((double)(dy * dy + dx * dx + dz * dz));
	dx = root.x - v.x;
	dy = root.y - v.y;
	dz = root.z - v.z;
	cPrimitive_SetSize(&self->wl.prim, (float)__builtin_sqrt((double)(dy * dy + dx * dx + dz * dz)));
	for (int i = 0; i < g_pWorld->ammo.count; i++) {
		for (int j = 0; j < 4; j++) {
			cWorld *wd = g_pWorld;
			if (!wd->ammo.items || (uint32_t)i >= (uint32_t)wd->ammo.count) {
				__asm__ volatile("int3");	/* the bounds check of Global/Table.h */
			}
			Str_PrintfQuiet(node, 0xff, "DC_%s_%02d", wd->ammo.items[i].name, j + 1);
			self->weapons.nodes[i][j] = CGameModel_FindRenderNode(model, node);
		}
	}
	cWeaponManager_ClearWeaps(&self->weapons);
	cWeaponManager_ClearAmmo(&self->weapons);
	cVehicleEffects_Init(&self->effects);
	cVehicleMotors_InitEffects(&self->motors);
	Veh_Ent(self)->vftable->SetEmbeddedFX(Veh_Ent(self), "EmbeddedFX");
}

void __thiscall cVehicleEntity_SetEmbeddedFX(cVehicleEntity *self, const char *fxType) {
	if (!strcmp(fxType, "EmbeddedFX")) {
		cEmbeddedFXVehicle *fx = Veh_FX(self);
		if (fx) {
			fx->vftable->Done(fx);
			if (Veh_FX(self)) {
				Veh_FX(self)->vftable->Delete(Veh_FX(self), 1);
				self->wl.obj.fx = NULL;
			}
		}
		void *mem = operator_new(CEMBEDDEDFXVEHICLE_SIZE);
		fx = mem ? cEmbeddedFXVehicle_ctor(mem) : NULL;
		self->wl.obj.fx = (struct cEmbeddedFX *)fx;
		cEmbeddedFXVehicle_SetTraffic(fx, self->typeName[0] == 'T');
		Veh_FX(self)->vftable->Init(Veh_FX(self));
		cEmbeddedFXVehicle_SetOwner(Veh_FX(self), Veh_Ent(self));
	}
	cVehicleEffects_Start(&self->effects, fxType);
	cObjEntity_SetEmbeddedFX(Veh_Ent(self), fxType);
}

void __fastcall cVehicleEntity_StartFade(cVehicleEntity *self) {
	if (Veh_FX(self)) {
		cEmbeddedFXVehicle_StartFade(Veh_FX(self));
	}
}

void __thiscall cVehicleEntity_SetAIRange(cVehicleEntity *self, byte on) {
	if (Veh_FX(self)) {
		cEmbeddedFXVehicle_SetAIRange(Veh_FX(self), on);
	}
}

void __thiscall cVehicleEntity_SetFX(cVehicleEntity *self, int fx) {
	cObjEntity_SetEffect(Veh_Ent(self), fx);
	if (CGameModel_IsLoaded(&self->pilot)) {
		CGameModel_SetEffect(&self->pilot, (uint32_t)fx);
	}
	cVehicleMotors_SetEffect(&self->motors, fx);
}

void __thiscall cVehicleEntity_SetFXParam(cVehicleEntity *self, int i, float v) {
	cObjEntity_SetEffectParam(Veh_Ent(self), i, v);
	if (CGameModel_IsLoaded(&self->pilot)) {
		uint32_t bits;
		memcpy(&bits, &v, sizeof(bits));
		CGameModel_SetEffectParam(&self->pilot, i, bits);
	}
	cVehicleMotors_SetEffectParam(&self->motors, i, v);
}

void __thiscall cVehicleEntity_SetActive(cVehicleEntity *self, byte on) {
	if (on == self->wl.obj.base.flag_4c) {
		return;
	}
	if (!on && self->wl.obj.base.flag_4c) {
		Controls_Reset(&self->controls);
		cWeaponManager_Update(&self->weapons, 0.005f);
		self->active = 0.0f;
	}
	cWithLifeEntity_SetActive(Veh_Ent(self), on);
	if (CGameModel_IsLoaded(&self->wl.obj.model) && self->pilotNode != -1) {
		if (self->wl.obj.base.flag_4c) {
			CGameModel_Enable(&self->pilot);
		} else {
			CGameModel_Disable(&self->pilot);
		}
	}
	cVehicleEffects_SetActive(&self->effects, on);
	cVehicleMotors_SetEnabled(&self->motors, on);
	if (self->slot != -1 && self->wl.obj.base.flag_4c) {
		g_pWorld->owners[self->slot] = Veh_Ent(self);
	}
}

/* the model drawn once with the angles and position of its model (the render pos and rot kept) */
void __thiscall cVehicleEntity_RenderNow(cVehicleEntity *self) {
	vec3_t tmp;

	if (!Obj_Vtbl(Veh_Ent(self))->GetModel(Veh_Ent(self)) || !CGameModel_IsLoaded(Obj_Vtbl(Veh_Ent(self))->GetModel(Veh_Ent(self)))) {
		return;
	}
	vec3_t pos = self->wl.obj.renderPos;
	vec3_t rot = self->wl.obj.renderAng;
	self->wl.obj.renderPos = *CGameModel_GetPos(Obj_Vtbl(Veh_Ent(self))->GetModel(Veh_Ent(self)), &tmp);
	self->wl.obj.renderAng = *CGameModel_GetRot(Obj_Vtbl(Veh_Ent(self))->GetModel(Veh_Ent(self)), &tmp);
	self->rendering = 1;
	Obj_Vtbl(Veh_Ent(self))->UpdateModel(Veh_Ent(self));
	uint32_t fx = CGameModel_GetEffect(&self->wl.obj.model, NULL);
	if (fx == 3) {
		Obj_Vtbl(Veh_Ent(self))->SetEffect(Veh_Ent(self), 0);
	}
	cWithLifeEntity_RenderNow(Veh_Ent(self));
	if (self->pilotNode != -1) {
		CGameModel_RenderNow(&self->pilot, 1);
	}
	cVehicleMotors_Render(&self->motors);
	if (fx == 3) {
		Obj_Vtbl(Veh_Ent(self))->SetEffect(Veh_Ent(self), 3);
	}
	if (Veh_FX(self)) {
		cEmbeddedFXVehicle_Render(Veh_FX(self));
	}
	self->wl.obj.renderPos = pos;
	self->wl.obj.renderAng = rot;
	Obj_Vtbl(Veh_Ent(self))->UpdateModel(Veh_Ent(self));
	self->rendering = 0;
}

void __thiscall cVehicleEntity_GetCamPoint(cVehicleEntity *self, vec3_t *pos, vec3_t *ang) {
	vec3_t v, r;

	if (!Veh_IsServer() || !self->netCtrl) {
		cObjEntity_GetCamera(Veh_Ent(self), pos, ang);
		return;
	}
	cNetVController *net = Veh_Net(self);
	ang->x = -net->ang.x;
	ang->y = net->ang.y + 3.1415927f;
	ang->z = 0.0f;
	v.x = self->wl.obj.camOff.x + 0.0f;
	v.y = self->wl.obj.camOff.y + Veh_Net(self)->height;
	v.z = self->wl.obj.camOff.z + 0.0f;
	vec3_t *rp = Vec3_Rotate(&r, &v, ang);
	net = Veh_Net(self);
	pos->x = net->pos.x + rp->x;
	pos->y = net->pos.y + rp->y;
	pos->z = net->pos.z + rp->z;
}

/* ---- the small vtable slots ---- */

const char *__thiscall cVehicleEntity_GetClassName(cVehicleEntity *self) {
	(void)self;
	return VEH_CLASSNAME;
}

int __thiscall cVehicleEntity_GetOwnerSlot(cEntity *e) {
	return Veh(e)->slot;
}

float __thiscall cVehicleEntity_GetWRadius(cVehicleEntity *self) {
	return self->wRadius;
}

void __thiscall cVehicleEntity_UpdateOff(cVehicleEntity *self) {
	self->boost = 0.0f;
	cWithLifeEntity_UpdateOff(Veh_Ent(self));
}

byte __thiscall cVehicleEntity_IsType(cVehicleEntity *self, const char *type) {
	if (!strcmp(type, TYEN_CAR)) {
		return 1;
	}
	return cWithLifeEntity_IsType(Veh_Ent(self), type);
}

/* the turn input curve */
float Vehicle_TurnCurve(float x) {
	if (!(__builtin_fabs((double)x) > 1.0)) {
		return x;
	}
	if (x > 0.0f) {
		return (float)__builtin_sqrt((double)x - 0.75) + 0.5f;
	}
	float s = -(float)__builtin_sqrt((double)-x - 0.75);
	return Weap_X87Sub(s, 0.5f);
}

/* ---- the script variables ---- */

static inline byte Veh_VarFloat(float *f, double *d, int set) {
	if ((byte)set) {
		*f = (float)*d;
	} else {
		*d = *f;
	}
	return 1;
}

static inline byte Veh_VarInt(int *v, int *i, int set) {
	if ((byte)set) {
		*v = *i;
	} else {
		*i = *v;
	}
	return 1;
}

static inline byte Veh_VarFlag(byte *v, int *i, int set) {
	if ((byte)set) {
		*v = *i != 0;
	} else {
		*i = *v != 0;
	}
	return 1;
}

byte __thiscall cVehicleEntity_VarF(cVehicleEntity *self, const char *name, double *d, int set) {
	if (!*name) {
		return 0;
	}
	if (!crt_strncmp("InCtrl_", name, 7) && Controls_VarF(&self->controls, name + 7, d, set)) {
		return 1;
	}
	if (!strncmp(name, "Traf_", 5)) {
		return cVehicleTraffic_VarF(&self->traffic, name + 5, d, set);
	}
	if (cWeaponManager_VarF(&self->weapons, name, d, (byte)set) ||
		Veh_MotorsVtbl(self)->VarF(&self->motors, name, d, set)) {
		return 1;
	}
	if (!strcmp(name, "FxBoostEngineTime")) {
		return Veh_VarFloat(&self->fxBoostEngineTime, d, set);
	}
	if (!strcmp(name, "VerticalBalance")) {
		return Veh_VarFloat(&self->vertBalance, d, set);
	}
	if (!strcmp(name, "BoostTime")) {
		if (!(byte)set) {
			*d = self->boostTime;
		} else if (*d >= 2.0 || !Net_IsNetEntity(Veh_Ent(self)) || !g_pApp->flags) {
			self->boostTime = (float)*d;
		}
		return 1;
	}
	if (!strcmp(name, "FxEngineTime")) {
		return Veh_VarFloat(&self->fxEngineTime, d, set);
	}
	if (!strcmp(name, "EMICounter")) {
		return Veh_VarFloat(&self->emi, d, set);
	}
	if (!strcmp(name, "MotorDCnt")) {
		return Veh_VarFloat(&self->cloud, d, set);
	}
	if (!strcmp(name, "MaxSpeed")) {
		if (!(byte)set) {
			cVehicleEntity_UpdateTrust(self);
		}
		return Veh_VarFloat(&self->maxSpeed, d, set);
	}
	if (!strcmp(name, "Fric")) {
		if (!(byte)set) {
			cVehicleEntity_UpdateTrust(self);
		}
		return Veh_VarFloat(&self->friction, d, set);
	}
	if (!strcmp(name, "TrustTime")) {
		return Veh_VarFloat(&self->trustTime, d, set);
	}
	if (!crt_strncmp("Can_", name, 4) && cAutoAim_VarF(&self->autoAim, name + 4, d, set)) {
		return 1;
	}
	if (!(byte)set) {
		if (!strcmp(name, "StrTrust")) {
			cVehicleEntity_UpdateTrust(self);
			*d = self->strTrust;
			return 1;
		}
		if (!strcmp(name, "BackTrust")) {
			cVehicleEntity_UpdateTrust(self);
			*d = self->backTrust;
			return 1;
		}
		if (!strcmp(name, "FrwTrust")) {
			cVehicleEntity_UpdateTrust(self);
			*d = self->frwTrust;
			return 1;
		}
		if (!strcmp(name, "Speed")) {
			*d = self->speed;
			return 1;
		}
	}
	if (!strcmp(name, "WRadius")) {
		return Veh_VarFloat(&self->wRadius, d, set);
	}
	if (!(byte)set && !strcmp(name, "AcMass")) {
		cVehicleEntity_UpdateTrust(self);
		*d = self->acMass;
		return 1;
	}
	if (!strcmp(name, "Style")) {
		Veh_VarFloat(&self->style, d, set);
		if ((byte)set) {
			cVehicleEntity_UpdateTrust(self);
		}
		return 1;
	}
	if (!strcmp(name, "Engine")) {
		Veh_VarFloat(&self->engine, d, set);
		if ((byte)set) {
			cVehicleEntity_UpdateTrust(self);
		}
		return 1;
	}
	return cWithLifeEntity_VarF(Veh_Ent(self), name, d, set);
}

byte __thiscall cVehicleEntity_VarI(cVehicleEntity *self, const char *name, int *i, int set) {
	if (!*name) {
		return 0;
	}
	size_t len = strlen(name);
	if (!strncmp(name, "Traf_", 5)) {
		return cVehicleTraffic_VarI(&self->traffic, name + 5, i, set);
	}
	if (cWeaponManager_VarI(&self->weapons, name, i, (byte)set) ||
		Veh_MotorsVtbl(self)->VarI(&self->motors, name, i, set)) {
		return 1;
	}
	if ((byte)set && len == 5 && name[4] == '0' && !strncmp(name, "AnmP", 4)) {
		Veh_Vtbl(self)->SetPilotAnim(self, 0, *i);
		return 1;
	}
	if (!strcmp(name, "FixedModel")) {
		return Veh_VarFlag(&self->fixedModel, i, set);
	}
	if (!strcmp(name, "Money")) {
		return Veh_VarInt(&self->money, i, set);
	}
	if (!(byte)set) {
		if (!strcmp(name, "SpeedKmH")) {
			*i = (int)(self->speed / 28.0f);
			return 1;
		}
	} else if (*i && !strcmp(name, "AvoidCrash")) {
		cVehicleEntity_UpdateSpeed(Veh_Ent(self));
		return 1;
	}
	if (!strcmp(name, "isInit")) {
		return Veh_VarFlag(&self->isInit, i, set);
	}
	if (!strcmp(name, "LaunchTaunts")) {
		if ((byte)set) {
			self->launchTaunts = *i > 0;
		} else {
			*i = self->launchTaunts != 0;
		}
		return 1;
	}
	if ((byte)set && !strcmp(name, "ForceVel")) {
		cVehicleEntity_SetNetFlag(Veh_Ent(self), (byte)*i);
		return 1;
	}
	if ((byte)set && !strcmp(name, "ForceAll")) {
		if (Veh_IsServer() && self->netCtrl) {
			self->netCtrl->force = *i ? 2 : 0;
		}
		return 1;
	}
	if (!strcmp(name, "MinLife")) {
		return Veh_VarInt(&self->minLife, i, set);
	}
	if (!strcmp(name, "MaxLife")) {
		return Veh_VarInt(&self->maxLife, i, set);
	}
	if (!strcmp(name, "UsrControl")) {
		if ((byte)set) {
			cVehicleEntity_SetUsrControl(self, *i);
		} else {
			*i = self->slot;
		}
		return 1;
	}
	if (!strcmp(name, "isBoost")) {
		if (!(byte)set) {
			*i = cVehicleEntity_IsBoostReady(self);
		} else if (!Net_IsNetEntity(Veh_Ent(self)) || !g_pApp->flags) {
			self->boost = *i ? 0.0f : self->boostTime;
		}
		return 1;
	}
	if (!strcmp(name, "BoostDeact")) {
		if (!(byte)set) {
			*i = self->cloudOn != 0;
		} else if (!Net_IsNetEntity(Veh_Ent(self)) || !g_pApp->flags) {
			self->cloudOn = *i != 0;
		}
		return 1;
	}
	return cWithLifeEntity_VarI(Veh_Ent(self), name, i, set);
}

byte __thiscall cVehicleEntity_VarS(cVehicleEntity *self, const char *name, char **s, int set) {
	if (!*name) {
		return 0;
	}
	size_t len = strlen(name);
	if (!strncmp(name, "Traf_", 5)) {
		return cVehicleTraffic_VarS(&self->traffic, name + 5, s, set);
	}
	if (cWeaponManager_VarS(&self->weapons, name, s, (byte)set) ||
		Veh_MotorsVtbl(self)->VarS(&self->motors, name, s, set)) {
		return 1;
	}
	if (!(byte)set && !strcmp(name, "Model")) {
		*s = self->typeName;
		return 1;
	}
	if (len == 5 && name[4] == '0' && !strncmp(name, "MPil", 4)) {
		if ((byte)set) {
			Veh_Vtbl(self)->SetPilot(self, 0, *s, 1.0f, 1.0f, 1.0f);
			return 1;
		}
		*s = (char *)Veh_Vtbl(self)->GetPilot(self, 0);
		return *s != NULL;
	}
	if (!strcmp(name, "Target")) {
		if ((byte)set) {
			Str_Copy(self->target, *s, 0x80);
		} else {
			*s = self->target;
		}
		return 1;
	}
	if (!crt_strncmp("Can_", name, 4) && cAutoAim_VarS(&self->autoAim, name + 4, s, set)) {
		return 1;
	}
	return cWithLifeEntity_VarS(Veh_Ent(self), name, s, set);
}

byte __thiscall cVehicleEntity_VarV(cVehicleEntity *self, const char *name, double *x, double *y, double *z,
	int set) {
	vec3_t pos, ang;

	if (!*name) {
		return 0;
	}
	size_t len = strlen(name);
	if (!strncmp(name, "Traf_", 5)) {
		return cVehicleTraffic_VarV(&self->traffic, name + 5, x, y, z, set);
	}
	if (cWeaponManager_VarV(&self->weapons, name, x, y, z, (byte)set) ||
		Veh_MotorsVtbl(self)->VarV(&self->motors, name, x, y, z, set)) {
		return 1;
	}
	if (!(byte)set && len == 5 && name[4] <= '3' && name[4] >= '0') {
		if (!strncmp(name, "PPil", 4)) {
			byte r = Veh_Vtbl(self)->GetPilotPos(self, name[4] - '0', &pos, &ang);
			if (r) {
				*x = pos.x;
				*y = pos.y;
				*z = pos.z;
			}
			return r;
		}
		if (!strncmp(name, "APil", 4)) {
			byte r = Veh_Vtbl(self)->GetPilotPos(self, name[4] - '0', &pos, &ang);
			if (r) {
				*x = (double)ang.x * VEH_DEG_PER_RAD_NUM / VEH_DEG_PER_RAD_DEN;
				*y = (double)ang.y * VEH_DEG_PER_RAD_NUM / VEH_DEG_PER_RAD_DEN;
				*z = (double)ang.z * VEH_DEG_PER_RAD_NUM / VEH_DEG_PER_RAD_DEN;
			}
			return r;
		}
	}
	return cWithLifeEntity_VarV(Veh_Ent(self), name, x, y, z, set);
}

/* ---- the flight (rva 0x10a730) ---- */

/* the magnet that pulls the ships back to the center in the free space */
#define g_FreeSpaceMagnetCenterX GAME_GLOBAL_IN(float, GVar_FreeSpaceMagnetCenterX, 16, 0x447f38)
#define g_FreeSpaceMagnetCenterY GAME_GLOBAL_IN(float, GVar_FreeSpaceMagnetCenterY, 16, 0x448160)
#define g_FreeSpaceMagnetCenterZ GAME_GLOBAL_IN(float, GVar_FreeSpaceMagnetCenterZ, 16, 0x447f80)
#define g_FreeSpaceMagnetLoRadius GAME_GLOBAL_IN(float, GVar_FreeSpaceMagnetLoRadius, 16, 0x447ef0)
#define g_FreeSpaceMagnetHiRadius GAME_GLOBAL_IN(float, GVar_FreeSpaceMagnetHiRadius, 16, 0x447ed8)
#define g_FreeSpaceMagnetAccel GAME_GLOBAL_IN(float, GVar_FreeSpaceMagnetAccel, 16, 0x448130)

#define VEH_PI_F 0x1.921fb6p+1f
#define VEH_PI_2_F 0x1.921fb6p+0f		/* the limits of the pitch */
#define VEH_TOP_SPEED 28000.0f
#define VEH_BOOST_SPEED 16800.0f		/* at least after a boost */

/* Vehicle_TurnCurve inlined, all in SSE */
static inline float Veh_StepCurve(float x) {
	if (!(__builtin_fabs((double)x) > 1.0)) {
		return x;
	}
	if (x > 0.0f) {
		return (float)__builtin_sqrt((double)x - 0.75) + 0.5f;
	}
	float s = (float)__builtin_sqrt((double)-x - 0.75);
	return -s - 0.5f;
}

/* the thrust towards frwTrust * thrustIn within trustTime, or at once (full thrust or backwards) */
static inline void Veh_StepThrust(cVehicleEntity *self, float dt) {
	float th = self->thrust;

	if (th >= 0.0f) {
		if (self->controls.fullThrust) {
			self->thrust = self->frwTrust * self->controls.acc;
			return;
		}
		float frw = self->frwTrust;
		float t = frw * dt / self->trustTime * self->controls.acc + th;
		self->thrust = t;
		if (t > frw) {
			self->thrust = frw;
		}
		return;
	}
	if (0.0f > self->controls.acc) {
		self->thrust = self->backTrust * self->controls.acc;
	} else {
		self->thrust = 0.0f;
	}
}

/* below the atmosphere: the thrust, the turn, the friction and the speed limit */
static void Veh_StepFly(cVehicleEntity *self, float dt, float ax, float ay) {
	vec3_t in, out;
	float vx = self->wl.obj.vel.x;
	float vy = self->wl.obj.vel.y;
	float vz = self->wl.obj.vel.z;
	float before = (float)__builtin_sqrt((double)(vx * vx + vy * vy + vz * vz));

	in.x = 0.0f;
	in.y = 0.0f;
	in.z = self->thrust * dt;
	const vec3_t *r = Vec3_RotateXY(&out, &in, ax, ay);
	self->wl.obj.vel.x = self->wl.obj.vel.x - r->x;
	self->wl.obj.vel.y = self->wl.obj.vel.y - r->y;
	self->wl.obj.vel.z = self->wl.obj.vel.z - r->z;
	in.x = 0.0f;
	in.z = 0.0f;
	in.y = self->turnSpeedY * dt;
	r = Vec3_RotateXY(&out, &in, self->wl.obj.ang.x, self->wl.obj.ang.y);
	self->wl.obj.vel.x = self->wl.obj.vel.x - r->x;
	self->wl.obj.vel.y = self->wl.obj.vel.y - r->y;
	self->wl.obj.vel.z = self->wl.obj.vel.z - r->z;
	/* the side push of the turn */
	float k = self->turnSpeedX * dt;
	double a = (double)self->wl.obj.ang.y + 1.5707963267948966;
	self->wl.obj.vel.x = (float)Game_Sin(a) * k + self->wl.obj.vel.x;
	self->wl.obj.vel.z = (float)Game_Cos(a) * k + self->wl.obj.vel.z;
	float fx = self->wl.obj.vel.x * dt;
	float fy = self->wl.obj.vel.y * dt;
	float fz = self->wl.obj.vel.z * dt;
	float fr = self->friction;
	fx = fx * fr;
	fy = fy * fr;
	fz = fz * fr;
	self->wl.obj.vel.x = self->wl.obj.vel.x - fx;
	self->wl.obj.vel.y = self->wl.obj.vel.y - fy;
	self->wl.obj.vel.z = self->wl.obj.vel.z - fz;
	vx = self->wl.obj.vel.x;
	vy = self->wl.obj.vel.y;
	vz = self->wl.obj.vel.z;
	float sp = (float)__builtin_sqrt((double)(vx * vx + vy * vy + vz * vz));
	float ms = self->maxSpeed;
	self->speed = sp;
	/* above MaxSpeed it does not get faster than it was */
	if (sp > ms && sp >= before) {
		float lim = Float_MaxSS(ms, before);
		float f = lim / sp;
		self->wl.obj.vel.x = f * self->wl.obj.vel.x;
		self->wl.obj.vel.y = self->wl.obj.vel.y * f;
		self->wl.obj.vel.z = self->wl.obj.vel.z * f;
		self->speed = lim;
	}
}

static void Veh_StepMagnet(cVehicleEntity *self, float dt) {
	float dy = g_FreeSpaceMagnetCenterY - self->wl.obj.base.pos.y;
	float dx = g_FreeSpaceMagnetCenterX - self->wl.obj.base.pos.x;
	float dz = g_FreeSpaceMagnetCenterZ - self->wl.obj.base.pos.z;
	float d = (float)__builtin_sqrt((double)(dy * dy + dx * dx + dz * dz));
	float lo = g_FreeSpaceMagnetLoRadius;

	if (!(d > lo)) {
		return;
	}
	float hi = g_FreeSpaceMagnetHiRadius;
	float m = Float_MinSS(hi, d);
	float f = (m - lo) / (hi - lo) * g_FreeSpaceMagnetAccel / m;
	float px = dx * dt;
	float py = dy * dt;
	float pz = dz * dt;
	self->wl.obj.vel.x = f * px + self->wl.obj.vel.x;
	self->wl.obj.vel.y = f * py + self->wl.obj.vel.y;
	self->wl.obj.vel.z = f * pz + self->wl.obj.vel.z;
}

void __thiscall cVehicleEntity_Step(cVehicleEntity *self, float dt) {
	vec3_t in, out;
	byte high = self->wl.obj.base.pos.y > g_AtmosphereLimit;
	float sx = Veh_StepCurve(self->controls.hRot);
	float sy = Veh_StepCurve(self->controls.vRot);

	if (self->pushTime > 0.0f) {
		self->pushTime = self->pushTime - dt;
		float k = dt * 1000.0f / self->wl.mass;
		self->wl.obj.vel.x = k * self->push.x + self->wl.obj.vel.x;
		self->wl.obj.vel.y = self->wl.obj.vel.y + self->push.y * k;
		self->wl.obj.vel.z = self->wl.obj.vel.z + self->push.z * k;
	}
	Veh_StepThrust(self, dt);
	float tx = self->controls.str;
	float ay = self->wl.obj.ang.y - tx * VEH_PI_F * 0.125f;
	self->turnSpeedX = tx * self->strTrust;
	float ty = self->controls.vStr;
	float ax = self->wl.obj.ang.x - ty * VEH_PI_F * 0.125f;
	self->turnSpeedY = ty * self->strTrust;
	/* the boost: forward at the boost speed */
	if (!self->traffic.active && !self->cloudOn && self->controls.boostKey && 0.0f >= self->boost &&
		self->thrust > self->frwTrust * 0.5f) {
		float s = self->speed;
		self->cloud = 0.0f;
		self->boost = self->boostTime;
		if (15400.0f > s) {
			self->speed = VEH_BOOST_SPEED;
		} else {
			self->speed = s + 1400.0f;
		}
		in.x = 0.0f;
		in.y = 0.0f;
		in.z = -self->speed;
		self->wl.obj.vel = *Vec3_RotateXY(&out, &in, ax, ay);
		cVehicleEntity_StartBoost(self);
	}
	if (high) {
		/* above the atmosphere the ship falls back, slower towards the space limit */
		float lim = (g_SpaceLimit - self->wl.obj.base.pos.y) * VEH_BOOST_SPEED / (g_SpaceLimit - g_AtmosphereLimit);
		float vy = self->wl.obj.vel.y - g_DebrisDefault * dt;
		self->wl.obj.vel.y = vy;
		if (vy > lim) {
			self->wl.obj.vel.y = lim;
		}
		float pitch = self->wl.obj.ang.x;
		self->thrust = 0.0f;
		if (pitch > 0.0f) {
			self->wl.obj.ang.x = pitch - dt;
		}
	} else {
		Veh_StepFly(self, dt, ax, ay);
	}
	if (g_FreeSpaceMagnet) {
		Veh_StepMagnet(self, dt);
	}
	float sp = self->speed;
	if (sp > VEH_TOP_SPEED) {
		float f = VEH_TOP_SPEED / sp;
		self->wl.obj.vel.x = f * self->wl.obj.vel.x;
		self->wl.obj.vel.y = self->wl.obj.vel.y * f;
		self->wl.obj.vel.z = self->wl.obj.vel.z * f;
		self->speed = VEH_TOP_SPEED;
	}
	self->wl.obj.base.pos.x = self->wl.obj.base.pos.x + self->wl.obj.vel.x * dt;
	self->wl.obj.base.pos.y = self->wl.obj.vel.y * dt + self->wl.obj.base.pos.y;
	self->wl.obj.base.pos.z = self->wl.obj.vel.z * dt + self->wl.obj.base.pos.z;
	/* the turn gets slower with the speed */
	float ms = self->maxSpeed;
	float g = (ms + ms) / (self->speed + ms);
	float rx = g * sy;
	self->wl.obj.rotVel.x = rx;
	float pitch = rx * dt + self->wl.obj.ang.x;
	self->wl.obj.ang.x = pitch;
	if (pitch > VEH_PI_2_F) {
		self->wl.obj.ang.x = VEH_PI_2_F;
	} else if (-VEH_PI_2_F > pitch) {
		self->wl.obj.ang.x = -VEH_PI_2_F;
	}
	float s2 = sx + sx;
	float roll = (float)Game_Atan((double)(s2 - self->controls.str)) * 0.5f;
	self->roll = roll;
	float az = self->wl.obj.ang.z;
	float rz = (roll - az) * 4.0f;
	self->wl.obj.rotVel.z = rz;
	float ry = s2 * g;
	self->wl.obj.rotVel.y = ry;
	self->wl.obj.ang.z = rz * dt + az;
	self->wl.obj.ang.y = ry * dt + self->wl.obj.ang.y;
}

/* ---- the model and the camera of a slot (vf24, rva 0x105ea0) ---- */

#define g_HiRacerFov GAME_GLOBAL_IN(float, GVar_HiRacerFov, 16, 0x447f20)		/* degrees at top speed */
#define g_LoRacerFov GAME_GLOBAL_IN(float, GVar_LoRacerFov, 16, 0x448250)
#define g_MotorTrustRotationX GAME_GLOBAL_IN(float, GVar_MotorTrustRotationX, 16, 0x4480a0)
#define g_MotorTrustRotationY GAME_GLOBAL_IN(float, GVar_MotorTrustRotationY, 16, 0x447f98)
#define g_PilotViewDist GAME_GLOBAL_IN(float, GVar_PilotViewDist, 16, 0x448238)	/* the pilot is drawn nearer to the camera */

#define VEH_CAM_PITCH -0x1.0c1524p-2f			/* -15 degrees */
#define VEH_FLOAT_PERIOD 25.132741228718345		/* 8 pi */

/* the camera of a player ship: the shake, the field of view by the speed, the distance and the motion blur */
static void Veh_UpdateCamera(cVehicleEntity *self) {
	cFollowCamEntity *cam = (cFollowCamEntity *)self->wl.obj.cam;
	float sh = self->shake;
	float lo = -sh;
	double range = (double)(sh - Float_Keep(lo));
	float r = (float)((double)(crt_rand() & 0x7fff) * 3.051850947599719e-05 * range + (double)lo);

	cam->camRot = r * 0x1.99999ap-7f;
	float fov = (g_HiRacerFov - g_LoRacerFov) * self->smoothSpeed / VEH_TOP_SPEED + g_LoRacerFov;
	cam->base.cam.fov = (float)((double)fov * M_PI_D / 180.0);
	cam->camDist = self->slot == 1 ? g_VehicleCamDist2 : g_VehicleCamDist;
	cam->camAng = VEH_CAM_PITCH;
	float m = cam->quakeTime * 4.0f;
	float c = 0x1.666666p-2f;
	float lim = c > m ? m : c;
	float q = self->shake * 0x1.99999ap-1f;
	float blur = lim > q ? Float_MinSS(m, c) : q;
	g_MotionBlurFactorBegin = 0.0f;
	g_MotionBlurFactorEnd = blur;
	g_MotionBlurOffVar = 0.0f;
	g_MotionBlurTime = 0.0f;
	g_MotionBlurEndTime = 0.0f;
}

/* the floating of the model and its tilt by the acceleration */
static void Veh_UpdateFloating(cVehicleEntity *self, float t, vec3_t *rot, vec3_t *pos) {
	float k = Float_MaxSS(0.5f, 1.0f - self->speed / VEH_BOOST_SPEED);
	float amp = (g_VehicleEditingMode ? 0x1.99999ap-3f : g_FXVehicleFloatingFactor) * k;

	pos->x = (float)(Game_Sin((double)t * 0.9) * 10.0) * amp + pos->x;
	pos->y = (float)(Game_Sin((double)t) * 10.0) * amp + pos->y;
	pos->z = (float)(Game_Sin((double)t * 1.1) * 10.0) * amp + pos->z;
	float rz = (float)(Game_Sin((double)(t + t)) / VEH_FLOAT_PERIOD) * amp + rot->z;
	float rx = (float)(Game_Sin((double)(t * 0x1.b33334p+0f)) / VEH_FLOAT_PERIOD) * amp + rot->x;
	rot->y = (float)(Game_Sin((double)(t * 0x1.266666p+1f)) / VEH_FLOAT_PERIOD) * amp + rot->y;
	rot->x = (float)Game_Atan((double)(self->localAcc.z / VEH_BOOST_SPEED)) * g_MotorTrustRotationY *
		self->vertBalance + rx;
	rot->z = (float)Game_Atan((double)(self->localAcc.x / -VEH_BOOST_SPEED)) * g_MotorTrustRotationX *
		self->vertBalance + rz;
}

/* the pilot model at its node when the camera is near or the ship is the target of player 0 */
static void Veh_UpdatePilotModel(cVehicleEntity *self, const vec3_t *pos, float dt) {
	vec3_t cam, ang, ppos;
	matrix_t mtx;
	byte near = 0;

	if (g_SplitScreenGame || self->wl.obj.base.flag_4c) {
		cam = g_pWorld->camPos[g_pWorld->curCam];
		float dy = pos->y - cam.y;
		float dx = pos->x - cam.x;
		float dz = pos->z - cam.z;
		float r = g_PilotViewDist;
		near = r * r > dy * dy + dx * dx + dz * dz;
	}
	byte target = 0;
	if (!near) {
		cEntity *u = cWorld_GetUsrEntity(g_pWorld, 0);
		if (u && u->vftable->IsType(u, TYEN_CAR) && Veh(u)->target[0]) {
			/* the first two bytes as a short, then the rest */
			uint16_t a, b;
			memcpy(&a, self->wl.obj.base.name, 2);
			memcpy(&b, Veh(u)->target, 2);
			target = a == b && !strcmp(self->wl.obj.base.name + 2, Veh(u)->target + 2);
		}
	}
	if (!self->wl.obj.base.flag_4c || (!near && !target)) {
		if (self->pilotNode != -1 && CGameModel_IsEnabled(&self->pilot)) {
			CGameModel_Disable(&self->pilot);
		}
		return;
	}
	if (self->pilotNode == -1) {
		return;
	}
	if (near) {
		CGameModel_Enable(&self->pilot);
	} else {
		CGameModel_Disable(&self->pilot);
	}
	CGameModel_NodeMatrix(&self->wl.obj.model, &mtx, (uint32_t)self->pilotNode, 0, NULL, 0);
	CGameModel_SetRot(&self->pilot, Matrix_GetEuler(&mtx, &ang));
	ppos.x = mtx.m[12];
	ppos.y = mtx.m[13];
	ppos.z = mtx.m[14];
	CGameModel_SetPos(&self->pilot, &ppos);
	Veh_Vtbl(self)->UpdatePilot(self, 0, dt);
}

void __thiscall cVehicleEntity_UpdateModel(cVehicleEntity *self) {
	vec3_t rot, pos;
	float h = (self->wl.obj.base.pos.y - g_AtmosphereLimit) / (g_SpaceLimit - g_AtmosphereLimit);
	float t = Timer_GetTime(&self->wl.obj.timer) + self->timeOfs;

	rot = self->wl.obj.renderAng;
	pos = self->wl.obj.renderPos;
	if (self->fixedModel) {
		rot.x = 0.0f;
		rot.y = 0.0f;
		rot.z = 0.0f;
	}
	/* the wobble above the atmosphere */
	h = Float_MinSS(1.0f, h);
	if (h > 0.0f) {
		double s = Game_Sin((double)(t * 0x1.333334p-1f)) * (double)h;
		rot.x = (float)(s + s) + rot.x;
		rot.y = (float)(Game_Sin((double)(t * 0.5f)) * (double)h * 1.600000023841858) + rot.y;
		rot.z = (float)(Game_Sin((double)(t * 0x1.666666p-1f)) * (double)h * 1.100000023841858) + rot.z;
	}
	if (self->wl.obj.cam && self->slot != -1) {
		Veh_UpdateCamera(self);
	}
	if (self->fixedModel || self->wl.obj.cam ||
		(self->wl.obj.move.active && self->wl.obj.move.time >= self->wl.obj.move.moveTime && CGameModel_IsVisible(&self->wl.obj.model))) {
		Veh_UpdateFloating(self, t, &rot, &pos);
	}
	if (CGameModel_IsLoaded(&self->wl.obj.model)) {
		float dt = cTimer_GetDelta(&self->wl.obj.timer);
		CGameModel_SetRot(&self->wl.obj.model, &rot);
		CGameModel_SetPos(&self->wl.obj.model, &pos);
		Veh_UpdatePilotModel(self, &pos, dt);
		cVehicleMotors_Update(Vehicle_Motors(self));
		if (!self->rendering) {
			cVehicleEffects_Update(&self->effects, dt);
		}
	}
	if (Veh_FX(self) && !self->rendering) {
		Veh_FX(self)->vftable->Update(Veh_FX(self));
	}
}
