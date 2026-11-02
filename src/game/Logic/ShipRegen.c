/*
 * Logic/ShipRegen.cpp of the game: the life regeneration (cShipRegen of cLogicControl, include/scrap/logic.h). The
 * player gets a point every RegenerationTimeRate (RegenerationCharTimeRate on foot) once it lost no life for
 * RegenerationStartTime (RegenerationCharStartTime), twice as fast in the easy game and never in the hard one or in
 * a split screen or network game. The ship of SLogic.SetShipToRegenerate gets its points from its own speeds (from
 * speed at LifeToLaunchCallback up to endSpeed at full life), LifeReachedCallback(name, 1 / -1) when it passes
 * LifeToLaunchCallback, and its life shows in the scorer item RegenerationItem.
 */
#include "scrap/gvar.h"
#include "scrap/fxvehicle.h"
#include "scrap/logic.h"
#include "scrap/scorer.h"
#include "scrap/vehicleentity.h"
#include "scrap/weapons.h"
#include "scrap/world.h"
#include "scrap/staticinit.h"

static cGVar GVar_RegenerationStartTime;

#define g_RegenerationStartTime (GVar_RegenerationStartTime.value.f)
static cGVar GVar_RegenerationTimeRate;

#define g_RegenerationTimeRate (GVar_RegenerationTimeRate.value.f)
static cGVar GVar_RegenerationCharStartTime;

#define g_RegenerationCharStartTime (GVar_RegenerationCharStartTime.value.f)
static cGVar GVar_RegenerationCharTimeRate;

#define g_RegenerationCharTimeRate (GVar_RegenerationCharTimeRate.value.f)
static cGVar GVar_RegenerationItem;

#define g_RegenerationItem (GVar_RegenerationItem.value.s)
static cGVar GVar_LifeToLaunchCallback;

#define g_LifeToLaunchCallback (GVar_LifeToLaunchCallback.value.i)
static cGVarFunction GVar_LifeReachedCallback;

#define g_LifeReachedCallback (((cPy_Function *)&GVar_LifeReachedCallback.func))

cShipRegen *ShipRegen_ctor(cShipRegen *self) {
	self->unused_00 = 0;
	self->ship = NULL;
	self->time = g_RegenerationTimeRate;
	self->life = g_StdMaxLife;
	self->noLoss = 0;
	return self;
}

void ShipRegen_dtor(cShipRegen *self) {
	if (self->ship) {
		operator_delete_sized(self->ship, sizeof(*self->ship));
	}
}

static void ShipRegen_Callback(cEntity *e, int dir) {
	cPy_Function_AddParamS(g_LifeReachedCallback, e->name);
	cPy_Function_AddParamI(g_LifeReachedCallback, dir);
	cPy_Function_Execute(g_LifeReachedCallback, 0);
}

/* the points of the ship of SetShipToRegenerate (LifeReachedCallback at LifeToLaunchCallback) and its bar */
static void ShipRegen_UpdateShip(cShipRegen *self, float dt) {
	cShipRegenShip *s = self->ship;

	if (!s->name[0]) {
		return;
	}
	cEntity *e = World_FindEntity(s->name);
	if (!e || !e->vftable->IsType(e, TYEN_CAR)) {
		return;
	}
	cVehicleEntity *v = Veh(e);
	s = self->ship;
	if (s->endRate > 0.0f && s->rate > 0.0f && v->wl.life > 0 && v->wl.life < v->maxLife) {
		int life = v->wl.life;
		int at = g_LifeToLaunchCallback;
		if (s->life >= at) {
			if (at > life) {
				ShipRegen_Callback(e, -1);
			}
		} else if (!(at > life)) {
			ShipRegen_Callback(e, 1);
		}
		self->ship->life = v->wl.life;
		self->ship->time = self->ship->time - dt;
		float range = (float)v->maxLife - (float)g_LifeToLaunchCallback;
		if (0.0f >= self->ship->time) {
			do {
				if (!WLife(e)->god) {
					v->wl.life++;
				}
				float f = ((float)v->wl.life - (float)g_LifeToLaunchCallback) / range;
				if (f > 1.0f) {
					break;
				}
				s = self->ship;
				if (0.0f > f) {
					s->time = s->rate + s->time;
				} else {
					s->time = (1.0f - f) * s->rate + s->endRate * f + s->time;
				}
			} while (0.0f >= self->ship->time);
		}
		if (v->wl.life > v->maxLife && !WLife(e)->god) {
			v->wl.life = v->maxLife;
		}
	}
	cScorerItem *item = cScorer_FindItem(Scorer_Get(0), g_RegenerationItem);
	if (item) {
		double val = (double)(v->wl.life - g_LifeToLaunchCallback) / (double)(v->maxLife - g_LifeToLaunchCallback);
		item->vftable->Var(item, "Value", 1, NULL, &val, NULL);
	}
}

void ShipRegen_Set(void *shipRegen, cEntity *ship, float speed, float endSpeed) {
	cShipRegen *self = shipRegen;

	if (!ship || !ship->vftable->IsType(ship, TYEN_WITH_LIFE) || WLife(ship)->life <= 0) {
		return;
	}
	if (!self->ship) {
		cShipRegenShip *s = operator_new(sizeof(*s));
		if (s) {
			memset(s, 0, sizeof(*s));
		}
		self->ship = s;
	}
	Str_Copy(self->ship->name, ship->name, sizeof(self->ship->name));
	float rate = 1.0f / speed;
	float endRate = 1.0f / endSpeed;
	self->ship->rate = rate;
	self->ship->endRate = endRate;
	self->ship->time = self->ship->rate;
	self->ship->life = WLife(ship)->life;
}

void ShipRegen_Update(cShipRegen *self, float dt) {
	if (g_pApp->flags || g_SplitScreenGame) {
		return;
	}
	if (self->ship) {
		ShipRegen_UpdateShip(self, dt);
	}
	cEntity *usr = cWorld_GetUsrEntity(g_pWorld, 0);
	if (!usr || !usr->vftable->IsType(usr, TYEN_WITH_LIFE) || WLife(usr)->life <= 0) {
		return;
	}
	byte onFoot = usr->vftable->IsType(usr, TYEN_WALKCHAR);
	float t = self->time - dt;
	self->time = t;
	if (!(0.0f >= t)) {
		return;
	}
	if (World_IsHardSingle()) {
		self->time = 5.0f;
		return;
	}
	self->noLoss = self->life == WLife(usr)->life;
	if (self->noLoss) {
		self->time = onFoot ? g_RegenerationCharTimeRate : g_RegenerationTimeRate;
		if (World_IsEasySingle()) {
			self->time = self->time * 0.5f;
		}
		int max = usr->vftable->IsType(usr, TYEN_CAR) ? Veh(usr)->minLife : g_StdMaxLife;
		if (!(max > WLife(usr)->life)) {
			self->noLoss = 0;
		}
		if (self->noLoss && !WLife(usr)->god) {
			WLife(usr)->life++;
			self->life = WLife(usr)->life;
			return;
		}
	} else {
		self->time = onFoot ? g_RegenerationCharStartTime : g_RegenerationStartTime;
		if (World_IsEasySingle()) {
			self->time = self->time * 0.5f;
		}
	}
	self->life = WLife(usr)->life;
}

/* the dynamic initializers of the globals here, run in the order of src/standalone/crtxc.inc */
INIT_GVAR_FLOAT(Init_GVar_RegenerationStartTime, &GVar_RegenerationStartTime, "RegenerationStartTime", 0, "after this time, life´s regeneration starts.(ship)", 3.0f)
INIT_GVAR_FLOAT(Init_GVar_RegenerationTimeRate, &GVar_RegenerationTimeRate, "RegenerationTimeRate", 0, "between RegenerationTimeRate sec, 1 point regenerates.(ship)", 0.1f)
INIT_GVAR_FLOAT(Init_GVar_RegenerationCharStartTime, &GVar_RegenerationCharStartTime, "RegenerationCharStartTime", 0, "after this time, life´s regeneration starts.(char)", 10.0f)
INIT_GVAR_FLOAT(Init_GVar_RegenerationCharTimeRate, &GVar_RegenerationCharTimeRate, "RegenerationCharTimeRate", 0, "between RegenerationTimeRate sec, 1 point regenerates.(char)", 1.0f)
FINI_GVAR_STRING(Fini_GVar_RegenerationItem, &GVar_RegenerationItem)
INIT_GVAR_STRING(Init_GVar_RegenerationItem, Fini_GVar_RegenerationItem, &GVar_RegenerationItem, "RegenerationItem", 0, "Name of the bar item to make the regeneration", "RegenerationBar")
INIT_GVAR_INT(Init_GVar_LifeToLaunchCallback, &GVar_LifeToLaunchCallback, "LifeToLaunchCallback", 0, "Life to launch callback", 30)
FINI_CALL(Fini_GVar_LifeReachedCallback, &GVar_LifeReachedCallback.func, cPy_Function_dtor)
INIT_GVAR_FUNCTION(Init_GVar_LifeReachedCallback, Fini_GVar_LifeReachedCallback, &GVar_LifeReachedCallback, "LifeReachedCallback", 0, "Life reached callback", "")
