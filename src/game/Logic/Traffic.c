/*
 * Logic/Traffic.cpp of the game: the traffic of the outdoor maps (cEnemiesTraffic of cLogicControl,
 * include/scrap/logic.h) in four lists: "Traffic", "FightingTraffic" (two ships of the traffic that fight each
 * other where the player can see them, ProbTrafficFighting), "ChasingTraffic" (chase the player) and
 * "EnemiesTraffic" (ships the player hit: they attack it for a while). Every 0.5 s: a new fight, the fights that
 * are over or far go back to the traffic (a fighter that stands for 2 s too), the chasers and enemies go for the
 * player, an enemy goes back to the traffic when the attack time is over, and OnNeedMoreTraffic /
 * OnNeedMoreChasingTraffic ask the script for ships (the farthest one goes when there are too many). While the boss
 * controls the robots the whole traffic attacks the player.
 */
#include <string.h>

#include "scrap/gvar.h"
#include "scrap/chartype.h"
#include "scrap/filepak.h"
#include "scrap/aivehicle.h"
#include "scrap/hashtable.h"
#include "scrap/logic.h"
#include "scrap/nodestraffic.h"
#include "scrap/objentity.h"
#include "scrap/scorer.h"
#include "scrap/scoreritems.h"
#include "scrap/scrap.h"
#include "scrap/scrapcollision.h"
#include "scrap/scrapmodel.h"
#include "scrap/vehicleentity.h"
#include "scrap/weapons.h"
#include "scrap/world.h"
#include "scrap/staticinit.h"

#define TRAFFIC_DAMAGE_NATIVE 0x15af10
#define TRAFFIC_DEATH_NATIVE 0x15b160
#define TRAFFIC_ONAIEND 0x15ae60
static cGVar GVar_MaxTrafficEnemy;
#define g_MaxTrafficEnemy (GVar_MaxTrafficEnemy.value.i)
static cGVarFunction GVar_OnNeedMoreTraffic;
#define g_OnNeedMoreTraffic (((cPy_Function *)&GVar_OnNeedMoreTraffic.func))
static cGVar GVar_MaxFightingTrafficSize;
#define g_MaxFightingTrafficSize (GVar_MaxFightingTrafficSize.value.f)
static cGVarFunction GVar_OnPlayerHelpedPolice;
#define g_OnPlayerHelpedPolice (((cPy_Function *)&GVar_OnPlayerHelpedPolice.func))
static cGVar GVar_ProbTrafficFighting;
#define g_ProbTrafficFighting (GVar_ProbTrafficFighting.value.f)
static cGVar GVar_TrafficRemovalDist;
#define g_TrafficRemovalDist (GVar_TrafficRemovalDist.value.f)
static cGVar GVar_TrafficDamageTolerance;
#define g_TrafficDamageTolerance (GVar_TrafficDamageTolerance.value.i)
static cGVar GVar_MaxTrafficEnemyTime;
#define g_MaxTrafficEnemyTime (GVar_MaxTrafficEnemyTime.value.f)
static cGVar GVar_MinTrafficEnemyTime;
#define g_MinTrafficEnemyTime (GVar_MinTrafficEnemyTime.value.f)
static cGVarFunction GVar_OnNeedMoreChasingTraffic;
#define g_OnNeedMoreChasingTraffic (((cPy_Function *)&GVar_OnNeedMoreChasingTraffic.func))
static cGVar GVar_iTrafficNumber;
#define g_iTrafficNumber (GVar_iTrafficNumber.value.i)
/* the callback of the AI when the move ends (rva 0x15c of the AI) */
#define AIV_OnEnd(ai) (((cAIVehicle *)(ai))->onArrive)

/* the AI back to the traffic: the patrol (state 3) with its defaults */
static void Traffic_Patrol(cAIVehicle *ai) {
	cAIVehicle_SetState(ai, AIV_IN_ROUTE, 1, 5.0, 2.0);
}

static inline float Traffic_Dist2(const vec3_t *a, float x, float y, float z) {
	float dx = a->x - x;
	float dy = a->y - y;
	float dz = a->z - z;

	return dy * dy + dx * dx + dz * dz;
}

/* the idle time of a car: the bits of a float in the low word of the value (x86 is little endian) */
static inline float *Traffic_Idle(cNameItem *it) {
	return (float *)&it->value;
}

static inline intptr_t Traffic_IdleValue(float t) {
	int32_t bits;
	memcpy(&bits, &t, sizeof(bits));
	return bits;
}

cEnemiesTraffic *cEnemiesTraffic_ctor(cEnemiesTraffic *self) {
	self->idle.numSlots = 0x1c9;
	self->idle.slots = New_Array(0x1c9, sizeof(*self->idle.slots));
	memset(self->idle.slots, 0, 0x1c9 * sizeof(*self->idle.slots));
	self->idle.locked = 0;
	self->idle.caseSensitive = 1;
	self->traffic = NULL;
	self->enemies = NULL;
	self->timer = 0.0f;
	cPy_Function_SetFunc(g_OnNeedMoreTraffic, str_Empty);
	g_iTrafficNumber = 0;
	self->usr = NULL;
	self->attackTime = 0.0f;
	self->bossRobots = -1;
	return self;
}

void cEnemiesTraffic_dtor(cEnemiesTraffic *self) {
	NameTable_Clear(&self->idle);
}

/* the two ships may fight (ProbTrafficFighting percent) */
static byte Traffic_FightChance(cEntity *a, cEntity *b) {
	if (a == b) {
		return 0;
	}
	return g_ProbTrafficFighting > (float)(Weap_Rand01() * (double)100.99f + 0.0);
}

/* the ship of the traffic farthest from the cameras goes, unless it is in view and nearer than
 * TrafficRemovalDist */
static void cEnemiesTraffic_RemoveFarthest(cEnemiesTraffic *self) {
	vec3_t tmp;
	float best = 0.0f;
	cEntity *far = NULL;

	for (Nto1Node *n = Logic_ListFirst(self->traffic); n;) {
		cEntity *e = n->data;
		n = Logic_ListNext(n);
		float d = cWorld_GetMinCamDistSq(g_pWorld, Entity_GetPos(e, &tmp));
		if (d > best) {
			far = e;
			best = d;
		}
	}
	if (!far) {
		return;
	}
	if (CGameModel_IsVisible(Obj_GetModel(far))) {
		float r = g_TrafficRemovalDist;
		if (!(best >= r * r)) {
			return;
		}
	}
	cEntity_Del(far);
}

/* a ship of a random traffic list with a free line to e (up to four tries), else the last one seen */
static cEntity *Traffic_FindOther(cEntity *e) {
	static const char *const lists[4] = { "Traffic", "HeavyTraffic", "MediumTraffic", "NormalTraffic" };
	vec3_t t1, t2;

	if (!e) {
		return NULL;
	}
	cEntityListData *list = (cEntityListData *)cWorld_GetList(g_pWorld, lists[crt_rand() % 4]);
	cEntity *found = NULL;
	int tries = 0;
	for (Nto1Node *n = Logic_ListFirst(list); n; n = Logic_ListNext(n)) {
		found = Logic_AsCar(n->data);
		CHECK(Entity_IsType(found, TYEN_CAR) && "wle->IsType(TYEN_VEHICLE)");
		if (found == e) {
			continue;
		}
		const vec3_t *to = Entity_GetPos(e, &t1);
		if (!Engine_TestLine(Entity_GetPos(found, &t2), to, NULL, NULL)) {
			return found;
		}
		if (++tries == 4) {
			return NULL;
		}
	}
	return found;
}

static cAIVehicle *Traffic_AI(cEntity *e) {
	cAIVehicle *ai = Entity_GetAI(e);

	CHECK(ai && "ai");
	return ai;
}

/* a fighter of the list FightingTraffic: back to the traffic when the fight is over or far from the player */
static byte Traffic_OnAIEnd(cAIVehicle *ai);
static Nto1Node *Traffic_UpdateFighter(cEnemiesTraffic *self, Nto1Node *n, cEntity *usr, byte alarm,
	const vec3_t *up) {
	vec3_t tmp;
	cEntity *e = n->data;

	CHECK(Entity_IsType(e, TYEN_CAR) && "wle->IsType(TYEN_VEHICLE)");
	cAIVehicle *ai = Traffic_AI(e);
	int st = cAIVehicle_GetState(ai);
	if (AIV_OnEnd(ai) != (void *)Traffic_OnAIEnd) {
		AIV_OnEnd(ai) = (void *)Traffic_OnAIEnd;
	}
	if (alarm || !st) {
		PoliceAI_RestoreParams(ai);
		if (st) {
			Traffic_Patrol(ai);
		} else if (!alarm) {
			cEntity *cop = ExtPolice_CallCop(e);
			if (cop) {
				cAIVehicle_SetState(ai, AIV_FIGHTING, cop);
				return Logic_ListNext(n);
			}
			Nto1Node *next = Logic_ListNext(n);
			const char *name = e->name;
			cNameItem *it = NameTable_Find(&self->idle, name, HashTable_Slot(&self->idle, name));
			if (!it) {
				intptr_t t = Traffic_IdleValue(0.0f);
				CHECK(!self->idle.locked);
				uint32_t slot = HashTable_Slot(&self->idle, name);
				if (!NameTable_Find(&self->idle, name, slot)) {
					NameTable_Insert(&self->idle, name, slot, &t);
				}
				return next;
			}
			float t = self->timer + *Traffic_Idle(it);
			if (!(t >= 2.0f)) {
				intptr_t v = Traffic_IdleValue(t);
				NameTable_Set(&self->idle, name, &v);
				return next;
			}
			Traffic_Patrol(ai);
			cEntity_SetList(e, "Traffic");
			NameTable_Delete(&self->idle, name);
			return next;
		}
		Nto1Node *next = Logic_ListNext(n);
		cEntity_SetList(e, "Traffic");
		return next;
	}
	if (st != 2 || !usr) {
		return Logic_ListNext(n);
	}
	if (AIMove_GetTargetEnt(ai) == usr) {
		Nto1Node *next = Logic_ListNext(n);
		PoliceAI_RestoreParams(ai);
		cEntity_SetList(e, "EnemiesTraffic");
		Taunts_Ship(e, 1);
		return next;
	}
	CGameModel *tm = NULL;
	cEntity *t = AIMove_GetTargetEnt(ai);
	if (t) {
		if (!Entity_IsType(t, TYEN_CAR)) {
			return Logic_ListNext(n);
		}
		tm = Obj_GetModel(t);
	}
	float r = g_TrafficFightingMaxDist;
	float r2 = r * r;
	if (!tm || CGameModel_IsVisible(tm) || CGameModel_IsVisible(Obj_GetModel(e)) ||
		!(Traffic_Dist2(Entity_GetPos(e, &tmp), up->x, up->y, up->z) > r2) ||
		!(Traffic_Dist2(CGameModel_GetPos(tm, &tmp), up->x, up->y, up->z) > r2)) {
		return Logic_ListNext(n);
	}
	Nto1Node *next = Logic_ListNext(n);
	PoliceAI_RestoreParams(ai);
	Traffic_Patrol(ai);
	cEntity_SetList(e, "Traffic");
	cEntity *o = AIMove_GetTargetEnt(ai);
	if (!o || !Entity_IsType(o, TYEN_CAR)) {
		return next;
	}
	cAIVehicle *oai = Entity_GetAI(o);
	if (!oai || !cEntity_GetListName(o) || strcmp(cEntity_GetListName(o), "FightingTraffic")) {
		return next;
	}
	Traffic_Patrol(oai);
	if (next && next->data == o) {
		next = Logic_ListNext(next);
	}
	PoliceAI_RestoreParams(oai);
	cEntity_SetList(o, "Traffic");
	return next;
}

static void cEnemiesTraffic_Think(cEnemiesTraffic *self) {
	vec3_t tmp, cp;

	if (self->bossRobots == -1) {
		const char *s = SaveVar_Get("RobotsControlledByBoss");
		if (s) {
			self->bossRobots = !strcmp(s, "1");
		}
	}
	cEntity *usr = ChType_Usr0();
	cEntity *f1 = NULL;
	cEntity *f2 = NULL;
	byte alarm = g_pWorld->alarm.on;
	byte canFight = usr && !alarm;
	byte f2Other = 0;
	vec3_t up;
	if (usr) {
		const vec3_t *p = Entity_GetPos(usr, &tmp);
		up.y = p->y;
		up.x = p->x;
		up.z = p->z;
	} else {
		/* the original copies its uninitialized buffer; the position is only used with a player */
		up.x = up.y = up.z = 0.0f;
	}
	int nFighting = Logic_Count(self->fighting->numElems);
	if (self->bossRobots) {
		canFight = 0;
	} else {
		const char *wp = g_CurrentWaypointName;
		/* no fights during a race */
		if (strcmp(wp, "") && *wp && World_FindEntity(wp)) {
			canFight = 0;
		} else if (canFight && g_MaxFightingTrafficSize > (float)nFighting) {
			int tries = 0;
			for (Nto1Node *n = Logic_ListFirst(self->traffic); n; n = Logic_ListNext(n)) {
				cEntity *e = n->data;
				CHECK(Entity_IsType(e, TYEN_CAR) && "wle->IsType(TYEN_VEHICLE)");
				cAIVehicle *ai = Traffic_AI(e);
				if (!cAIVehicle_GetState(ai)) {
					Traffic_Patrol(ai);
				}
				if (!CGameModel_IsVisible(Obj_GetModel(e))) {
					continue;
				}
				cp = g_pWorld->camPos[g_pWorld->curCam];
				if (Engine_TestLine(Entity_GetPos(e, &tmp), &cp, NULL, NULL)) {
					if (++tries == 4) {
						break;
					}
					continue;
				}
				f1 = e;
				f2 = Traffic_FindOther(e);
				if (f2 && cEntity_GetListName(f2) && strcmp(cEntity_GetListName(f2), self->traffic->name)) {
					f2Other = 1;
				}
				break;
			}
		}
	}
	int nEnemies = Logic_Count(self->enemies->numElems);
	if (canFight && f1 && f2 && !(g_NumEnemiesInRadar > nEnemies) &&
		g_MaxFightingTrafficSize > (float)(nEnemies + nFighting) && Traffic_FightChance(f1, f2)) {
		cEntity_SetList(f1, "FightingTraffic");
		cAIVehicle *ai = Traffic_AI(f1);
		PoliceAI_RandomParams(ai);
		cAIVehicle_SetState(ai, AIV_FIGHTING, f2);
		if (!f2Other) {
			cEntity_SetList(f2, "FightingTraffic");
			ai = Traffic_AI(f2);
			PoliceAI_RandomParams(ai);
			cAIVehicle_SetState(ai, AIV_FIGHTING, f1);
		}
	}
	for (Nto1Node *n = Logic_ListFirst(self->fighting); n;) {
		n = Traffic_UpdateFighter(self, n, usr, alarm, &up);
	}
	if (self->bossRobots) {
		cEntity *e = cEntityList_GetFirst((cEntityList *)self->traffic);
		while (e && Entity_IsType(e, TYEN_CAR)) {
			cEntity_SetList(e, "EnemiesTraffic");
			WLife(e)->arrow.tgType = 0;
			CHECK(Entity_IsType(e, TYEN_CAR) && "e->IsType(TYEN_VEHICLE)");
			cAIVehicle *ai = Traffic_AI(e);
			if (usr) {
				cAIVehicle_SetState(ai, AIV_FIGHTING, usr);
			}
			e = cEntityList_GetFirst((cEntityList *)self->traffic);
		}
	}
	int nChasing = Logic_Count(self->chasing->numElems);
	if (nChasing < g_MaxChasingShips) {
		cPy_Function_AddParamI(g_OnNeedMoreChasingTraffic, g_MaxChasingShips - nChasing);
		cPy_Function_Execute(g_OnNeedMoreChasingTraffic, 0);
	}
	for (Nto1Node *n = Logic_ListFirst(self->chasing); n;) {
		cEntity *e = Logic_AsCar(n->data);
		CHECK(Entity_IsType(e, TYEN_CAR) && "wle->IsType(TYEN_VEHICLE)");
		WLife(e)->arrow.tgType = 0;
		cAIVehicle *ai = Traffic_AI(e);
		n = Logic_ListNext(n);
		if (usr && (usr != self->usr || cAIVehicle_GetState(ai) != AIV_STATE_CHASE)) {
			cAIVehicle_SetState(ai, AIV_STATE_CHASE, usr, (double)g_AIChaseDefault1, (double)g_AIChaseDefault2);
		}
	}
	cEntity *last = NULL;
	cAIVehicle *lastAi = NULL;
	for (Nto1Node *n = Logic_ListFirst(self->enemies); n;) {
		last = n->data;
		CHECK(Entity_IsType(last, TYEN_CAR) && "wle->IsType(TYEN_VEHICLE)");
		lastAi = Traffic_AI(last);
		n = Logic_ListNext(n);
		if (AIV_OnEnd(lastAi) != (void *)Traffic_OnAIEnd) {
			AIV_OnEnd(lastAi) = (void *)Traffic_OnAIEnd;
		}
		if (usr && usr != self->usr) {
			cAIVehicle_SetState(lastAi, AIV_FIGHTING, usr);
		}
	}
	int nEnemies2 = Logic_Count(self->enemies->numElems);
	if (last && lastAi) {
		float t = g_MinTrafficEnemyTime;
		int max = g_MaxTrafficEnemy;
		if (!(max < nEnemies2)) {
			t = (t * ((float)nEnemies2 - 1.0f) + ((float)(max - nEnemies2) + 1.0f) * g_MaxTrafficEnemyTime) /
				(float)max;
		}
		if (self->attackTime > t) {
			self->attackTime = t;
		}
		if (0.0f >= self->attackTime) {
			self->attackTime = t;
			Traffic_Patrol(lastAi);
			WLife(last)->arrow.tgType = 3;
			cEntity_SetList(last, "Traffic");
			nEnemies2--;
			const char *s = SaveVar_Get("RobotsControlledByBoss");
			if (!g_pWorld->alarm.on || (s && !strcmp(s, "1"))) {
				Taunts_Ship(last, 7);
			}
		}
	}
	int total = Logic_Count(self->traffic->numElems) + Logic_Count(self->fighting->numElems) + nEnemies2;
	if (total < g_iTrafficNumber) {
		cPy_Function_AddParamI(g_OnNeedMoreTraffic, g_iTrafficNumber - total);
		cPy_Function_Execute(g_OnNeedMoreTraffic, 0);
	} else if (total > g_iTrafficNumber) {
		cEnemiesTraffic_RemoveFarthest(self);
	}
	self->usr = usr;
}

static void Traffic_DamageNative(cAbstractFile *params);
static void Traffic_DeathNative(cAbstractFile *params);
void cEnemiesTraffic_Init(cEnemiesTraffic *self) {
	static const char *const names[4] = { "EnemiesTraffic", "ChasingTraffic", "FightingTraffic", "Traffic" };
	cEntityListData **lists[4] = { &self->enemies, &self->chasing, &self->fighting, &self->traffic };

	for (int i = 0; i < 4; i++) {
		cEntityListData *l = cWorld_NewEntityList(g_pWorld, names[i]);
		*lists[i] = l;
		cPy_Function_SetNative(&l->onDamage, (void *)Traffic_DamageNative);
		cPy_Function_SetNative(&(*lists[i])->onDeath, (void *)Traffic_DeathNative);
	}
}

/* the AI callback of a fighter at the end of its move: back to the traffic */
static byte Traffic_OnAIEnd(cAIVehicle *ai) {
	if (!ai) {
		return 0;
	}
	cEntity *v = cAIVehicle_GetVehicle(ai);
	const char *list = cEntity_GetListName(v);
	if (!v || !list || strcmp(list, "FightingTraffic")) {
		return 0;
	}
	Traffic_Patrol(Entity_GetAI(v));
	cEntity_SetList(v, "Traffic");
	NameTable_Delete(&g_pWorld->logic.enemiesTraffic.idle, v->name);
	return 1;
}

/* the damage native of the lists: a ship the player hit hard enough attacks it (EnemiesTraffic) */
static void Traffic_DamageNative(cAbstractFile *params) {
	char name[0x80], attacker[0x80];
	float damage;

	cAbstractFile_ReadString(params, name, sizeof(name));
	cAbstractFile_ReadString(params, attacker, sizeof(attacker));
	VFile_Read(params, &damage, 4);
	cEntity *e = World_FindEntity(name);
	CHECK(Entity_IsType(e, TYEN_CAR) && "wle->IsType(TYEN_VEHICLE)");
	cAIVehicle *ai = Traffic_AI(e);
	cEntity *a = World_FindEntity(attacker);
	if (!a || a != ChType_Usr0()) {
		return;
	}
	if (WLife(e)->life + g_TrafficDamageTolerance > Veh(e)->maxLife) {
		return;
	}
	if (WLife(e)->arrow.tgType) {
		PoliceAI_RestoreParams(ai);
		WLife(e)->arrow.tgType = 0;
		cAIVehicle_SetState(ai, AIV_FIGHTING, a);
	}
	byte wasEnemy = cEntity_GetListName(e) && !strcmp(cEntity_GetListName(e), "EnemiesTraffic");
	cEntity_SetList(e, "EnemiesTraffic");
	g_pWorld->logic.enemiesTraffic.attackTime = g_MaxTrafficEnemyTime;
	if (!wasEnemy && WLife(e)->life > 0) {
		Taunts_Ship(e, 1);
	}
}

/* the death native: a ship the player killed raises the alarm, unless the police chased it */
static void Traffic_DeathNative(cAbstractFile *params) {
	char name[0x80], killer[0x80];

	cAbstractFile_ReadString(params, name, sizeof(name));
	cAbstractFile_ReadString(params, killer, sizeof(killer));
	cEntity *e = World_FindEntity(name);
	CHECK(Entity_IsType(e, TYEN_CAR) && "wle->IsType(TYEN_VEHICLE)");
	cEntity *k = World_FindEntity(killer);
	if (!k || k != ChType_Usr0()) {
		return;
	}
	if (!ExtPolice_IsChased(e)) {
		Alarm_Set(&g_pWorld->alarm, 1.0f);
		return;
	}
	if ((byte)cPy_Function_IsValid(g_OnPlayerHelpedPolice)) {
		cPy_Function_Execute(g_OnPlayerHelpedPolice, 0);
	}
}

void PoliceAI_RestoreParams(cAIVehicle *ai) {
	if (!ai) {
		return;
	}
	cAIVehicleParams1 p1 = *cAIVehicle_GetParamsInit(ai);
	*cAIVehicle_GetParams1(ai) = p1;
	cAIVehicleParams2 p2 = *cAIVehicle_GetParams2Init(ai);
	*cAIVehicle_GetParams2(ai) = p2;
	cAIVehicleParams3 p3 = *cAIVehicle_GetParams3Init(ai);
	*cAIVehicle_GetParams3(ai) = p3;
}

void PoliceAI_RandomParams(cAIVehicle *ai) {
	if (!ai) {
		return;
	}
	float f = (float)(Weap_Rand01() * (double)0.6f + (double)0.4f);
	cAIVehicle_GetParams1(ai)->damageGetAway = f;
	f = (float)(Weap_Rand01() * 0.25 + 0.25);
	cAIVehicle_GetParams1(ai)->damageItemLife = f;
	int i = crt_rand() % 8 + 3;
	cAIVehicle_GetParams2(ai)->maxTimeStrafe = i;
	f = (float)(Weap_Rand01() * 0.5 + 0.5);
	cAIVehicle_GetParams2(ai)->maxVelStrafe = f;
	cAIVehicle_GetParams2(ai)->aim = 10;
	cAIVehicle_GetParams3(ai)->cadence = 0;
	cAIVehicle_GetParams3(ai)->angAttack = 3.14f;
	cAIVehicle_ApplyParams(ai);
}

void cEnemiesTraffic_Update(cEnemiesTraffic *self, float dt) {
	float at = self->attackTime - dt;
	float t = dt + self->timer;

	self->timer = t;
	self->attackTime = at;
	if (0.0f > at) {
		self->attackTime = 0.0f;
	}
	if (!(0.5f >= t)) {
		cEnemiesTraffic_Think(self);
		self->timer = 0.0f;
	}
}

cGVar GVar_MaxChasingShips;

/* the dynamic initializers of the globals here, run in the order of src/standalone/crtxc.inc */
FINI_CALL(Fini_GVar_OnNeedMoreTraffic, &GVar_OnNeedMoreTraffic.func, cPy_Function_dtor)
INIT_GVAR_FUNCTION(Init_GVar_OnNeedMoreTraffic, Fini_GVar_OnNeedMoreTraffic, &GVar_OnNeedMoreTraffic, "OnNeedMoreTraffic", 0, "Called if the traffic is not enough", "")
FINI_CALL(Fini_GVar_OnNeedMoreChasingTraffic, &GVar_OnNeedMoreChasingTraffic.func, cPy_Function_dtor)
INIT_GVAR_FUNCTION(Init_GVar_OnNeedMoreChasingTraffic, Fini_GVar_OnNeedMoreChasingTraffic, &GVar_OnNeedMoreChasingTraffic, "OnNeedMoreChasingTraffic", 0, "Called if the chasing traffic is not enough", "")
FINI_CALL(Fini_GVar_OnPlayerHelpedPolice, &GVar_OnPlayerHelpedPolice.func, cPy_Function_dtor)
INIT_GVAR_FUNCTION(Init_GVar_OnPlayerHelpedPolice, Fini_GVar_OnPlayerHelpedPolice, &GVar_OnPlayerHelpedPolice, "OnPlayerHelpedPolice", 0, "Called if player helped police", "")
INIT_GVAR_INT(Init_GVar_iTrafficNumber, &GVar_iTrafficNumber, "iTrafficNumber", 0, "Sets the traffic on the sky", 0)
INIT_GVAR_INT(Init_GVar_TrafficDamageTolerance, &GVar_TrafficDamageTolerance, "TrafficDamageTolerance", 0, "Life Points to be your enemy", 10)
INIT_GVAR_FLOAT(Init_GVar_TrafficRemovalDist, &GVar_TrafficRemovalDist, "TrafficRemovalDist", 0, "Traffic removal dist", 100000.0f)
INIT_GVAR_FLOAT(Init_GVar_ProbTrafficFighting, &GVar_ProbTrafficFighting, "ProbTrafficFighting", 0, "Prob traffic fighting", 100.0f)
INIT_GVAR_FLOAT(Init_GVar_MaxFightingTrafficSize, &GVar_MaxFightingTrafficSize, "MaxFightingTrafficSize", 0, "Max fighting traffic size", 4.0f)
INIT_GVAR_FLOAT(Init_GVar_MaxTrafficEnemyTime, &GVar_MaxTrafficEnemyTime, "MaxTrafficEnemyTime", 0, "Max Time attacking of traffic", 10.0f)
INIT_GVAR_FLOAT(Init_GVar_MinTrafficEnemyTime, &GVar_MinTrafficEnemyTime, "MinTrafficEnemyTime", 0, "Min Time attacking of traffic", 1.0f)
INIT_GVAR_INT(Init_GVar_MaxTrafficEnemy, &GVar_MaxTrafficEnemy, "MaxTrafficEnemy", 0, "Maximun number of attaking traffic.", 5)
INIT_GVAR_INT(Init_GVar_MaxChasingShips, &GVar_MaxChasingShips, "MaxChasingShips", 0, "Maximun number of chasing ships.", -1)
