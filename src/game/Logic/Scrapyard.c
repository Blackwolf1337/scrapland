/*
 * Logic/Scrapyard.cpp of the game: the ship groups that guard their lairs in the scrapyards ("Mosquitos",
 * "MadHunters"; cShipGroup of cLogicControl, include/scrap/logic.h). Every 0.5 s each ship of the list attacks the
 * intruder (the ship of MosquitosTriggeredShipName / MadHuntersTriggeredShipName, else the player in its ship):
 * up to MinPerc...AttackingIntruder of Max... of them at once, the others a random ship near their lair. A ship
 * farther than Max...Dist from its lair flies back to it, and is deleted when it gets there; a ship without a lair
 * is deleted at once. The logic of a ship (cScrapyardLocalLogic) only has the script variable "LairPos". A dead
 * ship gives the money reward of its group.
 */
#include <string.h>

#include "scrap/aivehicle.h"
#include "scrap/chartype.h"
#include "scrap/hashtable.h"
#include "scrap/logic.h"
#include "scrap/nodestraffic.h"
#include "scrap/scrap.h"
#include "scrap/world.h"

#define CSCRAPYARDLOCALLOGIC_VFTABLE GAME_VFTABLE(cScrapyardLocalLogic_vftable)
#define SHIPGROUP_SPHERE_NATIVE 0x157f10
#define SHIPGROUP_DEATH_NATIVE 0x158020
#define SHIPGROUP_MAKELOGIC 0x158400
#define SHIPGROUP_ONAIEND 0x157e60
#define SHIPGROUP_MAX_FOUND 10

#define g_MaxMosquitosDist GAME_GLOBAL_IN(float, GVar_MaxMosquitosDist, 16)
#define g_MaxMosquitos GAME_GLOBAL_IN(int, GVar_MaxMosquitos, 16)
#define g_MinPercMosquitosAttackingIntruder GAME_GLOBAL_IN(float, GVar_MinPercMosquitosAttackingIntruder, 16)
#define g_MosquitosTriggeredShipName GAME_GLOBAL_IN(char *, GVar_MosquitosTriggeredShipName, 16)
#define g_MaxMadHuntersDist GAME_GLOBAL_IN(float, GVar_MaxMadHuntersDist, 16)
#define g_MaxMadHunters GAME_GLOBAL_IN(int, GVar_MaxMadHunters, 16)
#define g_MinPercMadHuntersAttackingIntruder GAME_GLOBAL_IN(float, GVar_MinPercMadHuntersAttackingIntruder, 16)
#define g_MadHuntersTriggeredShipName GAME_GLOBAL_IN(char *, GVar_MadHuntersTriggeredShipName, 16)
#define g_OnMosquitoDeathMoney GAME_GLOBAL_IN(int, GVar_OnMosquitoDeathMoney, 16)
#define g_OnMadHunterDeathMoney GAME_GLOBAL_IN(int, GVar_OnMadHunterDeathMoney, 16)
/* the ships the sphere native found around a lair */
#define g_ShipGroupFound GAME_GLOBAL(int, g_ShipGroupFound)
#define g_ShipGroupShips (GAME_ARRAY(cEntity *, g_ShipGroupShips))	/* [SHIPGROUP_MAX_FOUND] */
/* the callback of the AI when the move ends (rva 0x15c of the AI) */
#define AIV_OnEnd(ai) (((cAIVehicle *)(ai))->onArrive)

cShipGroupData *__thiscall cShipGroupData_ctor(cShipGroupData *self, const char *list) {
	cPy_Function_ctor(&self->onSphere);
	self->lairs.numSlots = 0x1c9;
	self->lairs.slots = New_Array(0x1c9, 4);
	memset(self->lairs.slots, 0, 0x724);
	self->lairs.locked = 0;
	self->lairs.caseSensitive = 1;
	self->listName = list;
	self->list = NULL;
	self->timer = 0.0f;
	cPy_Function_SetNative(&self->onSphere, (void *)ShipGroup_SphereNative);
	return self;
}

cShipGroup *__thiscall cShipGroup_ctor(cShipGroup *self, const char *list) {
	cShipGroupData *d = operator_new(sizeof(*d));

	self->d = d ? cShipGroupData_ctor(d, list) : NULL;
	return self;
}

void __fastcall cShipGroup_dtor(cShipGroup *self) {
	cShipGroupData *d = self->d;

	if (d) {
		WorldStrTable_Clear(&d->lairs);
		cPy_Function_dtor(&d->onSphere);
		operator_delete_sized(d, sizeof(*d));
	}
}

cScrapyardLocalLogic *__thiscall cScrapyardLocalLogic_Delete(cScrapyardLocalLogic *self, byte flags) {
	self->vftable = CSCRAPYARDLOCALLOGIC_VFTABLE;
	cLocalLogic_dtor(self);
	if (flags & 1) {
		operator_delete_sized(self, sizeof(*self));
	}
	return self;
}

/* the lair of the ship of that name, added or changed (the inlined Add without the warning) */
void __thiscall cShipGroupData_SetLair(cShipGroupData *self, const char *name, const vec3_t *pos) {
	HashTable *t = &self->lairs;

	if (WorldStrTable_Find(t, name, HashTable_Slot(t, name))) {
		WorldStrTable_Set(t, name, (const cValue12 *)pos);
		return;
	}
	CHECK(!t->locked);
	uint32_t slot = HashTable_Slot(t, name);
	if (!WorldStrTable_Find(t, name, slot)) {
		WorldStrTable_Insert(t, name, slot, (const cValue12 *)pos);
	}
}

void __thiscall cShipGroup_SetLair(cShipGroup *self, const char *name, const vec3_t *pos) {
	cShipGroupData_SetLair(self->d, name, pos);
}

byte __thiscall cShipGroup_DeleteLair(cShipGroup *self, const char *name) {
	return WorldStrTable_Delete(&self->d->lairs, name);
}

void __thiscall cShipGroup_GetLair(cShipGroup *self, const char *name, vec3_t *out) {
	HashTable *t = &self->d->lairs;
	cWorldStrItem *it = WorldStrTable_Find(t, name, HashTable_Slot(t, name));

	if (it) {
		memcpy(out, &it->value, sizeof(*out));
	}
}

/* the group of a ship: the mosquitos for the list "Mosquitos", else the mad hunters */
static cShipGroup *ShipGroup_Of(cEntity *e) {
	if (cEntity_GetListName(e) && !strcmp(cEntity_GetListName(e), "Mosquitos")) {
		return &g_pWorld->logic.mosquitos;
	}
	return &g_pWorld->logic.madHunters;
}

void __fastcall cShipGroupData_Think(cShipGroupData *self) {
	vec3_t tmp;
	Nto1Node *n = Logic_ListFirst(self->list);
	float maxDist;
	int maxAttack;
	const char *trig;

	if (!n) {
		return;
	}
	g_ShipGroupFound = 0;
	int count = 0;
	cEntity *usr = ChType_Usr0();
	if (!strcmp(self->list->name, "Mosquitos")) {
		maxDist = g_MaxMosquitosDist;
		maxAttack = (int)((float)g_MaxMosquitos * g_MinPercMosquitosAttackingIntruder);
		trig = g_MosquitosTriggeredShipName;
	} else {
		maxDist = g_MaxMadHuntersDist;
		maxAttack = (int)((float)g_MaxMadHunters * g_MinPercMadHuntersAttackingIntruder);
		trig = g_MadHuntersTriggeredShipName;
	}
	cEntity *target = World_FindEntity(trig);
	if (!target || !target->vftable->IsType(target, TYEN_CAR)) {
		if (!usr || !usr->vftable->IsType(usr, TYEN_CAR)) {
			return;
		}
		target = usr;
	}
	do {
		cEntity *ship = Logic_AsCar(n->data);
		CHECK(ship->vftable->IsType(ship, TYEN_CAR) && "attackerShip->IsType(TYEN_VEHICLE)");
		cAIVehicle *ai = ship->vftable->GetAI(ship);
		CHECK(ai && "ai = attackerShip->GetAI()");
		int st = cAIVehicle_GetState(ai);
		if (st != 1) {
			HashTable *t = &self->lairs;
			cWorldStrItem *it = WorldStrTable_Find(t, ship->name, HashTable_Slot(t, ship->name));
			if (!it) {
				cEntity_Del(ship);
				n = Logic_ListNext(n);
				continue;
			}
			vec3_t lair;
			memcpy(&lair, &it->value, sizeof(lair));
			const vec3_t *sp = ship->vftable->GetPos(ship, &tmp);
			float dx = lair.x - sp->x;
			float dy = lair.y - sp->y;
			float dz = lair.z - sp->z;
			float d = (float)__builtin_sqrt((double)(dy * dy + dx * dx + dz * dz));
			if (d > maxDist) {
				cAIVehicle_SetState(ai, 1, lair, 3000.0, 1, 1.0, 20.0, 20.0, 1);
				AIV_OnEnd(ai) = (void *)ShipGroup_OnAIEnd;
			} else if (st != 2 && (st != 7 || (usr && usr->vftable->IsType(usr, TYEN_CAR)))) {
				if (count < maxAttack) {
					count++;
					cAIVehicle_SetState(ai, 2, target);
				} else {
					cWorld_SphereCallF(g_pWorld, 1, &lair, maxDist, &self->onSphere, 1);
					if (g_ShipGroupFound > 0) {
						cAIVehicle_SetState(ai, 2, g_ShipGroupShips[crt_rand() % g_ShipGroupFound]);
					} else {
						cAIVehicle_SetState(ai, 2, target);
					}
				}
			}
		}
		cEntity *t = Logic_AttackTarget(ai);
		if (cAIVehicle_GetState(ai) != AIV_STATE_CHASE && t && !t->vftable->GetAI(t) && usr &&
			usr->vftable->IsType(usr, TYEN_WALKCHAR)) {
			cAIVehicle_SetState(ai, AIV_STATE_CHASE, usr, (double)g_AIChaseDefault1, (double)g_AIChaseDefault2);
		}
		n = Logic_ListNext(n);
	} while (n);
}

void __fastcall cShipGroup_Init(cShipGroup *self) {
	cShipGroupData *d = self->d;

	d->list = (cEntityListData *)cWorld_NewEntityList(g_pWorld, d->listName);
	cPy_Function_SetNative(&d->list->onDeath, (void *)ShipGroup_DeathNative);
	d->list->func = (int (*)(void))(void *)ShipGroup_MakeLogic;
}

/* the AI callback of a ship back at its lair: the ship goes */
byte ShipGroup_OnAIEnd(cAIVehicle *ai) {
	if (!ai) {
		return 0;
	}
	cEntity *v = cAIVehicle_GetVehicle(ai);
	WorldStrTable_Delete(&ShipGroup_Of(v)->d->lairs, v->name);
	cEntity_Del(cAIVehicle_GetVehicle(ai));
	return 1;
}

/* the sphere native: a ship with an AI or the player (not a mosquito) the ships may attack */
void ShipGroup_SphereNative(cAbstractFile *params) {
	char name[0x80];

	cAbstractFile_ReadString(params, name, sizeof(name));
	if (g_ShipGroupFound >= SHIPGROUP_MAX_FOUND || !name[0]) {
		return;
	}
	cEntity *e = World_FindEntity(name);
	if (!e) {
		return;
	}
	cEntity *usr = cWorld_GetUsrEntity(g_pWorld, 0);
	if (!e->vftable->GetAI(e) && e != usr) {
		return;
	}
	const char *list = cEntity_GetListName(e);
	if (list && list != LOGIC_EMPTY && !strcmp(list, "Mosquitos")) {
		return;
	}
	g_ShipGroupShips[g_ShipGroupFound] = e;
	g_ShipGroupFound++;
}

/* the death native of the lists: the money reward, the lair goes */
void ShipGroup_DeathNative(cAbstractFile *params) {
	char name[0x80];
	cShipGroup *g;
	int money;

	cAbstractFile_ReadString(params, name, sizeof(name));
	cEntity *e = Logic_AsCar(World_FindEntity(name));
	if (cEntity_GetListName(e) && !strcmp(cEntity_GetListName(e), "Mosquitos")) {
		g = &g_pWorld->logic.mosquitos;
		SaveVar_Inc("Stats.Mosquito", 1);
		money = g_OnMosquitoDeathMoney;
	} else {
		g = &g_pWorld->logic.madHunters;
		SaveVar_Inc("Stats.MadHunter", 1);
		money = g_OnMadHunterDeathMoney;
	}
	g_pWorld->money += money;
	WorldStrTable_Delete(&g->d->lairs, e->name);
}

void __thiscall cShipGroup_Update(cShipGroup *self, float dt) {
	cShipGroupData *d = self->d;
	float t = d->timer + dt;

	d->timer = t;
	if (!(0.5f >= t)) {
		d->timer = 0.0f;
		cShipGroupData_Think(d);
	}
}

/* vf06: "LairPos", the lair of the ship */
byte __thiscall cScrapyardLocalLogic_VarV(cScrapyardLocalLogic *self, const char *name, double *x, double *y, double *z,
	byte set) {
	if (strcmp(name, "LairPos")) {
		return cLocalLogic_VarVNone(self, name, x, y, z, set);
	}
	cEntity *e = Logic_AsCar(self->parent);
	cShipGroup *g = ShipGroup_Of(e);
	vec3_t v;
	if (set) {
		v.x = (float)*x;
		v.y = (float)*y;
		v.z = (float)*z;
		cShipGroupData_SetLair(g->d, e->name, &v);
		return 1;
	}
	/* the original leaves v uninitialized for a ship without a lair */
	v.x = v.y = v.z = 0.0f;
	HashTable *t = &g->d->lairs;
	cWorldStrItem *it = WorldStrTable_Find(t, e->name, HashTable_Slot(t, e->name));
	if (it) {
		memcpy(&v, &it->value, sizeof(v));
	}
	*x = (double)v.x;
	*y = (double)v.y;
	*z = (double)v.z;
	return 1;
}

/* the maker of the logics of the lists (cEntityListData.func) */
cLocalLogic *ShipGroup_MakeLogic(cEntity *e) {
	if (!e || !e->vftable->IsType(e, TYEN_CAR) || !e->vftable->IsType(e, TYEN_CAR)) {
		CHECK(!"e && e->IsType(TYEN_VEHICLE)");
	}
	cLocalLogic *l = operator_new(sizeof(*l));
	if (!l) {
		return NULL;
	}
	l->parent = e;
	l->vftable = CSCRAPYARDLOCALLOGIC_VFTABLE;
	return l;
}
