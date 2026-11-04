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

#include "scrap/gvar.h"
#include "scrap/aivehicle.h"
#include "scrap/chartype.h"
#include "scrap/hashtable.h"
#include "scrap/logic.h"
#include "scrap/nodestraffic.h"
#include "scrap/scrap.h"
#include "scrap/world.h"
#include "scrap/staticinit.h"

static const cLocalLogicVtbl cScrapyardLocalLogic_vftable;
#define CSCRAPYARDLOCALLOGIC_VFTABLE (&cScrapyardLocalLogic_vftable)
#define SHIPGROUP_MAX_FOUND 10

static cGVar GVar_MaxMosquitosDist;
#define g_MaxMosquitosDist (GVar_MaxMosquitosDist.value.f)
static cGVar GVar_MaxMosquitos;
#define g_MaxMosquitos (GVar_MaxMosquitos.value.i)
static cGVar GVar_MinPercMosquitosAttackingIntruder;
#define g_MinPercMosquitosAttackingIntruder (GVar_MinPercMosquitosAttackingIntruder.value.f)
static cGVar GVar_MosquitosTriggeredShipName;
#define g_MosquitosTriggeredShipName (GVar_MosquitosTriggeredShipName.value.s)
static cGVar GVar_MaxMadHuntersDist;
#define g_MaxMadHuntersDist (GVar_MaxMadHuntersDist.value.f)
static cGVar GVar_MaxMadHunters;
#define g_MaxMadHunters (GVar_MaxMadHunters.value.i)
static cGVar GVar_MinPercMadHuntersAttackingIntruder;
#define g_MinPercMadHuntersAttackingIntruder (GVar_MinPercMadHuntersAttackingIntruder.value.f)
static cGVar GVar_MadHuntersTriggeredShipName;
#define g_MadHuntersTriggeredShipName (GVar_MadHuntersTriggeredShipName.value.s)
static cGVar GVar_OnMosquitoDeathMoney;
#define g_OnMosquitoDeathMoney (GVar_OnMosquitoDeathMoney.value.i)
static cGVar GVar_OnMadHunterDeathMoney;
#define g_OnMadHunterDeathMoney (GVar_OnMadHunterDeathMoney.value.i)
/* the ships the sphere native found around a lair */
/* the callback of the AI when the move ends (rva 0x15c of the AI) */
#define AIV_OnEnd(ai) (((cAIVehicle *)(ai))->onArrive)

static void ShipGroup_SphereNative(cAbstractFile *params);
static cShipGroupData *cShipGroupData_ctor(cShipGroupData *self, const char *list) {
	cPy_Function_ctor(&self->onSphere);
	self->lairs.numSlots = 0x1c9;
	self->lairs.slots = New_Array(0x1c9, sizeof(*self->lairs.slots));
	memset(self->lairs.slots, 0, 0x1c9 * sizeof(*self->lairs.slots));
	self->lairs.locked = 0;
	self->lairs.caseSensitive = 1;
	self->listName = list;
	self->list = NULL;
	self->timer = 0.0f;
	cPy_Function_SetNative(&self->onSphere, (void *)ShipGroup_SphereNative);
	return self;
}

cShipGroup *cShipGroup_ctor(cShipGroup *self, const char *list) {
	cShipGroupData *d = operator_new(sizeof(*d));

	self->d = d ? cShipGroupData_ctor(d, list) : NULL;
	return self;
}

void cShipGroup_dtor(cShipGroup *self) {
	cShipGroupData *d = self->d;

	if (d) {
		WorldStrTable_Clear(&d->lairs);
		cPy_Function_dtor(&d->onSphere);
		operator_delete(d);
	}
}

static void cScrapyardLocalLogic_Free(cScrapyardLocalLogic *self) {
	cLocalLogic_dtor(self);
	operator_delete(self);
}

/* the lair of the ship of that name, added or changed (the inlined Add without the warning) */
static void cShipGroupData_SetLair(cShipGroupData *self, const char *name, const vec3_t *pos) {
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

/* the group of a ship: the mosquitos for the list "Mosquitos", else the mad hunters */
static cShipGroup *ShipGroup_Of(cEntity *e) {
	if (cEntity_GetListName(e) && !strcmp(cEntity_GetListName(e), "Mosquitos")) {
		return &g_pWorld->logic.mosquitos;
	}
	return &g_pWorld->logic.madHunters;
}

static int g_ShipGroupFound;
static cEntity *g_ShipGroupShips[10];

static byte ShipGroup_OnAIEnd(cAIVehicle *ai);
static void cShipGroupData_Think(cShipGroupData *self) {
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
	if (!target || !Entity_IsType(target, TYEN_CAR)) {
		if (!usr || !Entity_IsType(usr, TYEN_CAR)) {
			return;
		}
		target = usr;
	}
	do {
		cEntity *ship = Logic_AsCar(n->data);
		CHECK(Entity_IsType(ship, TYEN_CAR) && "attackerShip->IsType(TYEN_VEHICLE)");
		cAIVehicle *ai = Entity_GetAI(ship);
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
			const vec3_t *sp = Entity_GetPos(ship, &tmp);
			float dx = lair.x - sp->x;
			float dy = lair.y - sp->y;
			float dz = lair.z - sp->z;
			float d = (float)__builtin_sqrt((double)(dy * dy + dx * dx + dz * dz));
			if (d > maxDist) {
				cAIVehicle_SetState(ai, AIV_GO_TO, lair, 3000.0, 1, 1.0, 20.0, 20.0, 1);
				AIV_OnEnd(ai) = (void *)ShipGroup_OnAIEnd;
			} else if (st != 2 && (st != 7 || (usr && Entity_IsType(usr, TYEN_CAR)))) {
				if (count < maxAttack) {
					count++;
					cAIVehicle_SetState(ai, AIV_FIGHTING, target);
				} else {
					cWorld_SphereCallF(g_pWorld, 1, &lair, maxDist, &self->onSphere, 1);
					if (g_ShipGroupFound > 0) {
						cAIVehicle_SetState(ai, AIV_FIGHTING, g_ShipGroupShips[crt_rand() % g_ShipGroupFound]);
					} else {
						cAIVehicle_SetState(ai, AIV_FIGHTING, target);
					}
				}
			}
		}
		cEntity *t = Logic_AttackTarget(ai);
		if (cAIVehicle_GetState(ai) != AIV_STATE_CHASE && t && !Entity_GetAI(t) && usr &&
			Entity_IsType(usr, TYEN_WALKCHAR)) {
			cAIVehicle_SetState(ai, AIV_STATE_CHASE, usr, (double)g_AIChaseDefault1, (double)g_AIChaseDefault2);
		}
		n = Logic_ListNext(n);
	} while (n);
}

static void ShipGroup_DeathNative(cAbstractFile *params);
static cLocalLogic *ShipGroup_MakeLogic(cEntity *e);
void cShipGroup_Init(cShipGroup *self) {
	cShipGroupData *d = self->d;

	d->list = (cEntityListData *)cWorld_NewEntityList(g_pWorld, d->listName);
	cPy_Function_SetNative(&d->list->onDeath, (void *)ShipGroup_DeathNative);
	d->list->func = (int (*)(void))(void *)ShipGroup_MakeLogic;
}

/* the AI callback of a ship back at its lair: the ship goes */
static byte ShipGroup_OnAIEnd(cAIVehicle *ai) {
	if (!ai) {
		return 0;
	}
	cEntity *v = cAIVehicle_GetVehicle(ai);
	WorldStrTable_Delete(&ShipGroup_Of(v)->d->lairs, v->name);
	cEntity_Del(cAIVehicle_GetVehicle(ai));
	return 1;
}

/* the sphere native: a ship with an AI or the player (not a mosquito) the ships may attack */
static void ShipGroup_SphereNative(cAbstractFile *params) {
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
	if (!Entity_GetAI(e) && e != usr) {
		return;
	}
	const char *list = cEntity_GetListName(e);
	if (list && list != str_Empty && !strcmp(list, "Mosquitos")) {
		return;
	}
	g_ShipGroupShips[g_ShipGroupFound] = e;
	g_ShipGroupFound++;
}

/* the death native of the lists: the money reward, the lair goes */
static void ShipGroup_DeathNative(cAbstractFile *params) {
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

void cShipGroup_Update(cShipGroup *self, float dt) {
	cShipGroupData *d = self->d;
	float t = d->timer + dt;

	d->timer = t;
	if (!(0.5f >= t)) {
		d->timer = 0.0f;
		cShipGroupData_Think(d);
	}
}

/* vf06: "LairPos", the lair of the ship */
static byte cScrapyardLocalLogic_VarV(cScrapyardLocalLogic *self, const char *name, double *x, double *y, double *z,
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
static cLocalLogic *ShipGroup_MakeLogic(cEntity *e) {
	if (!e || !Entity_IsType(e, TYEN_CAR) || !Entity_IsType(e, TYEN_CAR)) {
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

/* the vtable of cScrapyardLocalLogic */
static const cLocalLogicVtbl cScrapyardLocalLogic_vftable = {
	.Free = cScrapyardLocalLogic_Free,
	.GetName = cLocalLogic_GetName,
	.AcceptsArrow = cLocalLogic_AcceptsArrowTrue,
	.VarI = cLocalLogic_VarIFalse,
	.VarF = cLocalLogic_VarFFalse,
	.VarS = cLocalLogic_VarSFalse,
	.VarV = cScrapyardLocalLogic_VarV,
};

/* the dynamic initializers of the globals here, run in the order of src/standalone/crtxc.inc */
INIT_GVAR_INT(Init_GVar_OnMosquitoDeathMoney, &GVar_OnMosquitoDeathMoney, "OnMosquitoDeathMoney", 0, "Mosquito death money reward", 100)
INIT_GVAR_INT(Init_GVar_OnMadHunterDeathMoney, &GVar_OnMadHunterDeathMoney, "OnMadHunterDeathMoney", 0, "Mad hunter death money reward", 300)
INIT_GVAR_FLOAT(Init_GVar_MinPercMosquitosAttackingIntruder, &GVar_MinPercMosquitosAttackingIntruder, "MinPercMosquitosAttackingIntruder", 0, "Min perc mosquitos attacking intruder", 0.3f)
INIT_GVAR_FLOAT(Init_GVar_MinPercMadHuntersAttackingIntruder, &GVar_MinPercMadHuntersAttackingIntruder, "MinPercMadHuntersAttackingIntruder", 0, "Min perc mad hunters attacking intruder", 0.5f)
INIT_GVAR_INT(Init_GVar_MaxMosquitos, &GVar_MaxMosquitos, "MaxMosquitos", 0, "Max mosquitos", 10)
INIT_GVAR_FLOAT(Init_GVar_MaxMosquitosDist, &GVar_MaxMosquitosDist, "MaxMosquitosDist", 0, "Max mosquitos dist", 30000.0f)
INIT_GVAR_INT(Init_GVar_MaxMadHunters, &GVar_MaxMadHunters, "MaxMadHunters", 0, "Max mad hunters", 2)
INIT_GVAR_FLOAT(Init_GVar_MaxMadHuntersDist, &GVar_MaxMadHuntersDist, "MaxMadHuntersDist", 0, "Max mad hunters dist", 70000.0f)
FINI_GVAR_STRING(Fini_GVar_MosquitosTriggeredShipName, &GVar_MosquitosTriggeredShipName)
INIT_GVAR_STRING(Init_GVar_MosquitosTriggeredShipName, Fini_GVar_MosquitosTriggeredShipName, &GVar_MosquitosTriggeredShipName, "MosquitosTriggeredShipName", 0, "Ship name that triggers mosquitos appearance", "")
FINI_GVAR_STRING(Fini_GVar_MadHuntersTriggeredShipName, &GVar_MadHuntersTriggeredShipName)
INIT_GVAR_STRING(Init_GVar_MadHuntersTriggeredShipName, Fini_GVar_MadHuntersTriggeredShipName, &GVar_MadHuntersTriggeredShipName, "MadHuntersTriggeredShipName", 0, "Ship name that triggers mad hunters appearance", "")
