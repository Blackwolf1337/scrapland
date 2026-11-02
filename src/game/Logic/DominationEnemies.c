/*
 * Logic/DominationEnemies.cpp of the game: the enemy side of the domination mode ("DominationEnemies",
 * include/scrap/logic.h). Every 0.5 s the ships go through cDominationLogic_UpdateShip, except the leader (the
 * ship of the engine variable LeaderName): it goes for the baton while nobody has it, attacks the player while the
 * player carries it, and takes it to the nearest zone its side does not hold while it has it.
 */
#include "scrap/chartype.h"
#include "scrap/filepak.h"
#include "scrap/hashtable.h"
#include "scrap/staticinit.h"

#define g_DominLeaderName (GVar_LeaderName.value.s)
#define g_OnDominEnemyDeath (((cPy_Function *)&GVar_OnDominEnemyDeath.func))
static cGVarFunction GVar_OnDominEnemyDeath;
static cGVar GVar_LeaderName;

/* defined further down or in another file without a header */
static void DominationEnemies_OnDeath(cAbstractFile *params);

/* the arguments of the AI state GO_TO the leader gets (the constants of the original) */
static void DominE_GoTo(cAIVehicle *ai, vec3_t pos, double dist) {
	cAIVehicle_SetState(ai, 1, pos, dist, 1, 1.0, 20.0, 20.0, 0);
}

static void cDominationSide_Think(cDominationSide *self) {
	vec3_t tmp;

	if (!Logic_ListFirst(self->list)) {
		return;
	}
	cEntity *usr = Logic_AsCar(ChType_Usr0());
	cEntity *leader = cDominationEnemiesLogic_GetLeader(NULL);
	for (Nto1Node *n = Logic_ListFirst(self->list); n; n = Logic_ListNext(n)) {
		cEntity *v = n->data;
		if (v && !v->vftable->IsType(v, TYEN_CAR)) {
			v = NULL;
		}
		CHECK(v && "v = AsVehicle (node->GetNode())");
		cAIVehicle *ai = v->vftable->GetAI(v);
		CHECK(ai && "ai = v->GetAI()");
		int st = cAIVehicle_GetState(ai);
		if (leader != v) {
			cDominationLogic_UpdateShip(self->owner, v, leader, usr);
			continue;
		}
		if (g_DominNumZones <= 0) {
			continue;
		}
		int baton = g_DominBatonState;
		if (baton == 0) {
			if (st != 1) {
				DominE_GoTo(ai, g_DominBatonPos, 1800.0);
			}
		} else if (baton == 2) {
			if (usr && st != AIV_STATE_ATTACK) {
				cAIVehicle_SetState(ai, AIV_STATE_ATTACK, usr);
			}
		} else if (baton == 1) {
			if (st == 0 || st == 3 || (st == 1 && self->zone >= 0 && (g_DominZones[self->zone].state == 2 ||
				g_DominZones[self->zone].state == 4))) {
				int z = cDominationLogic_NearestZone((cDominationLogic *)self, leader->vftable->GetPos(leader,
					&tmp));
				self->zone = z;
				if (z >= 0) {
					cEntity *ze = g_DominZones[z].zone;
					DominE_GoTo(ai, *ze->vftable->GetPos(ze, &tmp), 3000.0);
				}
			}
		}
	}
}

/* vf03 */
cEntityListData *cDominationEnemiesLogic_GetList(cDominationLogic *base) {
	cDominationEnemiesLogic *self = (cDominationEnemiesLogic *)base;
	return self->side->list;
}

/* the leader ship of the enemies (LeaderName) or NULL */
cEntity *cDominationEnemiesLogic_GetLeader(cDominationEnemiesLogic *self) {
	(void)self;
	return Logic_AsCar(World_FindEntity(g_DominLeaderName));
}

/* vf01 */
void cDominationEnemiesLogic_Init(cDominationLogic *base) {
	cDominationEnemiesLogic *self = (cDominationEnemiesLogic *)base;
	cDominationSide *s = self->side;

	CHECK(s->owner && "el");
	s->list = cWorld_NewEntityList(g_pWorld, "DominationEnemies");
	cPy_Function_SetNative(&s->list->onDeath, (nativefunc_t)DominationEnemies_OnDeath);
}

/* the deaths of both sides: the zone of the ship is free again, then the callback with the ship and its killer */
void Domination_OnDeath(cAbstractFile *params, cDominationLogic *side,
	cEntity *(*release)(cDominationLogic *, const char *), cPy_Function *cb) {
	char name[0x80], killer[0x80];

	cAbstractFile_ReadString(params, name, 0x80);
	cAbstractFile_ReadString(params, killer, 0x80);
	release(side, name);
	if (cPy_Function_IsValid(cb)) {
		cPy_Function_AddParamS(cb, name);
		cPy_Function_AddParamS(cb, killer);
		cPy_Function_Execute(cb, 0);
	}
}

/* the native of the deaths (onDeath of the list) */
static cEntity *cDominationEnemiesLogic_ReleaseZone(cDominationLogic *self, const char *name);
static void DominationEnemies_OnDeath(cAbstractFile *params) {
	Domination_OnDeath(params, &g_pWorld->logic.dominEnemies.base, cDominationEnemiesLogic_ReleaseZone,
		g_OnDominEnemyDeath);
}

/* the zone of the ship taken from the table; a zone of the side on the way (going) is held (held) again */
cEntity *Domination_ReleaseZone(cDominationLogic *self, const char *name, int going, int held) {
	cNameItem *it = NameTable_Find(&self->assigned, name, HashTable_Slot(&self->assigned, name));

	if (!it) {
		return NULL;
	}
	cEntity *zone = (cEntity *)it->value;
	for (int i = 0; i < MAX_DOMIN_ZONES; i++) {
		if (g_DominZones[i].zone == zone) {
			if (g_DominZones[i].state == going) {
				g_DominZones[i].state = held;
			}
			break;
		}
	}
	NameTable_Delete(&self->assigned, name);
	return zone;
}

/* the zone of the ship of that name taken from the table (a zone on the way of the side is free again) */
static cEntity *cDominationEnemiesLogic_ReleaseZone(cDominationLogic *self, const char *name) {
	return Domination_ReleaseZone(self, name, 4, 2);
}

/* vf02 */
void cDominationEnemiesLogic_Update(cDominationLogic *base, float dt) {
	cDominationEnemiesLogic *self = (cDominationEnemiesLogic *)base;
	cDominationSide *s = self->side;
	float t = s->timer + dt;

	s->timer = t;
	if (!(0.5f >= t)) {
		s->timer = 0.0f;
		cDominationSide_Think(s);
	}
}

static cGVarFunction GVar_OnDominEnemyDeath;
/* the engine variable LeaderName (the leader ship of the enemies) and the death callbacks of the sides */
static cGVar GVar_LeaderName;

/* the dynamic initializers of the globals here, run in the order of src/standalone/crtxc.inc */
FINI_CALL(Fini_GVar_OnDominEnemyDeath, &GVar_OnDominEnemyDeath.func, cPy_Function_dtor)
INIT_GVAR_FUNCTION(Init_GVar_OnDominEnemyDeath, Fini_GVar_OnDominEnemyDeath, &GVar_OnDominEnemyDeath, "OnDominEnemyDeath", 0, "Called when an enemy dies", "")
FINI_GVAR_STRING(Fini_GVar_LeaderName, &GVar_LeaderName)
INIT_GVAR_STRING(Init_GVar_LeaderName, Fini_GVar_LeaderName, &GVar_LeaderName, "LeaderName", 0, "Leader name", "")
