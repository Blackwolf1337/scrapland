/*
 * Logic/DeathMatch.cpp of the game: the ships of a deathmatch ("DeathMatch", include/scrap/logic.h). Every 0.5 s a
 * free ship, or one after a target without an AI while the player walks, attacks a random other ship or the
 * player; a ship hit below half its life turns against the attacker.
 */
#include "scrap/chartype.h"
#include "scrap/vehicleentity.h"

cDeathMatch *__fastcall cDeathMatch_ctor(cDeathMatch *self) {
	self->list = NULL;
	self->timer = 0.0f;
	return self;
}

void cDeathMatch_dtor(void) {
}

/* a random ship of the list or the player (the count of the list plus one), not exclude */
cEntity *__thiscall cDeathMatch_PickTarget(cDeathMatch *self, cEntity *exclude) {
	cEntity *usr = ChType_Usr0();
	Nto1Node *n = Logic_ListFirst(self->list);
	int num = Logic_Count(((EntityDNode *)n)->numElems);
	int r = crt_rand() % (num + 1);

	if (r == num && usr && usr->vftable->IsType(usr, TYEN_CAR)) {
		return Logic_AsCar(usr);
	}
	for (int i = 0; i != r; i++) {
		n = Logic_ListNext(n);
		if (!n) {
			return NULL;
		}
	}
	cEntity *e = Logic_AsCar(n->data);
	if (e == exclude) {
		return NULL;
	}
	return e;
}

void __fastcall cDeathMatch_Think(cDeathMatch *self) {
	cEntity *usr = ChType_Usr0();

	for (Nto1Node *n = Logic_ListFirst(self->list); n; n = Logic_ListNext(n)) {
		cEntity *e = n->data;
		CHECK(e->vftable->IsType(e, TYEN_CAR));
		cAIVehicle *ai = e->vftable->GetAI(e);
		CHECK(ai);
		int st = cAIVehicle_GetState(ai);
		if (st != 0 && st != 3) {
			cEntity *t = Logic_AttackTarget(ai);
			if (!t || t->vftable->GetAI(t) || !usr || !usr->vftable->IsType(usr, TYEN_WALKCHAR)) {
				continue;
			}
		}
		cEntity *t = cDeathMatch_PickTarget(self, e);
		if (t) {
			cAIVehicle_SetState(ai, AIV_STATE_ATTACK, t);
		}
	}
}

void __fastcall cDeathMatch_Init(cDeathMatch *self) {
	self->list = cWorld_NewEntityList(g_pWorld, "DeathMatch");
	cPy_Function_SetNative(&self->list->onDamage, (nativefunc_t)GAME_FUNC(DeathMatch_OnDamage, 0x1448b0));
}

/* the native of the hits: the ship and its attacker */
void DeathMatch_OnDamage(cAbstractFile *params) {
	char name[0x80], attacker[0x80];

	cAbstractFile_ReadString(params, name, 0x80);
	cAbstractFile_ReadString(params, attacker, 0x80);
	if (!name[0]) {
		return;
	}
	cEntity *v = World_FindEntity(name);
	if (!v || !v->vftable->IsType(v, TYEN_CAR)) {
		return;
	}
	CHECK(v->vftable->IsType(v, TYEN_CAR));
	cEntity *a = Logic_FindCar(attacker);
	if (!a) {
		return;
	}
	CHECK(a->vftable->IsType(a, TYEN_CAR));
	cAIVehicle *ai = v->vftable->GetAI(v);
	CHECK(ai);
	cVehicleEntity *veh = (cVehicleEntity *)v;
	if (veh->wl.life < veh->maxLife / 2) {
		cAIVehicle_SetState(ai, AIV_STATE_ATTACK, a);
	}
}

void __thiscall cDeathMatch_Update(cDeathMatch *self, float dt) {
	float t = self->timer + dt;

	self->timer = t;
	if (!(0.5f >= t)) {
		self->timer = 0.0f;
		cDeathMatch_Think(self);
	}
}
