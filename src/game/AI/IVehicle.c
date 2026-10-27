/*
 * The AI of the vehicles, original source AI/AIVehicle.cpp (include/scrap/aivehicle.h): construction, the update
 * slots and their dispatch by state, the events of the move, the state names and the debug text, the parameter
 * checks, the state kept for after an event, the item searches and the placing on a traffic track. The decisions of
 * the single states (the React, Think and Items functions), the internal state switch and the external state change
 * are still the exe's.
 */
#include <float.h>
#include <string.h>

#include "scrap/aivehicle.h"
#include "scrap/console.h"
#include "scrap/embeddedfxvehicle.h"
#include "scrap/fxmisc.h"
#include "scrap/itementity.h"
#include "scrap/scrap.h"
#include "scrap/vehicleentity.h"
#include "scrap/vehicletraffic.h"
#include "scrap/weapons.h"

#define g_RadiusObjectiveTraffic GAME_DATA(float, 0x4448e8)		/* RADIUS_OBJECTIVE_TRAFFIC */
#define g_RadiusObjectiveTrafficInDoor GAME_DATA(float, 0x444918)	/* RADIUS_OBJECTIVE_TRAFFIC_INDOOR */
#define g_RadiusObjectiveInRoute GAME_DATA(float, 0x444d60)		/* RADIUS_OBJECTIVE_IN_ROUTE */
#define g_VelTrafficLightRotAI GAME_DATA(float, 0x444a14)
#define AIV_NEAR_DIST2 225000000.0f		/* 15000 squared (rva 0x3a8e58) */
#define AIV_ROT_SPEED_A 8334.0f			/* rva 0x3a8e4c */
#define AIV_ROT_SPEED_B 4167.0f			/* rva 0x3a8e48 */
#define AIV_ROT_SPEED_F 0.025f			/* rva 0x3a8e2c */
#define AIV_HOOK_ANGLE 0.7853981633974483	/* pi / 4 (rva 0x3a8e30) */
#define AIV_ARRIVED_DIST2 4000000.0f		/* rva 0x3a8e54 */

/* the vehicle entity slots of the AI (thiscall) */
typedef struct AIVehEntVtbl {
	void *vf00_1f[32];
	vec3_t *(__thiscall *GetRot)(cEntity *self, vec3_t *out);	/* 0x80 */
	void(__thiscall *SetRot)(cEntity *self, const vec3_t *rot);	/* 0x84 */
	void *vf22;
	void(__thiscall *SetVel)(cEntity *self, const vec3_t *vel);	/* 0x8c */
} AIVehEntVtbl;
SD_OFFSET(AIVehEntVtbl, SetVel, 0x8c);

static inline const AIVehEntVtbl *AIV_EntVtbl(cEntity *e) {
	return (const AIVehEntVtbl *)e->vftable;
}

static inline cVehicleTraffic *AIV_VehTraffic(cEntity *v) {
	return (cVehicleTraffic *)Veh(v)->traffic;
}

static inline CNodesTraffic *AIV_Outdoor(void) {
	return (CNodesTraffic *)g_NodesTraffic;
}

static inline CNodesTraffic *AIV_Indoor(void) {
	return (CNodesTraffic *)g_NodesTrafficInDoor;
}

/* the states that change without asking the vehicle group (FREEZE.., the traffic) */
static inline int AIV_FreeState(int state) {
	return state == AIV_FREEZE || state == AIV_FREEZE_LOST || state == AIV_FREEZE_OBJ_NOT_ACCESS ||
		state == AIV_FREEZE_CHANGE_STATE || state == AIV_LOST_PATH || state == AIV_LOST_PATH_CHASE ||
		state == AIV_TRAFFIC || state == AIV_TRAFFIC_INDOOR;
}

/* ---- construction ---- */

void *__thiscall cAIVehicle_ctor(void *mem, cEntity *v, void *p1, void *p2, void *p3) {
	cAIVehicle *self = mem;

	cAIMove_ctor(mem, v);
	self->vftable = Game_Ptr(CAIVEHICLE_VFTABLE);
	cSlotController_ctor(AIV_Slot(&self->slotAttack), 0.2f);
	cSlotController_ctor(AIV_Slot(&self->slotGraph), 0.5f);
	cSlotController_ctor(AIV_Slot(&self->slotItems), 0.2f);
	cString_ctor(&self->itemLife.name);
	cString_ctor(&self->itemAmmo.name);
	cString_ctor(&self->itemAmmoK[0].name);
	cString_ctor(&self->itemAmmoK[1].name);
	cString_ctor(&self->itemAmmoK[2].name);
	self->vehicle = v;
	self->params = *(const cAIVehicleParams1 *)p1;
	self->paramsInit = *(const cAIVehicleParams1 *)p1;
	void *m = operator_new(CAIVEHICLECONTROL_SIZE);
	self->control = m ? cAIVehicleControl_ctor(m, v, self, p2) : NULL;
	CHECK(self->control);
	m = operator_new(CAIVEHICLESHOOT_SIZE);
	self->shoot = m ? cAIVehicleShoot_ctor(m, v, self, p3) : NULL;
	CHECK(self->shoot);
	self->state = AIV_STATE_NONE;
	self->internState = AIVI_STATE_INTERN_NONE;
	cAIVehicle_Debugf(self, "Entidad CREADA");
	self->damageTime = 0.0f;
	self->lifeMark = Veh(self->vehicle)->life;
	self->itemLifeMark = self->lifeMark;
	self->track = -1;
	self->node = -1;
	self->state = AIV_STATE_NONE;
	self->internState = AIVI_STATE_INTERN_NONE;
	cAIVehicle_SetStateIntern(self, AIV_FREEZE);
	self->prevState = 0;
	self->maxDistRepos = g_MaxDistReposAI;
	self->minDistRepos = g_MinDistReposAI;
	self->maxDistReposNotVis = g_MaxDistReposNotVisAI;
	self->minDistReposNotVis = g_MinDistReposNotVisAI;
	self->b1a8 = 0;
	self->trackPos.y = 0.0f;
	self->trackPos.z = 0.0f;
	self->trackPos.x = 0.0f;
	self->trackOfs.x = 0.0f;
	self->trackOfs.y = self->trackPos.y;
	self->trackOfs.z = self->trackPos.z;
	self->b2e0 = 0;
	self->f30c = 0.0f;
	self->rotB = 0.0f;
	self->rotA = 0.0f;
	self->f2f0 = 0.0f;
	self->f37c = 0.0f;
	self->itemAmmoDist = 0.0f;
	self->trackType = 5;
	self->b340 = 0;
	self->b354 = 0;
	self->itemAmmoBest = NULL;
	self->itemAmmoEnt[2] = NULL;
	self->itemAmmoEnt[1] = NULL;
	self->itemAmmoEnt[0] = NULL;
	self->itemLifeEnt = NULL;
	self->itemAmmoDistK[2] = 0.0f;
	self->itemAmmoDistK[1] = 0.0f;
	self->itemAmmoDistK[0] = 0.0f;
	self->i39c[2] = 0;
	self->i39c[1] = 0;
	self->i39c[0] = 0;
	self->b3a9[0] = 0;
	self->b3a9[1] = 0;
	self->b3a8 = 0;
	self->i3ac = 3;
	self->chaseMin = g_AIChaseDefault1;
	self->chaseMax = g_AIChaseDefault2;
	self->b318 = 0;
	self->b32c = 0;
	self->f330 = 0.0f;
	self->i334 = 0;
	self->onArrive = NULL;
	self->hookMode = 0;
	self->hookSaved = 0;
	cAIVehicle_CheckParams1(self, p1);
	cAIVehicle_CheckParams2(self, p2);
	cAIVehicle_CheckParams3(self, p3);
	return self;
}

void __fastcall cAIVehicle_dtor(cAIVehicle *self) {
	self->vftable = Game_Ptr(CAIVEHICLE_VFTABLE);
	if (self->control) {
		self->control->vftable->Delete(self->control, 1);
	}
	if (self->shoot) {
		self->shoot->vftable->Delete(self->shoot, 1);
	}
	cAIVehicle_Debugf(self, "Entidad DESTRUIDA");
	cString_dtor(&self->itemAmmoK[2].name);
	cString_dtor(&self->itemAmmoK[1].name);
	cString_dtor(&self->itemAmmoK[0].name);
	cString_dtor(&self->itemAmmo.name);
	cString_dtor(&self->itemLife.name);
	cAIMove_dtor(self);
}

cAIVehicle *__thiscall cAIVehicle_Delete(cAIVehicle *self, byte flags) {
	cAIVehicle_dtor(self);
	if (flags & 1) {
		operator_delete_sized(self, CAIVEHICLE_SIZE);
	}
	return self;
}

void __thiscall cAIVehicle_ApplyParams(cAIVehicle *self) {
	cAIVehicleControl_ApplyParams(self->control);
}

/* ---- the update slots ---- */

/* the reaction of the state to what happens around (the think slot of cAIMove) */
void __thiscall cAIVehicle_React(cAIVehicle *self) {
	switch (self->state) {
	case AIV_FIGHTING_HOOK:
		cAIVehicle_ReactHook(self);
		/* fall through */
	case AIV_FIGHTING:
		cAIVehicle_ReactFighting(self);
		return;
	case AIV_GET_AWAY_STOP:
		cAIVehicle_ReactGetAwayStop(self);
		return;
	case AIV_GET_AWAY:
		cAIVehicle_ReactGetAway(self);
		return;
	case AIV_LOST_PATH:
		cAIVehicle_ReactLostPath(self);
		return;
	case AIV_LOST_PATH_CHASE:
		cAIVehicle_ReactLostPathChase(self);
		return;
	}
}

/* the damage clock and the move of the state; 1 if the state did not change */
byte __thiscall cAIVehicle_Think(cAIVehicle *self) {
	float t = self->dt + self->damageTime;

	self->damageTime = t;
	if (t >= g_TimeDamageAI) {
		self->damageTime = 0.0f;
		self->lifeMark = Veh(self->vehicle)->life;
	}
	switch (self->state) {
	case AIV_GO_TO:
	case AIV_IN_ROUTE:
		cAIVehicle_ThinkGoTo(self);
		break;
	case AIV_FIGHTING:
		cAIVehicle_ThinkFighting(self);
		break;
	case AIV_GET_AWAY_STOP:
		cAIVehicle_ThinkGetAwayStop(self);
		break;
	case AIV_RACE:
		cAIVehicle_ThinkRace(self);
		break;
	case AIV_CHASE:
		cAIVehicle_ThinkChase(self);
		break;
	case AIV_FIGHTING_FREEZE:
		cAIVehicle_CheckTarget(self);
		break;
	case AIV_FIGHTING_HOOK:
		if (cAIVehicle_CheckTarget(self)) {
			self->f330 = self->f330 + self->dt;
		}
		break;
	case AIV_GET_AWAY:
		cAIVehicle_ThinkGetAway(self);
		break;
	case AIV_LOST_PATH:
		cAIVehicle_ThinkLostPath(self);
		break;
	case AIV_LOST_PATH_CHASE:
		cAIVehicle_ThinkLostPathChase(self);
		break;
	}
	return self->changed == 0;
}

void __thiscall cAIVehicle_ThinkHook(cAIVehicle *self) {
	if (cAIVehicle_CheckTarget(self)) {
		self->f330 = self->dt + self->f330;
	}
}

/* the items the state goes for */
void __thiscall cAIVehicle_CheckItems(cAIVehicle *self) {
	switch (self->state) {
	case AIV_FIGHTING:
		cAIVehicle_ItemsFighting(self);
		return;
	case AIV_IN_ROUTE:
		cAIVehicle_ItemsInRoute(self);
		return;
	case AIV_GET_AWAY_STOP:
		cAIVehicle_ItemsGetAwayStop(self);
		return;
	case AIV_CHASE:
		cAIVehicle_ItemsChase(self);
		return;
	case AIV_GET_AWAY:
		cAIVehicle_ItemsGetAway(self);
		return;
	}
}

/* the graph slot: the repositioning near the camera while it is in the graph */
void __thiscall cAIVehicle_UpdateGraph(cAIVehicle *self) {
	cSlotController *slot = AIV_Slot(&self->slotGraph);
	float t;

	if (!cSlotController_Tick(slot, self->dt) || !cSlotController_FireReset(slot, &t)) {
		return;
	}
	do {
		cWorld *w = g_pWorld;
		vec3_t cam = w->camPos[w->curCam];
		if (!g_NodesWorld->vftable->InGrid(g_NodesWorld, &cam)) {
			return;
		}
		switch (self->state) {
		case AIV_IN_ROUTE:
			cAIVehicle_UpdateRoute(self);
			break;
		case AIV_TRAFFIC:
			cAIVehicle_GraphTraffic(self);
			break;
		case AIV_TRAFFIC_INDOOR:
			cAIVehicle_GraphTrafficInDoor(self);
			break;
		case AIV_FREEZE_OBJ_NOT_ACCESS:
		case AIV_FREEZE_LOST:
			if (cAIVehicle_NeedRepos(self)) {
				cAIVehicle_Repos(self);
			}
			break;
		}
	} while (cSlotController_FireReset(slot, &t));
}

void __thiscall cAIVehicle_ReposIfFar(cAIVehicle *self) {
	if (cAIVehicle_NeedRepos(self)) {
		cAIVehicle_Repos(self);
	}
}

/* IN_ROUTE with Repos: the AI is put near the camera again; stopped if that fails */
void __thiscall cAIVehicle_UpdateRoute(cAIVehicle *self) {
	if (!self->repos) {
		return;
	}
	int in = self->internState;
	if (in != AIVI_STOPPED) {
		if (in != AIVI_FINISHING_LINE_LIFE && in != AIVI_FOLLOW_THE_PATH) {
			return;
		}
		if (!cAIVehicle_NeedRepos(self)) {
			return;
		}
	}
	if (!cAIVehicle_Repos(self) && self->internState != AIVI_STOPPED) {
		self->vehicle->vftable->SetActive(self->vehicle, 0);
		cAIVehicle_SetInternState(self, AIVI_STOPPED);
	}
}

/* the think slot: reaction, items and for FIGHTING the way back from an item */
void __thiscall cAIVehicle_UpdateThink(cAIVehicle *self) {
	cSlotController *slot = AIV_Slot(&self->slotThink);
	float t;

	if (!cSlotController_Tick(slot, self->dt) || !cSlotController_FireReset(slot, &t)) {
		return;
	}
	do {
		cAIVehicle_React(self);
		if (!self->changed) {
			cAIVehicle_CheckItems(self);
			if (!self->changed && self->state == AIV_FIGHTING) {
				cAIVehicle_FightingAfterItem(self);
			}
		}
	} while (cSlotController_FireReset(slot, &t));
}

/* the target still there and active; else FREEZE (and OnObjectiveNullAI for a target that went) */
byte __thiscall cAIVehicle_CheckTarget(cAIVehicle *self) {
	cEntity *t = AIV_Target(self);

	if (!t) {
		cAIVehicle_SetStateIntern(self, AIV_FREEZE);
		if (cPy_Function_IsValid(g_OnObjectiveNullAI)) {
			cPy_Function_AddParamS(g_OnObjectiveNullAI, self->vehicle->name);
			cPy_Function_Execute(g_OnObjectiveNullAI, 0);
		}
		return 0;
	}
	if (!t->flag_4c) {
		cAIVehicle_SetStateIntern(self, AIV_FREEZE);
		return 0;
	}
	return 1;
}

void __thiscall cAIVehicle_Draw(cAIVehicle *self) {
	cAIMove_Draw(self);
	cAIVehicleControl_Draw(self->control);
	cAIVehicleShoot_Draw(self->shoot);
}

void __thiscall cAIVehicle_Update(cAIVehicle *self, float dt) {
	if (self->pending.state != AIV_STATE_NONE && g_VehicleGroup->vftable->CanChange(g_VehicleGroup)) {
		cAIVehicle_RestoreState(self, &self->pending);
	}
	if (self->state == AIV_FREEZE) {
		return;
	}
	cAIMove_Update(self, dt);
	cAIVehicle_UpdateGraph(self);
	if (self->changed) {
		return;
	}
	cAIVehicle_Think(self);
	if (self->changed) {
		return;
	}
	cAIVehicle_UpdateThink(self);
	if (self->changed) {
		return;
	}
	cAIVehicleControl_Update(self->control, dt);
	if (self->changed) {
		return;
	}
	cAIVehicleShoot_Update(self->shoot, dt);
	if (self->changed) {
		return;
	}
	cAIVehicleShoot_Update2(self->shoot, dt);
}

void __thiscall cAIVehicle_UpdateOff(cAIVehicle *self, float dt) {
	cAIMove_UpdateOff(self, dt);
	cAIVehicle_UpdateGraph(self);
}

/* ---- the script variables: "Repos" here, the rest in cAIMove ---- */

byte __thiscall cAIVehicle_VarF(cAIVehicle *self, const char *name, double *d, byte set) {
	if (!*name) {
		return 0;
	}
	return cAIMove_VarF(self, name, d, set);
}

byte __thiscall cAIVehicle_VarI(cAIVehicle *self, const char *name, int *i, byte set) {
	if (!*name) {
		return 0;
	}
	if (!strcmp(name, "Repos")) {
		if (set) {
			self->repos = *i != 0;
			return 1;
		}
		*i = self->repos;
		return 1;
	}
	return cAIMove_VarI(self, name, i, set);
}

byte __thiscall cAIVehicle_VarS(cAIVehicle *self, const char *name, char **s, byte set) {
	if (!*name) {
		return 0;
	}
	return cAIMove_VarS(self, name, s, set);
}

byte __thiscall cAIVehicle_VarV(cAIVehicle *self, const char *name, double *x, double *y, double *z, byte set) {
	if (!*name) {
		return 0;
	}
	return cAIMove_VarV(self, name, x, y, z, set);
}

/* ---- the getters ---- */

cAIVehicleControl *__thiscall cAIVehicle_GetControl(cAIVehicle *self) {
	return self->control;
}

void __thiscall cAIVehicle_GetFadeDists(void *ai, float *nearDist, float *farDist) {
	cAIVehicle *self = ai;

	*nearDist = self->minDistRepos;
	*farDist = self->maxDistRepos;
}

void *__thiscall cAIVehicle_GetVehicle(cAIVehicle *ai) {
	return ai->vehicle;
}

byte __thiscall cAIVehicle_GetChanged(cAIVehicle *self) {
	return self->changed;
}

/* the external state as the scripts see it: GET_AWAY, and LOST_PATH out of GET_AWAY, count as FIGHTING */
int __thiscall cAIVehicle_GetState(cAIVehicle *ai) {
	int s = ai->state;

	if (s == AIV_LOST_PATH) {
		return ai->saved.state != AIV_GET_AWAY ? AIV_LOST_PATH : AIV_FIGHTING;
	}
	return s != AIV_GET_AWAY ? s : AIV_FIGHTING;
}

int __thiscall cAIVehicle_GetPrevState(cAIVehicle *self) {
	return self->prevState;
}

int __thiscall cAIVehicle_GetStateFor(cAIVehicle *self, int ev) {
	if (ev != AIV_GET_AWAY) {
		return self->state;
	}
	return AIV_FIGHTING;
}

int __thiscall cAIVehicle_GetInternState(cAIVehicle *ai) {
	return ai->internState;
}

long double __thiscall cAIVehicle_GetPathWidth(void *ai) {
	cAIVehicle *self = ai;

	if ((self->state == AIV_TRAFFIC || self->state == AIV_TRAFFIC_INDOOR) && self->internState != AIVI_STOPPED &&
			self->track >= 0) {
		CNodesTraffic *t = AIV_Outdoor();
		return t->vftable->GetRadius(t, self->track);
	}
	return 0.0L;
}

/* ---- the state names (the copies of the original write the string with its zero) ---- */

static const char *const s_InternStateNames[] = {
	"STOPPED", "FINISHING_LINE", "FINISHING_LINE_WITH_SHOOT_CM", "FINISHING_LINE_DIRECT", "FINISHING_LINE_LIFE",
	"FINISHING_LINE_AMMO", "CHASE_SHOOTING", "CHASE_NOT_SHOOTING", "FOLLOW_THE_PATH",
	"FOLLOW_THE_PATH_WITH_SHOOT_CM", "ORIENT_SHOOTING", "STATE_INTERN_NONE"
};

static const char *const s_StateNames[] = {
	"FREEZE", "GO_TO", "FIGHTING", "IN_ROUTE", "TRAFFIC", "GET_AWAY_STOP", "RACE", "CHASE", "FIGHTING_FREEZE",
	"TRAFFIC_INDOOR", "FIGHTING_HOOK", "FREEZE_OBJ_NOT_ACCESS", "GET_AWAY", "LOST_PATH", "LOST_PATH_CHASE",
	"FREEZE_LOST", "FREEZE_CHANGE_STATE", "STATE_NONE"
};

/* buf unchanged for an unknown state */
void __stdcall cAIVehicle_InternStateName(int state, char *buf) {
	if ((uint32_t)state < sizeof(s_InternStateNames) / sizeof(s_InternStateNames[0])) {
		memcpy(buf, s_InternStateNames[state], strlen(s_InternStateNames[state]) + 1);
	}
}

void __thiscall cAIVehicle_GetInternStateName(cAIVehicle *self, char *buf) {
	cAIVehicle_InternStateName(self->internState, buf);
}

void __stdcall cAIVehicle_StateName(int state, char *buf) {
	if ((uint32_t)state < sizeof(s_StateNames) / sizeof(s_StateNames[0])) {
		memcpy(buf, s_StateNames[state], strlen(s_StateNames[state]) + 1);
	}
}

void __thiscall cAIVehicle_GetStateName(cAIVehicle *self, char *buf) {
	cAIVehicle_StateName(self->state, buf);
}

/* ---- the parameter blocks (the ones of the move and the shoot control are kept there) ---- */

cAIVehicleParams1 *__thiscall cAIVehicle_GetParams1(cAIVehicle *ai) {
	return &ai->params;
}

cAIVehicleParams1 *__thiscall cAIVehicle_GetParamsInit(cAIVehicle *self) {
	return &self->paramsInit;
}

cAIVehicleParams2 *__thiscall cAIVehicle_GetParams2(cAIVehicle *ai) {
	return cAIVehicleControl_GetParams(ai->control);
}

cAIVehicleParams2 *__thiscall cAIVehicle_GetParams2Init(cAIVehicle *self) {
	return cAIVehicleControl_GetParamsInit(self->control);
}

cAIVehicleParams3 *__thiscall cAIVehicle_GetParams3(cAIVehicle *ai) {
	return cAIVehicleShoot_GetParams(ai->shoot);
}

cAIVehicleParams3 *__thiscall cAIVehicle_GetParams3Init(cAIVehicle *self) {
	return cAIVehicleShoot_GetParamsInit(self->shoot);
}

void __thiscall cAIVehicle_Setup(cAIVehicle *ai, cAIVehicleParams1 *p1, cAIVehicleParams2 *p2,
	cAIVehicleParams3 *p3) {
	cAIVehicle_CheckParams1(ai, p1);
	cAIVehicle_CheckParams2(ai, p2);
	cAIVehicle_CheckParams3(ai, p3);
}

/* a value out of the range is reported with the vehicle's name, then CHECKed (the CHECK range of
 * CONTROL_HOOK_RACE is narrower than the reported one) */
#define AIV_PARAM(self, bad, expr) \
	do { \
		if (bad) { \
			Con_Bluef("ERROR PARAM.: %s\n", (self)->vehicle->name); \
			CHECK(expr); \
		} \
	} while (0)

void __thiscall cAIVehicle_CheckParams1(cAIVehicle *self, const cAIVehicleParams1 *p) {
	AIV_PARAM(self, p->boostGetAway < 0 || p->boostGetAway > 1, (p->boostGetAway >= 0) && (p->boostGetAway <= 1));
	AIV_PARAM(self, p->boostItemAmmo < 0 || p->boostItemAmmo > 1,
		(p->boostItemAmmo >= 0) && (p->boostItemAmmo <= 1));
	AIV_PARAM(self, p->boostItemLife < 0 || p->boostItemLife > 1,
		(p->boostItemLife >= 0) && (p->boostItemLife <= 1));
	AIV_PARAM(self, p->hookRace < 0 || p->hookRace > 10, (p->hookRace >= 0) && (p->hookRace <= 1));
	AIV_PARAM(self, p->strafeEvasion < 0 || p->strafeEvasion > 1,
		(p->strafeEvasion >= 0) && (p->strafeEvasion <= 1));
	AIV_PARAM(self, !(p->damageGetAway >= 0.0f) || !(p->damageGetAway <= 1.0f),
		(p->damageGetAway >= 0.0f) && (p->damageGetAway <= 1.0f));
	AIV_PARAM(self, !(p->damageItemLife >= 0.0f) || !(p->damageItemLife <= 1.0f),
		(p->damageItemLife >= 0.0f) && (p->damageItemLife <= 1.0f));
	AIV_PARAM(self, p->hookMap < 0 || p->hookMap > 10, (p->hookMap >= 0) && (p->hookMap <= 10));
	AIV_PARAM(self, !(p->itemAmmo >= 0.0f) || !(p->itemAmmo <= 1.0f),
		(p->itemAmmo >= 0.0f) && (p->itemAmmo <= 1.0f));
	AIV_PARAM(self, p->percHook < 0 || p->percHook > 100, (p->percHook >= 0) && (p->percHook <= 100));
	AIV_PARAM(self, p->percHookGetAway < 0 || p->percHookGetAway > 100,
		(p->percHookGetAway >= 0) && (p->percHookGetAway <= 100));
}

void __thiscall cAIVehicle_CheckParams2(cAIVehicle *self, const cAIVehicleParams2 *p) {
	AIV_PARAM(self, p->aim < 1 || p->aim > 10, (p->aim >= 1) && (p->aim <= 10));
	AIV_PARAM(self, p->brake < 0 || p->brake > 1, (p->brake >= 0) && (p->brake <= 1));
	AIV_PARAM(self, p->strafe != 2, p->strafe == 2);
	AIV_PARAM(self, p->maxTimeStrafe < 0 || p->maxTimeStrafe > 10,
		(p->maxTimeStrafe >= 0) && (p->maxTimeStrafe <= 10));
	AIV_PARAM(self, !(p->maxVelStrafe >= 0.0f) || !(p->maxVelStrafe <= 1.0f),
		(p->maxVelStrafe >= 0.0f) && (p->maxVelStrafe <= 1.0f));
	AIV_PARAM(self, p->stupidPathfinding != 0, p->stupidPathfinding == 0);
}

void __thiscall cAIVehicle_CheckParams3(cAIVehicle *self, const cAIVehicleParams3 *p) {
	AIV_PARAM(self, !(p->angAttack >= 0.0f) || !((double)p->angAttack <= M_PI_D),
		(p->angAttack >= 0.0f) && ((double)p->angAttack <= M_PI_D));
	AIV_PARAM(self, p->cadence < 0 || p->cadence > 100, (p->cadence >= 0) && (p->cadence <= 100));
	AIV_PARAM(self, p->cadenceCM < 0 || p->cadenceCM > 7, (p->cadenceCM >= 0) && (p->cadenceCM <= 7));
	AIV_PARAM(self, !(p->counterattack >= 0.0f) || !(p->counterattack <= 1.0f),
		(p->counterattack >= 0.0f) && (p->counterattack <= 1.0f));
	AIV_PARAM(self, p->percCloud < 0 || p->percCloud > 100, (p->percCloud >= 0) && (p->percCloud <= 100));
	AIV_PARAM(self, p->percElectro < 0 || p->percElectro > 100, (p->percElectro >= 0) && (p->percElectro <= 100));
	AIV_PARAM(self, p->percSonic < 0 || p->percSonic > 100, (p->percSonic >= 0) && (p->percSonic <= 100));
}

/* ---- tests of the vehicle ---- */

byte __thiscall cAIVehicle_IsNear(cAIVehicle *self, const vec3_t *p) {
	vec3_t a;
	const vec3_t *v = self->vehicle->vftable->GetPos(self->vehicle, &a);
	float dy = v->y - p->y;
	float dx = v->x - p->x;
	float dz = v->z - p->z;

	return AIV_NEAR_DIST2 > dy * dy + dx * dx + dz * dz;
}

byte __thiscall cAIVehicle_ControlFlag(cAIVehicle *self) {
	return cAIVehicleControl_GetFlag(self->control);
}

byte __thiscall cAIVehicle_ControlFlag2(cAIVehicle *self) {
	return cAIVehicleControl_GetFlag2(self->control);
}

/* d from the vehicle to the point within HookRange, farther than r and at most 45 degrees off the nose */
static const vec3_t s_Nose = { 0.0f, 0.0f, -1.0f };

/* the direction of the nose of the vehicle */
static void AIV_NoseDir(cAIVehicle *self, vec3_t *dir) {
	vec3_t rot;

	AIV_EntVtbl(self->vehicle)->GetRot(self->vehicle, &rot);
	Vec3_Rotate(dir, &s_Nose, &rot);
}

static byte AIV_HookAngle(cAIVehicle *self, const vec3_t *d, float d2, float r) {
	vec3_t dir;

	if (d2 > g_HookRange * g_HookRange) {
		return 0;
	}
	r = r + r;
	if (r * r > d2) {
		return 0;
	}
	AIV_NoseDir(self, &dir);
	return !(Vec3_Angle(&dir, d) > AIV_HOOK_ANGLE);
}

byte __thiscall cAIVehicle_InHookRange(cAIVehicle *self, cEntity *e) {
	vec3_t a, b, d;
	const vec3_t *v = self->vehicle->vftable->GetPos(self->vehicle, &a);
	const vec3_t *p = e->vftable->GetPos(e, &b);

	d.x = p->x - v->x;
	d.y = p->y - v->y;
	d.z = p->z - v->z;
	return AIV_HookAngle(self, &d, d.y * d.y + d.x * d.x + d.z * d.z, Veh(self->vehicle)->radius + Veh(e)->radius);
}

byte __thiscall cAIVehicle_CanHook(cAIVehicle *self, cEntity *e) {
	vec3_t a, b;

	if (!cAIVehicle_InHookRange(self, e)) {
		return 0;
	}
	cEntity *v = self->vehicle;
	const vec3_t *to = e->vftable->GetPos(e, &a);
	const vec3_t *from = v->vftable->GetPos(v, &b);
	return Engine_TestLine(from, to, NULL, NULL) == 0;
}

byte __thiscall cAIVehicle_InHookRangePos(cAIVehicle *self, const vec3_t *p) {
	vec3_t a, d;
	const vec3_t *v = self->vehicle->vftable->GetPos(self->vehicle, &a);

	d.x = p->x - v->x;
	d.y = p->y - v->y;
	d.z = p->z - v->z;
	return AIV_HookAngle(self, &d, d.y * d.y + d.x * d.x + d.z * d.z, Veh(self->vehicle)->radius);
}

byte __stdcall cAIVehicle_IsGroupState(int state) {
	return !AIV_FreeState(state);
}

/* the vehicle is steered (thrust or turn) in a moving state */
byte __thiscall cAIVehicle_IsMoving(cAIVehicle *self) {
	int in = self->internState;

	if (in == AIVI_STOPPED || in == AIVI_STATE_INTERN_NONE || in == AIVI_ORIENT_SHOOTING) {
		return 0;
	}
	int s = self->state;
	if (s == AIV_FREEZE || s == AIV_FREEZE_OBJ_NOT_ACCESS || s == AIV_FREEZE_LOST || s == AIV_FREEZE_CHANGE_STATE ||
			s == AIV_FIGHTING_FREEZE || s == AIV_STATE_NONE) {
		return 0;
	}
	cVehicleEntity *v = Veh(self->vehicle);
	return v->thrustIn != 0.0f || v->turnX != 0.0f || v->turnY != 0.0f;
}

/* ---- the debug text: "[time] (vehicle) [state - internal state] - text" on the console ---- */

void cAIVehicle_Debugf(cAIVehicle *self, const char *fmt, ...) {
	char text[1024];
	char intern[256];
	char state[256];
	char time[12];
	va_list ap;

	crt_strtime(time);
	cAIVehicle_StateName(self->state, state);
	cAIVehicle_InternStateName(self->internState, intern);
	Str_PrintfQuiet(text, sizeof(text), "[%s] (%s) [%s - %s] - ", time, self->vehicle->name, state, intern);
	va_start(ap, fmt);
	Str_VPrintf(state, 0xff, fmt, ap);
	va_end(ap);
	crt_strncat(text, state, sizeof(text) - strlen(text) - 1);
	crt_strncat(text, "\n", sizeof(text) - strlen(text) - 1);
	Con_Print(0xffffff, text, 1);
}

/* ---- the events of the move (cAIMove), by state ---- */

void __thiscall cAIVehicle_Event(cAIVehicle *self, int ev) {
	byte done = 0;

	switch (self->state) {
	case AIV_GO_TO:
		done = cAIVehicle_EventGoTo(self, ev);
		break;
	case AIV_FIGHTING:
	case AIV_FIGHTING_HOOK:
		done = cAIVehicle_EventFighting(self, ev);
		break;
	case AIV_IN_ROUTE:
		done = cAIVehicle_EventInRoute(self, ev);
		break;
	case AIV_TRAFFIC:
		done = cAIVehicle_EventTraffic(self, ev);
		break;
	case AIV_GET_AWAY_STOP:
		done = cAIVehicle_EventGetAwayStop(self, ev);
		break;
	case AIV_CHASE:
		done = cAIVehicle_EventChase(self, ev);
		break;
	case AIV_TRAFFIC_INDOOR:
		done = cAIVehicle_EventTrafficInDoor(self, ev);
		break;
	case AIV_GET_AWAY:
		done = cAIVehicle_EventGetAway(self, ev);
		break;
	case AIV_LOST_PATH:
		done = cAIVehicle_EventLostPath(self, ev);
		break;
	}
	if (!done) {
		cAIVehicle_EventDefault(self, ev);
	}
}

byte __thiscall cAIVehicle_EventChase(cAIVehicle *self, int ev) {
	if (ev != 2) {
		return 0;
	}
	if (self->internState != AIVI_FINISHING_LINE_LIFE) {
		self->vftable->OnEvent(self, 2);
		cAIVehicle_SetInternState(self, AIVI_CHASE_NOT_SHOOTING, (double)self->chaseMin, (double)self->chaseMax);
		return 1;
	}
	self->vftable->OnEvent(self, 2);
	cAIVehicle_ItemsChase(self);
	return 1;
}

/* the events every state handles: 0 the path is lost, 1 the target is not reachable, 3 the target went */
byte __thiscall cAIVehicle_EventDefault(cAIVehicle *self, int ev) {
	switch (ev) {
	case 0: {
		vec3_t a;
		self->vftable->OnEvent(self, 0);
		cAIVehicle_SaveState(self, &self->saved);
		int s = self->state;
		int st = AIV_LOST_PATH;
		if (s == AIV_FIGHTING || s == AIV_GET_AWAY_STOP || s == AIV_CHASE || s == AIV_FIGHTING_HOOK ||
				s == AIV_GET_AWAY) {
			const vec3_t *v = cAIVehicleControl_GetDest(self->control, &a);
			if (v->x == -FLT_MAX && v->y == -FLT_MAX && v->z == -FLT_MAX) {
				st = AIV_LOST_PATH_CHASE;
			}
		}
		if (!cAIVehicle_SetStateIntern(self, st)) {
			cAIVehicle_SetStateIntern(self, AIV_FREEZE_LOST);
		}
		return 1;
	}
	case 1:
		self->vftable->OnEvent(self, 1);
		cAIVehicle_SetStateIntern(self, AIV_FREEZE_OBJ_NOT_ACCESS);
		return 1;
	case 3:
		self->vftable->OnEvent(self, 3);
		cAIVehicle_SetStateIntern(self, AIV_FREEZE);
		if (cPy_Function_IsValid(g_OnObjectiveNullAI)) {
			cPy_Function_AddParamS(g_OnObjectiveNullAI, self->vehicle->name);
			cPy_Function_Execute(g_OnObjectiveNullAI, 0);
		}
		return 1;
	}
	return 0;
}

byte __thiscall cAIVehicle_EventFighting(cAIVehicle *self, int ev) {
	if (ev != 2) {
		return 0;
	}
	switch (self->internState) {
	case AIVI_FINISHING_LINE_LIFE:
		self->vftable->OnEvent(self, 2);
		cAIVehicle_ItemsFighting(self);
		return 1;
	case AIVI_FINISHING_LINE_AMMO:
		self->vftable->OnEvent(self, 2);
		cAIVehicle_FightingAfterItem(self);
		return 1;
	}
	self->vftable->OnEvent(self, 2);
	cAIVehicle_SetInternState(self, AIVI_CHASE_SHOOTING, (double)self->chaseMin, (double)self->chaseMax);
	return 1;
}

/* the internal state FOLLOW_THE_PATH_WITH_SHOOT_CM from the target to the vehicle */
static void AIV_FollowAway(cAIVehicle *self, int ev) {
	vec3_t a, b;

	self->vftable->OnEvent(self, ev);
	cEntity *t = AIV_Target(self);
	const vec3_t *tp = t->vftable->GetPos(t, &a);
	const vec3_t *vp = self->vehicle->vftable->GetPos(self->vehicle, &b);
	vec3_t d;
	d.z = vp->z - tp->z;
	d.x = vp->x - tp->x;
	d.y = vp->y - tp->y;
	cAIVehicle_SetInternState(self, AIVI_FOLLOW_THE_PATH_WITH_SHOOT_CM, d, (double)g_RadiusObjectiveInRoute,
		self->params.boostGetAway, 0);
}

byte __thiscall cAIVehicle_EventGetAway(cAIVehicle *self, int ev) {
	if (self->internState == AIVI_FINISHING_LINE_LIFE) {
		if (ev == 2) {
			self->vftable->OnEvent(self, 2);
			cAIVehicle_ItemsGetAway(self);
			return 1;
		}
	}
	else if (self->internState == AIVI_FOLLOW_THE_PATH_WITH_SHOOT_CM && (ev == 2 || ev == 4)) {
		AIV_FollowAway(self, ev);
		return 1;
	}
	return 0;
}

byte __thiscall cAIVehicle_EventGetAwayStop(cAIVehicle *self, int ev) {
	if (self->internState == AIVI_FINISHING_LINE_LIFE) {
		if (ev == 2) {
			self->vftable->OnEvent(self, 2);
			cAIVehicle_ItemsGetAwayStop(self);
			return 1;
		}
	}
	else if (self->internState == AIVI_FOLLOW_THE_PATH_WITH_SHOOT_CM && (ev == 2 || ev == 4)) {
		AIV_FollowAway(self, ev);
		return 1;
	}
	return 0;
}

/* GO_TO arrived: OnItemTakenAI and the callback of the AI; FREEZE when neither took it and the route goes on */
byte __thiscall cAIVehicle_EventGoTo(cAIVehicle *self, int ev) {
	if (ev != 2) {
		return 0;
	}
	byte taken = 0;
	if (cPy_Function_IsValid(g_OnItemTakenAI)) {
		cPy_Function_AddParamS(g_OnItemTakenAI, self->vehicle->name);
		cPy_Function_Execute(g_OnItemTakenAI, 0);
		taken = 1;
	}
	if (self->onArrive) {
		taken = self->onArrive(self);
	}
	if (!taken && !self->b32c) {
		self->vftable->OnEvent(self, 2);
		cAIVehicle_SetStateIntern(self, AIV_FREEZE);
	}
	return 1;
}

byte __thiscall cAIVehicle_EventInRoute(cAIVehicle *self, int ev) {
	if (self->internState == AIVI_FINISHING_LINE_LIFE) {
		if (ev == 2) {
			self->vftable->OnEvent(self, 2);
			cAIVehicle_ItemsInRoute(self);
			return 1;
		}
	}
	else if (self->internState == AIVI_FOLLOW_THE_PATH && (ev == 2 || ev == 4)) {
		vec3_t dir;
		self->vftable->OnEvent(self, ev);
		AIV_NoseDir(self, &dir);
		cAIVehicle_SetInternState(self, AIVI_FOLLOW_THE_PATH, dir, (double)g_RadiusObjectiveInRoute, 0, 1,
			self->repos, (double)self->rotA, (double)self->rotB);
		return 1;
	}
	return 0;
}

byte __thiscall cAIVehicle_EventLostPath(cAIVehicle *self, int ev) {
	if (ev != 7) {
		return 0;
	}
	self->vftable->OnEvent(self, 7);
	cAIVehicle_SetStateIntern(self, AIV_FREEZE_LOST);
	return 1;
}

/* the vehicle on to the next node p of the track (FINISHING_LINE_DIRECT), the lane of the vehicle from its last
 * point to it */
static void AIV_NextNode(cAIVehicle *self, vec3_t *p, int track, int node, float radius) {
	vec3_t a;

	self->vftable->OnEvent(self, 2);
	*p = *cAIVehicle_TrackPoint(self, &a, p, track, node);
	self->track = track;
	self->node = node;
	cVehicleTraffic *vt = AIV_VehTraffic(self->vehicle);
	vt->begin = self->dest;
	AIV_VehTraffic(self->vehicle)->end = *p;
	cAIVehicle_SetInternState(self, AIVI_FINISHING_LINE_DIRECT, *p, (double)radius, 0, (double)self->rotA,
		(double)self->rotB);
}

/* arrived at a node: on to the next one, at the end of the track off the track and stopped */
byte __thiscall cAIVehicle_EventTraffic(cAIVehicle *self, int ev) {
	vec3_t p;
	int track, node;

	if (ev != 2) {
		return 0;
	}
	CNodesTraffic *t = AIV_Outdoor();
	if (t->vftable->NextAway(t, &p, self->track, self->node, &track, &node)) {
		AIV_NextNode(self, &p, track, node, g_RadiusObjectiveTraffic);
		return 1;
	}
	self->vftable->OnEvent(self, 2);
	t = AIV_Outdoor();
	t->vftable->RemoveVehicle(t, self->track);
	cAIVehicle_SetInternState(self, AIVI_STOPPED);
	self->vehicle->vftable->SetActive(self->vehicle, 0);
	return 1;
}

byte __thiscall cAIVehicle_EventTrafficInDoor(cAIVehicle *self, int ev) {
	vec3_t p, a;
	int track, node;

	if (ev != 2) {
		return 0;
	}
	CNodesTraffic *t = AIV_Indoor();
	if (t->vftable->NextAway(t, &p, self->track, self->node, &track, &node)) {
		AIV_NextNode(self, &p, track, node, g_RadiusObjectiveTrafficInDoor);
		return 1;
	}
	/* the end of the track: off once the vehicle is near the last point of the control (seen from above) */
	cAIVehicleControl_GetGoal(self->control, &p);
	p.y = 0.0f;
	self->vehicle->vftable->GetPos(self->vehicle, &a);
	a.y = 0.0f;
	float dx = p.x - a.x;
	float dy = p.y - 0.0f;
	float dz = p.z - a.z;
	if (!(dy * dy + dx * dx + dz * dz > AIV_ARRIVED_DIST2)) {
		self->vftable->OnEvent(self, 2);
		t = AIV_Indoor();
		t->vftable->RemoveVehicle(t, self->track);
		cAIVehicle_SetInternState(self, AIVI_STOPPED);
		self->vehicle->vftable->SetActive(self->vehicle, 0);
	}
	return 1;
}

/* ---- the traffic track ---- */

static void AIV_SetRotSpeed(cAIVehicle *self, float speed) {
	float a = speed + speed;
	self->rotB = speed * AIV_ROT_SPEED_F / AIV_ROT_SPEED_B;
	self->rotA = a / AIV_ROT_SPEED_A;
}

/* the vehicle put on the track at pos, turned to the node, on its way to the node with FINISHING_LINE_DIRECT;
 * 0 if an entity is in the way */
byte __thiscall cAIVehicle_PlaceOnTrack(cAIVehicle *ai, int type, const vec3_t *pos, int track, int node) {
	cAIVehicle *self = ai;
	cEntity *v = self->vehicle;
	cEntity *hit;
	vec3_t np, d, ang, tmp;
	cTrafficTrackInfo info;
	float speed;

	self->trackType = type;
	if (cWorld_TestSphereEntities(g_pWorld, v, (void *)1, pos, Veh(v)->radius * 3.0f, &hit, 1, 0)) {
		return 0;
	}
	cAIVehicleControl_ClearDest(self->control);
	self->track = track;
	self->node = node;
	self->vehicle->vftable->SetActive(self->vehicle, 0);
	self->vehicle->vftable->SetActive(self->vehicle, 1);
	self->vehicle->vftable->SetPos(self->vehicle, pos);
	CNodesTraffic *t = AIV_Indoor();
	t->vftable->GetNode(t, &np, self->track, self->node);
	d.x = pos->x - np.x;
	d.y = pos->y - np.y;
	d.z = pos->z - np.z;
	Vec3_GetRotAng(&ang, &d);
	AIV_EntVtbl(self->vehicle)->SetRot(self->vehicle, &ang);
	cAIVehicleControl_Stop(self->control);
	tmp.x = 0.0f;
	tmp.y = 0.0f;
	tmp.z = 0.0f;
	AIV_EntVtbl(self->vehicle)->SetVel(self->vehicle, &tmp);
	np = *cAIVehicle_TrackPointRandom(self, &tmp, &np, self->track, self->node);
	type = self->trackType;
	if (type == 1 || type == 0 || type == 2 ||
			((type == 4 || type == 3) && AIV_VehTraffic(self->vehicle)->type != 0)) {
		t = AIV_Indoor();
		t->vftable->GetInfo(t, &info, self->track);
		AIV_VehTraffic(self->vehicle)->speed = info.speed;
		speed = info.speed;
	}
	else {
		speed = g_VelTrafficLightRotAI;
	}
	AIV_SetRotSpeed(self, speed);
	const vec3_t *p = self->vehicle->vftable->GetPos(self->vehicle, &tmp);
	AIV_VehTraffic(self->vehicle)->begin = *p;
	AIV_VehTraffic(self->vehicle)->end = np;
	t = AIV_Indoor();
	t->vftable->AddVehicle(t, self->track);
	cAIVehicle_SetInternState(self, AIVI_FINISHING_LINE_DIRECT, np, (double)g_RadiusObjectiveTrafficInDoor, 0,
		(double)self->rotA, (double)self->rotB);
	self->i34 = 2;
	return 1;
}

/* on the track at its first node, on the way to the second */
byte __thiscall cAIVehicle_PlaceOnTrackStart(cAIVehicle *self, int track) {
	vec3_t p;
	CNodesTraffic *t = AIV_Indoor();

	t->vftable->GetNode(t, &p, track, 0);
	return cAIVehicle_PlaceOnTrack(self, self->trackType, &p, track, 1);
}

/* the state kept for after an event */
void __thiscall cAIVehicle_SaveState(cAIVehicle *self, cAIVehicleSaved *s) {
	cEntity *t = AIV_Target(self);

	if (t) {
		const char *src = t->name;
		char *dst = s->target;
		int n = sizeof(s->target);
		while (--n > 0 && *src) {
			*dst++ = *src++;
		}
		*dst = 0;
	}
	else {
		s->target[0] = 0;
	}
	s->b18 = self->b1a8;
	s->dest2 = self->dest2;
	s->dest = self->dest;
	s->speed = self->speed;
	s->f30c = self->f30c;
	s->repos = self->repos;
	s->f2f0 = self->f2f0;
	s->state = self->state;
	s->internState = self->internState;
	s->b20 = self->changing;
	s->chaseMin = self->chaseMin;
	s->chaseMax = self->chaseMax;
	s->b32c = self->b32c;
	s->rotA = self->rotA;
	s->rotB = self->rotB;
}

/* the point where the segment a b crosses the plane: 1, out set; 0 if both ends are on one side */
int SegmentPlane_Hit(vec3_t *out, const vec3_t *a, const vec3_t *b, const plane_t *pl) {
	float da = pl->n.x * a->x + pl->n.y * a->y + pl->n.z * a->z + pl->d;
	float db = b->x * pl->n.x + b->y * pl->n.y + b->z * pl->n.z + pl->d;

	if ((0.0f > db) == (0.0f > da)) {
		return 0;
	}
	float t = da / (da - db);
	out->x = (b->x - a->x) * t + a->x;
	out->y = (b->y - a->y) * t + a->y;
	out->z = (b->z - a->z) * t + a->z;
	return 1;
}

void __thiscall cAIVehicle_SetRotSpeed(cAIVehicle *self, float speed) {
	AIV_SetRotSpeed(self, speed);
}

/* the kind of the traffic track for the class of the vehicle, at random among the kinds the graph has; 5 none */
void __thiscall cAIVehicle_PickTrackType(cAIVehicle *self) {
	int list[10];
	int n = 0;
	CNodesTraffic *t = g_IsMapOutDoor ? AIV_Outdoor() : AIV_Indoor();
	int cls = AIV_VehTraffic(self->vehicle)->type;

	if (cls == 2) {
		if (t->vftable->NumByType(t, 0) > 0) {
			list[0] = 0;
			n = cls - 1;
		}
	}
	else if (cls != 0) {
		goto medium;
	}
	if (t->vftable->NumByType(t, 3) > 0) {
		list[n++] = 3;
	}
medium:
	if (cls == 1) {
		if (t->vftable->NumByType(t, 2) > 0) {
			list[n++] = 2;
		}
	}
	else if (cls != 0) {
		goto done;
	}
	if (t->vftable->NumByType(t, 4) > 0) {
		list[n++] = 4;
	}
	if (cls == 0 && t->vftable->NumByType(t, 1) > 0) {
		list[n++] = 1;
	}
done:
	if (n > 0) {
		self->trackType = list[crt_rand() % n];
		return;
	}
	self->trackType = 5;
}

void __thiscall cAIVehicle_SetInertia(cAIVehicle *ai, int on) {
	cAIVehicleControl_SetInertia(ai->control, on);
}

void __thiscall cAIVehicle_SetRotStatic(cAIVehicle *ai, float a, float b) {
	ai->rotA = a;
	ai->rotB = b;
	cAIVehicleControl_SetRotStatic(ai->control, a, b);
}

/* ---- the state changes ---- */

byte cAIVehicle_SetState(cAIVehicle *ai, int state, ...) {
	char name[256];
	va_list ap;

	cAIVehicle_StateName(state, name);
	ai->vftable->Debugf(ai, "Demanda externa de CAMBIO de ESTADO -> %s", name);
	va_start(ap, state);
	byte r = cAIVehicle_ChangeState(ai, state, ap);
	va_end(ap);
	return r;
}

/* the state change from inside the AI: the group lets the next change through at once */
byte cAIVehicle_SetStateIntern(cAIVehicle *self, int state, ...) {
	char name[256];
	va_list ap;

	cAIVehicle_StateName(state, name);
	self->vftable->Debugf(self, "Demanda interna de CAMBIO de ESTADO -> %s", name);
	if (!AIV_FreeState(state)) {
		g_VehicleGroup->vftable->AllowChange(g_VehicleGroup);
	}
	va_start(ap, state);
	byte r = cAIVehicle_ChangeState(self, state, ap);
	va_end(ap);
	return r;
}

/* the saved state taken again with its arguments; the target by its name */
static byte AIV_Restore(cAIVehicle *self, const cAIVehicleSaved *s, byte (*set)(cAIVehicle *self, int state, ...)) {
	cEntity *t;

	switch (s->state) {
	case AIV_FREEZE:
	case AIV_FREEZE_OBJ_NOT_ACCESS:
	case AIV_FREEZE_LOST:
	case AIV_FREEZE_CHANGE_STATE:
		return set(self, s->state);
	case AIV_GO_TO:
		return set(self, AIV_GO_TO, s->dest, (double)s->speed, s->b18, (double)s->f30c, (double)s->rotA,
			(double)s->rotB, s->b32c);
	case AIV_FIGHTING:
	case AIV_FIGHTING_FREEZE:
	case AIV_FIGHTING_HOOK:
	case AIV_GET_AWAY:
		t = World_FindEntity(s->target);
		return set(self, s->state, t);
	case AIV_IN_ROUTE:
		return set(self, AIV_IN_ROUTE, s->repos, (double)s->rotA, (double)s->rotB);
	case AIV_TRAFFIC:
		return set(self, AIV_TRAFFIC);
	case AIV_GET_AWAY_STOP:
		t = World_FindEntity(s->target);
		return set(self, s->state, t, (double)s->f2f0);
	case AIV_RACE:
		return set(self, AIV_RACE, s->dest, (double)s->speed, s->b18, s->b32c);
	case AIV_CHASE:
		t = World_FindEntity(s->target);
		return set(self, s->state, t, (double)s->chaseMin, (double)s->chaseMax);
	case AIV_TRAFFIC_INDOOR:
		return set(self, AIV_TRAFFIC_INDOOR);
	case AIV_LOST_PATH:
		return set(self, AIV_LOST_PATH);
	case AIV_LOST_PATH_CHASE:
		return set(self, AIV_LOST_PATH_CHASE);
	}
	return 1;
}

byte __thiscall cAIVehicle_RestoreState(cAIVehicle *self, const cAIVehicleSaved *s) {
	return AIV_Restore(self, s, cAIVehicle_SetState);
}

byte __thiscall cAIVehicle_RestoreStateIntern(cAIVehicle *self, const cAIVehicleSaved *s) {
	return AIV_Restore(self, s, cAIVehicle_SetStateIntern);
}

/* ---- the items ---- */

/* the nearest item entity of the list (kind < 0 the life items, else the ammo items of the kind) that is there
 * (respawn time not running) and not exclude; its distance into *dist, its description into out */
static cEntity *AIV_NearestItem(cAIVehicle *self, int kind, float *dist, cItemDesc *out, cEntity *exclude) {
	void *iter;
	cEntity *best = NULL;
	cItemDesc *it;
	cItemList *items = &g_pWorld->items;

	*dist = FLT_MAX;
	it = kind < 0 ? items->vftable->FirstLife(items, &iter) : items->vftable->FirstAmmo(items, kind, &iter);
	while (it) {
		cEntity *e = World_FindEntity(it->name);
		if (e != exclude && 0.0f >= ((cItemEntity *)e)->respawn) {
			vec3_t a, b;
			const vec3_t *p = e->vftable->GetPos(e, &a);
			const vec3_t *v = self->vehicle->vftable->GetPos(self->vehicle, &b);
			float dx = v->x - p->x;
			float dy = v->y - p->y;
			float dz = v->z - p->z;
			float d = (float)__builtin_sqrt((double)(dx * dx + dy * dy + dz * dz));
			if (*dist > d) {
				*dist = d;
				cString_Assign(&out->name, &it->name);
				out->value = it->value;
				best = e;
			}
		}
		items = &g_pWorld->items;
		it = kind < 0 ? items->vftable->NextLife(items, &iter) : items->vftable->NextAmmo(items, kind, &iter);
	}
	return best;
}

cEntity *__thiscall cAIVehicle_NearestLife(cAIVehicle *self, float *dist, cItemDesc *out, cEntity *exclude) {
	return AIV_NearestItem(self, -1, dist, out, exclude);
}

cEntity *__thiscall cAIVehicle_NearestAmmo(cAIVehicle *self, int kind, float *dist, cItemDesc *out,
	cEntity *exclude) {
	return AIV_NearestItem(self, kind, dist, out, exclude);
}

void __thiscall cAIVehicle_FindLife(cAIVehicle *self, cEntity *exclude) {
	float dist;

	self->itemLifeEnt = cAIVehicle_NearestLife(self, &dist, &self->itemLife, exclude);
}

cEntity *__thiscall cAIVehicle_GetNearestItemLife(cAIVehicle *ai) {
	cItemDesc desc;
	float dist;

	cString_ctor(&desc.name);
	cEntity *e = cAIVehicle_NearestLife(ai, &dist, &desc, NULL);
	cString_dtor(&desc.name);
	return e;
}

int __thiscall cAIVehicle_GetTrackType(cAIVehicle *self) {
	return self->trackType;
}

void __thiscall cAIVehicle_WeapListChanged(void *ai) {
	cAIVehicleShoot_WeapListChanged(((cAIVehicle *)ai)->shoot);
}
