/*
 * cMissileEntity, the shots of the weapons, see include/scrap/missileentity.h.
 */
#include <string.h>

#include "scrap/console.h"
#include "scrap/crt.h"
#include "scrap/fxcountermeasures.h"
#include "scrap/missileentity.h"

#define MISSILE_CLASSNAME ((const char *)Game_Ptr(0x3aea4c))	/* "Missile" */
#define MISSILE_RAND_SCALE 3.051850947599719e-05		/* 1 / 32767 */

#define MISSILE_PI_2 0x1.921fb6p+0f				/* pi / 2 as float (0x3fc90fdb) */

#define Debris_Next(d) (*(cDebris **)((byte *)(d) + 0x50))

/* the vtable slots used here */
typedef struct cMissileEntityVtbl {
	void *vf00_05[6];
	vec3_t *(__thiscall *GetPos)(cMissileEntity *self, vec3_t *out);		/* 0x18 */
	void *vf07;
	void(__thiscall *SetActive)(cMissileEntity *self, byte on);			/* 0x20 */
	void *vf09_32[24];
	void(__thiscall *SetAng)(cMissileEntity *self, const vec3_t *ang);		/* 0x84 */
	void *vf34_58[25];
	void(__thiscall *Swarm)(cMissileEntity *self, float dt);			/* 0xec vf59 */
	void *vf60;
	void(__thiscall *LifeTick)(cMissileEntity *self);				/* 0xf4 */
	void(__thiscall *UpdateVel)(cMissileEntity *self);				/* 0xf8 */
	byte(__thiscall *Hit)(cMissileEntity *self, int n, int unused);		/* 0xfc */
	void(__thiscall *AddToSwarm)(cMissileEntity *self, int n);			/* 0x100 */
	void(__thiscall *SetSwarm)(cMissileEntity *self, int n);			/* 0x104 */
	void(__thiscall *Explode)(cMissileEntity *self, const vec3_t *at);		/* 0x108 */
} cMissileEntityVtbl;
SD_OFFSET(cMissileEntityVtbl, SetAng, 0x84);
SD_OFFSET(cMissileEntityVtbl, Swarm, 0xec);
SD_OFFSET(cMissileEntityVtbl, Explode, 0x108);

typedef struct cWeapon cWeapon;

/* the fields of the first weapon that ScaleWeapon changes */
#define Weap_F40(w) (*(float *)((byte *)(w) + 0x40))
#define Weap_F48(w) (*(float *)((byte *)(w) + 0x48))
#define Weap_F4C(w) (*(float *)((byte *)(w) + 0x4c))
#define Weap_F50(w) (*(float *)((byte *)(w) + 0x50))
#define Weap_F74(w) (*(float *)((byte *)(w) + 0x74))
#define Weap_MaxCount(w) (*(int *)((byte *)(w) + 0x88))

static inline const cMissileEntityVtbl *Missile_Vtbl(cMissileEntity *e) {
	return (const cMissileEntityVtbl *)e->base.vftable;
}

static inline cEntity *Missile_Ent(cMissileEntity *e) {
	return &e->base;
}

/* the x87 a - b of the original (fsubr), in the precision the game set */
static inline float X87_Sub(float a, float b) {
	float r;

	__asm__("fsubrs %1" : "=t"(r) : "m"(a), "0"(b));
	return r;
}

/* lo + rand / 32767 * range in double, as the original */
static inline float Missile_Rand(double range, double lo) {
	return (float)((double)(crt_rand() & 0x7fff) * MISSILE_RAND_SCALE * range + lo);
}

/* OnHit with the missile, its parent and other */
static void Missile_CallHit(cMissileEntity *self, const char *other) {
	cPy_Function_AddParamS(&self->onHit, self->base.name);
	cPy_Function_AddParamS(&self->onHit, self->parent);
	cPy_Function_AddParamS(&self->onHit, other);
}

cEntity *__thiscall cMissileEntity_ctor(void *mem, const char *name, float x, float y, float z) {
	cMissileEntity *self = mem;

	cWithLifeEntity_ctor(mem, name, x, y, z);
	self->base.vftable = Game_Ptr(CMISSILEENTITY_VFTABLE);
	cPy_Function_ctor(&self->onBeeDeath);
	cPy_Function_ctor(&self->onHit);
	byte *p = self->obj140;
	self->lifeTime = 1.0f;
	self->speed = 0.0f;
	float v = *(float *)(p + 0xc);
	*(float *)(p + 0x4) = v;
	*(float *)(p + 0x8) = v;
	self->parent[0] = 0;
	self->target[0] = 0;
	self->base.mask |= 2;
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
	self->v598.x = 0.0f;
	self->v598.y = 0.0f;
	self->v598.z = 0.0f;
	self->hitType = MISSILE_HIT_HIT;
	self->noWorldHit = 0;
	self->fallOut = 0;
	self->i580 = 0;
	self->f28c = -1.0f;
	self->hideMain = 0;
	for (int i = 0; i < 4; i++) {
		self->hits[i][0] = 0;
	}
	self->flag584 = 1;
	self->flag585 = 0;
	self->hideMainVar = 0;
	return &self->base;
}

void __fastcall cMissileEntity_dtor(cMissileEntity *self) {
	int n = self->swarmCount;

	self->base.vftable = Game_Ptr(CMISSILEENTITY_VFTABLE);
	if (n > 0 || self->base.flag_4c) {
		cMissileEntity_Hit(self, n + 1, 0);
	}
	cPy_Function_dtor(&self->onHit);
	cPy_Function_dtor(&self->onBeeDeath);
	cWithLifeEntity_dtor(Missile_Ent(self));
}

cMissileEntity *__thiscall cMissileEntity_Delete(cMissileEntity *self, uint32_t flags) {
	cMissileEntity_dtor(self);
	if (flags & 1) {
		operator_delete_sized(self, CMISSILEENTITY_SIZE);
	}
	return self;
}

void __thiscall cMissileEntity_RotStep(cMissileEntity *self, float dt) {
	self->ang.y = self->rotVel.y * dt + self->ang.y;
	self->ang.z = self->rotVel.z * dt + self->ang.z;
	self->ang.x = self->ang.x + self->rotVel.x * dt;
	Missile_Vtbl(self)->UpdateVel(self);
}

void __thiscall cMissileEntity_Touch(cMissileEntity *self, cEntity *other, const vec3_t *point, const vec3_t *dir) {
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

void __thiscall cMissileEntity_LifeTick(cMissileEntity *self) {
	float t = X87_Sub(self->lifeTime, cTimer_GetDelta(&self->timer));

	self->lifeTime = t;
	if (0.0f > t) {
		if (self->hitType == MISSILE_HIT_INOUT) {
			cMissileEntity_SetHitType(self, MISSILE_HIT_NONE);
		}
		self->target[0] = 0;
		if (self->base.flag_4c) {
			Missile_Vtbl(self)->Hit(self, self->swarmCount + 1, 0);
		}
		cEntity_Del(Missile_Ent(self));
	}
}

void __thiscall cMissileEntity_Wobble(cMissileEntity *self, float k) {
	k = k * 0x1.921fb6p-2f;	/* pi / 8 */
	float neg = -k;
	double range = (double)(k - Float_Keep(neg));
	double lo = (double)neg;
	float a = Missile_Rand(range, lo);
	float b = Missile_Rand(range, lo);
	self->rotVel.x = b + self->rotVel.x;
	self->rotVel.y = self->rotVel.y + a;
	self->rotVel.z = self->rotVel.z + 0.0f;
	float lo2 = k * -8.0f;
	int r = crt_rand();
	float hi2 = k * 8.0f;
	float w = (float)((double)(r & 0x7fff) * MISSILE_RAND_SCALE * (double)(hi2 - lo2) + (double)lo2);
	/* the roll only grows */
	if (self->rotVel.z >= 0.0f && w > 0.0f) {
		self->rotVel.z = self->rotVel.z + w;
	}
	if (0.0f >= self->rotVel.z && 0.0f > w) {
		self->rotVel.z = self->rotVel.z + w;
	}
	self->ang.x = self->ang.x + self->rotVel.x;
	self->ang.y = self->rotVel.y + self->ang.y;
	self->ang.z = self->rotVel.z + self->ang.z;
	Missile_Vtbl(self)->UpdateVel(self);
}

void __thiscall cMissileEntity_Fall(cMissileEntity *self, float dt) {
	if (self->ang.x > -MISSILE_PI_2) {
		self->ang.x = self->ang.x - dt;
	}
	float z = self->ang.z;
	if (z > 0.0f) {
		self->ang.z = z + dt;
	} else if (0.0f > z) {
		self->ang.z = z - dt;
	}
	float g = -(g_DebrisDefault * dt);
	self->vel.x = self->vel.x + 0.0f;
	self->vel.y = self->vel.y + g;
	self->vel.z = self->vel.z + 0.0f;
}

/* a float variable: set from or get into *d */
static inline void Missile_VarFloat(float *f, double *d, int set) {
	if ((byte)set) {
		*f = (float)*d;
	} else {
		*d = *f;
	}
}

byte __thiscall cMissileEntity_VarF(cMissileEntity *self, const char *name, double *d, int set) {
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
			Missile_Vtbl(self)->UpdateVel(self);
		} else {
			*d = self->speed;
		}
		return 1;
	}
	return cWithLifeEntity_VarF(Missile_Ent(self), name, d, set);
}

int __fastcall cMissileEntity_GetSwarmCount(cMissileEntity *self) {
	if (Net_IsNetEntity(Missile_Ent(self))) {
		return self->netSwarmCount;
	}
	return self->swarmCount;
}

const char *__thiscall cMissileEntity_GetClassName(cMissileEntity *self) {
	(void)self;
	return MISSILE_CLASSNAME;
}

void __thiscall cMissileEntity_UpdateOff(cMissileEntity *self) {
	cObjEntity_UpdateTimer(Missile_Ent(self));
	Missile_Vtbl(self)->LifeTick(self);
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

byte __thiscall cMissileEntity_VarI(cMissileEntity *self, const char *name, int *i, int set) {
	if (!*name) {
		return 0;
	}
	/* the original compares "HideMain" a second time for the model flag: never reached */
	if (!strcmp(name, "HideMain")) {
		if ((byte)set) {
			self->hideMainVar = *i;
		} else {
			*i = self->hideMainVar;
		}
		return 1;
	}
	if ((byte)set && !strcmp(name, "AddToSwarm")) {
		Missile_Vtbl(self)->AddToSwarm(self, *i);
		return 1;
	}
	if (!strcmp(name, "nSwarm")) {
		if ((byte)set) {
			Missile_Vtbl(self)->SetSwarm(self, *i);
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

byte __thiscall cMissileEntity_IsType(cMissileEntity *self, const char *type) {
	if (!strcmp(type, MISSILE_CLASSNAME)) {
		return 1;
	}
	return cWithLifeEntity_IsType(Missile_Ent(self), type);
}

byte __thiscall cMissileEntity_Hit(cMissileEntity *self, int n, int unused) {
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
		Missile_Vtbl(self)->SetActive(self, 0);
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
			cPy_Function_AddParamS(&self->onBeeDeath, self->base.name);
			cPy_Function_AddParamF(&self->onBeeDeath, d->drop[0].x);
			cPy_Function_AddParamF(&self->onBeeDeath, d->drop[0].y);
			cPy_Function_AddParamF(&self->onBeeDeath, d->drop[0].z);
			cPy_Function_Execute(&self->onBeeDeath, 0);
			d->speed = 0.0f;
			cDebrisSys_Drop(sys, d);
		}
		return 0;
	}
	if (self->hitType == MISSILE_HIT_HIT) {
		Missile_CallHit(self, self->target);
		cPy_Function_Execute(&self->onHit, 0);
	}
	if (!self->flag584 && !self->flag585) {
		self->life = 0;
	}
	const vec3_t *at = &self->base.pos;
	if (self->target[0]) {
		cEntity *t = World_FindEntity(self->target);
		if (t) {
			at = t->vftable->GetPos(t, &tmp);
		}
	}
	Missile_Vtbl(self)->Explode(self, at);
	Missile_Vtbl(self)->SetActive(self, 0);
	return 1;
}

void __thiscall cMissileEntity_OnDeath(cMissileEntity *self) {
	self->target[0] = 0;
	cWithLifeEntity_OnDeath(Missile_Ent(self));
}

void __thiscall cMissileEntity_SetActive(cMissileEntity *self, byte on) {
	cWithLifeEntity_SetActive(Missile_Ent(self), on);
	if (self->hideMain && CGameModel_IsLoaded(&self->model)) {
		CGameModel_Disable(&self->model);
	}
}

void __thiscall cMissileEntity_SetAng(cMissileEntity *self, const vec3_t *ang) {
	vec3_t n;
	float x = NormAng(ang->x);
	float y = NormAng(ang->y);
	float z = NormAng(ang->z);

	n.x = x;
	n.y = y;
	n.z = z;
	cObjEntity_SetAng(Missile_Ent(self), &n);
}

void __thiscall cMissileEntity_SetFlag585(cMissileEntity *self, byte v) {
	self->flag585 = v;
}

void __thiscall cMissileEntity_SetHiddenSwarm(cMissileEntity *self, byte on) {
	if (!self->swarmCount) {
		self->hiddenSwarm = on;
		return;
	}
	Con_Warnf("WARNING: <%s>.HiddenSwarm can't be modified!\n", self->base.name);
}

void __thiscall cMissileEntity_SetHideMain(cMissileEntity *self, byte on) {
	self->hideMain = on;
	if (CGameModel_IsLoaded(&self->model)) {
		if (!self->hideMain && self->base.flag_4c) {
			CGameModel_Enable(&self->model);
		} else {
			CGameModel_Disable(&self->model);
		}
	}
}

void __thiscall cMissileEntity_SetHitType(cMissileEntity *self, int type) {
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

void __thiscall cMissileEntity_SetLifeTime(cMissileEntity *self, float t) {
	self->lifeTime = t;
}

void __thiscall cMissileEntity_SetType(cMissileEntity *self, int type) {
	self->type = type;
}

void __thiscall cMissileEntity_SetCount(cMissileEntity *self, int n) {
	self->swarmCount = n;
}

void __thiscall cMissileEntity_SetNoWorldHit(cMissileEntity *self, byte on) {
	self->noWorldHit = on;
}

void __thiscall cMissileEntity_SetParent(cMissileEntity *self, const char *name) {
	Str_Copy(self->parent, name, 0x80);
}

void __thiscall cMissileEntity_SetRadius(cMissileEntity *self, float r) {
	self->radius = r;
}

void __thiscall cMissileEntity_SetRadiusGrow(cMissileEntity *self, float r) {
	self->radiusGrow = r;
}

void __thiscall cMissileEntity_SetSpeed(cMissileEntity *self, float speed) {
	self->speed = speed;
	Missile_Vtbl(self)->UpdateVel(self);
}

void __thiscall cMissileEntity_SetSwDthTime(cMissileEntity *self, float t) {
	self->swDthTime = t;
}

void __thiscall cMissileEntity_SetSwarm(cMissileEntity *self, int n) {
	int c = self->swarmCount;

	if (n > c) {
		Missile_Vtbl(self)->AddToSwarm(self, n - c);
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

void __thiscall cMissileEntity_SetFlag584(cMissileEntity *self, byte v) {
	self->flag584 = v;
}

void __thiscall cMissileEntity_SetSwarmSpeed(cMissileEntity *self, float speed) {
	self->swarmSpeed = speed;
}

void __thiscall cMissileEntity_SetTarget(cMissileEntity *self, const char *name) {
	if (!name) {
		self->target[0] = 0;
		return;
	}
	Str_Copy(self->target, name, 0x80);
}

void __thiscall cMissileEntity_SetTopRadius(cMissileEntity *self, float r) {
	self->topRadius = r;
}

void __thiscall cMissileEntity_SetTurnSpeed(cMissileEntity *self, float speed) {
	self->turnSpeed = speed;
}

void __thiscall cMissileEntity_UpdateVel(cMissileEntity *self) {
	float sy, cy, sx, cx;

	X87_SinCos(self->ang.y, &sy, &cy);
	X87_SinCos(self->ang.x - MISSILE_PI_2, &sx, &cx);
	float speed = self->speed;
	self->vel.x = sx * sy * speed;
	self->vel.z = sx * cy * speed;
	self->vel.y = speed * cx;
}

void __thiscall cMissileEntity_SetVel(cMissileEntity *self, const vec3_t *vel) {
	vec3_t neg, rot, ang;

	neg.x = -vel->x;
	neg.y = -vel->y;
	neg.z = -vel->z;
	const vec3_t *r = Vec3_GetRotAng(&rot, &neg);
	ang = *r;
	self->speed = (float)__builtin_sqrt((double)(vel->x * vel->x + vel->y * vel->y + vel->z * vel->z));
	Missile_Vtbl(self)->SetAng(self, &ang);
	Missile_Vtbl(self)->UpdateVel(self);
}

void __thiscall cMissileEntity_Update(cMissileEntity *self) {
	int saved = g_MissileUpdating;

	g_MissileUpdating = 1;
	cWithLifeEntity_Update(Missile_Ent(self));
	float dt = cTimer_GetDelta(&self->timer);
	if (!(1e-6f >= dt)) {
		for (cDebris *d = self->swarm; d; d = Debris_Next(d)) {
			cDebris_Step(d, dt);
		}
		Missile_Vtbl(self)->Swarm(self, dt);
		Missile_Vtbl(self)->LifeTick(self);
		/* only here: a frame without time leaves it set */
		g_MissileUpdating = saved;
	}
}

void __thiscall cMissileEntity_ScaleWeapon(cMissileEntity *self, int unused) {
	(void)unused;
	cWeapon *w = PtrArray_Get(self->weapons, 0);
	if (!w) {
		return;
	}
	if (Weap_MaxCount(w) < self->swarmCount) {
		Weap_MaxCount(w) = self->swarmCount;
	}
	float f = (float)(self->swarmCount + 1) * Weap_F4C(w) / (float)(Weap_MaxCount(w) + 1);
	Weap_F50(w) = f;
	if (Weap_F74(w) > Weap_F48(w)) {
		Weap_F40(w) = Weap_F48(w) * f / Weap_F74(w);
	} else {
		Weap_F40(w) = f;
	}
}

byte __thiscall cMissileEntity_VarS(cMissileEntity *self, const char *name, char **s, int set) {
	static const char *const hitTypes[4] = { "None", "Hit", "InOut", "Inside" };
	static const uint32_t hitTypeStr[4] = { 0x3b2d30, 0x3b2d38, 0x3b2d3c, 0x3b2d44 };
	static const char *const types[4] = { "Linear", "Dumb", "Cloud", "Fall" };
	static const uint32_t typeStr[4] = { 0x3b2d4c, 0x3b2d5c, 0x3b2d54, 0x3a6ea0 };

	if (!*name) {
		return 0;
	}
	if (!strcmp(name, "OnBeeDeath")) {
		if ((byte)set) {
			cPy_Function_SetFunc(&self->onBeeDeath, *s);
		} else {
			*s = Game_Ptr(0x3b2cf4);	/* "<OnBeeDeath> function" */
		}
		return 1;
	}
	if (!strcmp(name, "OnHit")) {
		if ((byte)set) {
			cPy_Function_SetFunc(&self->onHit, *s);
		} else {
			*s = Game_Ptr(0x3b2d14);	/* "<OnHit> function" */
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
		*s = Game_Ptr(hitTypeStr[self->hitType]);
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
		*s = Game_Ptr(typeStr[self->type]);
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

byte __thiscall cMissileEntity_VarV(cMissileEntity *self, const char *name, double *x, double *y, double *z, int set) {
	if (!*name) {
		return 0;
	}
	return cWithLifeEntity_VarV(Missile_Ent(self), name, x, y, z, set);
}
