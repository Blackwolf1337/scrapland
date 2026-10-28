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

#define VEH_CLASSNAME ((const char *)Game_Ptr(0x3a77b8))	/* "Car" */
#define VEH_EMPTY ((const char *)Game_Ptr(0x3a3228))		/* "" */
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
#define g_PilotAnimNames ((const char **)Game_Ptr(0x4287e0))

/* the engine variables */
#define g_ShipPathFmt GAME_DATA(const char *, 0x448220)
#define g_VeCharsPathFmt GAME_DATA(const char *, 0x448058)
#define g_AmorPieceWeight GAME_DATA(float, 0x4480e8)
#define g_MotorDownFactor GAME_DATA(float, 0x448100)
#define g_VehicleCrashMinTime GAME_DATA(float, 0x4481a8)
#define g_VehicleCrashAccLo GAME_DATA(float, 0x448118)
#define g_VehicleCrashAccMed GAME_DATA(float, 0x4481f0)
#define g_VehicleCrashAccHi GAME_DATA(float, 0x447e60)
#define g_VehicleCrashAccHev GAME_DATA(float, 0x447ec0)
#define g_FxSpeedVariation GAME_DATA(float, 0x448208)
#define g_FxAngVariation GAME_DATA(float, 0x448028)
#define g_FxVelVariation GAME_DATA(float, 0x447fe0)
#define g_FxAccVariation GAME_DATA(float, 0x447e48)
#define g_FxBoostTimeDuration GAME_DATA(float, 0x448088)
#define g_VehicleEditingMode GAME_DATA(int, 0x447ea8)
#define g_NPCSndMotorVolFactor GAME_DATA(float, 0x448268)

/* the camera data of a player ship (vehicle + 0xf0) */
#define VehPlayer_Shake(p) (*(int *)((p) + 0x200))
#define VehPlayer_CrashA(p) (*(uint32_t *)((p) + 0x220))
#define VehPlayer_CrashB(p) (*(uint32_t *)((p) + 0x224))

/* the pilot slots: only slot 0 has storage, the original indexes by slot anyway */
static inline CGameModel *Veh_PilotModel(cVehicleEntity *v, int slot) {
	return (CGameModel *)((byte *)v + 0x500 + slot * (int)sizeof(CGameModel));
}

static inline int *Veh_PilotNode(cVehicleEntity *v, int slot) {
	return (int *)((byte *)v + 0x51c + slot * 4);
}

static inline char *Veh_PilotName(cVehicleEntity *v, int slot) {
	return (char *)v + 0x520 + slot * 0x20;
}

static inline int *Veh_PilotAnim(cVehicleEntity *v, int slot, int i) {
	return (int *)((byte *)v + 0x540 + (slot * VEH_PILOT_ANIMS + i) * 4);
}

static inline float *Veh_PilotAnimLen(cVehicleEntity *v, int slot, int i) {
	return (float *)((byte *)v + 0x554 + (slot * VEH_PILOT_ANIMS + i) * 4);
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
	self->histSpeed = Veh_Len(&self->vel);
	self->histVel = self->vel;
	self->histAcc.x = 0.0f;
	self->histAcc.y = 0.0f;
	self->histAcc.z = 0.0f;
	self->histAccLen = 0.0f;
}

/* ---- construction and destruction ---- */

cEntity *__thiscall cVehicleEntity_ctor(void *mem, const char *name, float x, float y, float z) {
	cVehicleEntity *self = mem;

	cWithLifeEntity_ctor(mem, name, x, y, z);
	self->base.vftable = Game_Ptr(CVEHICLEENTITY_VFTABLE);
	*(int *)(self->collision + 0) = 0;
	*(int *)(self->collision + 4) = 0;
	Controls_ctor(self->controls);
	crt_eh_vector_ctor(&self->pilot, sizeof(CGameModel), 1, Game_Ptr(CGAMEMODEL_CTOR), Game_Ptr(CGAMEMODEL_DTOR));
	cVehicleTraffic_ctor((cVehicleTraffic *)self->traffic);
	cWeaponManager_ctor(&self->weapons);
	cVehicleO648_ctor(self->o648);
	cAutoAim_ctor((cAutoAim *)self->autoAim);
	cVehicleMotors_ctor((cVehicleMotors *)self->motors);
	cVehicleEffects_ctor(&self->effects);
	self->base.mask |= 1;
	self->thrust = 0.0f;
	self->turnSpeedX = 0.0f;
	self->turnSpeedY = 0.0f;
	self->roll = 0.0f;
	self->speed = 0.0f;
	self->money = 0;
	self->trustTime = 0.5f;
	self->mass = 2800.0f;
	self->style = 2800.0f;
	self->engine = 2800.0f;
	cVehicleEntity_UpdateTrust(self);
	self->wRadius = 150.0f;
	cPrimitive_SetSize((cPrimitive *)((byte *)self + 0x39c), 130.0f);
	self->slot = -1;
	cVehicleMotors_SetOwner((cVehicleMotors *)self->motors, Veh_Ent(self));
	cWeaponManager_Init(&self->weapons, Veh_Ent(self));
	self->netCtrl = NULL;
	if (self->net) {
		operator_delete_sized(self->net, 0x5c);
		void *n = operator_new(CNETVCONTROLLER_SIZE);
		self->net = n ? cNetVController_ctor(n, Veh_Ent(self)) : NULL;
	}
	self->pilotNode = -1;
	/* the loop of the original runs over the animations of all slots with the stride of a slot (0x14): it also
	 * sets the first length, typeName + 0x0 and + 0x14 and cloud */
	for (int i = 0; i < VEH_PILOT_ANIMS; i++) {
		*(int *)((byte *)self + 0x540 + i * 0x14) = -1;
	}
	*(cVehicleEntity **)self->o648 = self;
	self->minLife = 100;
	self->maxLife = 150;
	self->acMass = self->mass;
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
	cVehicleTraffic_SetParent((cVehicleTraffic *)self->traffic, Veh_Ent(self));
	*((byte *)self + 0x384) = 1;
	self->fixedModel = 0;
	self->typeName[0] = 0;
	return Veh_Ent(self);
}

void __fastcall cAutoAim_dtor(void *self) {
	Nto1Node_Done((Nto1Node *)((byte *)self + 0x14));
}

void __fastcall cVehicleEntity_dtor(cVehicleEntity *self) {
	self->base.vftable = Game_Ptr(CVEHICLEENTITY_VFTABLE);
	if (self->netCtrl) {
		operator_delete_sized(self->netCtrl, 0x5c);
		self->netCtrl = NULL;
	}
	if (self->base.ai) {
		(*(void(__thiscall **)(void *, int))*(void **)self->base.ai)(self->base.ai, 1);
	}
	self->base.ai = NULL;
	cVehicleEffects_dtor(&self->effects);
	cVehicleMotors_dtor((cVehicleMotors *)self->motors);
	cAutoAim_dtor((cAutoAim *)self->autoAim);
	cVehicleO648_dtor(self->o648);
	cWeaponManager_dtor(&self->weapons);
	crt_eh_vector_dtor(&self->pilot, sizeof(CGameModel), 1, Game_Ptr(CGAMEMODEL_DTOR));
	cWithLifeEntity_dtor(&self->base);
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
	self->hitTime = Timer_GetTime(&self->timer);
	CHECK(Entity_IsType(e, TYEN_VEHICLE));
	cVehicleEntity *o = Veh(e);
	vec3_t selfVel = self->vel;
	vec3_t otherVel = o->vel;
	float dvx = self->vel.x - o->vel.x;
	float dvy = self->vel.y - o->vel.y;
	float dvz = self->vel.z - o->vel.z;
	vec3_t *po = e->vftable->GetPos(e, &tmp1);
	vec3_t *ps = self->base.vftable->GetPos(Veh_Ent(self), &tmp2);
	float py = ps->y - po->y, px = ps->x - po->x, pz = ps->z - po->z;
	float dist = (float)__builtin_sqrt((double)(py * py + px * px + pz * pz));
	float dot = py * dvy + px * dvx + pz * dvz;
	if ((dot > 0.0f || 1.0f > dvy * dvy + dvx * dvx + dvz * dvz || 10.0f > dist) && !self->dying && !o->dying) {
		/* inside each other: pushed apart along the line of the centers */
		float nx, ny, nz;
		Con_Debugf("LOW LEVEL WARNING : Problematic Collision between %s %s\n", e->name, self->base.name);
		if (!(0.0f >= dist)) {
			vec3_t *s = self->base.vftable->GetPos(Veh_Ent(self), &tmp2);
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
		float pen = (o->radius + self->radius) - dist;
		float k = pen / *(float *)(self->phys + 8) + 10.0f;
		self->vel.x = self->vel.x + nx * k;
		self->vel.y = self->vel.y + ny * k;
		self->vel.z = self->vel.z + nz * k;
		k = pen / *(float *)(self->phys + 8) + 10.0f;
		out.x = o->vel.x - nx * k;
		out.y = o->vel.y - ny * k;
		out.z = o->vel.z - nz * k;
	} else {
		out = o->vel;
		Vec3_Bounce(normal, self->mass, o->mass, &self->vel, &out);
	}
	Veh_Vtbl(o)->SetVel(o, &out);
	if (Net_IsNetEntity(Veh_Ent(self))) {
		if (o->netCtrl) {
			o->netCtrl->i6c = 3;
		} else {
			o->vel = otherVel;
		}
		if (self->netCtrl) {
			self->netCtrl->i6c = 3;
		} else {
			self->vel = selfVel;
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
	self->acMass = self->mass;
	if (self->life > self->minLife) {
		self->acMass = (float)(self->life - self->minLife) * g_AmorPieceWeight + self->mass;
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
		float k = dt * 1000.0f / self->mass;
		self->pushTime = self->pushTime - dt;
		self->vel.x = self->vel.x + self->push.x * k;
		self->vel.y = self->vel.y + self->push.y * k;
		self->vel.z = self->vel.z + self->push.z * k;
	}
}

void __thiscall cVehicleEntity_SetMass(cVehicleEntity *self, float mass) {
	CHECK(mass > 0.0f);
	self->mass = mass;
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
		return (self->net->keys >> 2) & 1;
	}
	return 0.0f >= self->noFire;
}

/* the speed and the acceleration from the history, the crash sounds at a sudden change */
void __thiscall cVehicleEntity_UpdateHistory(cVehicleEntity *self) {
	vec3_t acc, jerk;

	float dt = Weap_X87Sub(Timer_GetTime(&self->timer), self->histTime);
	if (0.001 >= (double)dt) {
		return;
	}
	self->speed = Veh_Len(&self->vel);
	float inv = 1.0f / dt;
	acc.x = (self->histVel.x - self->vel.x) * inv;
	acc.y = (self->histVel.y - self->vel.y) * inv;
	acc.z = (self->histVel.z - self->vel.z) * inv;
	jerk.x = self->histAcc.x + acc.x;
	jerk.y = self->histAcc.y + acc.y;
	jerk.z = self->histAcc.z + acc.z;
	float accLen = (float)__builtin_sqrt((double)(acc.y * acc.y + acc.x * acc.x + acc.z * acc.z));
	float jerkLen = (float)__builtin_sqrt((double)(jerk.y * jerk.y + jerk.x * jerk.x + jerk.z * jerk.z));
	float since = Weap_X87Sub(Timer_GetTime(&self->timer), self->hitTime);
	if (g_VehicleCrashMinTime > since && self->histAccLen > g_VehicleCrashAccLo) {
		Veh_Vtbl(self)->Crash(self, &jerk, jerkLen);
		self->histAcc.x = 0.0f;
		accLen = 0.0f;
		self->histAcc.y = 0.0f;
		self->histAcc.z = 0.0f;
	}
	self->histTime = self->histTime + dt;
	self->histVel = self->vel;
	self->histSpeed = self->speed;
	self->histAcc = acc;
	self->histAccLen = accLen;
}

void __thiscall cVehicleEntity_Crash(cVehicleEntity *self, const vec3_t *acc, float force) {
	(void)acc;
	byte *p = self->player;
	int snd;
	uint32_t shake;

	if (p) {
		VehPlayer_CrashA(p) = 0x3e4ccccd;	/* 0.2f */
	}
	if (g_VehicleCrashAccMed > force) {
		snd = 0x1b;
		shake = 0x3c0efa35;
	} else if (g_VehicleCrashAccHi > force) {
		snd = 0x1c;
		shake = 0x3c3ea2f1;
	} else if (g_VehicleCrashAccHev > force) {
		snd = 0x1d;
		shake = 0x3c8efa35;
	} else {
		snd = 0x1e;
		shake = 0x3cbea2f1;
	}
	if (p) {
		VehPlayer_CrashB(p) = shake;
	}
	Vehicle_StartSnd(Veh_Ent(self), snd);
}

/* the smooth values the effects, the camera and the sounds read */
void __thiscall cVehicleEntity_Smooth(cVehicleEntity *self, float dt) {
	vec3_t d, n, ang, local;

	float thrust = self->thrustIn;
	if (!(thrust > 0.0f)) {
		thrust = thrust * 0.5f;
	}
	float f = Float_MinSS(1.0f, self->speed / 16800.0f);
	float k = dt / ((1.0f - f) * self->fxEngineTime + self->fxBoostEngineTime * f);
	Float_Approach(&self->smoothTurnX, self->turnX * 0.5f, k);
	Float_Approach(&self->smoothThrust, thrust, k);
	Float_Approach(&self->smoothSpeed, self->speed,
		(float)__builtin_fabs((double)(self->speed - self->smoothSpeed)) * dt * g_FxSpeedVariation);
	float step = dt * 12.566371f;
	Float_Approach(&self->smoothRotVel.x, self->rotVel.x, step);
	Float_Approach(&self->smoothRotVel.y, self->rotVel.y, step);
	Float_Approach(&self->smoothRotVel.z, self->rotVel.z, step);
	d.x = self->ang.x - self->smoothAng.x;
	d.y = self->ang.y - self->smoothAng.y;
	d.z = self->ang.z - self->smoothAng.z;
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
	Vec3_RotateInv(&local, &self->vel, &ang);
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
	Float_Approach(&self->inAtmosphere, self->base.pos.y > g_AtmosphereLimit ? 0.0f : 1.0f, dt + dt);
	float on = 1.0f;
	if (!self->base.flag_4c ||
		(!(g_pApp->flags & APPF_CLIENT) && self->dying && self->f1a4 >= self->f1a8)) {
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
	vec3_t *pos = &self->base.pos;
	vec3_t to = *pos;

	float speed = Veh_Len(&self->vel);
	self->speed = speed;
	if (to.y > g_SpaceLimit) {
		to.y = g_SpaceLimit;
	}
	if (from->x != to.x || from->y != to.y || from->z != to.z) {
		if (cVehicleTraffic_Move((cVehicleTraffic *)self->traffic, from, &to, self->wRadius, pos, speed, (CSphereBox *)self->collision)) {
			self->hitTime = Timer_GetTime(&self->timer);
			float inv = 1.0f / dt;
			vec3_t v;
			v.x = (pos->x - from->x) * inv;
			v.y = (pos->y - from->y) * inv;
			v.z = (pos->z - from->z) * inv;
			float len = (float)__builtin_sqrt((double)(v.y * v.y + v.x * v.x + v.z * v.z));
			if (self->speed > len) {
				self->vel = v;
				self->speed = len;
			}
		}
	}
	if (cVehicleTraffic_CollideEntities((cVehicleTraffic *)self->traffic) &&
		cWorld_TestSphereEntities(g_pWorld, Veh_Ent(self), (void *)1, from, self->radius, NULL, 0,
			(int)(intptr_t)pos)) {
		self->hitTime = Timer_GetTime(&self->timer);
		to.x = from->x + self->vel.x * dt;
		to.y = from->y + self->vel.y * dt;
		to.z = from->z + self->vel.z * dt;
		cVehicleTraffic_Move((cVehicleTraffic *)self->traffic, from, &to, self->wRadius, pos, self->speed, (CSphereBox *)self->collision);
		if (cWorld_TestSphereEntities(g_pWorld, Veh_Ent(self), (void *)1, pos, self->radius, NULL, 1, 0)) {
			*pos = *from;
		}
	}
	if (self->base.pos.y > g_SpaceLimit) {
		*pos = *from;
	}
}

void __thiscall cVehicleEntity_NetMove(cVehicleEntity *self, float dt) {
	vec3_t from = self->base.pos;

	cVehicleEntity_Step(self, dt);
	cVehicleEntity_Collide(self, &from, dt);
	cNetVController *net = self->net;
	if (net->force) {
		net->force = 0;
		self->net->hit = 0;
		self->netCtrl->i6c = 0;
		self->vel = self->net->vel;
		self->base.pos = self->net->pos;
		self->ang.x = self->net->ang.x;
		self->ang.y = self->net->ang.y;
		if (self->player) {
			VehPlayer_Shake(self->player) = 10;
		}
	} else if (net->hit) {
		net->hit = 0;
		self->netCtrl->i6c = 0;
		self->vel = self->net->vel;
	}
}

void __thiscall cVehicleEntity_ClientMove(cVehicleEntity *self, float dt) {
	cNetVController *net = self->net;

	if (net->wait == 0.5f) {
		net->vftable->Move(net, Veh_Ent(self), dt, 1);
		return;
	}
	vec3_t from = self->base.pos;
	net->vftable->Move(net, Veh_Ent(self), dt, 1);
	if (!(self->net->flags & 0x40)) {
		self->rotVel.z = 0.0f;
	}
	cVehicleEntity_Collide(self, &from, dt);
}

void __thiscall cVehicleEntity_ServerMove(cVehicleEntity *self, float dt) {
	vec3_t *pos = &self->base.pos;
	vec3_t start = *pos;
	cNetVController *net = self->net;

	if (net->wait == 0.5f) {
		if (cWorld_TestSphereEntities(g_pWorld, Veh_Ent(self), (void *)1, &net->pos, self->radius, NULL, 1, 0)) {
			self->netCtrl->force = 2;
		}
		cNetVController *nc = self->netCtrl;
		if (!nc->force) {
			if (nc->hit) {
				self->net->flags &= 0xef;
			}
			self->net->vftable->Move(self->net, Veh_Ent(self), dt, 1);
			cAutoAim_Update((cAutoAim *)self->autoAim, Veh_Ent(self), dt);
		} else {
			self->net->wait = self->net->wait - dt;
		}
	} else {
		pos->x = pos->x + self->vel.x * dt;
		pos->y = self->vel.y * dt + pos->y;
		pos->z = self->vel.z * dt + pos->z;
		float wait = net->wait;
		if (wait > 0.0f) {
			net->wait = wait - dt;
		} else if (wait == -10000.0f) {
			float fr = self->friction;
			self->vel.x = self->vel.x - self->vel.x * dt * fr;
			self->vel.y = self->vel.y - self->vel.y * dt * fr;
			self->vel.z = self->vel.z - self->vel.z * dt * fr;
		} else {
			net->wait = -10000.0f;
			self->vel.x = 0.0f;
			self->vel.y = 0.0f;
			self->vel.z = 0.0f;
			self->rotVel.x = 0.0f;
			self->rotVel.y = 0.0f;
			self->rotVel.z = 0.0f;
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
	vec3_t from = self->base.pos;

	cVehicleEntity_Step(self, dt);
	cVehicleEntity_Collide(self, &from, dt);
}

void __thiscall cVehicleEntity_Tick(cVehicleEntity *self, float dt) {
	if ((Net_IsNetEntity(Veh_Ent(self)) && !Veh_IsPlayerSlot(self->slot)) || (Veh_IsServer() && self->netCtrl)) {
		Controls_FromNet(self->controls, Veh_Ent(self), dt);
		if (self->boost == 0.0f && !(self->net->keys & 0x1000)) {
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
		vec3_t from = self->base.pos;
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
	self->speedFactor = self->smoothSpeed / 16800.0f;
	cWithLifeEntity_Tick(Veh_Ent(self), dt);
	Veh_Vtbl(self)->UpdateHistory(self);
}

void __thiscall cVehicleEntity_Update(cVehicleEntity *self) {
	cObjEntity_UpdateTimer(Veh_Ent(self));
	float dt = cTimer_GetDelta(&self->timer);
	if (1e-06f >= dt) {
		return;
	}
	cVehicleEffects_UpdateState(&self->effects, dt);
	if (Veh_IsPlayerSlot(self->slot)) {
		cVehicleControls_Tick(self->controls, Veh_Ent(self), dt);
	}
	cWithLifeEntity_Update(Veh_Ent(self));
	Veh_Vtbl(self)->UpdateSounds(self, dt);
	cVehicleEntity_Smooth(self, dt);
}

void __thiscall cVehicleEntity_SetVel(cVehicleEntity *self, const vec3_t *vel) {
	if (self->dying) {
		return;
	}
	cObjEntity_SetVel(Veh_Ent(self), vel);
	float speed = Veh_Len(&self->vel);
	self->speed = speed;
	if (speed > 42000.0f) {
		float k = 42000.0f / speed;
		self->vel.x = self->vel.x * k;
		self->vel.y = self->vel.y * k;
		self->vel.z = self->vel.z * k;
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
	cSndInterface *snd = &self->sounds;
	cSndItem *a, *b, *c, *d;
	byte alt;

	alt = PtrArray_Get(snd, 8) == NULL;
	if (!alt) {
		alt = self->player || (self->dying && self->f1a4 >= self->f1a8);
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
		byte *p = self->player;
		if (!p) {
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
		if (p) {
			Veh_MotorSnd(d, t, f, fast);
		}
		Vehicle_SndCrossFade(b, a);
		Vehicle_SndCrossFade(d, c);
		if (self->base.vftable->GetAI(Veh_Ent(self)) || Net_IsNetEntity(Veh_Ent(self))) {
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
	self->net->vftable->Read(self->net, msg);
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
	cNetVController *net = self->netCtrl ? self->netCtrl : self->net;
	net->vftable->Write(net, (int)bits, Veh_Ent(self), msg, full);
}

void __thiscall cVehicleEntity_SetUsrControl(cVehicleEntity *self, int slot) {
	if (slot >= 0) {
		self->money = -1;
	}
	if (Veh_IsPlayerSlot(self->slot)) {
		Controls_Reset(self->controls);
	}
	int old = self->slot;
	byte *phys = self->phys;
	self->slot = slot;
	if (slot == -1) {
		*(uint32_t *)(phys + 0xc) = 0x3d4ccccd;	/* 0.05f */
		if ((uint32_t)old <= 3 && cWorld_GetUsrEntity(g_pWorld, old) == Veh_Ent(self)) {
			g_pWorld->owners[old] = NULL;
		}
	} else {
		*(uint32_t *)(phys + 0xc) = 0x3c888889;	/* 1 / 60.0f */
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
		if (self->base.ai) {
			(*(void(__thiscall **)(void *, int))*(void **)self->base.ai)(self->base.ai, 1);
		}
		self->base.ai = NULL;
		return;
	}
	if (!self->base.ai) {
		void *mem = operator_new(CAIVEHICLE_SIZE);
		self->base.ai = mem ? cAIVehicle_ctor(mem, Veh_Ent(self), p1, p2, p3) : NULL;
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
	CGameModel_NodoMatrix(&self->model, &m, (uint32_t)*Veh_PilotNode(self, slot), 0, NULL, 0);
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
	int n = CGameModel_FindNodoR(&self->model, node);
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
	CGameModel_SetPivotNodo(m, "DC_Ship_Link");
	CGameModel_SetPos(m, &self->base.pos);
	CGameModel_SetRot(m, &self->ang);
	if (!self->base.flag_4c) {
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
	float roll = self->renderRot.z;
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
		float loops = Weap_X87Div(Timer_GetTime(&self->timer), len);
		/* the relax animation plays in 5 of every 10 loops (the jump table of the original) */
		switch ((int)loops % 10) {
		case 0:
		case 2:
		case 4:
		case 7:
		case 9:
			t = (float)Game_Fmod((double)Timer_GetTime(&self->timer), (double)len);
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

	float dt = cTimer_GetDelta(&self->timer);
	if (self->pilotNode == -1) {
		return;
	}
	CGameModel *pm = &self->pilot;
	if (on) {
		CGameModel_Enable(pm);
	} else {
		CGameModel_Disable(pm);
	}
	CGameModel_NodoMatrix(&self->model, &m, (uint32_t)self->pilotNode, 0, NULL, 0);
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
	cVehicleMotors_ClearEffects((cVehicleMotors *)self->motors);
	if (CGameModel_IsLoaded(&self->pilot)) {
		CGameModel_Disable(&self->pilot);
	}
	self->pilotNode = -1;
	cVehicleMotors_Reset((cVehicleMotors *)self->motors);
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
	CGameModel *model = &self->model;
	vec3_t root = *CGameModel_NodoPos(model, &tmp, (uint32_t)CGameModel_FindNodoR(model, "DC_Root"), 0);
	vec3_t w = *CGameModel_NodoPos(model, &tmp, (uint32_t)CGameModel_FindNodoR(model, "DC_WRadius"), 0);
	vec3_t v = *CGameModel_NodoPos(model, &tmp, (uint32_t)CGameModel_FindNodoR(model, "DC_VRadius"), 0);
	float dx = root.x - w.x, dy = root.y - w.y, dz = root.z - w.z;
	self->wRadius = (float)__builtin_sqrt((double)(dy * dy + dx * dx + dz * dz));
	dx = root.x - v.x;
	dy = root.y - v.y;
	dz = root.z - v.z;
	cPrimitive_SetSize((cPrimitive *)((byte *)self + 0x39c), (float)__builtin_sqrt((double)(dy * dy + dx * dx + dz * dz)));
	for (int i = 0; i < g_pWorld->ammo.count; i++) {
		for (int j = 0; j < 4; j++) {
			cWorld *wd = g_pWorld;
			if (!wd->ammo.items || (uint32_t)i >= (uint32_t)wd->ammo.count) {
				__asm__ volatile("int3");	/* the bounds check of Global/Table.h */
			}
			Str_PrintfQuiet(node, 0xff, "DC_%s_%02d", wd->ammo.items[i].name, j + 1);
			self->weapons.nodes[i][j] = CGameModel_FindNodoR(model, node);
		}
	}
	cWeaponManager_ClearWeaps(&self->weapons);
	cWeaponManager_ClearAmmo(&self->weapons);
	cVehicleEffects_Init(&self->effects);
	cVehicleMotors_InitEffects((cVehicleMotors *)self->motors);
	Veh_Vtbl(self)->SetEmbeddedFX(self, "EmbeddedFX");
}

void __thiscall cVehicleEntity_SetEmbeddedFX(cVehicleEntity *self, const char *fxType) {
	if (!strcmp(fxType, "EmbeddedFX")) {
		cEmbeddedFXVehicle *fx = self->embeddedFX;
		if (fx) {
			fx->vftable->Done(fx);
			if (self->embeddedFX) {
				self->embeddedFX->vftable->Delete(self->embeddedFX, 1);
				self->embeddedFX = NULL;
			}
		}
		void *mem = operator_new(CEMBEDDEDFXVEHICLE_SIZE);
		fx = mem ? cEmbeddedFXVehicle_ctor(mem) : NULL;
		self->embeddedFX = fx;
		cEmbeddedFXVehicle_SetTraffic(fx, self->typeName[0] == 'T');
		self->embeddedFX->vftable->Init(self->embeddedFX);
		cEmbeddedFXVehicle_SetOwner(self->embeddedFX, Veh_Ent(self));
	}
	cVehicleEffects_Start(&self->effects, fxType);
	cObjEntity_SetEmbeddedFX(Veh_Ent(self), fxType);
}

void __fastcall cVehicleEntity_StartFade(cVehicleEntity *self) {
	if (self->embeddedFX) {
		cEmbeddedFXVehicle_StartFade(self->embeddedFX);
	}
}

void __thiscall cVehicleEntity_SetAIRange(cVehicleEntity *self, byte on) {
	if (self->embeddedFX) {
		cEmbeddedFXVehicle_SetAIRange(self->embeddedFX, on);
	}
}

void __thiscall cVehicleEntity_SetFX(cVehicleEntity *self, int fx) {
	cObjEntity_SetEffect(Veh_Ent(self), fx);
	if (CGameModel_IsLoaded(&self->pilot)) {
		CGameModel_SetEffect(&self->pilot, (uint32_t)fx);
	}
	cVehicleMotors_SetEffect((cVehicleMotors *)self->motors, fx);
}

void __thiscall cVehicleEntity_SetFXParam(cVehicleEntity *self, int i, float v) {
	cObjEntity_SetEffectParam(Veh_Ent(self), i, v);
	if (CGameModel_IsLoaded(&self->pilot)) {
		uint32_t bits;
		memcpy(&bits, &v, sizeof(bits));
		CGameModel_SetEffectParam(&self->pilot, i, bits);
	}
	cVehicleMotors_SetEffectParam((cVehicleMotors *)self->motors, i, v);
}

void __thiscall cVehicleEntity_SetActive(cVehicleEntity *self, byte on) {
	if (on == self->base.flag_4c) {
		return;
	}
	if (!on && self->base.flag_4c) {
		Controls_Reset(self->controls);
		cWeaponManager_Update(&self->weapons, 0.005f);
		self->active = 0.0f;
	}
	cWithLifeEntity_SetActive(Veh_Ent(self), on);
	if (CGameModel_IsLoaded(&self->model) && self->pilotNode != -1) {
		if (self->base.flag_4c) {
			CGameModel_Enable(&self->pilot);
		} else {
			CGameModel_Disable(&self->pilot);
		}
	}
	cVehicleEffects_SetActive(&self->effects, on);
	cVehicleMotors_SetEnabled((cVehicleMotors *)self->motors, on);
	if (self->slot != -1 && self->base.flag_4c) {
		g_pWorld->owners[self->slot] = Veh_Ent(self);
	}
}

/* the model drawn once with the angles and position of its model (the render pos and rot kept) */
void __thiscall cVehicleEntity_RenderNow(cVehicleEntity *self) {
	vec3_t tmp;

	if (!Veh_Vtbl(self)->GetModel(self) || !CGameModel_IsLoaded(Veh_Vtbl(self)->GetModel(self))) {
		return;
	}
	vec3_t pos = self->renderPos;
	vec3_t rot = self->renderRot;
	self->renderPos = *CGameModel_GetPos(Veh_Vtbl(self)->GetModel(self), &tmp);
	self->renderRot = *CGameModel_GetRot(Veh_Vtbl(self)->GetModel(self), &tmp);
	self->rendering = 1;
	Veh_Vtbl(self)->UpdateModel(self);
	uint32_t fx = CGameModel_GetEffect(&self->model, NULL);
	if (fx == 3) {
		Veh_Vtbl(self)->SetFX(self, 0);
	}
	cWithLifeEntity_RenderNow(Veh_Ent(self));
	if (self->pilotNode != -1) {
		CGameModel_RenderNow(&self->pilot, 1);
	}
	cVehicleMotors_Render((cVehicleMotors *)self->motors);
	if (fx == 3) {
		Veh_Vtbl(self)->SetFX(self, 3);
	}
	if (self->embeddedFX) {
		cEmbeddedFXVehicle_Render(self->embeddedFX);
	}
	self->renderPos = pos;
	self->renderRot = rot;
	Veh_Vtbl(self)->UpdateModel(self);
	self->rendering = 0;
}

void __thiscall cVehicleEntity_GetCamPoint(cVehicleEntity *self, vec3_t *pos, vec3_t *ang) {
	vec3_t v, r;

	if (!Veh_IsServer() || !self->netCtrl) {
		cObjEntity_GetCamera(Veh_Ent(self), pos, ang);
		return;
	}
	cNetVController *net = self->net;
	ang->x = -net->ang.x;
	ang->y = net->ang.y + 3.1415927f;
	ang->z = 0.0f;
	v.x = self->camOfs.x + 0.0f;
	v.y = self->camOfs.y + self->net->height;
	v.z = self->camOfs.z + 0.0f;
	vec3_t *rp = Vec3_Rotate(&r, &v, ang);
	net = self->net;
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
	if (!crt_strncmp("InCtrl_", name, 7) && Controls_VarF(self->controls, name + 7, d, set)) {
		return 1;
	}
	if (!strncmp(name, "Traf_", 5)) {
		return cVehicleTraffic_VarF((cVehicleTraffic *)self->traffic, name + 5, d, set);
	}
	if (cWeaponManager_VarF(&self->weapons, name, d, (byte)set) ||
		Veh_MotorsVtbl(self)->VarF(self->motors, name, d, set)) {
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
	if (!crt_strncmp("Can_", name, 4) && cAutoAim_VarF((cAutoAim *)self->autoAim, name + 4, d, set)) {
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
		return cVehicleTraffic_VarI((cVehicleTraffic *)self->traffic, name + 5, i, set);
	}
	if (cWeaponManager_VarI(&self->weapons, name, i, (byte)set) ||
		Veh_MotorsVtbl(self)->VarI(self->motors, name, i, set)) {
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
		return cVehicleTraffic_VarS((cVehicleTraffic *)self->traffic, name + 5, s, set);
	}
	if (cWeaponManager_VarS(&self->weapons, name, s, (byte)set) ||
		Veh_MotorsVtbl(self)->VarS(self->motors, name, s, set)) {
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
	if (!crt_strncmp("Can_", name, 4) && cAutoAim_VarS((cAutoAim *)self->autoAim, name + 4, s, set)) {
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
		return cVehicleTraffic_VarV((cVehicleTraffic *)self->traffic, name + 5, x, y, z, set);
	}
	if (cWeaponManager_VarV(&self->weapons, name, x, y, z, (byte)set) ||
		Veh_MotorsVtbl(self)->VarV(self->motors, name, x, y, z, set)) {
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
#define g_FreeSpaceMagnetCenterX GAME_DATA(float, 0x447f38)
#define g_FreeSpaceMagnetCenterY GAME_DATA(float, 0x448160)
#define g_FreeSpaceMagnetCenterZ GAME_DATA(float, 0x447f80)
#define g_FreeSpaceMagnetLoRadius GAME_DATA(float, 0x447ef0)
#define g_FreeSpaceMagnetHiRadius GAME_DATA(float, 0x447ed8)
#define g_FreeSpaceMagnetAccel GAME_DATA(float, 0x448130)

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
		if (self->fullThrust) {
			self->thrust = self->frwTrust * self->thrustIn;
			return;
		}
		float frw = self->frwTrust;
		float t = frw * dt / self->trustTime * self->thrustIn + th;
		self->thrust = t;
		if (t > frw) {
			self->thrust = frw;
		}
		return;
	}
	if (0.0f > self->thrustIn) {
		self->thrust = self->backTrust * self->thrustIn;
	} else {
		self->thrust = 0.0f;
	}
}

/* below the atmosphere: the thrust, the turn, the friction and the speed limit */
static void Veh_StepFly(cVehicleEntity *self, float dt, float ax, float ay) {
	vec3_t in, out;
	float vx = self->vel.x;
	float vy = self->vel.y;
	float vz = self->vel.z;
	float before = (float)__builtin_sqrt((double)(vx * vx + vy * vy + vz * vz));

	in.x = 0.0f;
	in.y = 0.0f;
	in.z = self->thrust * dt;
	const vec3_t *r = Vec3_RotateXY(&out, &in, ax, ay);
	self->vel.x = self->vel.x - r->x;
	self->vel.y = self->vel.y - r->y;
	self->vel.z = self->vel.z - r->z;
	in.x = 0.0f;
	in.z = 0.0f;
	in.y = self->turnSpeedY * dt;
	r = Vec3_RotateXY(&out, &in, self->ang.x, self->ang.y);
	self->vel.x = self->vel.x - r->x;
	self->vel.y = self->vel.y - r->y;
	self->vel.z = self->vel.z - r->z;
	/* the side push of the turn */
	float k = self->turnSpeedX * dt;
	double a = (double)self->ang.y + 1.5707963267948966;
	self->vel.x = (float)Game_Sin(a) * k + self->vel.x;
	self->vel.z = (float)Game_Cos(a) * k + self->vel.z;
	float fx = self->vel.x * dt;
	float fy = self->vel.y * dt;
	float fz = self->vel.z * dt;
	float fr = self->friction;
	fx = fx * fr;
	fy = fy * fr;
	fz = fz * fr;
	self->vel.x = self->vel.x - fx;
	self->vel.y = self->vel.y - fy;
	self->vel.z = self->vel.z - fz;
	vx = self->vel.x;
	vy = self->vel.y;
	vz = self->vel.z;
	float sp = (float)__builtin_sqrt((double)(vx * vx + vy * vy + vz * vz));
	float ms = self->maxSpeed;
	self->speed = sp;
	/* above MaxSpeed it does not get faster than it was */
	if (sp > ms && sp >= before) {
		float lim = Float_MaxSS(ms, before);
		float f = lim / sp;
		self->vel.x = f * self->vel.x;
		self->vel.y = self->vel.y * f;
		self->vel.z = self->vel.z * f;
		self->speed = lim;
	}
}

static void Veh_StepMagnet(cVehicleEntity *self, float dt) {
	float dy = g_FreeSpaceMagnetCenterY - self->base.pos.y;
	float dx = g_FreeSpaceMagnetCenterX - self->base.pos.x;
	float dz = g_FreeSpaceMagnetCenterZ - self->base.pos.z;
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
	self->vel.x = f * px + self->vel.x;
	self->vel.y = f * py + self->vel.y;
	self->vel.z = f * pz + self->vel.z;
}

void __thiscall cVehicleEntity_Step(cVehicleEntity *self, float dt) {
	vec3_t in, out;
	byte high = self->base.pos.y > g_AtmosphereLimit;
	float sx = Veh_StepCurve(self->stickX);
	float sy = Veh_StepCurve(self->stickY);

	if (self->pushTime > 0.0f) {
		self->pushTime = self->pushTime - dt;
		float k = dt * 1000.0f / self->mass;
		self->vel.x = k * self->push.x + self->vel.x;
		self->vel.y = self->vel.y + self->push.y * k;
		self->vel.z = self->vel.z + self->push.z * k;
	}
	Veh_StepThrust(self, dt);
	float tx = self->turnX;
	float ay = self->ang.y - tx * VEH_PI_F * 0.125f;
	self->turnSpeedX = tx * self->strTrust;
	float ty = self->turnY;
	float ax = self->ang.x - ty * VEH_PI_F * 0.125f;
	self->turnSpeedY = ty * self->strTrust;
	/* the boost: forward at the boost speed */
	if (!self->traffic[0x1c] && !self->cloudOn && self->boostKey && 0.0f >= self->boost &&
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
		self->vel = *Vec3_RotateXY(&out, &in, ax, ay);
		cVehicleEntity_StartBoost(self);
	}
	if (high) {
		/* above the atmosphere the ship falls back, slower towards the space limit */
		float lim = (g_SpaceLimit - self->base.pos.y) * VEH_BOOST_SPEED / (g_SpaceLimit - g_AtmosphereLimit);
		float vy = self->vel.y - g_DebrisDefault * dt;
		self->vel.y = vy;
		if (vy > lim) {
			self->vel.y = lim;
		}
		float pitch = self->ang.x;
		self->thrust = 0.0f;
		if (pitch > 0.0f) {
			self->ang.x = pitch - dt;
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
		self->vel.x = f * self->vel.x;
		self->vel.y = self->vel.y * f;
		self->vel.z = self->vel.z * f;
		self->speed = VEH_TOP_SPEED;
	}
	self->base.pos.x = self->base.pos.x + self->vel.x * dt;
	self->base.pos.y = self->vel.y * dt + self->base.pos.y;
	self->base.pos.z = self->vel.z * dt + self->base.pos.z;
	/* the turn gets slower with the speed */
	float ms = self->maxSpeed;
	float g = (ms + ms) / (self->speed + ms);
	float rx = g * sy;
	self->rotVel.x = rx;
	float pitch = rx * dt + self->ang.x;
	self->ang.x = pitch;
	if (pitch > VEH_PI_2_F) {
		self->ang.x = VEH_PI_2_F;
	} else if (-VEH_PI_2_F > pitch) {
		self->ang.x = -VEH_PI_2_F;
	}
	float s2 = sx + sx;
	float roll = (float)Game_Atan((double)(s2 - self->turnX)) * 0.5f;
	self->roll = roll;
	float az = self->ang.z;
	float rz = (roll - az) * 4.0f;
	self->rotVel.z = rz;
	float ry = s2 * g;
	self->rotVel.y = ry;
	self->ang.z = rz * dt + az;
	self->ang.y = ry * dt + self->ang.y;
}

/* ---- the model and the camera of a slot (vf24, rva 0x105ea0) ---- */

#define g_HiRacerFov GAME_DATA(float, 0x447f20)		/* degrees at top speed */
#define g_LoRacerFov GAME_DATA(float, 0x448250)
#define g_MotorTrustRotationX GAME_DATA(float, 0x4480a0)
#define g_MotorTrustRotationY GAME_DATA(float, 0x447f98)
#define g_PilotViewDist GAME_DATA(float, 0x448238)	/* the pilot is drawn nearer to the camera */

/* the camera data of the ship of a player (+0xf0) */
#define VehCam_Fov(p) (*(float *)((p) + 0xa8))
#define VehCam_Dist(p) (*(float *)((p) + 0x214))
#define VehCam_Pitch(p) (*(uint32_t *)((p) + 0x218))
#define VehCam_Shake(p) (*(float *)((p) + 0x21c))
#define VehCam_F220(p) (*(float *)((p) + 0x220))

#define VEH_CAM_PITCH 0xbe860a92u			/* -15 degrees */
#define VEH_FLOAT_PERIOD 25.132741228718345		/* 8 pi */

/* the camera of a player ship: the shake, the field of view by the speed, the distance and the motion blur */
static void Veh_UpdateCamera(cVehicleEntity *self) {
	byte *pl = self->player;
	float sh = self->shake;
	float lo = -sh;
	double range = (double)(sh - Float_Keep(lo));
	float r = (float)((double)(crt_rand() & 0x7fff) * 3.051850947599719e-05 * range + (double)lo);

	VehCam_Shake(pl) = r * 0x1.99999ap-7f;
	float fov = (g_HiRacerFov - g_LoRacerFov) * self->smoothSpeed / VEH_TOP_SPEED + g_LoRacerFov;
	VehCam_Fov(self->player) = (float)((double)fov * M_PI_D / 180.0);
	VehCam_Dist(self->player) = self->slot == 1 ? g_VehicleCamDist2 : g_VehicleCamDist;
	VehCam_Pitch(self->player) = VEH_CAM_PITCH;
	float m = VehCam_F220(self->player) * 4.0f;
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

	if (g_SplitScreenGame || self->base.flag_4c) {
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
			memcpy(&a, self->base.name, 2);
			memcpy(&b, Veh(u)->target, 2);
			target = a == b && !strcmp(self->base.name + 2, Veh(u)->target + 2);
		}
	}
	if (!self->base.flag_4c || (!near && !target)) {
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
	CGameModel_NodoMatrix(&self->model, &mtx, (uint32_t)self->pilotNode, 0, NULL, 0);
	CGameModel_SetRot(&self->pilot, Matrix_GetEuler(&mtx, &ang));
	ppos.x = mtx.m[12];
	ppos.y = mtx.m[13];
	ppos.z = mtx.m[14];
	CGameModel_SetPos(&self->pilot, &ppos);
	Veh_Vtbl(self)->UpdatePilot(self, 0, dt);
}

void __thiscall cVehicleEntity_UpdateModel(cVehicleEntity *self) {
	vec3_t rot, pos;
	float h = (self->base.pos.y - g_AtmosphereLimit) / (g_SpaceLimit - g_AtmosphereLimit);
	float t = Timer_GetTime(&self->timer) + self->timeOfs;

	rot = self->renderRot;
	pos = self->renderPos;
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
	if (self->player && self->slot != -1) {
		Veh_UpdateCamera(self);
	}
	if (self->fixedModel || self->player ||
		(self->dying && self->f1a4 >= self->f1a8 && CGameModel_IsVisible(&self->model))) {
		Veh_UpdateFloating(self, t, &rot, &pos);
	}
	if (CGameModel_IsLoaded(&self->model)) {
		float dt = cTimer_GetDelta(&self->timer);
		CGameModel_SetRot(&self->model, &rot);
		CGameModel_SetPos(&self->model, &pos);
		Veh_UpdatePilotModel(self, &pos, dt);
		cVehicleMotors_Update(Vehicle_Motors(self));
		if (!self->rendering) {
			cVehicleEffects_Update(&self->effects, dt);
		}
	}
	if (self->embeddedFX && !self->rendering) {
		self->embeddedFX->vftable->Update(self->embeddedFX);
	}
}
