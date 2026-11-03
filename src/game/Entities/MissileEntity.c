/*
 * cMissileEntity, the shots of the weapons, see include/scrap/missileentity.h.
 */
#include <string.h>

#include "scrap/gvar.h"
#include "scrap/console.h"
#include "scrap/crt.h"
#include "scrap/fxcharactions.h"
#include "scrap/fxcountermeasures.h"
#include "scrap/fxvehicle.h"
#include "scrap/missileentity.h"
#include "scrap/net.h"
#include "scrap/scrapcollision.h"
#include "scrap/vehicleentity.h"
#include "scrap/withlifeentity.h"
#include "scrap/staticinit.h"

#define CMISSILEENTITY_VFTABLE (&cMissileEntity_vftable.base.base.base)
static const cMissileEntityVtbl cMissileEntity_vftable;
#define MISSILE_CLASSNAME "Missile"

#define MISSILE_PI_2 0x1.921fb6p+0f				/* pi / 2 as float (0x3fc90fdb) */

#define Debris_Next(d) (((cDebris *)(d))->next)

static cGVar GVar_FXDumbMotorFile;

/* the debris systems of the fire effects (SetEmbeddedFX) */
#define g_MissileFXDumbMotor (GVar_FXDumbMotorFile.value.s)
static cGVar GVar_FXGearMissileFile;
#define g_MissileFXGearMissile (GVar_FXGearMissileFile.value.s)
static cGVar GVar_CloudFile;
#define g_MissileFXCloud (GVar_CloudFile.value.s)
static cGVar GVar_EMIFile;
#define g_MissileFXEMI (GVar_EMIFile.value.s)

#define MISSILE_ENTLIST 8			/* the entity lists the missiles hit */
#define MISSILE_NET_NONE 0xffff			/* no parent or target in the network state */

/* the vtable slots used here */
/* lo + rand / 32767 * range in double, as the original */
static inline float Missile_Rand(double range, double lo) {
	return (float)((double)(crt_rand() & 0x7fff) * RAND_SCALE * range + lo);
}

/* lo + rand / 32767 * (v - lo) with lo = -v, the random spread of the swarm */
static inline float Missile_RandSym(float v) {
	float lo = -v;
	float range = v - Float_Keep(lo);

	return Missile_Rand((double)range, (double)lo);
}

/* OnHit with the missile, its parent and other */
static void Missile_CallHit(cMissileEntity *self, const char *other) {
	cPy_Function_AddParamS(&self->onHit, self->wl.obj.base.name);
	cPy_Function_AddParamS(&self->onHit, self->parent);
	cPy_Function_AddParamS(&self->onHit, other);
}

cEntity *cMissileEntity_ctor(void *mem, const char *name, float x, float y, float z) {
	cMissileEntity *self = mem;

	cWithLifeEntity_ctor(mem, name, x, y, z);
	self->wl.obj.base.vftable = CMISSILEENTITY_VFTABLE;
	cPy_Function_ctor(&self->onBeeDeath);
	cPy_Function_ctor(&self->onHit);
	cSlotController *sc = self->wl.obj.slot;
	self->lifeTime = 1.0f;
	self->speed = 0.0f;
	float v = sc->minDiff;
	sc->time = v;
	sc->interval = v;
	self->parent[0] = 0;
	self->target[0] = 0;
	self->wl.obj.base.mask |= 2;
	self->type = 0;
	self->turnSpeed = MISSILE_PI_2;
	self->swarmSys[0] = 0;
	self->swarmCount = 0;
	self->swarm = NULL;
	self->swarmSpeed = 30000.0f;
	self->hiddenSwarm = 0;
	self->radius = 100.0f;
	self->topRadius = 1000.0f;
	self->radiusGrow = 1000.0f;
	self->netSwarmCount = 0;
	self->swDthTime = 0.0f;
	self->lastPos.x = 0.0f;
	self->lastPos.y = 0.0f;
	self->lastPos.z = 0.0f;
	self->hitType = MISSILE_HIT_HIT;
	self->noWorldHit = 0;
	self->fallOut = 0;
	self->beeTime = 0.0f;
	self->wl.fireAtt = -1.0f;
	self->hideMain = 0;
	for (int i = 0; i < 4; i++) {
		self->hits[i][0] = 0;
	}
	self->beesGather = 1;
	self->beesHit = 0;
	self->level = 0;
	return &self->wl.obj.base;
}

static byte cMissileEntity_Hit(cEntity *elem, int n, float unused);
static void cMissileEntity_dtor(cMissileEntity *self) {
	int n = self->swarmCount;

	self->wl.obj.base.vftable = CMISSILEENTITY_VFTABLE;
	if (n > 0 || self->wl.obj.base.active) {
		cMissileEntity_Hit(&self->wl.obj.base, n + 1, 0);
	}
	cPy_Function_dtor(&self->onHit);
	cPy_Function_dtor(&self->onBeeDeath);
	cWithLifeEntity_dtor(Missile_Ent(self));
}

static cEntity *cMissileEntity_Delete(cEntity *base, uint32_t flags) {
	cMissileEntity *self = (cMissileEntity *)base;
	cMissileEntity_dtor(self);
	if (flags & 1) {
		operator_delete_sized(self, CMISSILEENTITY_SIZE);
	}
	return base;
}

static void cMissileEntity_RotStep(cEntity *base, float dt) {
	cMissileEntity *self = (cMissileEntity *)base;
	self->wl.obj.ang.y = self->wl.obj.rotVel.y * dt + self->wl.obj.ang.y;
	self->wl.obj.ang.z = self->wl.obj.rotVel.z * dt + self->wl.obj.ang.z;
	self->wl.obj.ang.x = self->wl.obj.ang.x + self->wl.obj.rotVel.x * dt;
	Missile_Vtbl(self)->UpdateVel(&self->wl.obj.base);
}

/* the WithLife entities the missiles touched, by name (4 of 0x80) */
static char g_MissileHits[4][0x80];

static void cMissileEntity_Touch(cEntity *base, cEntity *other, const vec3_t *point, const vec3_t *dir) {
	cMissileEntity *self = (cMissileEntity *)base;
	(void)point;
	(void)dir;
	if (!other) {
		return;
	}
	if (self->parent[0] && !strcmp(other->name, self->parent)) {
		return;
	}
	if (!other->vftable->IsType(other, TYEN_WITH_LIFE)) {
		return;
	}
	/* the name into the first free one of the four */
	for (int i = 0; i < 4; i++) {
		char *dst = g_MissileHits[i];
		if (!dst[0]) {
			const char *src = other->name;
			int k = 0;
			while (k < 0x7f && src[k]) {
				dst[k] = src[k];
				k++;
			}
			dst[k] = 0;
			return;
		}
	}
}

static void cMissileEntity_SetHitType(cMissileEntity *self, int type);
static void cMissileEntity_LifeTick(cEntity *elem) {
	cMissileEntity *self = (cMissileEntity *)elem;
	float t = (float)(self->lifeTime - (cTimer_GetDelta(&self->wl.obj.timer)));

	self->lifeTime = t;
	if (0.0f > t) {
		if (self->hitType == MISSILE_HIT_INOUT) {
			cMissileEntity_SetHitType(self, MISSILE_HIT_NONE);
		}
		self->target[0] = 0;
		if (self->wl.obj.base.active) {
			Missile_Vtbl(self)->Hit(&self->wl.obj.base, self->swarmCount + 1, 0);
		}
		cEntity_Del(Missile_Ent(self));
	}
}

static void cMissileEntity_Wobble(cEntity *base, float k) {
	cMissileEntity *self = (cMissileEntity *)base;
	k = k * 0x1.921fb6p-2f;	/* pi / 8 */
	float neg = -k;
	double range = (double)(k - Float_Keep(neg));
	double lo = (double)neg;
	float a = Missile_Rand(range, lo);
	float b = Missile_Rand(range, lo);
	self->wl.obj.rotVel.x = b + self->wl.obj.rotVel.x;
	self->wl.obj.rotVel.y = self->wl.obj.rotVel.y + a;
	self->wl.obj.rotVel.z = self->wl.obj.rotVel.z + 0.0f;
	float lo2 = k * -8.0f;
	int r = crt_rand();
	float hi2 = k * 8.0f;
	float w = (float)((double)(r & 0x7fff) * RAND_SCALE * (double)(hi2 - lo2) + (double)lo2);
	/* the roll only grows */
	if (self->wl.obj.rotVel.z >= 0.0f && w > 0.0f) {
		self->wl.obj.rotVel.z = self->wl.obj.rotVel.z + w;
	}
	if (0.0f >= self->wl.obj.rotVel.z && 0.0f > w) {
		self->wl.obj.rotVel.z = self->wl.obj.rotVel.z + w;
	}
	self->wl.obj.ang.x = self->wl.obj.ang.x + self->wl.obj.rotVel.x;
	self->wl.obj.ang.y = self->wl.obj.rotVel.y + self->wl.obj.ang.y;
	self->wl.obj.ang.z = self->wl.obj.rotVel.z + self->wl.obj.ang.z;
	Missile_Vtbl(self)->UpdateVel(&self->wl.obj.base);
}

static void cMissileEntity_Fall(cEntity *base, float dt) {
	cMissileEntity *self = (cMissileEntity *)base;
	if (self->wl.obj.ang.x > -MISSILE_PI_2) {
		self->wl.obj.ang.x = self->wl.obj.ang.x - dt;
	}
	float z = self->wl.obj.ang.z;
	if (z > 0.0f) {
		self->wl.obj.ang.z = z + dt;
	} else if (0.0f > z) {
		self->wl.obj.ang.z = z - dt;
	}
	float g = -(g_DebrisDefault * dt);
	self->wl.obj.vel.x = self->wl.obj.vel.x + 0.0f;
	self->wl.obj.vel.y = self->wl.obj.vel.y + g;
	self->wl.obj.vel.z = self->wl.obj.vel.z + 0.0f;
}

/* a float variable: set from or get into *d */
static inline void Missile_VarFloat(float *f, double *d, int set) {
	if ((byte)set) {
		*f = (float)*d;
	} else {
		*d = *f;
	}
}

static byte cMissileEntity_VarF(cEntity *base, const char *name, double *d, int set) {
	cMissileEntity *self = (cMissileEntity *)base;
	if (!*name) {
		return 0;
	}
	if (!strcmp(name, "SwDthTime")) {
		/* reversed in the original: a set reads */
		if ((byte)set) {
			*d = self->swDthTime;
		} else {
			self->swDthTime = (float)*d;
		}
		return 1;
	}
	if (!strcmp(name, "RadiusGrow")) {
		Missile_VarFloat(&self->radiusGrow, d, set);
		return 1;
	}
	if (!strcmp(name, "TopRadius")) {
		Missile_VarFloat(&self->topRadius, d, set);
		return 1;
	}
	if (!strcmp(name, "Radius")) {
		Missile_VarFloat(&self->radius, d, set);
		return 1;
	}
	if (!strcmp(name, "SwarmSpeed")) {
		Missile_VarFloat(&self->swarmSpeed, d, set);
		return 1;
	}
	if (!strcmp(name, "TurnSpeed")) {
		if ((byte)set) {
			self->turnSpeed = (float)(*d * M_PI_D / 180.0);
		} else {
			*d = (double)self->turnSpeed * 180.0 / M_PI_D;
		}
		return 1;
	}
	if (!strcmp(name, "LifeTime")) {
		Missile_VarFloat(&self->lifeTime, d, set);
		return 1;
	}
	if (!strcmp(name, "Speed")) {
		if ((byte)set) {
			self->speed = (float)*d;
			Missile_Vtbl(self)->UpdateVel(&self->wl.obj.base);
		} else {
			*d = self->speed;
		}
		return 1;
	}
	return cWithLifeEntity_VarF(Missile_Ent(self), name, d, set);
}

int cMissileEntity_GetSwarmCount(cMissileEntity *self) {
	if (Net_IsNetEntity(Missile_Ent(self))) {
		return self->netSwarmCount;
	}
	return self->swarmCount;
}

static const char *cMissileEntity_GetClassName(cEntity *base) {
	cMissileEntity *self = (cMissileEntity *)base;
	(void)self;
	return MISSILE_CLASSNAME;
}

static void cMissileEntity_UpdateOff(cEntity *base) {
	cMissileEntity *self = (cMissileEntity *)base;
	cObjEntity_UpdateTimer(Missile_Ent(self));
	Missile_Vtbl(self)->LifeTick(&self->wl.obj.base);
	cWithLifeEntity_UpdateOff(Missile_Ent(self));
}

/* a flag variable */
static inline void Missile_VarFlag(byte *flag, int *i, int set) {
	if ((byte)set) {
		*flag = *i != 0;
	} else {
		*i = *flag != 0;
	}
}

static byte cMissileEntity_VarI(cEntity *base, const char *name, int *i, int set) {
	cMissileEntity *self = (cMissileEntity *)base;
	if (!*name) {
		return 0;
	}
	/* the original compares "HideMain" a second time for the model flag: never reached */
	if (!strcmp(name, "HideMain")) {
		if ((byte)set) {
			self->level = *i;
		} else {
			*i = self->level;
		}
		return 1;
	}
	if ((byte)set && !strcmp(name, "AddToSwarm")) {
		Missile_Vtbl(self)->AddToSwarm(&self->wl.obj.base, *i);
		return 1;
	}
	if (!strcmp(name, "nSwarm")) {
		if ((byte)set) {
			Missile_Vtbl(self)->SetSwarm(&self->wl.obj.base, *i);
		} else {
			*i = self->swarmCount;
		}
		return 1;
	}
	if (!(byte)set && !strcmp(name, "NetnSwarm")) {
		*i = Net_IsNetEntity(Missile_Ent(self)) ? self->netSwarmCount : self->swarmCount;
		return 1;
	}
	if (!strcmp(name, "FallOutAtmosphere")) {
		Missile_VarFlag(&self->fallOut, i, set);
		return 1;
	}
	if (!strcmp(name, "HiddenSwarm")) {
		if ((byte)set) {
			cMissileEntity_SetHiddenSwarm(self, *i != 0);
		} else {
			*i = self->hiddenSwarm != 0;
		}
		return 1;
	}
	if (!strcmp(name, "NoWorldHit")) {
		Missile_VarFlag(&self->noWorldHit, i, set);
		return 1;
	}
	return cWithLifeEntity_VarI(Missile_Ent(self), name, i, set);
}

static byte cMissileEntity_IsType(cEntity *base, const char *type) {
	cMissileEntity *self = (cMissileEntity *)base;
	if (!strcmp(type, MISSILE_CLASSNAME)) {
		return 1;
	}
	return cWithLifeEntity_IsType(Missile_Ent(self), type);
}

/* n bees die (hidden swarm: only counted), or the missile explodes when there are not that many; 1 if it did */
static byte cMissileEntity_Hit(cEntity *elem, int n, float unused) {
	cMissileEntity *self = (cMissileEntity *)elem;
	vec3_t tmp;

	(void)unused;
	if (self->hiddenSwarm) {
		int c = self->swarmCount;
		int left = c - n;
		if (left >= 0) {
			/* only counted: the top radius shrinks with them */
			self->swarmCount = left;
			self->topRadius = (float)(left + 1) * self->topRadius / (float)(c + 1);
			return 0;
		}
		if (self->hitType == MISSILE_HIT_HIT) {
			Missile_CallHit(self, self->target);
			cPy_Function_Execute(&self->onHit, 0);
		}
		self->swarmCount = 0;
		Missile_Ent(self)->vftable->SetActive(Missile_Ent(self), 0);
		return 0;
	}
	if (self->swarmCount - n >= (self->hideMain != 0)) {
		for (int k = n; k > 0; k--) {
			int c = self->swarmCount;
			float tr = self->topRadius;
			self->swarmCount = c - 1;
			self->topRadius = tr - tr / (float)(c + 1);
			cDebrisSys *sys = cWorld_GetDebrisSys(g_pWorld, self->swarmSys);
			cDebris *d = self->swarm;
			self->swarm = Debris_Next(d);
			cPy_Function_AddParamS(&self->onBeeDeath, self->wl.obj.base.name);
			cPy_Function_AddParamF(&self->onBeeDeath, d->drop[0].x);
			cPy_Function_AddParamF(&self->onBeeDeath, d->drop[0].y);
			cPy_Function_AddParamF(&self->onBeeDeath, d->drop[0].z);
			cPy_Function_Execute(&self->onBeeDeath, 0);
			d->life = 0.0f;
			cDebrisSys_Drop(sys, d);
		}
		return 0;
	}
	if (self->hitType == MISSILE_HIT_HIT) {
		Missile_CallHit(self, self->target);
		cPy_Function_Execute(&self->onHit, 0);
	}
	if (!self->beesGather && !self->beesHit) {
		self->wl.life = 0;
	}
	const vec3_t *at = &self->wl.obj.base.pos;
	if (self->target[0]) {
		cEntity *t = World_FindEntity(self->target);
		if (t) {
			at = t->vftable->GetPos(t, &tmp);
		}
	}
	Missile_Vtbl(self)->Explode(&self->wl.obj.base, at);
	Missile_Ent(self)->vftable->SetActive(Missile_Ent(self), 0);
	return 1;
}

static void cMissileEntity_OnDeath(cEntity *base) {
	cMissileEntity *self = (cMissileEntity *)base;
	self->target[0] = 0;
	cWithLifeEntity_OnDeath(Missile_Ent(self));
}

static void cMissileEntity_SetActive(cEntity *base, byte on) {
	cMissileEntity *self = (cMissileEntity *)base;
	cWithLifeEntity_SetActive(Missile_Ent(self), on);
	if (self->hideMain && CGameModel_IsLoaded(&self->wl.obj.model)) {
		CGameModel_Disable(&self->wl.obj.model);
	}
}

static void cMissileEntity_SetAng(cEntity *base, const vec3_t *ang) {
	cMissileEntity *self = (cMissileEntity *)base;
	vec3_t n;
	float x = NormAng(ang->x);
	float y = NormAng(ang->y);
	float z = NormAng(ang->z);

	n.x = x;
	n.y = y;
	n.z = z;
	cObjEntity_SetAng(Missile_Ent(self), &n);
}

void cMissileEntity_SetBeesHit(cMissileEntity *self, byte v) {
	self->beesHit = v;
}

void cMissileEntity_SetHiddenSwarm(cMissileEntity *self, byte on) {
	if (!self->swarmCount) {
		self->hiddenSwarm = on;
		return;
	}
	Con_Warnf("WARNING: <%s>.HiddenSwarm can't be modified!\n", self->wl.obj.base.name);
}

void cMissileEntity_SetHideMain(cMissileEntity *self, byte on) {
	self->hideMain = on;
	if (CGameModel_IsLoaded(&self->wl.obj.model)) {
		if (!self->hideMain && self->wl.obj.base.active) {
			CGameModel_Enable(&self->wl.obj.model);
		} else {
			CGameModel_Disable(&self->wl.obj.model);
		}
	}
}

static void cMissileEntity_SetHitType(cMissileEntity *self, int type) {
	if (type == self->hitType) {
		return;
	}
	if (self->hitType == MISSILE_HIT_INOUT) {
		/* the entities it went through get their OnHit when it changes */
		for (int i = 0; i < 4; i++) {
			if (self->hits[i][0]) {
				Missile_CallHit(self, self->hits[i]);
				cPy_Function_AddParamI(&self->onHit, 0);
				cPy_Function_Execute(&self->onHit, 0);
			}
		}
	}
	self->hitType = type;
}

void cMissileEntity_SetLifeTime(cMissileEntity *self, float t) {
	self->lifeTime = t;
}

void cMissileEntity_SetType(cMissileEntity *self, int type) {
	self->type = type;
}

void cMissileEntity_SetCount(cMissileEntity *self, int n) {
	self->swarmCount = n;
}

void cMissileEntity_SetNoWorldHit(cMissileEntity *self, byte on) {
	self->noWorldHit = on;
}

void cMissileEntity_SetParent(cMissileEntity *self, const char *name) {
	Str_Copy(self->parent, name, 0x80);
}

void cMissileEntity_SetRadius(cMissileEntity *self, float r) {
	self->radius = r;
}

void cMissileEntity_SetRadiusGrow(cMissileEntity *self, float r) {
	self->radiusGrow = r;
}

void cMissileEntity_SetSpeed(cMissileEntity *self, float speed) {
	self->speed = speed;
	Missile_Vtbl(self)->UpdateVel(&self->wl.obj.base);
}

void cMissileEntity_SetSwDthTime(cMissileEntity *self, float t) {
	self->swDthTime = t;
}

/* more bees are added, fewer go back to the debris system */
static void cMissileEntity_SetSwarm(cEntity *base, int n) {
	cMissileEntity *self = (cMissileEntity *)base;
	int c = self->swarmCount;

	if (n > c) {
		Missile_Vtbl(self)->AddToSwarm(&self->wl.obj.base, n - c);
		return;
	}
	if (self->hiddenSwarm) {
		self->swarmCount = n;
		return;
	}
	if (n < c && self->swarmSys[0]) {
		cDebrisSys *sys = cWorld_GetDebrisSys(g_pWorld, self->swarmSys);
		if (sys && self->swarm) {
			do {
				cDebris *d = self->swarm;
				self->swarm = Debris_Next(d);
				cDebrisSys_Release(sys, d);
				self->swarmCount--;
			} while (self->swarmCount > n && self->swarm);
		}
	}
}

void cMissileEntity_SetBeesGather(cMissileEntity *self, byte v) {
	self->beesGather = v;
}

void cMissileEntity_SetSwarmSpeed(cMissileEntity *self, float speed) {
	self->swarmSpeed = speed;
}

void cMissileEntity_SetTarget(cMissileEntity *self, const char *name) {
	if (!name) {
		self->target[0] = 0;
		return;
	}
	Str_Copy(self->target, name, 0x80);
}

void cMissileEntity_SetTopRadius(cMissileEntity *self, float r) {
	self->topRadius = r;
}

void cMissileEntity_SetTurnSpeed(cMissileEntity *self, float speed) {
	self->turnSpeed = speed;
}

/* the velocity from the angles and the speed */
static void cMissileEntity_UpdateVel(cEntity *base) {
	cMissileEntity *self = (cMissileEntity *)base;
	float sy, cy, sx, cx;

	Float_SinCos(self->wl.obj.ang.y, &sy, &cy);
	Float_SinCos(self->wl.obj.ang.x - MISSILE_PI_2, &sx, &cx);
	float speed = self->speed;
	self->wl.obj.vel.x = sx * sy * speed;
	self->wl.obj.vel.z = sx * cy * speed;
	self->wl.obj.vel.y = speed * cx;
}

static void cMissileEntity_SetVel(cEntity *base, const vec3_t *vel) {
	cMissileEntity *self = (cMissileEntity *)base;
	vec3_t neg, rot, ang;

	neg.x = -vel->x;
	neg.y = -vel->y;
	neg.z = -vel->z;
	const vec3_t *r = Vec3_GetRotAng(&rot, &neg);
	ang = *r;
	self->speed = (float)__builtin_sqrt((double)(vel->x * vel->x + vel->y * vel->y + vel->z * vel->z));
	Obj_Vtbl(Missile_Ent(self))->SetAng(Missile_Ent(self), &ang);
	Missile_Vtbl(self)->UpdateVel(&self->wl.obj.base);
}

static void cMissileEntity_Update(cEntity *base) {
	cMissileEntity *self = (cMissileEntity *)base;
	int saved = g_MissileUpdating;

	g_MissileUpdating = 1;
	cWithLifeEntity_Update(Missile_Ent(self));
	float dt = cTimer_GetDelta(&self->wl.obj.timer);
	if (!(1e-6f >= dt)) {
		for (cDebris *d = self->swarm; d; d = Debris_Next(d)) {
			cDebris_Step(d, dt);
		}
		Missile_Vtbl(self)->Swarm(&self->wl.obj.base, dt);
		Missile_Vtbl(self)->LifeTick(&self->wl.obj.base);
		/* only here: a frame without time leaves it set */
		g_MissileUpdating = saved;
	}
}

/* the volume of the first sound by the number of bees (the most seen is kept in its ints[0]) */
static void cMissileEntity_ScaleSwarmSound(cEntity *base, float unused) {
	cMissileEntity *self = (cMissileEntity *)base;
	(void)unused;
	cSndItem *snd = PtrArray_Get(&self->wl.obj.snd, 0);
	if (!snd) {
		return;
	}
	if (snd->ints[0] < self->swarmCount) {
		snd->ints[0] = self->swarmCount;
	}
	float f = (float)(self->swarmCount + 1) * snd->vols[0] / (float)(snd->ints[0] + 1);
	snd->vols[1] = f;
	if (snd->times[0] > snd->time) {
		snd->volume = snd->time * f / snd->times[0];
	} else {
		snd->volume = f;
	}
}

static byte cMissileEntity_VarS(cEntity *base, const char *name, char **s, int set) {
	cMissileEntity *self = (cMissileEntity *)base;
	static const char *const hitTypes[4] = { "None", "Hit", "InOut", "Inside" };
	static const char *const types[4] = { "Linear", "Dumb", "Cloud", "Fall" };

	if (!*name) {
		return 0;
	}
	if (!strcmp(name, "OnBeeDeath")) {
		if ((byte)set) {
			cPy_Function_SetFunc(&self->onBeeDeath, *s);
		} else {
			*s = "<OnBeeDeath> function";
		}
		return 1;
	}
	if (!strcmp(name, "OnHit")) {
		if ((byte)set) {
			cPy_Function_SetFunc(&self->onHit, *s);
		} else {
			*s = "<OnHit> function";
		}
		return 1;
	}
	if (!strcmp(name, "HitType")) {
		if ((byte)set) {
			/* in the order None, Hit, InOut, Inside */
			for (int i = 0; i < 4; i++) {
				if (!crt_stricmp(*s, hitTypes[i])) {
					cMissileEntity_SetHitType(self, i);
					return 1;
				}
			}
			return 0;
		}
		if ((uint32_t)self->hitType > 3) {
			return 0;
		}
		char *const hitTypeStr[4] = { "None", "Hit",
			"InOut", "Inside" };
		*s = hitTypeStr[self->hitType];
		return 1;
	}
	if (!strcmp(name, "Type")) {
		if ((byte)set) {
			/* the original tries Linear, Cloud, Dumb, Fall */
			static const int order[4] = { 0, 2, 1, 3 };
			for (int k = 0; k < 4; k++) {
				if (!crt_stricmp(*s, types[order[k]])) {
					self->type = order[k];
					return 1;
				}
			}
			return 0;
		}
		if ((uint32_t)self->type > 3) {
			return 0;
		}
		char *const typeStr[4] = { "Linear", "Dumb",
			"Cloud", "Fall" };
		*s = typeStr[self->type];
		return 1;
	}
	if (!strcmp(name, "Parent")) {
		if ((byte)set) {
			Str_Copy(self->parent, *s, 0x80);
		} else {
			*s = self->parent;
		}
		return 1;
	}
	if (!strcmp(name, "Target")) {
		if ((byte)set) {
			Str_Copy(self->target, *s, 0x80);
		} else {
			*s = self->target;
		}
		return 1;
	}
	return cWithLifeEntity_VarS(Missile_Ent(self), name, s, set);
}

static byte cMissileEntity_VarV(cEntity *base, const char *name, double *x, double *y, double *z, int set) {
	cMissileEntity *self = (cMissileEntity *)base;
	if (!*name) {
		return 0;
	}
	return cWithLifeEntity_VarV(Missile_Ent(self), name, x, y, z, set);
}

/* vf12: the model, its debris system of the bees has the same name */
static void cMissileEntity_SetModel(cEntity *elem, const char *file, float sx, float sy, float sz, int flag) {
	cMissileEntity *self = (cMissileEntity *)elem;
	vec3_t scale, root, link, rot, pos, cam;

	(void)flag;
	if (!file || !*file) {
		return;
	}
	scale.x = sx;
	scale.y = sy;
	scale.z = sz;
	CGameModel_LoadScaled(&self->wl.obj.model, file, &scale);
	Str_Copy(self->swarmSys, file, 0x80);
	CGameModel_SetPivotNode(&self->wl.obj.model, "DC_Root");
	int il = CGameModel_FindRenderNode(&self->wl.obj.model, "DC_Ship_Link");
	int ir = CGameModel_FindRenderNode(&self->wl.obj.model, "DC_Root");
	if (il != -1 && ir != -1) {
		/* the link node goes to the position */
		const vec3_t *r = CGameModel_NodePos(&self->wl.obj.model, &root, (uint32_t)ir, 0);
		const vec3_t *l = CGameModel_NodePos(&self->wl.obj.model, &link, (uint32_t)il, 0);
		scale.y = l->y - r->y;
		scale.z = l->z - r->z;
		scale.x = l->x - r->x;
		const vec3_t *d = Vec3_Rotate(&rot, &scale, &self->wl.obj.ang);
		pos.y = self->wl.obj.base.pos.y - d->y;
		pos.z = self->wl.obj.base.pos.z - d->z;
		pos.x = self->wl.obj.base.pos.x - d->x;
		Missile_Ent(self)->vftable->SetPos(Missile_Ent(self), &pos);
	}
	CGameModel_SetPos(&self->wl.obj.model, &self->wl.obj.base.pos);
	CGameModel_SetRot(&self->wl.obj.model, &self->wl.obj.ang);
	int ic = CGameModel_FindRenderNode(&self->wl.obj.model, "DC_Camera");
	if (ic != -1) {
		WLife(self)->obj.camOff = *CGameModel_NodePos(&self->wl.obj.model, &cam, (uint32_t)ic, 0);
	}
	cMissileEntity_SetHideMain(self, self->hideMain);
}

/* a new element of the debris system of an effect, with the life time (plus 1.5) as its speed */
static cDebris *Missile_FXDebris(cMissileEntity *self, const char *sysName, byte extra) {
	cDebrisSys *sys = cWorld_GetDebrisSys(g_pWorld, sysName);
	cDebris *d = cDebrisSys_NewElement(sys);

	d->life = extra ? self->lifeTime + 1.5f : self->lifeTime;
	cDebrisSys_Drop(sys, d);
	return d;
}

/* vf15: the fire effects DumbMotor, GearMissile, Cloud and EMI (a debris element of the life time) */
static void cMissileEntity_SetEmbeddedFX(cEntity *elem, const char *name) {
	cMissileEntity *self = (cMissileEntity *)elem;
	if (self->wl.obj.base.deadNode.target) {
		return;
	}
	if (!strcmp(name, "DumbMotor")) {
		cObjElementFXFiredEntity *el = Debris_Element(Missile_FXDebris(self, g_MissileFXDumbMotor, 1));
		if (!el) {
			return;
		}
		if (!el->on) {
			el->on = 1;
		}
		cObjElementFXFiredEntity_SetEntity(el, Missile_Ent(self));
		return;
	}
	if (!strcmp(name, "GearMissile")) {
		cDebris *d = Missile_FXDebris(self, g_MissileFXGearMissile, 0);
		cObjElementFXGearMissile *el = Debris_Element(d);
		if (!el) {
			return;
		}
		if (!el->started) {
			el->started = 1;
		}
		((cObjElementFXGearMissile *)el)->entity = (cEntity *)self;
		cObjElementFXGearMissile_Run(el, d);
		return;
	}
	if (!strcmp(name, "Cloud")) {
		cDebris *d = Missile_FXDebris(self, g_MissileFXCloud, 1);
		cObjElementFXDebrisCloud *el = Debris_Element(d);
		if (!el) {
			return;
		}
		if (!el->on) {
			el->on = 1;
		}
		cObjElementFXDebrisCloud_SetEntity(el, Missile_Ent(self));
		cObjElementFXDebrisCloud_Warm(el, d);
		return;
	}
	if (!strcmp(name, "EMI")) {
		cObjElementFXEMI *el = Debris_Element(Missile_FXDebris(self, g_MissileFXEMI, 0));
		if (!el) {
			return;
		}
		if (!el->on) {
			el->on = 1;
		}
		cObjElementFXEMI_SetEntity(el, Missile_Ent(self));
	}
}

/* the turn to the target, at most turnSpeed * dt per angle */
static inline float Missile_TurnStep(float d, float lim) {
	if (d > lim) {
		return lim;
	}
	return Float_MaxSS(-lim, d);
}

/* the flight of a network copy: the state of the server */
static void Missile_NetTick(cMissileEntity *self, float dt) {
	cNetController *net = WLife(self)->obj.net;

	if (net->age == 0.5f) {
		/* a new state: the velocity draws the missile to its position */
		self->wl.obj.ang = net->ang;
		self->wl.obj.vel = net->vel;
		float vy = (net->pos.y - self->wl.obj.base.pos.y) * 2.0f + self->wl.obj.vel.y;
		float vx = self->wl.obj.vel.x + (net->pos.x - self->wl.obj.base.pos.x) * 2.0f;
		float vz = (net->pos.z - self->wl.obj.base.pos.z) * 2.0f + self->wl.obj.vel.z;
		self->wl.obj.vel.y = vy;
		self->wl.obj.vel.x = vx;
		self->wl.obj.vel.z = vz;
		self->speed = (float)__builtin_sqrt((double)(vx * vx + vy * vy + vz * vz));
		net->age = net->age - dt;
		if (self->netSwarmCount < self->swarmCount && self->wl.obj.base.active) {
			self->wl.obj.base.pos = net->pos;
			Missile_Vtbl(self)->Hit(&self->wl.obj.base, self->swarmCount - self->netSwarmCount, dt);
			return;
		}
		self->wl.obj.base.pos.x = self->wl.obj.vel.x * dt + self->wl.obj.base.pos.x;
		self->wl.obj.base.pos.y = self->wl.obj.vel.y * dt + self->wl.obj.base.pos.y;
		self->wl.obj.base.pos.z = self->wl.obj.vel.z * dt + self->wl.obj.base.pos.z;
		return;
	}
	self->wl.obj.base.pos.x = self->wl.obj.base.pos.x + self->wl.obj.vel.x * dt;
	self->wl.obj.base.pos.y = self->wl.obj.vel.y * dt + self->wl.obj.base.pos.y;
	self->wl.obj.base.pos.z = self->wl.obj.vel.z * dt + self->wl.obj.base.pos.z;
	float age = net->age;
	if (age > 0.0f) {
		net->age = age - dt;
		return;
	}
	self->wl.obj.vel.x = 0.0f;
	self->wl.obj.vel.y = 0.0f;
	self->wl.obj.vel.z = 0.0f;
	float x = self->wl.obj.vel.x;
	float y = self->wl.obj.vel.y;
	float z = self->wl.obj.vel.z;
	self->speed = (float)__builtin_sqrt((double)(x * x + y * y + z * z));
}

/* the turn to the living target (or the target is dropped when it is gone) */
static void Missile_FollowTarget(cMissileEntity *self, float dt) {
	vec3_t tmp, tmp2, dir, rot;
	cEntity *t = World_FindEntity(self->target);

	if (!t) {
		self->target[0] = 0;
		return;
	}
	cEntity *wl = t->vftable->IsType(t, TYEN_WITH_LIFE) ? t : NULL;
	if (wl && WLife(wl)->life <= 0) {
		return;
	}
	const vec3_t *tp = t->vftable->GetPos(t, &tmp);
	const vec3_t *mp = Missile_Ent(self)->vftable->GetPos(Missile_Ent(self), &tmp2);
	dir.x = mp->x - tp->x;
	dir.y = mp->y - tp->y;
	dir.z = mp->z - tp->z;
	float lim = self->turnSpeed * dt;
	const vec3_t *r = Vec3_GetRotAng(&rot, &dir);
	float dx = NormAng(r->x - self->wl.obj.ang.x);
	float dy = NormAng(r->y - self->wl.obj.ang.y);
	float cx = Missile_TurnStep(dx, lim);
	float cy = Missile_TurnStep(dy, lim);
	self->wl.obj.ang.x = self->wl.obj.ang.x + cx;
	self->wl.obj.ang.y = self->wl.obj.ang.y + cy;
	self->wl.obj.ang.z = self->wl.obj.ang.z + 0.0f;
	Missile_Vtbl(self)->UpdateVel(&self->wl.obj.base);
}

/* InOut: OnHit (out) for the entities the missile left, OnHit (in) for the new ones in the free slots */
static void Missile_InOut(cMissileEntity *self, const vec3_t *old, const vec3_t *pos) {
	for (int k = 0; k < 4; k++) {
		g_MissileHits[k][0] = 0;
	}
	/* the Touch of the entities in the sphere fills g_MissileHits */
	cWorld_TestSphereEntities(g_pWorld, Missile_Ent(self), MISSILE_ENTLIST, old, self->radius, NULL, 0,
		pos);
	for (int i = 0; i < 4; i++) {
		char *h = self->hits[i];
		if (!h[0]) {
			continue;
		}
		int k = 0;
		while (k < 4 && strcmp(g_MissileHits[k], h)) {
			k++;
		}
		if (k == 4) {
			Missile_CallHit(self, h);
			cPy_Function_AddParamI(&self->onHit, 0);
			cPy_Function_Execute(&self->onHit, 0);
			h[0] = 0;
		}
	}
	for (int k = 0; k < 4; k++) {
		const char *g = g_MissileHits[k];
		if (!g[0]) {
			continue;
		}
		/* the last free slot */
		int slot = -1;
		int i;
		for (i = 0; i < 4; i++) {
			if (!strcmp(g, self->hits[i])) {
				break;
			}
			if (!self->hits[i][0]) {
				slot = i;
			}
		}
		if (i < 4 || slot == -1) {
			continue;
		}
		Missile_CallHit(self, g);
		cPy_Function_AddParamI(&self->onHit, 1);
		cPy_Function_Execute(&self->onHit, 0);
		/* both are 0x80 bytes, the names of Touch are shorter */
		char *dst = self->hits[slot];
		int c = 0;
		do {
			dst[c] = g[c];
		} while (g[c++]);
	}
}

/* the flight of the server or of a local game; 1 when a hit ended the tick */
static byte Missile_LocalTick(cMissileEntity *self, float dt) {
	vec3_t old, hit, tmp;
	vec3_t *pos = &self->wl.obj.base.pos;

	float n = (float)self->swarmCount * self->swDthTime;
	if (n > self->lifeTime && self->swarmCount > (self->hideMain != 0)) {
		Missile_Vtbl(self)->Hit(&self->wl.obj.base, 1, dt);
	}
	if (self->fallOut && Missile_Ent(self)->vftable->GetPos(Missile_Ent(self), &tmp)->y > g_AtmosphereLimit) {
		self->fallOut = 0;
		self->type = 3;
	}
	switch (self->type) {
	case 1:
		Missile_Vtbl(self)->Wobble(&self->wl.obj.base, dt);
		break;
	case 2:
		Missile_Vtbl(self)->RotStep(&self->wl.obj.base, dt);
		break;
	case 3:
		Missile_Vtbl(self)->Fall(&self->wl.obj.base, dt);
		break;
	}
	old = *pos;
	pos->y = self->wl.obj.vel.y * dt + pos->y;
	pos->z = self->wl.obj.vel.z * dt + pos->z;
	pos->x = pos->x + self->wl.obj.vel.x * dt;
	if (self->target[0]) {
		Missile_FollowTarget(self, dt);
	}
	byte hitWorld = Engine_TestLine(&old, pos, &hit, NULL);
	if (hitWorld) {
		*pos = hit;
	}
	if (!self->noWorldHit && Engine_TestSphere(&old, pos, self->radius, hitWorld ? pos : NULL, NULL)) {
		if (self->swarmCount <= 0) {
			if (self->hitType == MISSILE_HIT_INOUT) {
				cMissileEntity_SetHitType(self, MISSILE_HIT_NONE);
			}
			self->target[0] = 0;
		}
		if (!hitWorld) {
			int k = self->beesHit ? self->swarmCount + 1 : 1;
			if (Missile_Vtbl(self)->Hit(&self->wl.obj.base, k, dt)) {
				return 1;
			}
		}
	}
	cEntity *found = NULL;
	switch (self->hitType) {
	case MISSILE_HIT_HIT:
		cWorld_AddIgnore(g_pWorld, World_FindEntity(self->parent));
		if (cWorld_TestSphereEntities(g_pWorld, Missile_Ent(self), MISSILE_ENTLIST, &old, self->radius,
			&found, 1, pos)) {
			Str_Copy(self->target, found->name, 0x80);
			if (Missile_Vtbl(self)->Hit(&self->wl.obj.base, self->swarmCount + 1, dt)) {
				return 1;
			}
		}
		break;
	case MISSILE_HIT_INOUT:
		Missile_InOut(self, &old, pos);
		break;
	case MISSILE_HIT_INSIDE:
		if (cWorld_TestSphereEntities(g_pWorld, Missile_Ent(self), MISSILE_ENTLIST, &old, self->radius,
			&found, 1, pos)) {
			Missile_CallHit(self, found->name);
			cPy_Function_Execute(&self->onHit, 0);
		}
		break;
	}
	if (hitWorld) {
		if (self->hitType == MISSILE_HIT_INOUT) {
			cMissileEntity_SetHitType(self, MISSILE_HIT_NONE);
		}
		self->target[0] = 0;
		if (Missile_Vtbl(self)->Hit(&self->wl.obj.base, self->swarmCount + 1, dt)) {
			return 1;
		}
	}
	return 0;
}

/* vf23 (every slot): the flight, the turn to the target and the hits of the world and the entities */
static void cMissileEntity_Tick(cEntity *base, float dt) {
	cMissileEntity *self = (cMissileEntity *)base;
	if (Net_IsNetEntity(Missile_Ent(self))) {
		Missile_NetTick(self, dt);
	} else if (Missile_LocalTick(self, dt)) {
		return;
	}
	float r = self->radiusGrow * dt + self->radius;
	self->radius = r;
	if (r > self->topRadius) {
		self->radius = self->topRadius;
		self->radiusGrow = 0.0f;
	}
	Missile_Vtbl(self)->ScaleSwarmSound(&self->wl.obj.base, dt);
	cWithLifeEntity_Tick(Missile_Ent(self), dt);
}

/* the network id of the player entity of that name, 0xffff for none */
static inline uint32_t Missile_NetId(const char *name) {
	int id = Net_GetPlayerIndex(name);

	return id == -1 ? MISSILE_NET_NONE : (uint32_t)(uint16_t)id;
}

/* vf41, vf43: the parent or target and the bees in the network state */
static void cMissileEntity_NetWrite(cEntity *elem, float dist2, void *msg, int full) {
	cMissileEntity *self = (cMissileEntity *)elem;
	cAbstractFile *f = msg;
	cNetController *net = WLife(self)->obj.net;
	byte active = self->wl.obj.base.active;
	byte flags = (byte)((active ? 0 : 0x80) | ((25000000.0f > dist2) + 0x1e));
	uint32_t v;

	net->vftable->Write(net, flags, Missile_Ent(self), msg, full);
	if (!active) {
		v = Missile_NetId(self->target);
		f->vftable->Write(f, &v, 2);
		return;
	}
	v = Missile_NetId(self->parent);
	f->vftable->Write(f, &v, 2);
	v = (byte)(((byte)self->level << 6) | ((byte)self->swarmCount & 0x3f));
	f->vftable->Write(f, &v, 1);
	/* the fire of the Napalm: the delay per life point in 1/100 s */
	float t = WLife(self)->fireDelay;
	if (0.01f > t || 0.01f > WLife(self)->fireRemain) {
		v = 0;
	} else {
		v = (byte)(int)(t * 100.0f);
	}
	f->vftable->Write(f, &v, 1);
}

static void cMissileEntity_NetRead(cEntity *base, void *msg, int isNew) {
	cMissileEntity *self = (cMissileEntity *)base;
	cAbstractFile *f = msg;
	uint16_t id = 0;
	byte b = 0;

	WLife(self)->obj.net->vftable->Read(WLife(self)->obj.net, msg);
	if ((byte)isNew) {
		Obj_Vtbl(Missile_Ent(self))->SetAng(Missile_Ent(self), &WLife(self)->obj.net->ang);
		Missile_Ent(self)->vftable->SetPos(Missile_Ent(self), &WLife(self)->obj.net->pos);
	}
	if ((int8_t)WLife(self)->obj.net->flags < 0) {
		/* inactive: only the target */
		self->netSwarmCount = -1;
		f->vftable->Read(f, &id, 2);
		if (id == MISSILE_NET_NONE) {
			self->target[0] = 0;
			return;
		}
		Str_PrintfQuiet(self->target, 0x80, "net%d", (uint32_t)id);
		return;
	}
	f->vftable->Read(f, &id, 2);
	if (id == MISSILE_NET_NONE) {
		self->parent[0] = 0;
	} else {
		Str_PrintfQuiet(self->parent, 0x80, "net%d", (uint32_t)id);
	}
	f->vftable->Read(f, &b, 1);
	self->level = b >> 6;
	self->netSwarmCount = b & 0x3f;
	f->vftable->Read(f, &b, 1);
	WLife(self)->fireDelay = (float)b / 100.0f;
	WLife(self)->fireRemain = b ? 10000.0f : 0.0f;
}

/* a bee inside the radius: a random push every 0.05 s of beeTime */
static void Missile_BeePush(cMissileEntity *self, cDebris *d, float dt, float sp) {
	float t = self->beeTime + dt;

	self->beeTime = t;
	if (!((double)t > 0.05)) {
		return;
	}
	float lo = -sp;
	float range = sp - Float_Keep(lo);
	do {
		float rz = Missile_Rand((double)range, (double)lo);
		float ry = Missile_Rand((double)range, (double)lo);
		float rx = Missile_Rand((double)range, (double)lo);
		d->drop[2].x = d->drop[2].x + rx * 0.025f;
		d->drop[2].y = d->drop[2].y + ry * 0.025f;
		d->drop[2].z = d->drop[2].z + rz * 0.025f;
		self->beeTime = self->beeTime - 0.05f;
	} while ((double)self->beeTime > 0.05);
}

/* vf59: the bees fly around the missile within the radius */
static void cMissileEntity_Swarm(cEntity *base, float dt) {
	cMissileEntity *self = (cMissileEntity *)base;
	vec3_t tmp, view, a, n;
	float r2 = self->radius * self->radius;
	float sp = self->swarmSpeed + self->speed;
	cDebris *d = self->swarm;

	if (self->lastPos.x == 0.0f && self->lastPos.y == 0.0f && self->lastPos.z == 0.0f) {
		self->lastPos = *Obj_Vtbl(Missile_Ent(self))->GetViewPos(Missile_Ent(self), &tmp);
	}
	/* the bees move with the missile */
	const vec3_t *p = Obj_Vtbl(Missile_Ent(self))->GetViewPos(Missile_Ent(self), &tmp);
	float mx = p->x - self->lastPos.x;
	float my = p->y - self->lastPos.y;
	float mz = p->z - self->lastPos.z;
	for (; d; d = Debris_Next(d)) {
		d->drop[0].z = d->drop[2].z * dt + mz + d->drop[0].z;
		d->drop[0].x = d->drop[2].x * dt + mx + d->drop[0].x;
		d->drop[0].y = d->drop[2].y * dt + my + d->drop[0].y;
		d->drop[1].x = d->drop[1].x + dt * d->drop[3].x;
		d->drop[1].y = d->drop[3].y * dt + d->drop[1].y;
		d->drop[1].z = d->drop[3].z * dt + d->drop[1].z;
		d->drop[1].x = NormAng(d->drop[1].x);
		d->drop[1].y = NormAng(d->drop[1].y);
		p = Obj_Vtbl(Missile_Ent(self))->GetViewPos(Missile_Ent(self), &view);
		float dy = p->y - d->drop[0].y;
		float dx = p->x - d->drop[0].x;
		float dz = p->z - d->drop[0].z;
		float q = dy * dy + dx * dx + dz * dz;
		if (r2 > q) {
			Missile_BeePush(self, d, dt, sp);
		} else {
			/* outside: back onto the sphere, flying in */
			float inv = 1.0f / (float)__builtin_sqrt((double)q);
			float nx = dx * inv;
			float ny = dy * inv;
			float nz = dz * inv;
			float rad = self->radius;
			float ox = nx * rad;
			float oy = ny * rad;
			float oz = nz * rad;
			p = Obj_Vtbl(Missile_Ent(self))->GetViewPos(Missile_Ent(self), &view);
			d->drop[0].x = p->x - ox;
			d->drop[0].y = p->y - oy;
			d->drop[0].z = p->z - oz;
			d->drop[2].x = nx * sp * dt;
			d->drop[2].y = ny * sp * dt;
			d->drop[2].z = nz * sp * dt;
		}
		if (self->type != 2) {
			/* turned to the angles of the missile within the step */
			const vec3_t *va = Obj_Vtbl(Missile_Ent(self))->GetViewAng(Missile_Ent(self), &view);
			a.y = va->y - d->drop[1].y;
			a.x = va->x - d->drop[1].x;
			a.z = va->z - d->drop[1].z;
			const vec3_t *na = Vec3_NormAng(&n, &a);
			float inv = 1.0f / dt;
			d->drop[3] = *na;
			d->drop[3].x = d->drop[3].x * inv;
			d->drop[3].y = d->drop[3].y * inv;
			d->drop[3].z = d->drop[3].z * inv;
		}
	}
	self->lastPos = *Obj_Vtbl(Missile_Ent(self))->GetViewPos(Missile_Ent(self), &tmp);
}

vec3_t g_DebrisDrop[4];

/* vf64: n new bees */
static void cMissileEntity_AddToSwarm(cEntity *base, int n) {
	cMissileEntity *self = (cMissileEntity *)base;
	if (!self->hiddenSwarm) {
		cDebrisSys *sys = cWorld_GetDebrisSys(g_pWorld, self->swarmSys);
		float ax = NormAng(self->wl.obj.ang.x);
		float ay = NormAng(self->wl.obj.ang.y);
		if (sys && n > 0) {
			for (int k = n; k; k--) {
				cDebris *prev = self->swarm;
				cDebris *d = cDebrisSys_NewElement(sys);
				self->swarm = d;
				Debris_Next(d) = prev;
				d = self->swarm;
				d->drop[0].x = g_DebrisDrop[0].x;
				d->drop[0].y = g_DebrisDrop[0].y;
				d->drop[0].z = g_DebrisDrop[0].z;
				d->drop[1].x = ax;
				d->drop[1].y = ay;
				d->drop[1].z = 0.0f;
				d->drop[2].x = 0.0f;
				d->drop[2].y = 0.0f;
				d->drop[2].z = 0.0f;
				if (self->type != 2) {
					d->drop[3].x = 0.0f;
					d->drop[3].y = 0.0f;
					d->drop[3].z = 0.0f;
				} else {
					float rz = Missile_RandSym(g_DebrisDrop[3].z);
					float ry = Missile_RandSym(g_DebrisDrop[3].y);
					float rx = Missile_RandSym(g_DebrisDrop[3].x);
					d->drop[3].x = rx;
					d->drop[3].y = ry;
					d->drop[3].z = rz;
				}
				cDebris_Step(self->swarm, 0.0f);
				/* the debris starts with its model */
				CGameModel_Enable((CGameModel *)self->swarm);
			}
		}
	}
	self->swarmCount += n;
	if (self->type == 2) {
		float rz = Missile_RandSym(g_DebrisDrop[3].z);
		float ry = Missile_RandSym(g_DebrisDrop[3].y);
		float rx = Missile_RandSym(g_DebrisDrop[3].x);
		self->wl.obj.rotVel.x = rx;
		self->wl.obj.rotVel.y = ry;
		self->wl.obj.rotVel.z = rz;
	}
}

/* vf66: the bees fly to at (beesGather) or die where they are */
static void cMissileEntity_Explode(cEntity *elem, const vec3_t *at) {
	cMissileEntity *self = (cMissileEntity *)elem;
	vec3_t neg, rot;
	cDebrisSys *sys = cWorld_GetDebrisSys(g_pWorld, self->swarmSys);

	if (sys) {
		while (self->swarm) {
			cDebris *d = self->swarm;
			float vy = d->drop[2].y + self->wl.obj.vel.y;
			float vx = d->drop[2].x + self->wl.obj.vel.x;
			float vz = d->drop[2].z + self->wl.obj.vel.z;
			float sp = (float)__builtin_sqrt((double)(vy * vy + vx * vx + vz * vz));
			if (!(sp > 1.0f)) {
				self->swarm = Debris_Next(d);
				cDebrisSys_Release(sys, d);
				continue;
			}
			float ax = at->x;
			float ay = at->y;
			float az = at->z;
			float ey = ay - d->drop[0].y;
			float ex = ax - d->drop[0].x;
			float ez = az - d->drop[0].z;
			float dist = (float)__builtin_sqrt((double)(ey * ey + ex * ex + ez * ez));
			/* the time to get there */
			float t = (dist + dist) / sp;
			if (0.01 > (double)t) {
				t = 0.01f;
			}
			if (self->beesGather) {
				float k = 1.0f / t;
				d->drop[2].x = (ax - d->drop[0].x) * k;
				d->drop[2].y = (ay - d->drop[0].y) * k;
				d->drop[2].z = (az - d->drop[0].z) * k;
				cDebris *s = self->swarm;
				neg.y = -s->drop[2].y;
				neg.x = -s->drop[2].x;
				neg.z = -s->drop[2].z;
				s->drop[3] = *Vec3_GetRotAng(&rot, &neg);
				s = self->swarm;
				s->drop[3].z = s->drop[3].z - s->drop[1].z;
				s->drop[3].x = s->drop[3].x - s->drop[1].x;
				s->drop[3].y = s->drop[3].y - s->drop[1].y;
				self->swarm->drop[3].x = NormAng(self->swarm->drop[3].x);
				self->swarm->drop[3].y = NormAng(self->swarm->drop[3].y);
				s = self->swarm;
				s->drop[3].x = k * s->drop[3].x;
				s->drop[3].y = k * s->drop[3].y;
				s->drop[3].z = k * s->drop[3].z;
				self->swarm->life = t;
			} else {
				d->life = 0.0f;
				if (!self->beesHit) {
					cPy_Function_AddParamS(&self->onBeeDeath, self->wl.obj.base.name);
					cPy_Function_AddParamF(&self->onBeeDeath, d->drop[0].x);
					cPy_Function_AddParamF(&self->onBeeDeath, d->drop[0].y);
					cPy_Function_AddParamF(&self->onBeeDeath, d->drop[0].z);
					cPy_Function_Execute(&self->onBeeDeath, 0);
					d->life = 0.0f;
				}
			}
			self->swarm = Debris_Next(self->swarm);
			cDebrisSys_Drop(sys, d);
		}
	}
	self->swarmCount = 0;
}

/* the vtable of cMissileEntity */
static const cMissileEntityVtbl cMissileEntity_vftable = {
	.base.base.base.Update = cMissileEntity_Update,
	.base.base.base.Render = cEntity_Render,
	.base.base.base.UpdateOff = cMissileEntity_UpdateOff,
	.base.base.base.Delete = cMissileEntity_Delete,
	.base.base.base.GetClassName = cMissileEntity_GetClassName,
	.base.base.base.IsType = cMissileEntity_IsType,
	.base.base.base.GetPos = cObjEntity_GetPos,
	.base.base.base.SetPos = cWithLifeEntity_SetPos,
	.base.base.base.SetActive = cMissileEntity_SetActive,
	.base.base.base.TestLine = cWithLifeEntity_TestLine,
	.base.base.base.TestSphere = cWithLifeEntity_TestSphere,
	.base.base.base.Touch = cMissileEntity_Touch,
	.base.base.base.SetModel = cMissileEntity_SetModel,
	.base.base.base.GetTargetAngle = cObjEntity_GetTargetAngle,
	.base.base.base.AimWeapon = cEntity_AimWeapon,
	.base.base.base.SetEmbeddedFX = cMissileEntity_SetEmbeddedFX,
	.base.base.base.VarI = cMissileEntity_VarI,
	.base.base.base.VarF = cMissileEntity_VarF,
	.base.base.base.VarS = cMissileEntity_VarS,
	.base.base.base.VarV = cMissileEntity_VarV,
	.base.base.base.Vf20 = cEntity_Vf20,
	.base.base.base.Vf21 = cEntity_Vf21,
	.base.base.base.GetAI = cEntity_GetAI,
	.base.base.Tick = cMissileEntity_Tick,
	.base.base.UpdateModel = cObjEntity_UpdateModel,
	.base.base.GetNodeSlot = cWithLifeEntity_GetNodeSlot,
	.base.base.GetRenderPos = cObjEntity_GetRenderPos,
	.base.base.GetRenderAng = cObjEntity_GetRenderAng,
	.base.base.GetSndAng = cObjEntity_GetSndAng,
	.base.base.GetViewPos = cObjEntity_GetRenderPos,
	.base.base.GetViewAng = cObjEntity_GetRenderAng,
	.base.base.GetSndPos = cWithLifeEntity_GetSndPos,
	.base.base.GetAng = cObjEntity_GetAng,
	.base.base.SetAng = cMissileEntity_SetAng,
	.base.base.GetVel = cObjEntity_GetVel,
	.base.base.SetVel = cMissileEntity_SetVel,
	.base.base.GetRotVel = cObjEntity_GetRotVel,
	.base.base.SetRotVel = cObjEntity_SetRotVel,
	.base.base.GetModel = cObjEntity_GetModel,
	.base.base.GetCamera = cObjEntity_GetCamera,
	.base.base.GetRadius = cWithLifeEntity_GetRadius,
	.base.base.NetWrite = cMissileEntity_NetWrite,
	.base.base.NetNeedsSend = cEntity_NetNeedsSendTrue,
	.base.base.NetRead = cMissileEntity_NetRead,
	.base.base.SetEffect = cObjEntity_SetEffect,
	.base.base.SetEffectParam = cObjEntity_SetEffectParam,
	.base.base.Render = cWithLifeEntity_RenderNow,
	.base.CheckLife = cWithLifeEntity_CheckLife,
	.base.UnlinkListen = cWithLifeEntity_Vf48,
	.base.OnDeath = cMissileEntity_OnDeath,
	.base.OnDamage = cWithLifeEntity_OnDamage,
	.base.SetMass = cWithLifeEntity_SetMass,
	.base.GetAction = cWithLifeEntity_GetAction,
	.base.GetActCtrl = cWithLifeEntity_GetActCtrl,
	.base.SetActCtrl = cWithLifeEntity_SetActCtrl,
	.base.IsEMIed = cWithLifeEntity_IsEMIed,
	.Wobble = cMissileEntity_Wobble,
	.RotStep = cMissileEntity_RotStep,
	.Fall = cMissileEntity_Fall,
	.Swarm = cMissileEntity_Swarm,
	.ScaleSwarmSound = cMissileEntity_ScaleSwarmSound,
	.LifeTick = cMissileEntity_LifeTick,
	.UpdateVel = cMissileEntity_UpdateVel,
	.Hit = cMissileEntity_Hit,
	.AddToSwarm = cMissileEntity_AddToSwarm,
	.SetSwarm = cMissileEntity_SetSwarm,
	.Explode = cMissileEntity_Explode,
};

cGVar GVar_Gravity;
cGVar GVar_ActiveExtrapolation;

/* the dynamic initializers of the globals here, run in the order of src/standalone/crtxc.inc */
FINI_GVAR_STRING(Fini_GVar_FXGearMissileFile, &GVar_FXGearMissileFile)
INIT_GVAR_STRING(Init_GVar_FXGearMissileFile, Fini_GVar_FXGearMissileFile, &GVar_FXGearMissileFile, "FXGearMissileFile", 0, "Default Model for Gear Missile Effect", "Models/GFX/Gear/Missile.M3D")
INIT_GVAR_INT(Init_GVar_ActiveExtrapolation, &GVar_ActiveExtrapolation, "ActiveExtrapolation", 0, "1 Uses Extrapolation if possible, 0 Uses Interpolation allways", 0)
FINI_GVAR_STRING(Fini_GVar_FXDumbMotorFile, &GVar_FXDumbMotorFile)
INIT_GVAR_STRING(Init_GVar_FXDumbMotorFile, Fini_GVar_FXDumbMotorFile, &GVar_FXDumbMotorFile, "FXDumbMotorFile", 0, "Default Model for Dumb Motor Effect", "Models/GFX/DumbMotor.M3D")
FINI_GVAR_STRING(Fini_GVar_CloudFile, &GVar_CloudFile)
INIT_GVAR_STRING(Init_GVar_CloudFile, Fini_GVar_CloudFile, &GVar_CloudFile, "CloudFile", 0, "Default Model for Debris Cloud", "Models/Weapons/Cloud/Cloud.M3D")
FINI_GVAR_STRING(Fini_GVar_EMIFile, &GVar_EMIFile)
INIT_GVAR_STRING(Init_GVar_EMIFile, Fini_GVar_EMIFile, &GVar_EMIFile, "EMIFile", 0, "Default Model for Electro Magnetic Interference", "Models/Weapons/EMI/EMI.M3D")
INIT_GVAR_FLOAT(Init_GVar_Gravity, &GVar_Gravity, "Gravity", 0, "Global Gravity\n", 4900.0f)
