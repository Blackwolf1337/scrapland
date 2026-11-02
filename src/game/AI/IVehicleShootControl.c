/*
 * The shoot control of the vehicle AI, original source AI/AIVehicleShootControl.cpp (include/scrap/aivehiclecontrol.h):
 * the table of the weapons the vehicle has, the choice of the weapon and of the countermeasure by the distance to
 * the target and the ammo left, and the fire and ammo buttons of the controls (fire in the cone of the target,
 * countermeasures at a target behind, the hook).
 */
#include <string.h>

#include "scrap/gvar.h"
#include "scrap/aivehicle.h"
#include "scrap/console.h"
#include "scrap/fxcountermeasures.h"
#include "scrap/fxmisc.h"
#include "scrap/fxweapons.h"
#include "scrap/scrapfont.h"
#include "scrap/vehicleentity.h"
#include "scrap/weapons.h"
#include "scrap/weaps2.h"
#include "scrap/world.h"

/* the engine variables */
#define g_ShowVehWeapAI (&(*(cString *)&GAME_GVAR(GVar_ShowVehWeapAI)->value))
extern cGVar GVar_TimeShootAI;
#define g_TimeShootAI (GVar_TimeShootAI.value.f)
extern cGVar GVar_TimeShootCMAI;
#define g_TimeShootCMAI (GVar_TimeShootCMAI.value.f)
extern cGVar GVar_TimeChangeWeapAI;
#define g_TimeChangeWeapAI (GVar_TimeChangeWeapAI.value.f)
extern cGVar GVar_TimeChangeWeapCMAI;
#define g_TimeChangeWeapCMAI (GVar_TimeChangeWeapCMAI.value.f)
extern cGVar GVar_ShortDistShootAI;
#define g_ShortDistShootAI (GVar_ShortDistShootAI.value.f)
extern cGVar GVar_OffsetAngShootAI;
#define g_OffsetAngShootAI (GVar_OffsetAngShootAI.value.f)
extern cGVar GVar_MinDistCMAI;
#define g_MinDistCMAI (GVar_MinDistCMAI.value.f)
extern cGVar GVar_VulcanIDWeap;
#define g_VulcanIDWeap (GVar_VulcanIDWeap.value.i)
extern cGVar GVar_VulcanDistLimit;
#define g_VulcanDistLimit (GVar_VulcanDistLimit.value.f)
extern cGVar GVar_VulcanDistLimitUPG;
#define g_VulcanDistLimitUPG (GVar_VulcanDistLimitUPG.value.f)
extern cGVar GVar_TeslaDistLimit;
#define g_TeslaDistLimit (GVar_TeslaDistLimit.value.f)
extern cGVar GVar_SwarmDistLimit;
#define g_SwarmDistLimit (GVar_SwarmDistLimit.value.f)

#define AIVS_AMMO_HOOK 3		/* the ammo kind of the hook */

/* the nose and the tail of a vehicle in its own frame */
static const vec3_t aivs_fwd = { 0.0f, 0.0f, -1.0f };
static const vec3_t aivs_back = { 0.0f, 0.0f, 1.0f };

static inline cTargetLock *AIVS_Weapons(cAIVehicleShoot *self) {
	return &Veh(self->vehicle)->weapons;
}

static inline cEntity *AIVS_Target(cAIVehicleShoot *self) {
	return self->target->target;
}

static inline float AIVS_Ammo(cAIVehicleShoot *self, int kind) {
	return cTargetLock_GetAmmo(AIVS_Weapons(self), kind);
}

/* the distance from the vehicle to the target (the vehicle asked first) */
static float AIVS_TargetDist(cAIVehicleShoot *self) {
	vec3_t a, b;
	cEntity *t = AIVS_Target(self);
	const vec3_t *p = self->vehicle->vftable->GetPos(self->vehicle, &a);
	const vec3_t *q = t->vftable->GetPos(t, &b);
	float dx = q->x - p->x, dy = q->y - p->y, dz = q->z - p->z;

	return (float)__builtin_sqrt((double)(dy * dy + dx * dx + dz * dz));
}

/* the target minus the vehicle */
static void AIVS_TargetDir(cAIVehicleShoot *self, vec3_t *d) {
	vec3_t a, b;
	cEntity *t = AIVS_Target(self);
	const vec3_t *p = self->vehicle->vftable->GetPos(self->vehicle, &a);
	const vec3_t *q = t->vftable->GetPos(t, &b);

	d->x = q->x - p->x;
	d->y = q->y - p->y;
	d->z = q->z - p->z;
}

/* the ammo kind of a weapon: 0 VULCAN, DEVASTATOR; 2 TESLA, ATPC; 1 SWARM, INFERNO; -1 the others */
static int AIVS_AmmoKind(int w) {
	if (w == g_VulcanIDWeap || w == g_DevastatorIDWeap) {
		return 0;
	}
	if (w == g_TeslaIDWeap || w == g_ATPCIDWeap) {
		return 2;
	}
	if (w == g_SwarmIDWeap || w == g_InfernoIDWeap) {
		return 1;
	}
	return -1;
}

/* the event 3 (target gone) when the AI targets an entity that is not active any more */
static byte AIVS_TargetDied(cAIVehicleShoot *self, const char *fmt) {
	const char *name = self->vehicle->name;
	int in = cAIVehicle_GetInternState(self->ai);
	int st = cAIVehicle_GetState(self->ai);

	Con_Bluef(fmt, name, st, in);
	self->ai->vftable->Event(&self->ai->base.base, 3);
	return 0;
}

static inline byte AIVS_TargetActive(cAIVehicleShoot *self) {
	cEntity *t = AIVS_Target(self);
	return t && t->active;
}

/* ---- construction ---- */

cAIVehicleShoot *cAIVehicleShoot_ctor(void *mem, cEntity *v, cAIVehicle *ai, void *params) {
	cAIVehicleShoot *self = mem;

	self->vftable = CAIVEHICLESHOOT_VFTABLE;
	cSlotController_ctor(&self->slotFire, 0.2f);
	cSlotController_ctor(&self->slotFireCM, 0.2f);
	cSlotController_ctor(&self->slotWeap, 0.5f);
	if (!v || !ai) {
		Con_Bluef("ERROR: Inicializaci\xf3n AI Vehicle Shoot Control. Entidad = NULL.\n");
		CheckFailed("cAIVehicleShootControl_Fail_Entity_NULL", __FILE__, __TIMESTAMP__, __LINE__);
	}
	self->ai = ai;
	self->params = *(const cAIVehicleParams3 *)params;
	self->paramsInit = *(const cAIVehicleParams3 *)params;
	self->vehicle = v;
	self->ctl = &Veh(v)->controls;
	self->cadenceMul = 1;
	self->cadenceMulCM = 2;
	self->weap.weapon = -1;
	self->cm.weapon = -1;
	cAIVehicleShoot_InitWeapons(self);
	self->dt = 0.0f;
	self->fireTime = 0.0f;
	self->fireTimeCM = 0.0f;
	self->weapTime = 0.0f;
	self->weapTimeCM = 0.0f;
	self->mode = 0;
	self->modeCM = 0;
	self->target = NULL;
	self->lifeMark = Veh(self->vehicle)->wl.life;
	self->damaged = 0;
	return self;
}

cAIVehicleShoot *cAIVehicleShoot_Delete(cAIVehicleShoot *self, byte flags) {
	self->vftable = CAIVEHICLESHOOT_VFTABLE;
	if (flags & 1) {
		operator_delete_sized(self, CAIVEHICLESHOOT_SIZE);
	}
	return self;
}

/* ---- the checks of the target ---- */

byte cAIVehicleShoot_CheckTarget(cAIVehicleShoot *self) {
	if (self->mode == 1 && !AIVS_TargetActive(self)) {
		return AIVS_TargetDied(self, "OBJECTIVE_DIED - cAIVehicleShootControl::CheckIntensiveShoot - Entidad: %s - "
			"State: %d - StateIntern: %d\n");
	}
	return 1;
}

byte cAIVehicleShoot_CheckTargetCM(cAIVehicleShoot *self) {
	if (self->modeCM == 1) {
		if (!AIVS_TargetActive(self)) {
			return AIVS_TargetDied(self, "OBJECTIVE_DIED - cAIVehicleShootControl::CheckIntensiveShootCM - "
				"WITH_SHOOT_CM -Entidad: %s - State: %d - StateIntern: %d\n");
		}
	} else if (self->modeCM == 2 && self->hook && !AIVS_TargetActive(self)) {
		return AIVS_TargetDied(self, "OBJECTIVE_DIED - cAIVehicleShootControl::CheckIntensiveShootCM - "
			"WITH_SHOOT_CM_HOOK - Entidad: %s - State: %d - StateIntern: %d\n");
	}
	return 1;
}

/* ---- the weapon and the countermeasure in use ---- */

void cAIVehicleShoot_UpdateWeapon(cAIVehicleShoot *self) {
	cSlotController *slot = &self->slotWeap;
	float t;

	self->weapTime = self->dt + self->weapTime;
	if (!(byte)cSlotController_Tick(slot, self->dt) || !cSlotController_FireReset(slot, &t)) {
		return;
	}
	do {
		byte empty = 0;
		int kind = AIVS_AmmoKind(self->weap.weapon);
		if (kind < 0 || AIVS_Ammo(self, kind) == 0.0f) {
			empty = 1;
		}
		if (!cAIVehicleShoot_InRange(self, self->weap.weapon) || self->weapTime >= g_TimeChangeWeapAI || empty) {
			cAIVehicleShoot_TargetInSight(self, 1);
			if (self->weap.weapon != -1) {
				self->weapTime = 0.0f;
			}
		}
	} while (cSlotController_FireReset(slot, &t));
}

void cAIVehicleShoot_UpdateCM(cAIVehicleShoot *self) {
	cSlotController *slot = &self->slotWeap;
	float t;

	self->weapTimeCM = self->dt + self->weapTimeCM;
	if (!(byte)cSlotController_Tick(slot, self->dt) || !cSlotController_FireReset(slot, &t)) {
		return;
	}
	do {
		int kind = AIVS_AmmoKind(self->cm.weapon);
		if (kind < 0 || AIVS_Ammo(self, kind) == 0.0f || self->weapTimeCM >= g_TimeChangeWeapCMAI) {
			cAIVehicleShoot_SelectCM(self);
			if (self->weap.weapon != -1) {
				self->weapTimeCM = 0.0f;
			}
		}
	} while (cSlotController_FireReset(slot, &t));
}

/* ---- the buttons ---- */

/* the counterattack factor: 0 when it hits, 1 when it does not apply */
static int AIVS_Counter(cAIVehicleShoot *self) {
	int r = 1;

	if (self->damaged && self->params.counterattack > 0.0f) {
		float c = self->params.counterattack;
		self->damaged = 0;
		if (c == 1.0f) {
			r = 0;
		} else {
			r = crt_rand();
			r %= (int)(5.0f / (1.0f - self->params.counterattack));
		}
	}
	return r;
}

/* the fire button pulled by the cadence (the counterattack after damage); the slow weapons less often when the
 * target looks away */
void cAIVehicleShoot_PullFire(cAIVehicleShoot *self) {
	int w = self->weap.weapon;
	int r;

	if (w == g_LaserIDWeap || w == g_VulcanIDWeap || w == g_DevastatorIDWeap || w == g_TeslaIDWeap) {
		if (self->params.cadence == 0) {
			r = 0;
		} else {
			r = crt_rand();
			r %= (self->params.cadence + 1) * self->cadenceMul;
		}
	} else if (w == g_SwarmIDWeap || w == g_ATPCIDWeap || w == g_InfernoIDWeap) {
		vec3_t d, rot, fwd, back;

		AIVS_TargetDir(self, &d);
		cEntity *t = AIVS_Target(self);
		Vec3_Rotate(&fwd, &aivs_fwd, Obj_Vtbl(t)->GetAng(t, &rot));
		if (self->params.cadence == 0) {
			r = 0;
		} else {
			back.x = -d.x;
			back.y = -d.y;
			back.z = -d.z;
			double ang = Vec3_Angle(&back, &fwd);
			if ((double)self->params.angAttack >= ang) {
				r = crt_rand();
				r %= (self->params.cadence + 1) * self->cadenceMul * 2;
			} else {
				r = crt_rand();
				r %= (self->params.cadence + 1) * self->cadenceMul * 4;
			}
		}
	} else {
		return;
	}
	int c = AIVS_Counter(self);
	if (r == 0 || c == 0) {
		self->ctl->fire = 1;
		self->fireTime = 0.0f;
	}
}

void cAIVehicleShoot_Fire(cAIVehicleShoot *self) {
	cSlotController *slot = &self->slotFire;
	float t;

	if (self->weap.weapon == -1) {
		self->ctl->fire = 0;
		goto done;
	}
	self->fireTime = self->dt + self->fireTime;
	if (!cAIVehicle_InSight(self->ai) ||
		(self->ctl->fire && self->params.cadence > 0 && self->fireTime > g_TimeShootAI)) {
		self->ctl->fire = 0;
		return;
	}
	if (self->lifeMark > Veh(self->vehicle)->wl.life) {
		self->damaged = 1;
	}
	if ((byte)cSlotController_Tick(slot, self->dt) && cSlotController_FireReset(slot, &t)) {
		do {
			vec3_t rot, fwd, d;
			cEntity *tg = AIVS_Target(self);
			vec3_t a, b;
			const vec3_t *p = self->vehicle->vftable->GetPos(self->vehicle, &a);
			const vec3_t *q = tg->vftable->GetPos(tg, &b);
			float dx = q->x - p->x, dy = q->y - p->y, dz = q->z - p->z;
			cEntity *tr = AIVS_Target(self);
			float r = Obj_Vtbl(tr)->GetRadius(tr);
			float dist = (float)__builtin_sqrt((double)(dy * dy + dx * dx + dz * dz));
			double cone = atan2((double)r, (double)dist);
			Vec3_Rotate(&fwd, &aivs_fwd, Obj_Vtbl(self->vehicle)->GetAng(self->vehicle, &rot));
			AIVS_TargetDir(self, &d);
			double ang = Vec3_Angle(&fwd, &d);
			if ((double)g_OffsetAngShootAI + cone > ang) {
				if (!self->ctl->fire) {
					cAIVehicleShoot_PullFire(self);
				}
			} else {
				self->ctl->fire = 0;
			}
		} while (cSlotController_FireReset(slot, &t));
	}
done:
	self->lifeMark = Veh(self->vehicle)->wl.life;
}

/* the ammo button for countermeasures at a target near behind the vehicle (or while it strafes) */
void cAIVehicleShoot_FireCM(cAIVehicleShoot *self) {
	cSlotController *slot = &self->slotFireCM;
	float t;

	if (self->cm.weapon == -1) {
		return;
	}
	t = self->dt + self->fireTimeCM;
	self->fireTimeCM = t;
	if (self->ctl->cm && self->params.cadenceCM > 0 && t > g_TimeShootCMAI) {
		self->ctl->cm = 0;
		return;
	}
	if (!(byte)cSlotController_Tick(slot, self->dt) || !cSlotController_FireReset(slot, &t)) {
		return;
	}
	do {
		float dist = AIVS_TargetDist(self);
		if (!(g_MinDistCMAI > dist)) {
			goto far;
		}
		if (!cAIVehicleControl_GetStrafing(cAIVehicle_GetControl(self->ai))) {
			vec3_t d, rot, fwd;
			AIVS_TargetDir(self, &d);
			cEntity *tr = AIVS_Target(self);
			float r = Obj_Vtbl(tr)->GetRadius(tr);
			double cone = atan2((double)r, (double)dist);
			Vec3_Rotate(&fwd, &aivs_back, Obj_Vtbl(self->vehicle)->GetAng(self->vehicle, &rot));
			double ang = Vec3_Angle(&fwd, &d);
			if (!((double)g_OffsetAngShootAI + cone > ang)) {
				goto far;
			}
		}
		{
			int n = self->params.cadenceCM;
			if (n == 0 || crt_rand() % ((n + 1) * self->cadenceMulCM) == 0) {
				self->ctl->cm = 1;
				self->fireTimeCM = 0.0f;
			}
		}
		continue;
far:
		if (self->params.cadenceCM == 0) {
			self->ctl->cm = 0;
		}
	} while (cSlotController_FireReset(slot, &t));
}

/* the hook: the ammo button while there is hook energy, at a locked point or the target within the hook range */
void cAIVehicleShoot_FireHook(cAIVehicleShoot *self) {
	cSlotController *slot = &self->slotFireCM;
	float t;

	if (!(byte)cSlotController_Tick(slot, self->dt) || !cSlotController_FireReset(slot, &t)) {
		return;
	}
	do {
		byte ok = 0;
		if (self->ctl->cm && AIVS_Ammo(self, AIVS_AMMO_HOOK) > 1.0f) {
			ok = 1;
		} else if (AIVS_Ammo(self, AIVS_AMMO_HOOK) > 100.0f) {
			ok = 1;
		}
		if (self->ctl->cm && self->hook) {
			if (cTargetLock_GetPoint(AIVS_Weapons(self))) {
				goto off;
			}
			if (cTargetLock_GetEntity(AIVS_Weapons(self))) {
				const char *name = AIVS_Target(self)->name;
				if (strcmp(name, cTargetLock_GetEntity(AIVS_Weapons(self))) != 0) {
					goto off;
				}
			}
		}
		if (!ok) {
			goto off;
		}
		if (self->hook) {
			vec3_t a, b;
			cEntity *tg = AIVS_Target(self);
			const vec3_t *p = self->vehicle->vftable->GetPos(self->vehicle, &a);
			const vec3_t *q = tg->vftable->GetPos(tg, &b);
			float dx = q->x - p->x, dy = q->y - p->y, dz = q->z - p->z;
			if (!(g_HookRange * g_HookRange > dy * dy + dx * dx + dz * dz)) {
				goto off;
			}
		}
		self->ctl->cmPrev = 1;
		self->ctl->cm = 1;
		self->fireTime = 0.0f;
		continue;
off:
		self->ctl->cm = 0;
	} while (cSlotController_FireReset(slot, &t));
}

/* ---- the choice of the weapons ---- */

/* the weapon in use by the distance to the target, when set; 1 if any weapon fits the distance */
byte cAIVehicleShoot_TargetInSight(cAIVehicleShoot *self, int arg) {
	int nearW[3], farW[7];
	int nn = 0, nf = 0;
	float dist = AIVS_TargetDist(self);
	cAIVSWeap sel;
	int i;

	if (self->weaps[AIVS_VULCAN].avail && AIVS_Ammo(self, 0) > 0.0f && g_VulcanDistLimit > dist) {
		nearW[nn++] = AIVS_VULCAN;
	}
	if (self->weaps[AIVS_DEVASTATOR].avail && AIVS_Ammo(self, 0) > 0.0f && g_DevastatorDistLimit > dist) {
		nearW[nn++] = AIVS_DEVASTATOR;
	}
	if (self->weaps[AIVS_TESLA].avail && AIVS_Ammo(self, 2) > 0.0f && g_TeslaDistLimit > dist) {
		nearW[nn++] = AIVS_TESLA;
	}
	if (self->weaps[AIVS_VULCAN].avail && AIVS_Ammo(self, 0) > 0.0f && self->weaps[AIVS_VULCAN].kind == 2 &&
		g_VulcanDistLimitUPG > dist) {
		farW[nf++] = AIVS_VULCAN;
	}
	if (self->weaps[AIVS_SWARM].avail && AIVS_Ammo(self, 1) > 0.0f && g_SwarmDistLimit > dist) {
		farW[nf++] = AIVS_SWARM;
	}
	if (self->weaps[AIVS_INFERNO].avail && g_InfernoDistLimit > dist) {
		float cost = (float)g_InfernoAmmoCost;
		if (cAIVehicleShoot_WeapAmmo(self, g_InfernoIDWeap) > cost) {
			farW[nf++] = AIVS_INFERNO;
		}
	}
	if (self->weaps[AIVS_ATPC].avail && AIVS_Ammo(self, 2) > 0.0f && g_ATPCRange > dist) {
		farW[nf++] = AIVS_ATPC;
	}
	if ((byte)arg) {
		/* the original copies the fields of an unset local but the weapon when nothing fits */
		memset(&sel, 0, sizeof(sel));
		sel.weapon = -1;
		if (g_ShortDistShootAI > dist) {
			if (nn) {
				goto pickNear;
			}
			if (nf) {
				i = farW[crt_rand() % nf];
				goto pick;
			}
			goto none;
		}
		if (nf) {
			i = farW[crt_rand() % nf];
			goto pick;
		}
		if (!nn) {
			goto none;
		}
pickNear:
		i = nearW[crt_rand() % nn];
pick:
		sel = self->weaps[i];
		if (sel.weapon != -1) {
			Controls_SetWeapon(self->ctl, sel.weapon);
			goto store;
		}
none:
		if (g_LaserDistLimit > dist) {
			sel = self->weaps[AIVS_LASER];
			if (sel.weapon != -1) {
				Controls_SetWeapon(self->ctl, sel.weapon);
			}
		}
store:
		self->weap = sel;
	}
	return nn || nf;
}

/* the countermeasure by the use percentages of the parameters among the kinds with ammo */
void cAIVehicleShoot_SelectCM(cAIVehicleShoot *self) {
	int c0 = -1, c1 = -1, c2 = -1;
	int n = 0;
	cAIVSWeap sel;
	int i;

	if (AIVS_Ammo(self, 0) > 0.0f) {
		if (self->weaps[AIVS_VULCAN].avail) {
			c0 = AIVS_VULCAN;
			n = 1;
		} else if (self->weaps[AIVS_DEVASTATOR].avail) {
			c0 = AIVS_DEVASTATOR;
			n = 1;
		}
	}
	if (AIVS_Ammo(self, 1) > 0.0f) {
		if (self->weaps[AIVS_SWARM].avail) {
			c1 = AIVS_SWARM;
			n++;
		} else if (self->weaps[AIVS_INFERNO].avail) {
			c1 = AIVS_INFERNO;
			n++;
		}
	}
	if (AIVS_Ammo(self, 2) > 0.0f) {
		if (self->weaps[AIVS_ATPC].avail) {
			c2 = AIVS_ATPC;
			n++;
		} else if (self->weaps[AIVS_TESLA].avail) {
			c2 = AIVS_TESLA;
			n++;
		}
	}
	/* the original copies the fields of an unset local but the weapon when nothing is chosen */
	memset(&sel, 0, sizeof(sel));
	sel.weapon = -1;
	if (n) {
		int r = crt_rand() % 101;
		int ps = self->params.percSonic;
		if (ps && r <= ps) {
			if (c1 == -1) {
				goto store;
			}
			i = c1;
		} else if (self->params.percCloud && r <= self->params.percCloud + ps) {
			if (c0 == -1) {
				goto store;
			}
			i = c0;
		} else if (self->params.percElectro && c2 != -1) {
			i = c2;
		} else {
			goto store;
		}
		sel = self->weaps[i];
		if (sel.weapon != -1) {
			Controls_SetWeapon(self->ctl, sel.weapon);
		}
	}
store:
	self->cm = sel;
}

/* ---- the update ---- */

void cAIVehicleShoot_Update(cAIVehicleShoot *self, float dt) {
	if (g_NotShootAI == 0) {
		if (!cAIVehicle_GetChanged(self->ai) &&
			(!cAIVehicle_Think(self->ai) || !cAIVehicleShoot_CheckTarget(self))) {
			return;
		}
		self->dt = dt;
		if (self->mode != 0) {
			if (self->mode == 1) {
				cAIVehicleShoot_Fire(self);
				cAIVehicleShoot_UpdateWeapon(self);
			}
			return;
		}
	}
	self->ctl->fire = 0;
}

void cAIVehicleShoot_Update2(cAIVehicleShoot *self, float dt) {
	if (g_NotShootAI == 0) {
		if (!cAIVehicle_GetChanged(self->ai) &&
			(!cAIVehicle_Think(self->ai) || !cAIVehicleShoot_CheckTargetCM(self))) {
			return;
		}
		self->dt = dt;
		switch (self->modeCM) {
		case 0:
			break;
		case 1:
			cAIVehicleShoot_FireCM(self);
			cAIVehicleShoot_UpdateCM(self);
			return;
		case 2:
			cAIVehicleShoot_FireHook(self);
			return;
		default:
			return;
		}
	}
	self->ctl->cm = 0;
}

void cAIVehicleShoot_Draw(cAIVehicleShoot *self) {
	char name[0x14];

	if (cString_EqStr(g_ShowVehWeapAI, self->vehicle->name)) {
		CFont_PrintfColor(g_FontTerminal8, 10.0f, 10.0f, 0xffffff, "ENTIDAD: %s", self->vehicle->name);
		cAIVehicleShoot_WeaponName(self->weap.weapon, name);
		CFont_PrintfColor(g_FontTerminal8, 10.0f, 30.0f, 0xffff00, "Weapon Act.: %s", name);
		float a = cAIVehicleShoot_WeapAmmo(self, self->weap.weapon);
		CFont_PrintfColor(g_FontTerminal8, 10.0f, 50.0f, 0xffff00, "Ammo: %f", (double)a);
	}
}

/* ---- the ammo ---- */

/* the ammo of the weapons that fit the distance */
float cAIVehicleShoot_AmmoAt(cAIVehicleShoot *self, float dist) {
	float s = 0.0f;
	byte b;

	if (g_ShortDistShootAI > dist) {
		if (self->weaps[AIVS_VULCAN].avail || self->weaps[AIVS_DEVASTATOR].avail) {
			s = AIVS_Ammo(self, 0) + 0.0f;
		}
		b = self->weaps[AIVS_TESLA].avail;
	} else {
		if (self->weaps[AIVS_VULCAN].avail && self->weaps[AIVS_VULCAN].kind == 2) {
			s = AIVS_Ammo(self, 0) + 0.0f;
		}
		if (self->weaps[AIVS_SWARM].avail || self->weaps[AIVS_INFERNO].avail) {
			s = AIVS_Ammo(self, 1) + s;
		}
		b = self->weaps[AIVS_ATPC].avail;
	}
	if (b) {
		s = AIVS_Ammo(self, 2) + s;
	}
	return s;
}

static inline float AIVS_MaxAmmo(int kind) {
	return TABLE_AT(g_pWorld->ammo, kind)->max;
}

/* the most ammo of the weapons that fit the distance */
float cAIVehicleShoot_MaxAmmoAt(cAIVehicleShoot *self, float dist) {
	float s = 0.0f;

	if (g_ShortDistShootAI > dist) {
		if (self->weaps[AIVS_VULCAN].avail || self->weaps[AIVS_DEVASTATOR].avail) {
			s = s + AIVS_MaxAmmo(0);
		}
		if (!self->weaps[AIVS_TESLA].avail) {
			return s;
		}
	} else {
		if (self->weaps[AIVS_VULCAN].avail && self->weaps[AIVS_VULCAN].kind == 2) {
			s = s + AIVS_MaxAmmo(0);
		}
		if (self->weaps[AIVS_SWARM].avail || self->weaps[AIVS_INFERNO].avail) {
			s = s + AIVS_MaxAmmo(1);
		}
		if (!self->weaps[AIVS_ATPC].avail) {
			return s;
		}
	}
	return AIVS_MaxAmmo(2) + s;
}

float cAIVehicleShoot_MaxAmmo(cAIVehicleShoot *self) {
	float s = 0.0f;

	if (self->weaps[AIVS_VULCAN].avail || self->weaps[AIVS_DEVASTATOR].avail) {
		s = s + AIVS_MaxAmmo(0);
	}
	if (self->weaps[AIVS_TESLA].avail || self->weaps[AIVS_ATPC].avail) {
		s = s + AIVS_MaxAmmo(2);
	}
	if (self->weaps[AIVS_SWARM].avail || self->weaps[AIVS_INFERNO].avail) {
		s = AIVS_MaxAmmo(1) + s;
	}
	return s;
}

float cAIVehicleShoot_TotalAmmo(cAIVehicleShoot *self) {
	float s = 0.0f;

	if (self->weaps[AIVS_VULCAN].avail || self->weaps[AIVS_DEVASTATOR].avail) {
		s = AIVS_Ammo(self, 0) + 0.0f;
	}
	if (self->weaps[AIVS_SWARM].avail || self->weaps[AIVS_INFERNO].avail) {
		s = AIVS_Ammo(self, 1) + s;
	}
	if (self->weaps[AIVS_TESLA].avail || self->weaps[AIVS_ATPC].avail) {
		s = AIVS_Ammo(self, 2) + s;
	}
	return s;
}

/* the ammo of the kind of weapon w, 0 for the laser and unknown ones */
float cAIVehicleShoot_WeapAmmo(cAIVehicleShoot *self, int w) {
	float a = 0.0f;
	int kind = AIVS_AmmoKind(w);

	if (kind >= 0) {
		a = AIVS_Ammo(self, kind);
	}
	return a;
}

void cAIVehicleShoot_GetInfo(cAIVehicleShoot *self, int *a, int *kind, byte *flag) {
	*a = self->mode;
	*kind = self->modeCM;
	*flag = self->hook;
}

cAIVehicleParams3 *cAIVehicleShoot_GetParams(cAIVehicleShoot *self) {
	return &self->params;
}

cAIVehicleParams3 *cAIVehicleShoot_GetParamsInit(cAIVehicleShoot *self) {
	return &self->paramsInit;
}

cAIVSWeap *cAIVehicleShoot_GetWeaps(cAIVehicleShoot *self) {
	return self->weaps;
}

/* the name of the weapon for the debug text; the bytes of the original string copies */
void cAIVehicleShoot_WeaponName(int w, char *buf) {
	if (w == g_VulcanIDWeap) {
		memcpy(buf, "VULCAN", 7);
	} else if (w == g_DevastatorIDWeap) {
		memcpy(buf, "DEVASTATOR", 11);
	} else if (w == g_TeslaIDWeap) {
		memcpy(buf, "TESLA", 6);
	} else if (w == g_ATPCIDWeap) {
		memcpy(buf, "ATPC", 5);
	} else if (w == g_SwarmIDWeap) {
		memcpy(buf, "SWARM", 6);
	} else if (w == g_InfernoIDWeap) {
		memcpy(buf, "INFERNO", 8);
	} else if (w == g_LaserIDWeap) {
		memcpy(buf, "LASER", 6);
	} else {
		memcpy(buf, "NONE", 5);
	}
}

byte cAIVehicleShoot_HasHook(cAIVehicleShoot *self) {
	return self->weaps[AIVS_LASER].avail;
}

byte cAIVehicleShoot_HasAmmo(cAIVehicleShoot *self, float factor) {
	if (self->weaps[AIVS_VULCAN].avail || self->weaps[AIVS_DEVASTATOR].avail) {
		const cAmmoInfo *info = TABLE_AT(g_pWorld->ammo, 0);
		if (AIVS_Ammo(self, 0) >= info->max * factor) {
			return 1;
		}
	}
	if (self->weaps[AIVS_SWARM].avail) {
		const cAmmoInfo *info = TABLE_AT(g_pWorld->ammo, 1);
		if (AIVS_Ammo(self, 1) >= info->max * factor) {
			return 1;
		}
	}
	if (self->weaps[AIVS_INFERNO].avail) {
		vec3_t d;
		AIVS_TargetDir(self, &d);
		if (self->weaps[AIVS_INFERNO].avail) {
			float dist = (float)__builtin_sqrt((double)(d.y * d.y + d.x * d.x + d.z * d.z));
			if (g_InfernoDistLimit > dist) {
				float cost = (float)g_InfernoAmmoCost;
				if (cAIVehicleShoot_WeapAmmo(self, g_InfernoIDWeap) > cost) {
					return 1;
				}
			}
		}
	}
	if (self->weaps[AIVS_TESLA].avail || self->weaps[AIVS_ATPC].avail) {
		const cAmmoInfo *info = TABLE_AT(g_pWorld->ammo, 2);
		if (AIVS_Ammo(self, 2) >= info->max * factor) {
			return 1;
		}
	}
	return 0;
}

/* the inferno at dist with ammo for a shot */
byte cAIVehicleShoot_InfernoReady(cAIVehicleShoot *self, float dist) {
	if (self->weaps[AIVS_INFERNO].avail && g_InfernoDistLimit > dist) {
		float cost = (float)g_InfernoAmmoCost;
		if (cAIVehicleShoot_WeapAmmo(self, g_InfernoIDWeap) > cost) {
			return 1;
		}
	}
	return 0;
}

byte cAIVehicleShoot_UsesAmmo(cAIVehicleShoot *self, int kind, float factor) {
	if (g_ShortDistShootAI > factor) {
		if (kind == 0) {
			return self->weaps[AIVS_VULCAN].avail || self->weaps[AIVS_DEVASTATOR].avail;
		}
		return kind == 2 && self->weaps[AIVS_TESLA].avail;
	}
	if (kind == 0) {
		return self->weaps[AIVS_VULCAN].avail && self->weaps[AIVS_VULCAN].kind == 2;
	}
	if (kind == 1) {
		return self->weaps[AIVS_SWARM].avail || self->weaps[AIVS_INFERNO].avail;
	}
	return kind == 2 && self->weaps[AIVS_ATPC].avail;
}

/* the target within the range of weapon w */
byte cAIVehicleShoot_InRange(cAIVehicleShoot *self, int w) {
	float dist = AIVS_TargetDist(self);

	if (w == g_VulcanIDWeap) {
		return g_VulcanDistLimit > dist || (self->weaps[AIVS_VULCAN].kind == 2 && g_VulcanDistLimitUPG > dist);
	}
	if (w == g_DevastatorIDWeap) {
		return g_DevastatorDistLimit > dist;
	}
	if (w == g_TeslaIDWeap) {
		return g_TeslaDistLimit > dist;
	}
	if (w == g_ATPCIDWeap) {
		return g_ATPCRange > dist;
	}
	if (w == g_SwarmIDWeap) {
		return g_SwarmDistLimit > dist;
	}
	if (w == g_InfernoIDWeap) {
		return cAIVehicleShoot_InfernoReady(self, dist);
	}
	if (w == g_LaserIDWeap) {
		return g_LaserDistLimit > dist;
	}
	return 0;
}

/* the modes of the fire and the ammo buttons (a: 0 off, 1 fire; b: 0 off, 1 countermeasures, 2 the hook);
 * with hook and no target both off and 0 */
byte cAIVehicleShoot_SetMode(cAIVehicleShoot *self, int a, int b, byte c) {
	byte r = 1;

	if (!self->ai->vftable->GetTarget(&self->ai->base.base)->target && c) {
		a = 0;
		b = 0;
		r = 0;
	}
	self->target = self->ai->vftable->GetTarget(&self->ai->base.base);
	self->hook = c;
	self->mode = a;
	self->modeCM = b;
	self->fireTime = 0.0f;
	self->fireTimeCM = 0.0f;
	self->weapTime = g_TimeChangeWeapAI;
	self->weapTimeCM = g_TimeChangeWeapCMAI;
	self->lifeMark = Veh(self->vehicle)->wl.life;
	if (a == 0) {
		cAIVehicleShoot_Update(self, 0.0f);
	} else if (a == 1) {
		cAIVehicleShoot_TargetInSight(self, 1);
	}
	if (b == 0) {
		cAIVehicleShoot_Update2(self, 0.0f);
	} else if (b == 2) {
		self->cm = self->weaps[AIVS_LASER];
		Controls_SetWeapon(self->ctl, self->cm.weapon);
	} else if (b == 1) {
		cAIVehicleShoot_SelectCM(self);
	}
	return r;
}

/* the table of the weapons from the slots of the vehicle; kind and upgrade of a weapon the vehicle does not have
 * stay those of the one before it, like the locals of the original */
void cAIVehicleShoot_InitWeapons(cAIVehicleShoot *self) {
	cTargetLock *wm = AIVS_Weapons(self);
	int kind = 0, upgrade = 0;

	for (int i = 0; i < AIVS_NUM; i++) {
		cAIVSWeap *e = &self->weaps[i];
		int w;
		byte avail;
		switch (i) {
		case AIVS_LASER:
			w = g_LaserIDWeap;
			break;
		case AIVS_VULCAN:
			w = g_VulcanIDWeap;
			break;
		case AIVS_DEVASTATOR:
			w = g_DevastatorIDWeap;
			break;
		case AIVS_ATPC:
			w = g_ATPCIDWeap;
			break;
		case AIVS_TESLA:
			w = g_TeslaIDWeap;
			break;
		case AIVS_SWARM:
			w = g_SwarmIDWeap;
			break;
		default:
			w = g_InfernoIDWeap;
			break;
		}
		if (cWeaponManager_GetSlots(wm, w)) {
			switch (i) {
			case AIVS_LASER:
				upgrade = cWeaponManager_GetUpgrade(wm, g_LaserIDWeap);
				kind = 0;
				break;
			case AIVS_VULCAN:
				upgrade = cWeaponManager_GetUpgrade(wm, g_VulcanIDWeap);
				kind = upgrade == 2 ? 2 : 0;
				break;
			case AIVS_DEVASTATOR:
				upgrade = cWeaponManager_GetUpgrade(wm, g_DevastatorIDWeap);
				kind = 0;
				break;
			case AIVS_ATPC:
				upgrade = cWeaponManager_GetUpgrade(wm, g_ATPCIDWeap);
				kind = 1;
				break;
			case AIVS_TESLA:
				upgrade = cWeaponManager_GetUpgrade(wm, g_TeslaIDWeap);
				kind = 0;
				break;
			case AIVS_SWARM:
				upgrade = cWeaponManager_GetUpgrade(wm, g_SwarmIDWeap);
				kind = 1;
				break;
			default:
				upgrade = cWeaponManager_GetUpgrade(wm, g_InfernoIDWeap);
				kind = 1;
				break;
			}
			avail = 1;
		} else {
			avail = 0;
		}
		e->kind = kind;
		e->unused_04 = 0;
		e->unused_08 = 0;
		e->weapon = w;
		e->upgrade = upgrade;
		e->unused_14 = 0;
		e->avail = avail;
	}
}

void cAIVehicleShoot_WeapListChanged(cAIVehicleShoot *self) {
	cAIVehicleShoot_InitWeapons(self);
	if (self->target->target) {
		cAIVehicleShoot_TargetInSight(self, 1);
		cAIVehicleShoot_SelectCM(self);
	}
}
