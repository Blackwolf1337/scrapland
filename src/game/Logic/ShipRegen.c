/*
 * Logic/ShipRegen.cpp of the game: the life regeneration (cShipRegen of cLogicControl, include/scrap/logic.h). The
 * player gets a point every RegenerationTimeRate (RegenerationCharTimeRate on foot) once it lost no life for
 * RegenerationStartTime (RegenerationCharStartTime), twice as fast in the easy game and never in the hard one or in
 * a split screen or network game. The ship of SLogic.SetShipToRegenerate gets its points from its own speeds (from
 * speed at LifeToLaunchCallback up to endSpeed at full life), LifeReachedCallback(name, 1 / -1) when it passes
 * LifeToLaunchCallback, and its life shows in the scorer item RegenerationItem.
 */
#include "scrap/fxvehicle.h"
#include "scrap/logic.h"
#include "scrap/scorer.h"
#include "scrap/vehicleentity.h"
#include "scrap/weapons.h"
#include "scrap/world.h"

#define g_RegenerationStartTime GAME_DATA(float, 0x44d7a0)
#define g_RegenerationTimeRate GAME_DATA(float, 0x44d7b8)
#define g_RegenerationCharStartTime GAME_DATA(float, 0x44d7f4)
#define g_RegenerationCharTimeRate GAME_DATA(float, 0x44d824)
#define g_RegenerationItem GAME_DATA(char *, 0x44d80c)
#define g_LifeToLaunchCallback GAME_DATA(int, 0x44d83c)
#define g_LifeReachedCallback ((cPy_Function *)Game_Ptr(0x44d7d0))

cShipRegen *__fastcall ShipRegen_ctor(cShipRegen *self) {
	self->b0 = 0;
	self->ship = NULL;
	self->time = g_RegenerationTimeRate;
	self->life = g_StdMaxLife;
	self->noLoss = 0;
	return self;
}

void __fastcall ShipRegen_dtor(cShipRegen *self) {
	if (self->ship) {
		operator_delete_sized(self->ship, sizeof(*self->ship));
	}
}

static void ShipRegen_Callback(cEntity *e, int dir) {
	cPy_Function_AddParamS(g_LifeReachedCallback, e->name);
	cPy_Function_AddParamI(g_LifeReachedCallback, dir);
	cPy_Function_Execute(g_LifeReachedCallback, 0);
}

void __thiscall ShipRegen_UpdateShip(cShipRegen *self, float dt) {
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
	if (s->endRate > 0.0f && s->rate > 0.0f && v->life > 0 && v->life < v->maxLife) {
		int life = v->life;
		int at = g_LifeToLaunchCallback;
		if (s->life >= at) {
			if (at > life) {
				ShipRegen_Callback(e, -1);
			}
		} else if (!(at > life)) {
			ShipRegen_Callback(e, 1);
		}
		self->ship->life = v->life;
		self->ship->time = self->ship->time - dt;
		float range = (float)v->maxLife - (float)g_LifeToLaunchCallback;
		if (0.0f >= self->ship->time) {
			do {
				if (!Entity_God(e)) {
					v->life++;
				}
				float f = ((float)v->life - (float)g_LifeToLaunchCallback) / range;
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
		if (v->life > v->maxLife && !Entity_God(e)) {
			v->life = v->maxLife;
		}
	}
	cScorerItem *item = cScorer_FindItem(Scorer_Get(0), g_RegenerationItem);
	if (item) {
		double val = (double)(v->life - g_LifeToLaunchCallback) / (double)(v->maxLife - g_LifeToLaunchCallback);
		item->vftable->Var(item, "Value", 1, NULL, &val, NULL);
	}
}

void __thiscall ShipRegen_Set(void *shipRegen, cEntity *ship, float speed, float endSpeed) {
	cShipRegen *self = shipRegen;

	if (!ship || !ship->vftable->IsType(ship, TYEN_WITH_LIFE) || Entity_LifeI(ship) <= 0) {
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
	self->ship->life = Entity_LifeI(ship);
}

void __thiscall ShipRegen_Update(cShipRegen *self, float dt) {
	if (g_pApp->flags || g_SplitScreenGame) {
		return;
	}
	if (self->ship) {
		ShipRegen_UpdateShip(self, dt);
	}
	cEntity *usr = cWorld_GetUsrEntity(g_pWorld, 0);
	if (!usr || !usr->vftable->IsType(usr, TYEN_WITH_LIFE) || Entity_LifeI(usr) <= 0) {
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
	self->noLoss = self->life == Entity_LifeI(usr);
	if (self->noLoss) {
		self->time = onFoot ? g_RegenerationCharTimeRate : g_RegenerationTimeRate;
		if (World_IsEasySingle()) {
			self->time = self->time * 0.5f;
		}
		int max = usr->vftable->IsType(usr, TYEN_CAR) ? Veh(usr)->minLife : g_StdMaxLife;
		if (!(max > Entity_LifeI(usr))) {
			self->noLoss = 0;
		}
		if (self->noLoss && !Entity_God(usr)) {
			Entity_LifeI(usr)++;
			self->life = Entity_LifeI(usr);
			return;
		}
	} else {
		self->time = onFoot ? g_RegenerationCharStartTime : g_RegenerationStartTime;
		if (World_IsEasySingle()) {
			self->time = self->time * 0.5f;
		}
	}
	self->life = Entity_LifeI(usr);
}
