/*
 * The AI of the vehicles, original source AI/AIVehicle.cpp (include/scrap/aivehicle.h): construction, the update
 * slots and their dispatch by state, the events of the move, the state names and the debug text, the parameter
 * checks, the state kept for after an event, the item searches and the placing on a traffic track. The decisions of
 * the single states (the React, Think and Items functions), the internal state switch and the external state change
 * are still the exe's.
 */
#include <float.h>
#include <string.h>

#include "scrap/gvar.h"
#include "scrap/aivehicle.h"
#include "scrap/camcontrol.h"
#include "scrap/console.h"
#include "scrap/embeddedfxvehicle.h"
#include "scrap/fxmisc.h"
#include "scrap/itementity.h"
#include "scrap/scrap.h"
#include "scrap/vehicleentity.h"
#include "scrap/vehicletraffic.h"
#include "scrap/weapons.h"
#include "scrap/staticinit.h"

#define CAIVEHICLE_VFTABLE (&cAIVehicle_vftable)
#define g_MaxDistReposNotVisAI (GVar_MaxDistReposNotVisAI.value.f)
#define g_MinDistReposNotVisAI (GVar_MinDistReposNotVisAI.value.f)
#define g_TimeDamageAI (GVar_TimeDamageAI.value.f)
#define g_OnObjectiveNullAI (&GVar_OnObjectiveNullAI.func)
#define g_OnItemTakenAI (&GVar_OnItemTakenAI.func)
static const cAIMoveVtbl cAIVehicle_vftable;
static cGVar GVar_TimeDamageAI;
static cGVarFunction GVar_OnObjectiveNullAI;
static cGVarFunction GVar_OnItemTakenAI;
static cGVar GVar_MinDistReposNotVisAI;
static cGVar GVar_MaxDistReposNotVisAI;

static cGVar GVar_RADIUS_OBJECTIVE_TRAFFIC;
#define g_RadiusObjectiveTraffic (GVar_RADIUS_OBJECTIVE_TRAFFIC.value.f)		/* RADIUS_OBJECTIVE_TRAFFIC */
static cGVar GVar_RADIUS_OBJECTIVE_TRAFFIC_INDOOR;
#define g_RadiusObjectiveTrafficInDoor (GVar_RADIUS_OBJECTIVE_TRAFFIC_INDOOR.value.f)	/* RADIUS_OBJECTIVE_TRAFFIC_INDOOR */
extern cGVar GVar_RADIUS_OBJECTIVE_IN_ROUTE;
#define g_RadiusObjectiveInRoute (GVar_RADIUS_OBJECTIVE_IN_ROUTE.value.f)		/* RADIUS_OBJECTIVE_IN_ROUTE */
static cGVar GVar_VelTrafficLightRotAI;
#define g_VelTrafficLightRotAI (GVar_VelTrafficLightRotAI.value.f)
cGVar GVar_MaxDistAroundReposAI;

#define g_MaxDistAroundReposAI (GVar_MaxDistAroundReposAI.value.f)
#define AIV_NEAR_DIST2 225000000.0f		/* 15000 squared */
#define AIV_ROT_SPEED_A 8334.0f
#define AIV_ROT_SPEED_B 4167.0f
#define AIV_ROT_SPEED_F 0.025f
#define AIV_HOOK_ANGLE 0.7853981633974483	/* pi / 4 */
#define AIV_ARRIVED_DIST2 4000000.0f

static inline cVehicleTraffic *AIV_VehTraffic(cEntity *v) {
	return &Veh(v)->traffic;
}

static inline CNodesTraffic *AIV_Outdoor(void) {
	return (CNodesTraffic *)g_NodesTraffic;
}

CNodesTraffic *g_NodesTrafficInDoor;

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

static void cAIVehicle_CheckParams1(cAIVehicle *self, const cAIVehicleParams1 *p);
static void cAIVehicle_CheckParams2(cAIVehicle *self, const cAIVehicleParams2 *p);
static void cAIVehicle_CheckParams3(cAIVehicle *self, const cAIVehicleParams3 *p);
static void cAIVehicle_Debugf(cAI *base, const char *fmt, ...);
static byte cAIVehicle_SetStateIntern(cAIVehicle *self, int state, ...);
void *cAIVehicle_ctor(void *mem, cEntity *v, void *p1, void *p2, void *p3) {
	cAIVehicle *self = mem;

	cAIMove_ctor(&self->base, v);
	self->vftable = CAIVEHICLE_VFTABLE;
	cSlotController_ctor(&self->slotAttack, 0.2f);
	cSlotController_ctor(&self->slotGraph, 0.5f);
	cSlotController_ctor(&self->slotItems, 0.2f);
	cString_ctor(&self->itemLife.name);
	cString_ctor(&self->itemAmmo.name);
	cString_ctor(&self->itemAmmoK[0].name);
	cString_ctor(&self->itemAmmoK[1].name);
	cString_ctor(&self->itemAmmoK[2].name);
	self->vehicle = v;
	self->params = *(const cAIVehicleParams1 *)p1;
	self->paramsInit = *(const cAIVehicleParams1 *)p1;
	void *m = operator_new(sizeof(cAIVehicleControl));
	self->control = m ? cAIVehicleControl_ctor(m, v, self, p2) : NULL;
	CHECK(self->control);
	m = operator_new(sizeof(cAIVehicleShoot));
	self->shoot = m ? cAIVehicleShoot_ctor(m, v, self, p3) : NULL;
	CHECK(self->shoot);
	self->state = AIV_STATE_NONE;
	self->internState = AIVI_STATE_INTERN_NONE;
	cAIVehicle_Debugf(&self->base.base, "Entidad CREADA");
	self->damageTime = 0.0f;
	self->lifeMark = Veh(self->vehicle)->wl.life;
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
	self->boost = 0;
	self->trackPos.y = 0.0f;
	self->trackPos.z = 0.0f;
	self->trackPos.x = 0.0f;
	self->trackOfs.x = 0.0f;
	self->trackOfs.y = self->trackPos.y;
	self->trackOfs.z = self->trackPos.z;
	self->reposBehind = 0;
	self->accel = 0.0f;
	self->rotB = 0.0f;
	self->rotA = 0.0f;
	self->awayDist = 0.0f;
	self->itemAmmoStart = 0.0f;
	self->itemAmmoLevel = 0.0f;
	self->trackType = 5;
	self->goingForLife = 0;
	self->goingForAmmo = 0;
	self->itemAmmoBest = NULL;
	self->itemAmmoEnt[2] = NULL;
	self->itemAmmoEnt[1] = NULL;
	self->itemAmmoEnt[0] = NULL;
	self->itemLifeEnt = NULL;
	self->itemAmmoDistK[2] = 0.0f;
	self->itemAmmoDistK[1] = 0.0f;
	self->itemAmmoDistK[0] = 0.0f;
	self->unused_39c[2] = 0;
	self->unused_39c[1] = 0;
	self->unused_39c[0] = 0;
	self->unused_3a9[0] = 0;
	self->unused_3a9[1] = 0;
	self->unused_3a8 = 0;
	self->ammoSearch = 3;
	self->chaseMin = g_AIChaseDefault1;
	self->chaseMax = g_AIChaseDefault2;
	self->unused_318 = 0;
	self->stopAtGoal = 0;
	self->targetTime = 0.0f;
	self->hookSide = 0;
	self->onArrive = NULL;
	self->hookMode = 0;
	self->hookSaved = 0;
	cAIVehicle_CheckParams1(self, p1);
	cAIVehicle_CheckParams2(self, p2);
	cAIVehicle_CheckParams3(self, p3);
	return self;
}

/* ---- AI/AIVehicle.cpp (src/game/AI/IVehicle.c); more in ai.h, nodestraffic.h, embeddedfxvehicle.h, weapons.h,
 * vehicletraffic.h and vehicleentity.h (the constructor) ---- */
static void cAIVehicle_dtor(cAIVehicle *self) {
	self->vftable = CAIVEHICLE_VFTABLE;
	if (self->control) {
		AIVC_Delete(self->control, 1);
	}
	if (self->shoot) {
		AIVShoot_Delete(self->shoot, 1);
	}
	cAIVehicle_Debugf(&self->base.base, "Entidad DESTRUIDA");
	cString_dtor(&self->itemAmmoK[2].name);
	cString_dtor(&self->itemAmmoK[1].name);
	cString_dtor(&self->itemAmmoK[0].name);
	cString_dtor(&self->itemAmmo.name);
	cString_dtor(&self->itemLife.name);
	cAIMove_dtor(&self->base);
}

static cAI *cAIVehicle_Delete(cAI *base, byte flags) {
	cAIVehicle *self = (cAIVehicle *)base;
	cAIVehicle_dtor(self);
	if (flags & 1) {
		operator_delete_sized(self, CAIVEHICLE_SIZE);
	}
	return base;
}

void cAIVehicle_ApplyParams(cAIVehicle *self) {
	cAIVehicleControl_ApplyParams(self->control);
}

/* ---- the update slots ---- */

/* the reaction of the state to what happens around (the think slot of cAIMove) */
static void cAIVehicle_ReactFighting(cAIVehicle *self);
static void cAIVehicle_ReactGetAway(cAIVehicle *self);
static void cAIVehicle_ReactGetAwayStop(cAIVehicle *self);
static void cAIVehicle_ReactHook(cAIVehicle *self);
static void cAIVehicle_ReactLostPath(cAIVehicle *self);
static void cAIVehicle_React(cAIVehicle *self) {
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
	case AIV_LOST_PATH_CHASE:
		cAIVehicle_ReactLostPath(self);
		return;
	}
}

/* the damage clock and the move of the state; 1 if the state did not change */
static byte cAIVehicle_CheckTarget(cAIVehicle *self);
static void cAIVehicle_ThinkChase(cAIVehicle *self);
static void cAIVehicle_ThinkFighting(cAIVehicle *self);
static void cAIVehicle_ThinkGetAway(cAIVehicle *self);
static void cAIVehicle_ThinkGetAwayStop(cAIVehicle *self);
static void cAIVehicle_ThinkGoTo(cAIVehicle *self);
static void cAIVehicle_ThinkLostPath(cAIVehicle *self);
static void cAIVehicle_ThinkRace(cAIVehicle *self);
byte cAIVehicle_Think(cAIVehicle *self) {
	float t = self->base.base.dt + self->damageTime;

	self->damageTime = t;
	if (t >= g_TimeDamageAI) {
		self->damageTime = 0.0f;
		self->lifeMark = Veh(self->vehicle)->wl.life;
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
			self->targetTime = self->targetTime + self->base.base.dt;
		}
		break;
	case AIV_GET_AWAY:
		cAIVehicle_ThinkGetAway(self);
		break;
	case AIV_LOST_PATH:
	case AIV_LOST_PATH_CHASE:
		cAIVehicle_ThinkLostPath(self);
		break;
	}
	return self->base.base.changed == 0;
}

/* the items the state goes for */
static void cAIVehicle_ItemsChase(cAIVehicle *self);
static void cAIVehicle_ItemsFighting(cAIVehicle *self);
static void cAIVehicle_ItemsGetAway(cAIVehicle *self);
static void cAIVehicle_ItemsGetAwayStop(cAIVehicle *self);
static void cAIVehicle_ItemsInRoute(cAIVehicle *self);
static void cAIVehicle_CheckItems(cAIVehicle *self) {
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
static void cAIVehicle_GraphTraffic(cAIVehicle *self);
static void cAIVehicle_GraphTrafficInDoor(cAIVehicle *self);
static byte cAIVehicle_NeedRepos(cAIVehicle *self);
static byte cAIVehicle_Repos(cAIVehicle *self);
static void cAIVehicle_UpdateRoute(cAIVehicle *self);
static void cAIVehicle_UpdateGraph(cAIVehicle *self) {
	cSlotController *slot = &self->slotGraph;
	float t;

	if (!cSlotController_Tick(slot, self->base.base.dt) || !cSlotController_FireReset(slot, &t)) {
		return;
	}
	do {
		cWorld *w = g_pWorld;
		vec3_t cam = w->camPos[w->curCam];
		if (!NodesWorld_InGrid(g_NodesWorld, &cam)) {
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

/* the camera entity of view 0: its rotation at 0x88, its field of view at 0xa8 */
#define AIV_CAM_ROT(c) ((const vec3_t *)&((cCamControl *)(c))->cam.rot)
#define AIV_CAM_FOV(c) (((cCamControl *)(c))->cam.fov)

/* the vehicle too far from the camera: in view farther than MaxDistReposAI, out of view farther than
 * MaxDistReposNotVisAI or (unseen) farther than MinDistReposNotVisAI; the repos distances of the AI reset */
static byte cAIVehicle_NeedRepos(cAIVehicle *self) {
	static const vec3_t front = { 0.0f, 0.0f, 1.0f };
	vec3_t a, d, dir;
	cWorld *w = g_pWorld;
	vec3_t cam = w->camPos[w->curCam];
	cEntity *c = World_FindEntity(cWorld_GetCamName(g_pWorld, 0));
	const vec3_t *v = Entity_GetPos(self->vehicle, &a);

	d.y = v->y - cam.y;
	d.x = v->x - cam.x;
	d.z = v->z - cam.z;
	float dist = (float)__builtin_sqrt((double)(d.y * d.y + d.x * d.x + d.z * d.z));
	self->maxDistRepos = g_MaxDistReposAI;
	self->minDistRepos = g_MinDistReposAI;
	self->maxDistReposNotVis = g_MaxDistReposNotVisAI;
	self->minDistReposNotVis = g_MinDistReposNotVisAI;
	Vec3_Rotate(&dir, &front, AIV_CAM_ROT(c));
	double angle = Vec3_Angle(&d, &dir);
	if (!(angle > (double)(AIV_CAM_FOV(c) * 0.5f * 1.5f))) {
		if (!(dist > self->maxDistRepos)) {
			return 0;
		}
	}
	else if (!(dist > self->maxDistReposNotVis)) {
		if (!(dist > self->minDistReposNotVis) || AI_IsVisible(self)) {
			return 0;
		}
	}
	self->reposBehind = 0;
	return 1;
}

/* IN_ROUTE with Repos: the AI is put near the camera again; stopped if that fails */
static void cAIVehicle_SetInternState(cAIVehicle *self, int state, ...);
static void cAIVehicle_UpdateRoute(cAIVehicle *self) {
	if (!self->base.repos) {
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
		Entity_SetActive(self->vehicle, 0);
		cAIVehicle_SetInternState(self, AIVI_STOPPED);
	}
}

/* the think slot: reaction, items and for FIGHTING the way back from an item */
static void cAIVehicle_FightingAfterItem(cAIVehicle *self);
static void cAIVehicle_UpdateThink(cAIVehicle *self) {
	cSlotController *slot = &self->base.base.slotThink;
	float t;

	if (!cSlotController_Tick(slot, self->base.base.dt) || !cSlotController_FireReset(slot, &t)) {
		return;
	}
	do {
		cAIVehicle_React(self);
		if (!self->base.base.changed) {
			cAIVehicle_CheckItems(self);
			if (!self->base.base.changed && self->state == AIV_FIGHTING) {
				cAIVehicle_FightingAfterItem(self);
			}
		}
	} while (cSlotController_FireReset(slot, &t));
}

/* the target still there and active; else FREEZE (and OnObjectiveNullAI for a target that went) */
static byte cAIVehicle_CheckTarget(cAIVehicle *self) {
	cEntity *t = AIV_Target(self);

	if (!t) {
		cAIVehicle_SetStateIntern(self, AIV_FREEZE);
		if (cPy_Function_IsValid(g_OnObjectiveNullAI)) {
			cPy_Function_AddParamS(g_OnObjectiveNullAI, self->vehicle->name);
			cPy_Function_Execute(g_OnObjectiveNullAI, 0);
		}
		return 0;
	}
	if (!t->active) {
		cAIVehicle_SetStateIntern(self, AIV_FREEZE);
		return 0;
	}
	return 1;
}

static void cAIVehicle_Draw(cAI *elem) {
	cAIVehicle *self = (cAIVehicle *)elem;
	cAIMove_Draw(&self->base.base);
	cAIVehicleControl_Draw(self->control);
	cAIVehicleShoot_Draw(self->shoot);
}

cAIVehicleGroup *g_VehicleGroup;

static byte cAIVehicle_RestoreState(cAIVehicle *self, const cAIVehicleSaved *s);
static void cAIVehicle_Update(cAI *elem, float dt) {
	cAIVehicle *self = (cAIVehicle *)elem;
	if (self->pending.state != AIV_STATE_NONE && AIGroup_CanChange(g_VehicleGroup)) {
		cAIVehicle_RestoreState(self, &self->pending);
	}
	if (self->state == AIV_FREEZE) {
		return;
	}
	cAIMove_Update(&self->base.base, dt);
	cAIVehicle_UpdateGraph(self);
	if (self->base.base.changed) {
		return;
	}
	cAIVehicle_Think(self);
	if (self->base.base.changed) {
		return;
	}
	cAIVehicle_UpdateThink(self);
	if (self->base.base.changed) {
		return;
	}
	cAIVehicleControl_Update(self->control, dt);
	if (self->base.base.changed) {
		return;
	}
	cAIVehicleShoot_Update(self->shoot, dt);
	if (self->base.base.changed) {
		return;
	}
	cAIVehicleShoot_Update2(self->shoot, dt);
}

static void cAIVehicle_UpdateOff(cAI *elem, float dt) {
	cAIVehicle *self = (cAIVehicle *)elem;
	cAIMove_UpdateOff(&self->base.base, dt);
	cAIVehicle_UpdateGraph(self);
}

/* ---- the script variables: "Repos" here, the rest in cAIMove ---- */

static byte cAIVehicle_VarF(cAI *elem, const char *name, double *d, byte set) {
	cAIVehicle *self = (cAIVehicle *)elem;
	if (!*name) {
		return 0;
	}
	return cAIMove_VarF(&self->base.base, name, d, set);
}

static byte cAIVehicle_VarI(cAI *elem, const char *name, int *i, byte set) {
	cAIVehicle *self = (cAIVehicle *)elem;
	if (!*name) {
		return 0;
	}
	if (!strcmp(name, "Repos")) {
		if (set) {
			self->base.repos = *i != 0;
			return 1;
		}
		*i = self->base.repos;
		return 1;
	}
	return cAIMove_VarI(&self->base.base, name, i, set);
}

static byte cAIVehicle_VarS(cAI *elem, const char *name, char **s, byte set) {
	cAIVehicle *self = (cAIVehicle *)elem;
	if (!*name) {
		return 0;
	}
	return cAIMove_VarS(&self->base.base, name, s, set);
}

static byte cAIVehicle_VarV(cAI *elem, const char *name, double *x, double *y, double *z, byte set) {
	cAIVehicle *self = (cAIVehicle *)elem;
	if (!*name) {
		return 0;
	}
	return cAIMove_VarV(&self->base.base, name, x, y, z, set);
}

/* ---- the getters ---- */

cAIVehicleControl *cAIVehicle_GetControl(cAIVehicle *self) {
	return self->control;
}

void cAIVehicle_GetFadeDists(void *ai, float *nearDist, float *farDist) {
	cAIVehicle *self = ai;

	*nearDist = self->minDistRepos;
	*farDist = self->maxDistRepos;
}

void *cAIVehicle_GetVehicle(cAIVehicle *ai) {
	return ai->vehicle;
}

byte cAIVehicle_GetChanged(cAIVehicle *self) {
	return self->base.base.changed;
}

/* the external state as the scripts see it: GET_AWAY, and LOST_PATH out of GET_AWAY, count as FIGHTING */
int cAIVehicle_GetState(cAIVehicle *ai) {
	int s = ai->state;

	if (s == AIV_LOST_PATH) {
		return ai->saved.state != AIV_GET_AWAY ? AIV_LOST_PATH : AIV_FIGHTING;
	}
	return s != AIV_GET_AWAY ? s : AIV_FIGHTING;
}

int cAIVehicle_GetInternState(cAIVehicle *ai) {
	return ai->internState;
}

long double cAIVehicle_GetPathWidth(void *ai) {
	cAIVehicle *self = ai;

	if ((self->state == AIV_TRAFFIC || self->state == AIV_TRAFFIC_INDOOR) && self->internState != AIVI_STOPPED &&
			self->track >= 0) {
		CNodesTraffic *t = AIV_Outdoor();
		return Traffic_GetRadius(t, self->track);
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
static void cAIVehicle_InternStateName(int state, char *buf) {
	if ((uint32_t)state < sizeof(s_InternStateNames) / sizeof(s_InternStateNames[0])) {
		memcpy(buf, s_InternStateNames[state], strlen(s_InternStateNames[state]) + 1);
	}
}

void cAIVehicle_GetInternStateName(cAIVehicle *self, char *buf) {
	cAIVehicle_InternStateName(self->internState, buf);
}

static void cAIVehicle_StateName(int state, char *buf) {
	if ((uint32_t)state < sizeof(s_StateNames) / sizeof(s_StateNames[0])) {
		memcpy(buf, s_StateNames[state], strlen(s_StateNames[state]) + 1);
	}
}

void cAIVehicle_GetStateName(cAIVehicle *self, char *buf) {
	cAIVehicle_StateName(self->state, buf);
}

/* ---- the parameter blocks (the ones of the move and the shoot control are kept there) ---- */

cAIVehicleParams1 *cAIVehicle_GetParams1(cAIVehicle *ai) {
	return &ai->params;
}

cAIVehicleParams1 *cAIVehicle_GetParamsInit(cAIVehicle *self) {
	return &self->paramsInit;
}

cAIVehicleParams2 *cAIVehicle_GetParams2(cAIVehicle *ai) {
	return cAIVehicleControl_GetParams(ai->control);
}

cAIVehicleParams2 *cAIVehicle_GetParams2Init(cAIVehicle *self) {
	return cAIVehicleControl_GetParamsInit(self->control);
}

cAIVehicleParams3 *cAIVehicle_GetParams3(cAIVehicle *ai) {
	return cAIVehicleShoot_GetParams(ai->shoot);
}

cAIVehicleParams3 *cAIVehicle_GetParams3Init(cAIVehicle *self) {
	return cAIVehicleShoot_GetParamsInit(self->shoot);
}

void cAIVehicle_Setup(cAIVehicle *ai, cAIVehicleParams1 *p1, cAIVehicleParams2 *p2,
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

static void cAIVehicle_CheckParams1(cAIVehicle *self, const cAIVehicleParams1 *p) {
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

static void cAIVehicle_CheckParams2(cAIVehicle *self, const cAIVehicleParams2 *p) {
	AIV_PARAM(self, p->aim < 1 || p->aim > 10, (p->aim >= 1) && (p->aim <= 10));
	AIV_PARAM(self, p->brake < 0 || p->brake > 1, (p->brake >= 0) && (p->brake <= 1));
	AIV_PARAM(self, p->strafe != 2, p->strafe == 2);
	AIV_PARAM(self, p->maxTimeStrafe < 0 || p->maxTimeStrafe > 10,
		(p->maxTimeStrafe >= 0) && (p->maxTimeStrafe <= 10));
	AIV_PARAM(self, !(p->maxVelStrafe >= 0.0f) || !(p->maxVelStrafe <= 1.0f),
		(p->maxVelStrafe >= 0.0f) && (p->maxVelStrafe <= 1.0f));
	AIV_PARAM(self, p->stupidPathfinding != 0, p->stupidPathfinding == 0);
}

static void cAIVehicle_CheckParams3(cAIVehicle *self, const cAIVehicleParams3 *p) {
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

byte cAIVehicle_ControlFlag(cAIVehicle *self) {
	return cAIVehicleControl_GetFlag(self->control);
}

byte cAIVehicle_InSight(cAIVehicle *self) {
	return cAIVehicleControl_InSight(self->control);
}

/* d from the vehicle to the point within HookRange, farther than r and at most 45 degrees off the nose */
static const vec3_t s_Nose = { 0.0f, 0.0f, -1.0f };

/* the direction of the nose of the vehicle */
static void AIV_NoseDir(cAIVehicle *self, vec3_t *dir) {
	vec3_t rot;

	Obj_GetAng(self->vehicle, &rot);
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

/* e within HookRange, not touching and at most 45 degrees off the nose; CanHook also in sight */
static byte cAIVehicle_InHookRange(cAIVehicle *self, cEntity *e) {
	vec3_t a, b, d;
	const vec3_t *v = Entity_GetPos(self->vehicle, &a);
	const vec3_t *p = Entity_GetPos(e, &b);

	d.x = p->x - v->x;
	d.y = p->y - v->y;
	d.z = p->z - v->z;
	return AIV_HookAngle(self, &d, d.y * d.y + d.x * d.x + d.z * d.z, Veh(self->vehicle)->wl.prim.radius + Veh(e)->wl.prim.radius);
}

static byte cAIVehicle_CanHook(cAIVehicle *self, cEntity *e) {
	vec3_t a, b;

	if (!cAIVehicle_InHookRange(self, e)) {
		return 0;
	}
	cEntity *v = self->vehicle;
	const vec3_t *to = Entity_GetPos(e, &a);
	const vec3_t *from = Entity_GetPos(v, &b);
	return Engine_TestLine(from, to, NULL, NULL) == 0;
}

static byte cAIVehicle_InHookRangePos(cAIVehicle *self, const vec3_t *p) {
	vec3_t a, d;
	const vec3_t *v = Entity_GetPos(self->vehicle, &a);

	d.x = p->x - v->x;
	d.y = p->y - v->y;
	d.z = p->z - v->z;
	return AIV_HookAngle(self, &d, d.y * d.y + d.x * d.x + d.z * d.z, Veh(self->vehicle)->wl.prim.radius);
}

/* the vehicle is steered (thrust or turn) in a moving state */
static byte cAIVehicle_IsMoving(cAI *base) {
	cAIVehicle *self = (cAIVehicle *)base;
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
	return v->controls.acc != 0.0f || v->controls.str != 0.0f || v->controls.vStr != 0.0f;
}

/* ---- the debug text: "[time] (vehicle) [state - internal state] - text" on the console ---- */

static void cAIVehicle_Debugf(cAI *base, const char *fmt, ...) {
	cAIVehicle *self = (cAIVehicle *)base;
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

/* the handler of the state, else the default one; the result of the one that ran (callers test al) */
static byte cAIVehicle_EventChase(cAIVehicle *self, int ev);
static byte cAIVehicle_EventDefault(cAIVehicle *self, int ev);
static byte cAIVehicle_EventFighting(cAIVehicle *self, int ev);
static byte cAIVehicle_EventGetAway(cAIVehicle *self, int ev);
static byte cAIVehicle_EventGetAwayStop(cAIVehicle *self, int ev);
static byte cAIVehicle_EventGoTo(cAIVehicle *self, int ev);
static byte cAIVehicle_EventInRoute(cAIVehicle *self, int ev);
static byte cAIVehicle_EventLostPath(cAIVehicle *self, int ev);
static byte cAIVehicle_EventTraffic(cAIVehicle *self, int ev);
static byte cAIVehicle_EventTrafficInDoor(cAIVehicle *self, int ev);
static byte cAIVehicle_Event(cAI *base, int ev) {
	cAIVehicle *self = (cAIVehicle *)base;
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
		done = cAIVehicle_EventDefault(self, ev);
	}
	return done;
}

static byte cAIVehicle_EventChase(cAIVehicle *self, int ev) {
	if (ev != AIEV_OBJECTIVE_REACHED) {
		return 0;
	}
	if (self->internState != AIVI_FINISHING_LINE_LIFE) {
		AI_OnEvent(self, AIEV_OBJECTIVE_REACHED);
		cAIVehicle_SetInternState(self, AIVI_CHASE_NOT_SHOOTING, (double)self->chaseMin, (double)self->chaseMax);
		return 1;
	}
	AI_OnEvent(self, AIEV_OBJECTIVE_REACHED);
	cAIVehicle_ItemsChase(self);
	return 1;
}

/* the events every state handles: 0 the path is lost, 1 the target is not reachable, 3 the target went */
static void cAIVehicle_SaveState(cAIVehicle *self, cAIVehicleSaved *s);
static byte cAIVehicle_EventDefault(cAIVehicle *self, int ev) {
	switch (ev) {
	case AIEV_LOST_AGENT: {
		vec3_t a;
		AI_OnEvent(self, AIEV_LOST_AGENT);
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
	case AIEV_OBJECTIVE_NOT_FOUND:
		AI_OnEvent(self, AIEV_OBJECTIVE_NOT_FOUND);
		cAIVehicle_SetStateIntern(self, AIV_FREEZE_OBJ_NOT_ACCESS);
		return 1;
	case AIEV_OBJECTIVE_DIED:
		AI_OnEvent(self, AIEV_OBJECTIVE_DIED);
		cAIVehicle_SetStateIntern(self, AIV_FREEZE);
		if (cPy_Function_IsValid(g_OnObjectiveNullAI)) {
			cPy_Function_AddParamS(g_OnObjectiveNullAI, self->vehicle->name);
			cPy_Function_Execute(g_OnObjectiveNullAI, 0);
		}
		return 1;
	}
	return 0;
}

static byte cAIVehicle_EventFighting(cAIVehicle *self, int ev) {
	if (ev != AIEV_OBJECTIVE_REACHED) {
		return 0;
	}
	switch (self->internState) {
	case AIVI_FINISHING_LINE_LIFE:
		AI_OnEvent(self, AIEV_OBJECTIVE_REACHED);
		cAIVehicle_ItemsFighting(self);
		return 1;
	case AIVI_FINISHING_LINE_AMMO:
		AI_OnEvent(self, AIEV_OBJECTIVE_REACHED);
		cAIVehicle_FightingAfterItem(self);
		return 1;
	}
	AI_OnEvent(self, AIEV_OBJECTIVE_REACHED);
	cAIVehicle_SetInternState(self, AIVI_CHASE_SHOOTING, (double)self->chaseMin, (double)self->chaseMax);
	return 1;
}

/* the internal state FOLLOW_THE_PATH_WITH_SHOOT_CM from the target to the vehicle */
static void AIV_AwayFromTarget(cAIVehicle *self) {
	vec3_t a, b;

	cEntity *t = AIV_Target(self);
	const vec3_t *tp = Entity_GetPos(t, &a);
	const vec3_t *vp = Entity_GetPos(self->vehicle, &b);
	vec3_t d;
	d.z = vp->z - tp->z;
	d.x = vp->x - tp->x;
	d.y = vp->y - tp->y;
	cAIVehicle_SetInternState(self, AIVI_FOLLOW_THE_PATH_WITH_SHOOT_CM, d, (double)g_RadiusObjectiveInRoute,
		self->params.boostGetAway, 0);
}

static void AIV_FollowAway(cAIVehicle *self, int ev) {
	AI_OnEvent(self, ev);
	AIV_AwayFromTarget(self);
}

static byte cAIVehicle_EventGetAway(cAIVehicle *self, int ev) {
	if (self->internState == AIVI_FINISHING_LINE_LIFE) {
		if (ev == AIEV_OBJECTIVE_REACHED) {
			AI_OnEvent(self, AIEV_OBJECTIVE_REACHED);
			cAIVehicle_ItemsGetAway(self);
			return 1;
		}
	}
	else if (self->internState == AIVI_FOLLOW_THE_PATH_WITH_SHOOT_CM && (ev == AIEV_OBJECTIVE_REACHED || ev == AIEV_OBJECTIVE_NEAR)) {
		AIV_FollowAway(self, ev);
		return 1;
	}
	return 0;
}

static byte cAIVehicle_EventGetAwayStop(cAIVehicle *self, int ev) {
	if (self->internState == AIVI_FINISHING_LINE_LIFE) {
		if (ev == AIEV_OBJECTIVE_REACHED) {
			AI_OnEvent(self, AIEV_OBJECTIVE_REACHED);
			cAIVehicle_ItemsGetAwayStop(self);
			return 1;
		}
	}
	else if (self->internState == AIVI_FOLLOW_THE_PATH_WITH_SHOOT_CM && (ev == AIEV_OBJECTIVE_REACHED || ev == AIEV_OBJECTIVE_NEAR)) {
		AIV_FollowAway(self, ev);
		return 1;
	}
	return 0;
}

/* GO_TO arrived: OnItemTakenAI and the callback of the AI; FREEZE when neither took it and the route goes on */
static byte cAIVehicle_EventGoTo(cAIVehicle *self, int ev) {
	if (ev != AIEV_OBJECTIVE_REACHED) {
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
	if (!taken && !self->stopAtGoal) {
		AI_OnEvent(self, AIEV_OBJECTIVE_REACHED);
		cAIVehicle_SetStateIntern(self, AIV_FREEZE);
	}
	return 1;
}

static byte cAIVehicle_EventInRoute(cAIVehicle *self, int ev) {
	if (self->internState == AIVI_FINISHING_LINE_LIFE) {
		if (ev == AIEV_OBJECTIVE_REACHED) {
			AI_OnEvent(self, AIEV_OBJECTIVE_REACHED);
			cAIVehicle_ItemsInRoute(self);
			return 1;
		}
	}
	else if (self->internState == AIVI_FOLLOW_THE_PATH && (ev == AIEV_OBJECTIVE_REACHED || ev == AIEV_OBJECTIVE_NEAR)) {
		vec3_t dir;
		AI_OnEvent(self, ev);
		AIV_NoseDir(self, &dir);
		cAIVehicle_SetInternState(self, AIVI_FOLLOW_THE_PATH, dir, (double)g_RadiusObjectiveInRoute, 0, 1,
			self->base.repos, (double)self->rotA, (double)self->rotB);
		return 1;
	}
	return 0;
}

static byte cAIVehicle_EventLostPath(cAIVehicle *self, int ev) {
	if (ev != AIEV_DONT_GO) {
		return 0;
	}
	AI_OnEvent(self, AIEV_DONT_GO);
	cAIVehicle_SetStateIntern(self, AIV_FREEZE_LOST);
	return 1;
}

/* the vehicle on to the next node p of the track (FINISHING_LINE_DIRECT), the lane of the vehicle from its last
 * point to it */
static vec3_t *cAIVehicle_TrackPoint(cAIVehicle *self, vec3_t *out, const vec3_t *p, int track, int node);
static void AIV_NextNode(cAIVehicle *self, vec3_t *p, int track, int node, float radius) {
	vec3_t a;

	AI_OnEvent(self, AIEV_OBJECTIVE_REACHED);
	*p = *cAIVehicle_TrackPoint(self, &a, p, track, node);
	self->track = track;
	self->node = node;
	cVehicleTraffic *vt = AIV_VehTraffic(self->vehicle);
	vt->begin = self->base.dest;
	AIV_VehTraffic(self->vehicle)->end = *p;
	cAIVehicle_SetInternState(self, AIVI_FINISHING_LINE_DIRECT, *p, (double)radius, 0, (double)self->rotA,
		(double)self->rotB);
}

/* arrived at a node: on to the next one, at the end of the track off the track and stopped */
static byte cAIVehicle_EventTraffic(cAIVehicle *self, int ev) {
	vec3_t p;
	int track, node;

	if (ev != AIEV_OBJECTIVE_REACHED) {
		return 0;
	}
	CNodesTraffic *t = AIV_Outdoor();
	if (Traffic_NextAway(t, &p, self->track, self->node, &track, &node)) {
		AIV_NextNode(self, &p, track, node, g_RadiusObjectiveTraffic);
		return 1;
	}
	AI_OnEvent(self, AIEV_OBJECTIVE_REACHED);
	t = AIV_Outdoor();
	Traffic_RemoveVehicle(t, self->track);
	cAIVehicle_SetInternState(self, AIVI_STOPPED);
	Entity_SetActive(self->vehicle, 0);
	return 1;
}

static byte cAIVehicle_EventTrafficInDoor(cAIVehicle *self, int ev) {
	vec3_t p, a;
	int track, node;

	if (ev != AIEV_OBJECTIVE_REACHED) {
		return 0;
	}
	CNodesTraffic *t = AIV_Indoor();
	if (Traffic_NextAway(t, &p, self->track, self->node, &track, &node)) {
		AIV_NextNode(self, &p, track, node, g_RadiusObjectiveTrafficInDoor);
		return 1;
	}
	/* the end of the track: off once the vehicle is near the last point of the control (seen from above) */
	cAIVehicleControl_GetGoal(self->control, &p);
	p.y = 0.0f;
	Entity_GetPos(self->vehicle, &a);
	a.y = 0.0f;
	float dx = p.x - a.x;
	float dy = p.y - 0.0f;
	float dz = p.z - a.z;
	if (!(dy * dy + dx * dx + dz * dz > AIV_ARRIVED_DIST2)) {
		AI_OnEvent(self, AIEV_OBJECTIVE_REACHED);
		t = AIV_Indoor();
		Traffic_RemoveVehicle(t, self->track);
		cAIVehicle_SetInternState(self, AIVI_STOPPED);
		Entity_SetActive(self->vehicle, 0);
	}
	return 1;
}

/* ---- the traffic track ---- */

static void AIV_SetRotSpeed(cAIVehicle *self, float speed) {
	float a = speed + speed;
	self->rotB = speed * AIV_ROT_SPEED_F / AIV_ROT_SPEED_B;
	self->rotA = a / AIV_ROT_SPEED_A;
}

/* the vehicle on its way to np, the node of its track (FINISHING_LINE_DIRECT): the speed of the track for the kinds of
 * track the vehicle keeps to, the turn speeds, the lane from the vehicle to np, one more vehicle on the track */
static void AIV_StartTrack(cAIVehicle *self, byte indoor, const vec3_t *np) {
	cTrafficTrackInfo info;
	vec3_t a;
	float speed;
	int type = self->trackType;

	if (type == 1 || type == 0 || type == 2 ||
			((type == 4 || type == 3) && AIV_VehTraffic(self->vehicle)->type != 0)) {
		CNodesTraffic *t = indoor ? AIV_Indoor() : AIV_Outdoor();
		Traffic_GetInfo(t, &info, self->track);
		AIV_VehTraffic(self->vehicle)->speed = info.speed;
		speed = info.speed;
	}
	else {
		speed = g_VelTrafficLightRotAI;
	}
	AIV_SetRotSpeed(self, speed);
	const vec3_t *p = Entity_GetPos(self->vehicle, &a);
	AIV_VehTraffic(self->vehicle)->begin = *p;
	AIV_VehTraffic(self->vehicle)->end = *np;
	CNodesTraffic *t = indoor ? AIV_Indoor() : AIV_Outdoor();
	Traffic_AddVehicle(t, self->track);
	cAIVehicle_SetInternState(self, AIVI_FINISHING_LINE_DIRECT, *np,
		(double)(indoor ? g_RadiusObjectiveTrafficInDoor : g_RadiusObjectiveTraffic), 0, (double)self->rotA,
		(double)self->rotB);
}

/* the vehicle put on the track at pos, turned to the node, on its way to the node with FINISHING_LINE_DIRECT;
 * 0 if an entity is in the way */
static vec3_t *cAIVehicle_TrackPointRandom(cAIVehicle *self, vec3_t *out, const vec3_t *p, int track, int node);
byte cAIVehicle_PlaceOnTrack(cAIVehicle *ai, int type, const vec3_t *pos, int track, int node) {
	cAIVehicle *self = ai;
	cEntity *v = self->vehicle;
	cEntity *hit;
	vec3_t np, d, ang, tmp;

	self->trackType = type;
	if (cWorld_TestSphereEntities(g_pWorld, v, 1, pos, Veh(v)->wl.prim.radius * 3.0f, &hit, 1, 0)) {
		return 0;
	}
	cAIVehicleControl_ClearDest(self->control);
	self->track = track;
	self->node = node;
	Entity_SetActive(self->vehicle, 0);
	Entity_SetActive(self->vehicle, 1);
	Entity_SetPos(self->vehicle, pos);
	CNodesTraffic *t = AIV_Indoor();
	Traffic_GetNode(t, &np, self->track, self->node);
	d.x = pos->x - np.x;
	d.y = pos->y - np.y;
	d.z = pos->z - np.z;
	Vec3_GetRotAng(&ang, &d);
	Obj_SetAng(self->vehicle, &ang);
	cAIVehicleControl_Stop(self->control);
	tmp.x = 0.0f;
	tmp.y = 0.0f;
	tmp.z = 0.0f;
	Obj_SetVel(self->vehicle, &tmp);
	np = *cAIVehicle_TrackPointRandom(self, &tmp, &np, self->track, self->node);
	AIV_StartTrack(self, 1, &np);
	self->base.base.seen = 2;
	return 1;
}

/* the state kept for after an event */
static void cAIVehicle_SaveState(cAIVehicle *self, cAIVehicleSaved *s) {
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
	s->boost = self->boost;
	s->dir = self->base.dir;
	s->dest = self->base.dest;
	s->dist = self->base.dist;
	s->accel = self->accel;
	s->repos = self->base.repos;
	s->awayDist = self->awayDist;
	s->state = self->state;
	s->internState = self->internState;
	s->changing = self->base.base.changing;
	s->chaseMin = self->chaseMin;
	s->chaseMax = self->chaseMax;
	s->stopAtGoal = self->stopAtGoal;
	s->rotA = self->rotA;
	s->rotB = self->rotB;
}

/* the point where the segment a b crosses the plane: 1, out set; 0 if both ends are on one side */
static int SegmentPlane_Hit(vec3_t *out, const vec3_t *a, const vec3_t *b, const plane_t *pl) {
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

#define AIV_TRACK_DIR_MIN 0.1f		/* rva 0x3aab08, compared as double */
#define AIV_TRACK_RAY 600000.0f

/* a component near 0 for the track directions (fabs as double against 0.1) */
static inline int AIV_TrackSmall(float v) {
	return (double)AIV_TRACK_DIR_MIN > __builtin_fabs((double)(v - 0.0f));
}

static inline int AIV_TrackSmall3(const vec3_t *v) {
	return AIV_TrackSmall(v->x) && AIV_TrackSmall(v->y) && AIV_TrackSmall(v->z);
}

/* the width left for the vehicle on the track: the track radius less the vehicle radius */
/* v / |v| with the factor 1 / len in float, v unchanged for the length 0 (inlined in AIVehicle.cpp) */
static void AIV_Unit(vec3_t *out, const vec3_t *v) {
	float len = (float)__builtin_sqrt((double)(v->x * v->x + v->y * v->y + v->z * v->z));

	if (len == 0.0f) {
		*out = *v;
		return;
	}
	float k = 1.0f / len;
	out->x = v->x * k;
	out->y = v->y * k;
	out->z = v->z * k;
}

#define AIV_HOOK_AHEAD 2.0f

/* a line from the vehicle to a point at HookRange in turn ahead-right, ahead-up and ahead-left (hookSide counts); 1 if
 * it hits the scene, the hit point in p */
static byte cAIVehicle_TestHookPoint(cAIVehicle *self, vec3_t *p) {
	vec3_t rot, f, d, c, n, e, h, u, o, tmp;
	static const vec3_t back = { 0.0f, 0.0f, -1.0f };

	Obj_GetAng(self->vehicle, &rot);
	vec3_t z = back;
	Vec3_Rotate(&f, &z, &rot);
	AIV_Unit(&d, &f);
	c.x = d.y * 0.0f - d.z;
	c.y = d.z * 0.0f - d.x * 0.0f;
	c.z = d.x - d.y * 0.0f;
	if (c.x == 0.0f && c.y == 0.0f && c.z == 0.0f) {
		c.x = 1.0f;
		c.y = 0.0f;
		c.z = 0.0f;
	}
	AIV_Unit(&n, &c);
	int mode = self->hookSide;
	if (mode == 0) {
		e.x = n.z * d.y - n.y * d.z;
		e.y = n.x * d.z - n.z * d.x;
		e.z = n.y * d.x - n.x * d.y;
		AIV_Unit(&n, &e);
		h.x = n.x + d.x * AIV_HOOK_AHEAD;
		h.y = n.y + d.y * AIV_HOOK_AHEAD;
		h.z = n.z + d.z * AIV_HOOK_AHEAD;
	} else if (mode == 1) {
		h.x = n.x + d.x * AIV_HOOK_AHEAD;
		h.y = n.y + d.y * AIV_HOOK_AHEAD;
		h.z = n.z + d.z * AIV_HOOK_AHEAD;
	} else {
		h.x = d.x * AIV_HOOK_AHEAD - n.x;
		h.y = d.y * AIV_HOOK_AHEAD - n.y;
		h.z = d.z * AIV_HOOK_AHEAD - n.z;
	}
	self->hookSide = (mode + 1) % 3;
	AIV_Unit(&u, &h);
	o.x = u.x * g_HookRange;
	o.z = u.z * g_HookRange;
	o.y = u.y * g_HookRange;
	const vec3_t *pos = Entity_GetPos(self->vehicle, &tmp);
	vec3_t to;
	to.y = pos->y + o.y;
	to.z = pos->z + o.z;
	to.x = pos->x + o.x;
	pos = Entity_GetPos(self->vehicle, &tmp);
	return Engine_TestLine(pos, &to, p, NULL) != 0;
}

/* the point of the track node for the vehicle: offset by trackOfs on the tracks 0..2; on 3 and 4 a light vehicle
 * goes from its last point along the node direction to the plane through p across the track */
static vec3_t *cAIVehicle_TrackPoint(cAIVehicle *self, vec3_t *out, const vec3_t *p, int track, int node) {
	CNodesTraffic *t = g_IsMapOutDoor ? AIV_Outdoor() : AIV_Indoor();

	switch (self->trackType) {
	case 0:
	case 1:
	case 2:
		out->x = self->trackOfs.x + p->x;
		out->y = self->trackOfs.y + p->y;
		out->z = self->trackOfs.z + p->z;
		return out;
	case 3:
	case 4:
		if (AIV_VehTraffic(self->vehicle)->type != 0) {
			*out = *p;
			return out;
		}
		vec3_t a, b, d, n;
		Traffic_GetDirs(t, &a, &b, track, node);
		d.x = a.x - b.x;
		d.y = a.y - b.y;
		d.z = a.z - b.z;
		Vec3_NormalizeTo(&n, &d);
		if (AIV_TrackSmall3(&n)) {
			Con_Bluef("ERROR TRACK: Track: %d Node: %d\n", track, node);
			CheckFailed("TRACK_ERRONEO", __FILE__, __TIMESTAMP__, __LINE__);
		}
		plane_t pl;
		pl.n = n;
		pl.d = -(p->y * n.y + p->x * n.x + p->z * n.z);
		vec3_t from = self->trackPos;
		vec3_t to;
		to.x = self->trackPos.x + (-a.x) * AIV_TRACK_RAY;
		to.y = self->trackPos.y + (-a.y) * AIV_TRACK_RAY;
		to.z = self->trackPos.z + (-a.z) * AIV_TRACK_RAY;
		if (!SegmentPlane_Hit(out, &from, &to, &pl)) {
			Con_Bluef("ERROR TRACK: Track: %d Node: %d\n", track, node);
			CheckFailed("PROBLEMA_TRACK", __FILE__, __TIMESTAMP__, __LINE__);
		}
		self->trackPos = *out;
		return out;
	}
	return out;
}

static float AIV_TrackRoom(cAIVehicle *self, CNodesTraffic *t, int track) {
	float rt = Traffic_GetRadius(t, track);
	return rt - Obj_GetRadius(self->vehicle);
}

/* rand() in -w..w as the original computes it */
static inline float AIV_TrackRand(double range, double base) {
	return (float)((double)(crt_rand() & 0x7fff) * RAND_SCALE * range + base);
}

/* A x (0, 1, 0) with the products by 0 of the original, (1, 0, 0) when that is near 0 too */
static void AIV_TrackSide(vec3_t *c, const vec3_t *a) {
	c->x = a->y * 0.0f - a->z;
	c->y = a->z * 0.0f - a->x * 0.0f;
	c->z = a->x - a->y * 0.0f;
	if (AIV_TrackSmall3(c)) {
		c->x = 1.0f;
		c->z = 0.0f;
		c->y = 0.0f;
	}
}

/* ---- the repositioning and the track points ---- */
static vec3_t *cAIVehicle_TrackPointRandom(cAIVehicle *self, vec3_t *out, const vec3_t *p, int track, int node) {
	CNodesTraffic *t = g_IsMapOutDoor ? AIV_Outdoor() : AIV_Indoor();
	float w;
	double range, base;

	switch (self->trackType) {
	case 0:
	case 1:
	case 2:
		w = AIV_TrackRoom(self, t, track);
		range = (double)(w - -w);
		base = (double)-w;
		self->trackOfs.x = AIV_TrackRand(range, base);
		self->trackOfs.y = AIV_TrackRand(range, base);
		self->trackOfs.z = AIV_TrackRand(range, base);
		out->x = p->x + self->trackOfs.x;
		out->y = self->trackOfs.y + p->y;
		out->z = self->trackOfs.z + p->z;
		return out;
	case 3:
	case 4:
		break;
	default:
		return out;
	}
	if (AIV_VehTraffic(self->vehicle)->type != 0) {
		self->trackOfs.x = 0.0f;
		self->trackOfs.y = 0.0f;
		self->trackOfs.z = 0.0f;
		*out = *p;
		return out;
	}
	vec3_t a, b, c, n, m, u, q;
	Traffic_GetDirs(t, &a, &b, track, node);
	if (AIV_TrackSmall3(&b)) {
		AIV_TrackSide(&c, &a);
	} else {
		c.x = a.x + b.x;
		c.y = a.y + b.y;
		c.z = a.z + b.z;
		if (AIV_TrackSmall3(&c)) {
			AIV_TrackSide(&c, &a);
		}
	}
	n = *Vec3_NormalizeTo(&q, &c);
	w = AIV_TrackRoom(self, t, track);
	range = (double)(w - -w);
	base = (double)-w;
	float r = AIV_TrackRand(range, base);
	u.x = n.x * r;
	u.y = n.y * r;
	u.z = n.z * r;
	c.x = n.z * a.y - n.y * a.z;
	c.y = n.x * a.z - n.z * a.x;
	c.z = n.y * a.x - n.x * a.y;
	m = *Vec3_NormalizeTo(&q, &c);
	r = AIV_TrackRand(range, base);
	self->trackOfs.x = m.x * r + u.x;
	self->trackOfs.y = m.y * r + u.y;
	self->trackOfs.z = m.z * r + u.z;
	n = *Vec3_NormalizeTo(&q, &self->trackOfs);
	self->trackOfs.x = n.x * w;
	self->trackOfs.y = n.y * w;
	self->trackOfs.z = n.z * w;
	Traffic_GetNode(t, &q, track, node);
	self->trackPos.x = q.x + self->trackOfs.x;
	self->trackPos.y = self->trackOfs.y + q.y;
	self->trackPos.z = self->trackOfs.z + q.z;
	*out = self->trackPos;
	return out;
}

static cGVar GVar_VelReposBackAI;
#define g_VelReposBackAI (GVar_VelReposBackAI.value.f)	/* VelReposBackAI */
#define AIV_REPOS_HALF 0.5f
#define AIV_REPOS_FOV 1.5f
#define AIV_REPOS_SPHERE 3.0f
#define AIV_REPOS_SPHERE_W 5.0f

/* a float in an int argument slot that the callee does not use: its bits, as the original pushes them */
static inline int AIV_FloatArg(float f) {
	int i;
	memcpy(&i, &f, sizeof(i));
	return i;
}

/* the camera of view 0, its position and its direction (z axis turned by its angles) */
static cCamControl *AIV_ReposCam(vec3_t *camPos, vec3_t *dir) {
	*camPos = g_pWorld->camPos[g_pWorld->curCam];
	cCamControl *cam = (cCamControl *)World_FindEntity(cWorld_GetCamName(g_pWorld, 0));
	vec3_t z = { 0.0f, 0.0f, 1.0f };
	Vec3_Rotate(dir, &z, &cam->cam.rot);
	return cam;
}

/* the ship the camera follows is slower than VelReposBackAI */
static int AIV_ReposSlowShip(cCamControl *cam) {
	vec3_t tmp;

	if (!Entity_IsType(cam, FCAM_CLASSNAME)) {
		return 0;
	}
	cEntity *ship = cCamEntity_GetTarget(&cam->base);
	if (!ship) {
		return 0;
	}
	const vec3_t *v = Obj_GetVel(ship, &tmp);
	float speed = (float)__builtin_sqrt((double)(v->x * v->x + v->y * v->y + v->z * v->z));
	return g_VelReposBackAI > speed;
}

/* the old destination dropped, behind kept for the next repos */
static void AIV_ReposStart(cAIVehicle *self, byte behind) {
	cAIVehicleControl_ClearDest(self->control);
	self->reposBehind = behind != 0;
	cAIVehicleGroup_ResetRepos(g_VehicleGroup);
}

/* the vehicle shown again at p with a fade in */
static void AIV_ReposShow(cAIVehicle *self, const vec3_t *p) {
	Entity_SetActive(self->vehicle, 0);
	Entity_SetActive(self->vehicle, 1);
	Entity_SetPos(self->vehicle, p);
	cVehicleEntity_StartFade(Veh(self->vehicle));
}

/* turned along d and stopped */
static void AIV_ReposStop(cAIVehicle *self, const vec3_t *d) {
	vec3_t ang;

	Vec3_GetRotAng(&ang, d);
	Obj_SetAng(self->vehicle, &ang);
	cAIVehicleControl_Stop(self->control);
	vec3_t zero = { 0.0f, 0.0f, 0.0f };
	Obj_SetVel(self->vehicle, &zero);
}

/* a traffic node near the camera of the view: in the cone of view (dir) between minVis and maxVis, else behind it
 * (-dir) between minNotVis and maxNotVis; the camera that follows a slow ship goes behind first one time in two.
 * The vehicle is put there, turned to the node and stopped; 0 if no node is free */
static void cAIVehicle_PickTrackType(cAIVehicle *self);
static byte cAIVehicle_ReposTraffic(cAIVehicle *self, vec3_t *out, float minVis, float maxVis, float minNotVis,
	float maxNotVis, float around) {
	vec3_t camPos, dir, back, p;
	cCamControl *cam = AIV_ReposCam(&camPos, &dir);
	int track = self->track;
	int node = self->node;
	byte vis = 1;
	byte behind = 0;
	byte r;

	if (AIV_ReposSlowShip(cam) && !self->reposBehind) {
		vis = crt_rand() % 2 == 0;
	}
	cAIVehicle_PickTrackType(self);
	CNodesTraffic *t = AIV_Outdoor();
	float cone = cam->cam.fov * AIV_REPOS_HALF * AIV_REPOS_FOV;
	if (vis) {
		r = Traffic_RandomNode(t, &p, self->trackType, &camPos, minVis, maxVis, AIV_FloatArg(around), &dir, cone,
			&track, &node);
		if (!r) {
			if (self->reposBehind) {
				return r;
			}
			back.x = -dir.x;
			back.y = -dir.y;
			back.z = -dir.z;
			t = AIV_Outdoor();
			r = Traffic_RandomNode(t, &p, self->trackType, &camPos, minNotVis, maxNotVis, AIV_FloatArg(around),
				&back, M_PI_F - cam->cam.fov * AIV_REPOS_HALF * AIV_REPOS_FOV, &track, &node);
			if (!r) {
				return r;
			}
			behind = 1;
		}
	} else {
		back.x = -dir.x;
		back.y = -dir.y;
		back.z = -dir.z;
		r = Traffic_RandomNode(t, &p, self->trackType, &camPos, minNotVis, maxNotVis, AIV_FloatArg(around), &back,
			M_PI_F - cam->cam.fov * AIV_REPOS_HALF * AIV_REPOS_FOV, &track, &node);
		if (r) {
			behind = 1;
		} else {
			t = AIV_Outdoor();
			r = Traffic_RandomNode(t, &p, self->trackType, &camPos, minVis, maxVis, AIV_FloatArg(around), &dir,
				cam->cam.fov * AIV_REPOS_HALF * AIV_REPOS_FOV, &track, &node);
			if (!r) {
				return r;
			}
		}
	}
	cEntity *hit;
	if (cWorld_TestSphereEntities(g_pWorld, self->vehicle, 0x11, &p, Veh(self->vehicle)->wl.prim.radius *
			AIV_REPOS_SPHERE, &hit, 1, 0)) {
		return 0;
	}
	AIV_ReposStart(self, behind);
	self->track = track;
	self->node = node;
	AIV_ReposShow(self, &p);
	t = AIV_Outdoor();
	Traffic_GetNode(t, out, self->track, self->node);
	vec3_t d;
	d.x = p.x - out->x;
	d.y = p.y - out->y;
	d.z = p.z - out->z;
	AIV_ReposStop(self, &d);
	return r;
}

/* the vehicle to a point of visibility near the camera, as ReposTraffic without the tracks; then to IN_ROUTE */
static byte cAIVehicle_Repos(cAIVehicle *self) {
	vec3_t camPos, dir, back, p;
	CNodesWorld *nw;
	byte behind = 0;
	byte r;

	if (!cAIVehicleGroup_ReposReady(g_VehicleGroup)) {
		return 0;
	}
	cCamControl *cam = AIV_ReposCam(&camPos, &dir);
	if (AIV_ReposSlowShip(cam) && !self->reposBehind && crt_rand() % 2 != 0) {
		back.x = -dir.x;
		back.y = -dir.y;
		back.z = -dir.z;
		nw = g_NodesWorld;
		r = CNodesWorld_GetRacePointDir(nw, &p, &camPos, self->minDistReposNotVis, self->maxDistReposNotVis,
			AIV_FloatArg(g_MaxDistAroundReposAI), &back, M_PI_F - cam->cam.fov * AIV_REPOS_HALF * AIV_REPOS_FOV);
		if (r) {
			behind = 1;
		} else {
			nw = g_NodesWorld;
			r = CNodesWorld_GetRacePointDir(nw, &p, &camPos, self->minDistRepos, self->maxDistRepos,
				AIV_FloatArg(g_MaxDistAroundReposAI), &dir, cam->cam.fov * AIV_REPOS_HALF * AIV_REPOS_FOV);
		}
		if (!r) {
			return 0;
		}
	} else {
		nw = g_NodesWorld;
		r = CNodesWorld_GetRacePointDir(nw, &p, &camPos, self->minDistRepos, self->maxDistRepos,
			AIV_FloatArg(g_MaxDistAroundReposAI), &dir, cam->cam.fov * AIV_REPOS_HALF * AIV_REPOS_FOV);
		if (!r) {
			if (self->reposBehind) {
				return 0;
			}
			back.x = -dir.x;
			back.y = -dir.y;
			back.z = -dir.z;
			nw = g_NodesWorld;
			r = CNodesWorld_GetRacePointDir(nw, &p, &camPos, self->minDistReposNotVis, self->maxDistReposNotVis,
				AIV_FloatArg(g_MaxDistAroundReposAI), &back, M_PI_F - cam->cam.fov * AIV_REPOS_HALF *
				AIV_REPOS_FOV);
			if (!r) {
				return 0;
			}
			behind = 1;
		}
	}
	cEntity *hit;
	if (cWorld_TestSphereEntities(g_pWorld, self->vehicle, 0x11, &p, Veh(self->vehicle)->wl.prim.radius *
			AIV_REPOS_SPHERE_W, &hit, 1, 0)) {
		return 0;
	}
	AIV_ReposStart(self, behind);
	AIV_ReposShow(self, &p);
	vec3_t d;
	d.x = p.x - camPos.x;
	d.y = p.y - camPos.y;
	d.z = p.z - camPos.z;
	AIV_ReposStop(self, &d);
	cAIVehicle_SetStateIntern(self, AIV_IN_ROUTE, 1, (double)self->rotA, (double)self->rotB);
	return 1;
}

/* the kind of the traffic track for the class of the vehicle, at random among the kinds the graph has; 5 none */
static void cAIVehicle_PickTrackType(cAIVehicle *self) {
	int list[10];
	int n = 0;
	CNodesTraffic *t = g_IsMapOutDoor ? AIV_Outdoor() : AIV_Indoor();
	int cls = AIV_VehTraffic(self->vehicle)->type;

	if (cls == 2) {
		if (Traffic_NumByType(t, 0) > 0) {
			list[0] = 0;
			n = cls - 1;
		}
	}
	else if (cls != 0) {
		goto medium;
	}
	if (Traffic_NumByType(t, 3) > 0) {
		list[n++] = 3;
	}
medium:
	if (cls == 1) {
		if (Traffic_NumByType(t, 2) > 0) {
			list[n++] = 2;
		}
	}
	else if (cls != 0) {
		goto done;
	}
	if (Traffic_NumByType(t, 4) > 0) {
		list[n++] = 4;
	}
	if (cls == 0 && Traffic_NumByType(t, 1) > 0) {
		list[n++] = 1;
	}
done:
	if (n > 0) {
		self->trackType = list[crt_rand() % n];
		return;
	}
	self->trackType = 5;
}

void cAIVehicle_SetInertia(cAIVehicle *ai, int on) {
	cAIVehicleControl_SetInertia(ai->control, on);
}

void cAIVehicle_SetRotStatic(cAIVehicle *ai, float a, float b) {
	ai->rotA = a;
	ai->rotB = b;
	cAIVehicleControl_SetRotStatic(ai->control, a, b);
}

/* ---- the state changes ---- */

static byte cAIVehicle_ChangeState(cAIVehicle *self, int state, va_list ap);
byte cAIVehicle_SetState(cAIVehicle *ai, int state, ...) {
	char name[256];
	va_list ap;

	cAIVehicle_StateName(state, name);
	AI_Debugf(ai, "Demanda externa de CAMBIO de ESTADO -> %s", name);
	va_start(ap, state);
	byte r = cAIVehicle_ChangeState(ai, state, ap);
	va_end(ap);
	return r;
}

/* the state change from inside the AI: the group lets the next change through at once */
static byte cAIVehicle_SetStateIntern(cAIVehicle *self, int state, ...) {
	char name[256];
	va_list ap;

	cAIVehicle_StateName(state, name);
	AI_Debugf(self, "Demanda interna de CAMBIO de ESTADO -> %s", name);
	if (!AIV_FreeState(state)) {
		AIGroup_AllowChange(g_VehicleGroup);
	}
	va_start(ap, state);
	byte r = cAIVehicle_ChangeState(self, state, ap);
	va_end(ap);
	return r;
}

/* the stopped count d (+1 into STOPPED, -1 out of it) of a counter of the group, not below 0 */
static void AIV_CountStopped(int *n, int d) {
	*n += d;
	if (*n < 0) {
		*n = 0;
	}
}

/* the internal state (AIVI_) and its move; the arguments by state, floats as double, bytes as int:
 * STOPPED, ORIENT_SHOOTING: none
 * FINISHING_LINE: vec3_t dest, dist, boost, a, b, accel, stopAtGoal
 * FINISHING_LINE_WITH_SHOOT_CM, _LIFE, _AMMO: vec3_t dest, dist (WITH_SHOOT_CM also boost)
 * FINISHING_LINE_DIRECT: vec3_t dest, dist, boost, rotA, rotB
 * CHASE_SHOOTING: chaseMin, chaseMax; CHASE_NOT_SHOOTING: chaseMin, rotB
 * FOLLOW_THE_PATH: vec3_t dir, dist, boost, flat, repos, a, b; FOLLOW_THE_PATH_WITH_SHOOT_CM: vec3_t dir, dist,
 * boost, flat */
static void cAIVehicle_SetInternState(cAIVehicle *self, int state, ...) {
	static const double d1 = 1.0, d05 = 0.5, d3 = 3.0, d20 = 20.0;
	char name[256];
	va_list ap;
	byte ok;
	int sa = 0, sb = 0;	/* the arguments of cAIVehicleShoot_SetMode */
	byte sc = 1;

	cAIVehicle_InternStateName(state, name);
	AI_Debugf(self, "CAMBIO de ESTADO INTERNO -> %s", name);
	int old = self->internState;
	int d = 0;
	self->base.base.changed = 1;
	self->internState = state;
	if (state == AIVI_STOPPED) {
		if (old != AIVI_STOPPED) {
			d = 1;
		}
	}
	else if (old == AIVI_STOPPED) {
		d = -1;
	}
	g_VehicleGroup->numStopped += d;
	switch (self->state) {
	case AIV_IN_ROUTE:
		AIV_CountStopped(&g_VehicleGroup->numStoppedRoute, d);
		break;
	case AIV_TRAFFIC:
	case AIV_TRAFFIC_INDOOR: {
		int cls = AIV_VehTraffic(self->vehicle)->type;
		if (cls == 2) {
			AIV_CountStopped(&g_VehicleGroup->numStoppedHeavy, d);
		}
		else if (cls == 1) {
			AIV_CountStopped(&g_VehicleGroup->numStoppedMedium, d);
		}
		else {
			AIV_CountStopped(&g_VehicleGroup->numStoppedLight, d);
		}
		break;
	}
	}
	va_start(ap, state);
	switch (self->internState) {
	case AIVI_STOPPED:
		self->base.moveMode = 0;
		self->reposBehind = 0;
		ok = cAIVehicleControl_SetMove(self->control, AIVC_STOP);
		break;
	case AIVI_FINISHING_LINE: {
		self->base.dest = va_arg(ap, vec3_t);
		double dist = va_arg(ap, double);
		int boost = va_arg(ap, int);
		float a = (float)va_arg(ap, double);
		float b = (float)va_arg(ap, double);
		float accel = (float)va_arg(ap, double);
		self->stopAtGoal = (byte)va_arg(ap, int);
		self->boost = (byte)boost;
		self->base.moveMode = 1;
		self->base.dist = (float)dist;
		self->accel = accel;
		ok = cAIVehicleControl_SetMove(self->control, AIVC_POINT, self->base.dest, (double)self->base.dist, self->boost,
			(double)self->accel, (double)a, (double)b, self->stopAtGoal, 0);
		if (!ok) {
			goto done;
		}
		if (self->state == AIV_RACE) {
			if (AIV_Target(self)) {
				sb = 2;
			}
			else if (cTargetLock_GetPoint(&Veh(self->vehicle)->weapons)) {
				sb = 2;
				sc = 0;
			}
		}
		goto shoot;
	}
	case AIVI_FINISHING_LINE_WITH_SHOOT_CM:
		self->base.dest = va_arg(ap, vec3_t);
		self->base.dist = (float)va_arg(ap, double);
		self->boost = (byte)va_arg(ap, int);
		self->base.moveMode = 1;
		ok = cAIVehicleControl_SetMove(self->control, AIVC_POINT, self->base.dest, (double)self->base.dist, self->boost, d1, d3,
			d3, 0, 0);
		sb = 1;
		break;
	case AIVI_FINISHING_LINE_DIRECT: {
		self->base.dest = va_arg(ap, vec3_t);
		double dist = va_arg(ap, double);
		int boost = va_arg(ap, int);
		float a = (float)va_arg(ap, double);
		float b = (float)va_arg(ap, double);
		self->boost = (byte)boost;
		self->base.moveMode = 2;
		self->base.dist = (float)dist;
		ok = cAIVehicleControl_SetMove(self->control, AIVC_DIRECT, self->base.dest, (double)self->base.dist, self->boost, d1,
			(double)a, (double)b);
		break;
	}
	case AIVI_FINISHING_LINE_LIFE:
	case AIVI_FINISHING_LINE_AMMO: {
		int boost = self->internState == AIVI_FINISHING_LINE_LIFE ? self->params.boostItemLife :
			self->params.boostItemAmmo;
		self->base.dest = va_arg(ap, vec3_t);
		self->base.moveMode = 1;
		self->base.dist = (float)va_arg(ap, double);
		ok = cAIVehicleControl_SetMove(self->control, AIVC_POINT, self->base.dest, (double)self->base.dist, boost, d1, d20, d20,
			0, self->params.strafeEvasion == 1);
		if (!ok) {
			goto done;
		}
		if (self->state == AIV_FIGHTING || self->state == AIV_GET_AWAY || self->state == AIV_GET_AWAY_STOP) {
			sb = 1;
		}
		goto shoot;
	}
	case AIVI_CHASE_SHOOTING:
		self->base.moveMode = 3;
		self->chaseMin = (float)va_arg(ap, double);
		self->chaseMax = (float)va_arg(ap, double);
		ok = cAIVehicleControl_SetMove(self->control, AIVC_CHASE, (double)self->chaseMin, (double)self->chaseMax, 1, d20,
			d20);
		if (!ok) {
			goto done;
		}
		switch (self->state) {
		case AIV_FIGHTING:
			sa = 1;
			goto shoot;
		case AIV_FIGHTING_HOOK:
		case AIV_RACE:
			sb = 2;
			goto shoot;
		}
		goto done;
	case AIVI_CHASE_NOT_SHOOTING:
		self->base.moveMode = 3;
		self->chaseMin = (float)va_arg(ap, double);
		self->rotB = (float)va_arg(ap, double);
		ok = cAIVehicleControl_SetMove(self->control, AIVC_CHASE, (double)self->chaseMin, (double)self->chaseMax, 1, d20,
			d20);
		break;
	case AIVI_FOLLOW_THE_PATH: {
		self->base.dir = va_arg(ap, vec3_t);
		double dist = va_arg(ap, double);
		int boost = va_arg(ap, int);
		int flat = va_arg(ap, int);
		self->base.repos = (byte)va_arg(ap, int);
		float a = (float)va_arg(ap, double);
		float b = (float)va_arg(ap, double);
		self->base.pathB = (byte)flat;
		self->boost = (byte)boost;
		self->base.moveMode = 7;
		self->base.dist = (float)dist;
		ok = cAIVehicleControl_SetMove(self->control, AIVC_PATH, self->base.dir, (double)self->base.dist, self->boost, d05,
			self->base.base.changing == 0, self->base.pathB, (double)a, (double)b, 0);
		break;
	}
	case AIVI_FOLLOW_THE_PATH_WITH_SHOOT_CM: {
		self->base.dir = va_arg(ap, vec3_t);
		double dist = va_arg(ap, double);
		int boost = va_arg(ap, int);
		int flat = va_arg(ap, int);
		self->base.pathB = (byte)flat;
		self->boost = (byte)boost;
		self->base.moveMode = 7;
		self->base.dist = (float)dist;
		ok = cAIVehicleControl_SetMove(self->control, AIVC_PATH, self->base.dir, (double)self->base.dist, self->boost, d1,
			self->base.base.changing == 0, self->base.pathB, d3, d3, self->params.strafeEvasion == 1);
		sb = 1;
		break;
	}
	case AIVI_ORIENT_SHOOTING:
		self->base.moveMode = 6;
		ok = cAIVehicleControl_SetMove(self->control, AIVC_ORIENT);
		sa = 1;
		break;
	default:
		goto done;
	}
	if (!ok) {
		goto done;
	}
shoot:
	cAIVehicleShoot_SetMode(self->shoot, sa, sb, sc);
done:
	va_end(ap);

}

#define g_RadiusShipGraph g_NWNodeRadius			/* RADIUS_SHIP_GRAPH */
cGVar GVar_DepthPathRouteOutDoorAI;

#define g_DepthPathRouteOutDoorAI (GVar_DepthPathRouteOutDoorAI.value.i)
#define g_DepthPathRouteOutDoorAIInit (GVar_DepthPathRouteOutDoorAI.def.i)	/* its default */
#define AIV_LOST_SPEED 600.0

/* the name of the target into the pending state (the original copies without a bound) */
static void AIV_PendingTarget(cAIVehicle *self, const cEntity *e) {
	const char *src = e->name;
	char *dst = self->pending.target;
	int n = sizeof(self->pending.target);

	while (--n > 0 && *src) {
		*dst++ = *src++;
	}
	*dst = 0;
}

/* the target entity e linked (the link it had let go) */
static void AIV_LinkTarget(cAIVehicle *self, cEntity *e) {
	if (AIV_Target(self) == e) {
		return;
	}
	if (AIV_Target(self)) {
		Nto1Node_UnlinkChecked(&self->base.target);
	}
	Nto1Head_Link(&WLife(e)->targets, &self->base.target);
}

/* the states from FIGHTING_HOOK on; 0 for an unknown state or a target the AI cannot have */
static byte cAIVehicle_ChangeStateMore(cAIVehicle *self, int state, byte allowed, va_list ap) {
	cEntity *e;
	vec3_t a, b;

	switch (state) {
	case AIV_FIGHTING_HOOK:
		e = va_arg(ap, cEntity *);
		if (!AIMove_CanTarget(self, e)) {
			return 0;
		}
		AIV_PendingTarget(self, e);
		if (!allowed) {
			return 1;
		}
		AIV_LinkTarget(self, e);
		self->targetTime = 0.0f;
		cAIVehicle_SetInternState(self, AIVI_CHASE_SHOOTING, (double)g_AIChaseDefault1, (double)g_AIChaseDefault2);
		return 1;
	case AIV_FREEZE_OBJ_NOT_ACCESS:
	case AIV_FREEZE_LOST:
	case AIV_FREEZE_CHANGE_STATE:
		cAIVehicle_SetInternState(self, AIVI_STOPPED);
		return 1;
	case AIV_GET_AWAY:
		e = va_arg(ap, cEntity *);
		if (!AIMove_CanTarget(self, e)) {
			return 0;
		}
		AIV_PendingTarget(self, e);
		if (!allowed) {
			return 1;
		}
		AIV_LinkTarget(self, e);
		g_DepthPathRouteOutDoorAI = 6;
		AIV_AwayFromTarget(self);
		cAIVehicle_CheckItems(self);
		return 1;
	case AIV_LOST_PATH:
	case AIV_LOST_PATH_CHASE: {
		const vec3_t *p = Entity_GetPos(self->vehicle, &b);
		CNodesWorld_NearestPointVis(g_NodesWorld, p, &a);
		cAIVehicle_SetInternState(self, AIVI_FINISHING_LINE_DIRECT, a, AIV_LOST_SPEED, 0, 20.0, 20.0);
		cAIVehicleControl_SetDirect(self->control, 1);
		cVehicleTraffic_SetOff(AIV_VehTraffic(self->vehicle), 1);
		return 1;
	}
	}
	Con_Bluef("ERROR PARAM. AI: ESTADO INEXISTENTE.\n");
	return 0;
}

/* the external state with its arguments (see SAIPy.c); a change the vehicle group does not let through yet (or one of
 * a vehicle whose traffic lane is off) waits as pending, the move in FREEZE_CHANGE_STATE meanwhile; 0 for arguments
 * that do not fit (the AI then in FREEZE) */
static byte cAIVehicle_ChangeState(cAIVehicle *self, int state, va_list ap) {
	char name[256];
	byte allowed = 1;
	byte ok = 1;
	cEntity *e;

	if (state == AIV_LOST_PATH) {
		goto change;
	}
	if (!AIV_FreeState(state)) {
		allowed = AIGroup_CanChange(g_VehicleGroup);
	}
	if (state == AIV_LOST_PATH_CHASE || !AIV_VehTraffic(self->vehicle)->off) {
		if (allowed) {
			goto change;
		}
	}
	else {
		allowed = 0;
	}
	self->pending.state = state;
	goto common;
change:
	if (state != AIV_FREEZE_CHANGE_STATE) {
		self->pending.state = AIV_STATE_NONE;
	}
	if (state == AIV_TRAFFIC && !g_IsMapOutDoor) {
		state = AIV_TRAFFIC_INDOOR;
	}
	self->base.base.stateTime = 0.0f;
	self->base.base.changing = 1;
	self->base.base.changed = 1;
	Entity_SetActive(self->vehicle, 1);
	self->lifeMark = Veh(self->vehicle)->wl.life;
	cAIVehicleControl_SetDirect(self->control, 0);
	self->prevState = self->state;
	self->state = state;
	if (state != AIV_RACE) {
		cWeaponManager_LockEntity(&Veh(self->vehicle)->weapons, NULL);
		cWeaponManager_LockPoint(&Veh(self->vehicle)->weapons, NULL);
	}
common: {
	int s = self->state;
	if (s != AIV_TRAFFIC && s != AIV_TRAFFIC_INDOOR && s != AIV_FREEZE && s != AIV_FREEZE_LOST &&
			s != AIV_FREEZE_OBJ_NOT_ACCESS && s != AIV_FREEZE_CHANGE_STATE && s != AIV_FIGHTING_FREEZE) {
		cEntity *v = self->vehicle;
		if (!(Obj_GetRadius(v) < g_RadiusShipGraph)) {
			const char *vname = self->vehicle->name;
			Con_Bluef("RADIO VEHICULO INVALIDO: %s %f\n", vname,
				(double)Obj_GetRadius(self->vehicle));
		}
		CHECK(Obj_GetRadius(self->vehicle) < g_RadiusShipGraph);
	}
	}
	if (AIV_Target(self)) {
		CHECK(self->base.target.data);
		Nto1Node_Unlink(&self->base.target);
	}
	switch (state) {
	case AIV_FREEZE:
		cAIVehicle_SetInternState(self, AIVI_STOPPED);
		break;
	case AIV_GO_TO: {
		cAIVehicleSaved *p = &self->pending;
		p->dest = va_arg(ap, vec3_t);
		p->dist = (float)va_arg(ap, double);
		p->boost = (byte)va_arg(ap, int);
		p->accel = (float)va_arg(ap, double);
		p->rotA = (float)va_arg(ap, double);
		p->rotB = (float)va_arg(ap, double);
		p->stopAtGoal = (byte)va_arg(ap, int);
		if (!allowed) {
			goto wait;
		}
		self->rotA = p->rotA;
		self->rotB = p->rotB;
		cAIVehicle_SetInternState(self, AIVI_FINISHING_LINE, p->dest, (double)p->dist, p->boost, (double)p->rotA,
			(double)p->rotB, (double)p->accel, p->stopAtGoal);
		break;
	}
	case AIV_RACE: {
		cAIVehicleSaved *p = &self->pending;
		p->dest = va_arg(ap, vec3_t);
		p->dist = (float)va_arg(ap, double);
		p->boost = (byte)va_arg(ap, int);
		p->stopAtGoal = (byte)va_arg(ap, int);
		if (!allowed) {
			goto wait;
		}
		Nto1Node_UnlinkChecked(&self->base.target);
		self->rotA = 20.0f;
		self->rotB = 20.0f;
		cAIVehicle_SetInternState(self, AIVI_FINISHING_LINE, p->dest, (double)p->dist, p->boost, 20.0, 20.0, 1.0,
			p->stopAtGoal);
		break;
	}
	case AIV_FIGHTING_FREEZE:
	case AIV_FIGHTING:
	case AIV_CHASE:
	case AIV_GET_AWAY_STOP:
		e = va_arg(ap, cEntity *);
		if (!AIMove_CanTarget(self, e)) {
			ok = 0;
			break;
		}
		AIV_PendingTarget(self, e);
		if (state == AIV_CHASE) {
			self->pending.chaseMin = (float)va_arg(ap, double);
			self->pending.chaseMax = (float)va_arg(ap, double);
		}
		if (!allowed) {
			goto wait;
		}
		AIV_LinkTarget(self, e);
		switch (state) {
		case AIV_FIGHTING_FREEZE:
			self->rotA = 20.0f;
			self->rotB = 20.0f;
			cAIVehicle_SetInternState(self, AIVI_ORIENT_SHOOTING);
			break;
		case AIV_FIGHTING:
			self->rotA = 20.0f;
			self->rotB = 20.0f;
			cAIVehicle_SetInternState(self, AIVI_CHASE_SHOOTING, (double)g_AIChaseDefault1,
				(double)g_AIChaseDefault2);
			self->goingForAmmo = 0;
			self->goingForLife = 0;
			cAIVehicle_CheckItems(self);
			break;
		case AIV_CHASE:
			self->rotA = 20.0f;
			self->rotB = 20.0f;
			cAIVehicle_SetInternState(self, AIVI_CHASE_NOT_SHOOTING, (double)self->pending.chaseMin,
				(double)self->pending.chaseMax);
			self->goingForAmmo = 0;
			self->goingForLife = 0;
			cAIVehicle_CheckItems(self);
			break;
		case AIV_GET_AWAY_STOP:
			self->awayDist = (float)va_arg(ap, double);
			g_DepthPathRouteOutDoorAI = 6;
			self->rotA = 3.0f;
			self->rotB = 2.0f;
			AIV_AwayFromTarget(self);
			cAIVehicle_CheckItems(self);
			break;
		}
		break;
	case AIV_IN_ROUTE: {
		cAIVehicleSaved *p = &self->pending;
		p->repos = (byte)va_arg(ap, int);
		p->rotA = (float)va_arg(ap, double);
		p->rotB = (float)va_arg(ap, double);
		if (!allowed) {
			goto wait;
		}
		g_DepthPathRouteOutDoorAI = g_DepthPathRouteOutDoorAIInit;
		/* a random direction in the plane, x and z in -1..1 */
		double r = (double)(crt_rand() & 0x7fff) * RAND_SCALE;
		vec3_t dir;
		dir.x = (float)(r + r - 1.0);
		r = (double)(crt_rand() & 0x7fff) * RAND_SCALE;
		self->rotA = p->rotA;
		dir.z = (float)(r + r - 1.0);
		dir.y = 0.0f;
		self->rotB = p->rotB;
		cAIVehicle_SetInternState(self, AIVI_FOLLOW_THE_PATH, dir, (double)g_RadiusObjectiveInRoute, 0, 1, p->repos,
			(double)self->rotA, (double)self->rotB);
		cAIVehicle_CheckItems(self);
		break;
	}
	case AIV_TRAFFIC: {
		vec3_t p, tmp;
		self->base.repos = 1;
		cAIVehicle_PickTrackType(self);
		AIV_VehTraffic(self->vehicle)->active = 0;
		Entity_SetActive(self->vehicle, 0);
		self->base.trafficPlaced = 0;
		if (self->trackType == 5) {
			ok = 0;
			break;
		}
		self->base.trafficPlaced = 1;
		self->reposBehind = 0;
		if (!cAIVehicle_ReposTraffic(self, &p, 0.0f, self->maxDistRepos, 0.0f, self->maxDistReposNotVis,
				g_MaxDistAroundReposAI)) {
			self->reposBehind = 0;
			Entity_SetActive(self->vehicle, 0);
			cAIVehicle_SetInternState(self, AIVI_STOPPED);
			break;
		}
		p = *cAIVehicle_TrackPointRandom(self, &tmp, &p, self->track, self->node);
		AIV_VehTraffic(self->vehicle)->active = 1;
		AIV_StartTrack(self, 0, &p);
		break;
	}
	case AIV_TRAFFIC_INDOOR:
		cAIVehicle_PickTrackType(self);
		AIV_VehTraffic(self->vehicle)->active = 0;
		Entity_SetActive(self->vehicle, 0);
		if (self->trackType == 5) {
			ok = 0;
			break;
		}
		CNodesTrafficInDoor_AddVehicle((CNodesTrafficInDoor *)g_NodesTrafficInDoor, self);
		cAIVehicle_SetInternState(self, AIVI_STOPPED);
		break;
	default:
		ok = cAIVehicle_ChangeStateMore(self, state, allowed, ap);
		break;
	}
	if (allowed) {
		self->base.base.changing = 0;
		if (!ok) {
			goto bad;
		}
		if (!AIV_FreeState(state)) {
			AIGroup_ResetChange(g_VehicleGroup);
		}
		cAIVehicle_StateName(state, name);
		if (!AIV_Target(self)) {
			AI_Debugf(self, "CAMBIO DE ESTADO REALIZADO -> %s", name);
		}
		else {
			AI_Debugf(self, "CAMBIO DE ESTADO REALIZADO -> %s - OBJETIVO: %s", name,
				AIV_Target(self)->name);
		}
		return ok;
	}
	if (ok) {
		goto waiting;
	}
bad:
	Con_Bluef("CAMBIO de ESTADO con PARAMETROS INCORRECTOS\n");
	cAIVehicle_SetStateIntern(self, AIV_FREEZE);
	return ok;
wait:
	ok = 1;
waiting:
	if (!AIV_VehTraffic(self->vehicle)->off) {
		AI_Debugf(self, "En ESPERA CAMBIO de ESTADO -> FREEZE_CHANGE_STATE");
		cAIVehicle_SetStateIntern(self, AIV_FREEZE_CHANGE_STATE);
	}
	return ok;
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
		return set(self, AIV_GO_TO, s->dest, (double)s->dist, s->boost, (double)s->accel, (double)s->rotA,
			(double)s->rotB, s->stopAtGoal);
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
		return set(self, s->state, t, (double)s->awayDist);
	case AIV_RACE:
		return set(self, AIV_RACE, s->dest, (double)s->dist, s->boost, s->stopAtGoal);
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

static byte cAIVehicle_RestoreState(cAIVehicle *self, const cAIVehicleSaved *s) {
	return AIV_Restore(self, s, cAIVehicle_SetState);
}

static byte cAIVehicle_RestoreStateIntern(cAIVehicle *self, const cAIVehicleSaved *s) {
	return AIV_Restore(self, s, cAIVehicle_SetStateIntern);
}

/* ---- the items ---- */

/* the nearest item entity of the list (kind < 0 the life items, else the ammo items of the kind) that is there
 * (respawn time not running) and not exclude; its distance into *dist, its description into out */
static cEntity *AIV_NearestItem(cAIVehicle *self, int kind, float *dist, cItemDesc *out, cEntity *exclude) {
	void *iter;
	cEntity *best = NULL;
	cItemDesc *it;
	cInfItem *items = &g_pWorld->logic.items;

	*dist = FLT_MAX;
	it = kind < 0 ? InfItem_FirstLife(items, &iter) : InfItem_FirstAmmo(items, kind, &iter);
	while (it) {
		cEntity *e = World_FindEntity(it->name);
		if (e != exclude && 0.0f >= ((cItemEntity *)e)->respawn) {
			vec3_t a, b;
			const vec3_t *p = Entity_GetPos(e, &a);
			const vec3_t *v = Entity_GetPos(self->vehicle, &b);
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
		items = &g_pWorld->logic.items;
		it = kind < 0 ? InfItem_NextLife(items, &iter) : InfItem_NextAmmo(items, kind, &iter);
	}
	return best;
}

/* the nearest item entity of the life list or of the ammo kind that is there and not exclude */
static cEntity *cAIVehicle_NearestLife(cAIVehicle *self, float *dist, cItemDesc *out, cEntity *exclude) {
	return AIV_NearestItem(self, -1, dist, out, exclude);
}

static cEntity *cAIVehicle_NearestAmmo(cAIVehicle *self, int kind, float *dist, cItemDesc *out,
	cEntity *exclude) {
	return AIV_NearestItem(self, kind, dist, out, exclude);
}

cEntity *cAIVehicle_GetNearestItemLife(cAIVehicle *ai) {
	cItemDesc desc;
	float dist;

	cString_ctor(&desc.name);
	cEntity *e = cAIVehicle_NearestLife(ai, &dist, &desc, NULL);
	cString_dtor(&desc.name);
	return e;
}

/* the nearest ammo item of the three kinds the vehicle has weapons for and is short of; the kinds are tried in the
 * order 0, 2, 1 and kept in the slots 0, 1, 2 */
static cEntity *cAIVehicle_NearestAmmoAny(cAIVehicle *self, float *dist, cItemDesc *out, cEntity *exclude,
	float factor) {
	static const int kinds[3] = { 0, 2, 1 };
	byte found = 0;
	int best = 0;

	*dist = FLT_MAX;
	for (int i = 0; i < 3; i++) {
		int kind = kinds[i];
		if (!cAIVehicleShoot_UsesAmmo(self->shoot, kind, factor)) {
			continue;
		}
		cAmmoInfo *info = TABLE_AT(g_pWorld->ammo, kind);
		float ammo = cTargetLock_GetAmmo(&Veh(self->vehicle)->weapons, kind);
		if (!(info->max > ammo)) {
			continue;
		}
		cEntity *e = cAIVehicle_NearestAmmo(self, kind, &self->itemAmmoDistK[i], &self->itemAmmoK[i], exclude);
		self->itemAmmoEnt[i] = e;
		if (e && (i == 0 || *dist > self->itemAmmoDistK[i])) {
			*dist = self->itemAmmoDistK[i];
			found = 1;
			best = i;
		}
	}
	if (!found) {
		return NULL;
	}
	self->itemAmmoLevel = cTargetLock_GetAmmo(&Veh(self->vehicle)->weapons, kinds[best]);
	cEntity *e = self->itemAmmoEnt[best];
	cString_Assign(&out->name, &self->itemAmmoK[best].name);
	out->value = self->itemAmmoK[best].value;
	return e;
}

/* itemAmmoBest: the nearest ammo item, with a random factor first (0 or FLT_MAX), the other one when none */
static void cAIVehicle_FindAmmo(cAIVehicle *self, cEntity *exclude) {
	float dist, factor;

	if (crt_rand() % 2 == 0) {
		self->ammoSearch = 0;
		factor = 0.0f;
	}
	else {
		factor = FLT_MAX;
		self->ammoSearch = 1;
	}
	self->itemAmmoBest = cAIVehicle_NearestAmmoAny(self, &dist, &self->itemAmmo, exclude, factor);
	if (self->itemAmmoBest) {
		return;
	}
	if (self->ammoSearch == 0) {
		factor = FLT_MAX;
		self->ammoSearch = 1;
	}
	else {
		factor = 0.0f;
		self->ammoSearch = 0;
	}
	self->itemAmmoBest = cAIVehicle_NearestAmmoAny(self, &dist, &self->itemAmmo, exclude, factor);
}

static cGVar GVar_DistNotGetAwayAI;

/* ---- the reactions, the hook and the items of the states ---- */

#define g_DistNotGetAwayAI (GVar_DistNotGetAwayAI.value.f)
static cGVar GVar_DistGetAwayBoostAI;
#define g_DistGetAwayBoostAI (GVar_DistGetAwayBoostAI.value.f)
static cGVar GVar_MaxDistElevatorAI;
#define g_MaxDistElevatorAI (GVar_MaxDistElevatorAI.value.f)
static cGVar GVar_MinDistChaseElevatorAI;
#define g_MinDistChaseElevatorAI (GVar_MinDistChaseElevatorAI.value.f)
static cGVar GVar_MaxDistChaseElevatorAI;
#define g_MaxDistChaseElevatorAI (GVar_MaxDistChaseElevatorAI.value.f)
#define AIV_LOST_TIME 15.0f
#define AIV_ITEM_NEAR 10000.0f

/* the target within dist on each axis */
static byte AIV_TargetInBox(cAIVehicle *self, float dist) {
	vec3_t a, b;
	cEntity *t = AIV_Target(self);
	const vec3_t *tp = Entity_GetPos(t, &a);
	const vec3_t *vp = Entity_GetPos(self->vehicle, &b);

	return (double)dist > __builtin_fabs((double)(vp->x - tp->x)) &&
		(double)dist > __builtin_fabs((double)(vp->y - tp->y)) &&
		(double)dist > __builtin_fabs((double)(vp->z - tp->z));
}

/* the line from the vehicle to the target hits the level */
static byte AIV_TargetBlocked(cAIVehicle *self) {
	vec3_t a, b;
	cEntity *t = AIV_Target(self);
	const vec3_t *tp = Entity_GetPos(t, &a);
	const vec3_t *vp = Entity_GetPos(self->vehicle, &b);

	return Engine_TestLine(vp, tp, NULL, NULL);
}

/* the sphere of the vehicle free of the level */
static byte AIV_PlaceFree(cAIVehicle *self) {
	vec3_t a, b;
	cEntity *v = self->vehicle;
	float r = Obj_GetRadius(v);
	const vec3_t *pa = Entity_GetPos(v, &a);
	const vec3_t *pb = Entity_GetPos(v, &b);

	return !Engine_TestSphere(pb, pa, r, NULL, NULL);
}

/* the vehicle closer than 15000 to dest */
static byte AIV_NearDest(cAIVehicle *self) {
	vec3_t a;
	const vec3_t *v = Entity_GetPos(self->vehicle, &a);
	float dy = v->y - self->base.dest.y;
	float dx = v->x - self->base.dest.x;
	float dz = v->z - self->base.dest.z;

	return AIV_NEAR_DIST2 > dy * dy + dx * dx + dz * dz;
}

/* the FIGHTING_HOOK the move takes at random (USE_PERC_HOOK_GET_AWAY) when the vehicle has the hook, with the aim at
 * 10 until it lets go; 1 if it did */
static byte AIV_HookChance(cAIVehicle *self, int mode) {
	if (!cAIVehicleShoot_HasHook(self->shoot) || self->params.percHookGetAway <= 0 ||
			crt_rand() % 101 > self->params.percHookGetAway) {
		return 0;
	}
	cAIVehicleParams2 *p = cAIVehicleControl_GetParams(self->control);
	self->hookSaved = p->aim;
	p->aim = 10;
	self->hookMode = mode;
	cAIVehicle_SetStateIntern(self, AIV_FIGHTING_HOOK, AIV_Target(self));
	return 1;
}

static cEntity *AIV_FindLifeItem(cAIVehicle *self) {
	float dist;
	cEntity *e = cAIVehicle_NearestLife(self, &dist, &self->itemLife, NULL);

	self->itemLifeEnt = e;
	return e;
}

/* to the item e with FINISHING_LINE_LIFE or FINISHING_LINE_AMMO */
static void AIV_GoToItem(cAIVehicle *self, int intern, cEntity *e) {
	vec3_t a;
	const vec3_t *p = Entity_GetPos(e, &a);

	cAIVehicle_SetInternState(self, intern, *p, (double)((cItemEntity *)e)->radius);
}

static void AIV_GoToLife(cAIVehicle *self, cEntity *e) {
	self->itemLifeMark = Veh(self->vehicle)->wl.life;
	AIV_GoToItem(self, AIVI_FINISHING_LINE_LIFE, e);
}

static void AIV_Chase(cAIVehicle *self, int intern) {
	self->goingForLife = 0;
	cAIVehicle_SetInternState(self, intern, (double)self->chaseMin, (double)self->chaseMax);
}

/* the life of the vehicle below FACTOR_DAMAGE_ITEM_LIFE of its maximum */
static byte AIV_NeedLife(cAIVehicle *self) {
	cVehicleEntity *v = Veh(self->vehicle);

	return (float)v->maxLife * self->params.damageItemLife > (float)v->wl.life;
}

/* the life item the AI goes for was taken (it respawns): on to the next one, unless the vehicle got life meanwhile */
enum { AIV_ITEM_THERE, AIV_ITEM_NEXT, AIV_ITEM_GOT_LIFE, AIV_ITEM_NONE };
static int AIV_RetryLife(cAIVehicle *self) {
	if (!(((cItemEntity *)self->itemLifeEnt)->respawn > 0.0f)) {
		return AIV_ITEM_THERE;
	}
	if (Veh(self->vehicle)->wl.life > self->itemLifeMark) {
		return AIV_ITEM_GOT_LIFE;
	}
	cEntity *e = AIV_FindLifeItem(self);
	if (!e) {
		return AIV_ITEM_NONE;
	}
	AIV_GoToLife(self, e);
	return AIV_ITEM_NEXT;
}

/* the life item when the life went below the factor (FINISHING_LINE_LIFE) */
static void AIV_LifeIfLow(cAIVehicle *self) {
	if (!AIV_NeedLife(self)) {
		return;
	}
	cEntity *e = AIV_FindLifeItem(self);
	if (e) {
		self->goingForLife = 1;
		AIV_GoToLife(self, e);
	}
}

/* FIGHTING: the chase distances for inside a hangar (an elevator of the group near the target) and back; the hook
 * at random (USE_PERC_HOOK) when the target is in range */
static void cAIVehicle_ReactFighting(cAIVehicle *self) {
	vec3_t a;
	float dist = g_MaxDistElevatorAI;
	cEntity *t = AIV_Target(self);
	const vec3_t *p = Entity_GetPos(t, &a);
	byte near = cAIVehicleGroup_ElevatorNear(g_VehicleGroup, p, dist);
	float cm = self->chaseMin;

	if (cm == g_AIChaseDefault1) {
		if (near) {
			cAIVehicle_SetInternState(self, AIVI_CHASE_SHOOTING, (double)g_MinDistChaseElevatorAI,
				(double)g_MaxDistChaseElevatorAI);
		}
	}
	else if (cm == g_MinDistChaseElevatorAI && !near) {
		cAIVehicle_SetInternState(self, AIVI_CHASE_SHOOTING, (double)g_AIChaseDefault1, (double)g_AIChaseDefault2);
	}
	if (cAIVehicleShoot_HasHook(self->shoot) && self->params.percHook > 0) {
		t = AIV_Target(self);
		if (!WithLife_IsEMIed(t) && cAIVehicleControl_GetFlag(self->control) &&
				cAIVehicle_InHookRange(self, AIV_Target(self)) && crt_rand() % 101 <= self->params.percHook) {
			cAIVehicle_SetStateIntern(self, AIV_FIGHTING_HOOK, AIV_Target(self));
		}
	}
}

/* FIGHTING_HOOK: the hook kept while the target is in range (1 s of a hook out of a GET_AWAY, else 3 s), then back to
 * what the AI did before it */
static void cAIVehicle_ReactHook(cAIVehicle *self) {
	cEntity *t = AIV_Target(self);

	if (!WithLife_IsEMIed(t) && cAIVehicleControl_GetFlag(self->control) &&
			cAIVehicle_InHookRange(self, AIV_Target(self))) {
		if (self->params.percHookGetAway > 0 && self->hookMode != 0) {
			if (!(self->targetTime > 1.0f)) {
				return;
			}
		}
		else if (self->params.percHook <= 0 || !(self->targetTime > 3.0f)) {
			return;
		}
	}
	if (self->params.percHookGetAway <= 0 || self->hookMode == 0) {
		if (self->params.percHook > 0) {
			cAIVehicle_SetStateIntern(self, AIV_FIGHTING, AIV_Target(self));
		}
		return;
	}
	cAIVehicleControl_GetParams(self->control)->aim = self->hookSaved;
	switch (self->hookMode) {
	case 1:
		cAIVehicle_SetStateIntern(self, AIV_GET_AWAY, AIV_Target(self));
		break;
	case 2: {
		/* FIGHTING without a new hook, then to the life item */
		int keep = self->params.percHookGetAway;
		t = AIV_Target(self);
		self->itemLifeMark = Veh(self->vehicle)->wl.life;
		self->params.percHookGetAway = 0;
		cAIVehicle_SetStateIntern(self, AIV_FIGHTING, t);
		self->params.percHookGetAway = keep;
		cEntity *e = AIV_FindLifeItem(self);
		if (e) {
			AIV_GoToItem(self, AIVI_FINISHING_LINE_LIFE, e);
			self->hookMode = 0;
			return;
		}
		break;
	}
	case 3: {
		t = AIV_Target(self);
		self->itemAmmoStart = self->itemAmmoLevel;
		cAIVehicle_SetStateIntern(self, AIV_FIGHTING, t);
		cAIVehicle_FindAmmo(self, NULL);
		cEntity *e = self->itemAmmoBest;
		if (e) {
			AIV_GoToItem(self, AIVI_FINISHING_LINE_AMMO, e);
			self->hookMode = 0;
			return;
		}
		break;
	}
	}
	self->hookMode = 0;
}

/* GET_AWAY: FIGHTING again when the target is no longer close (DistNotGetAwayAI), the boost while it is very close */
static void cAIVehicle_ReactGetAway(cAIVehicle *self) {
	if (self->internState != AIVI_FOLLOW_THE_PATH_WITH_SHOOT_CM) {
		return;
	}
	if (!AIV_TargetInBox(self, g_DistNotGetAwayAI)) {
		cEntity *t = AIV_Target(self);
		self->lifeMark = Veh(self->vehicle)->wl.life;
		cAIVehicle_SetStateIntern(self, AIV_FIGHTING, t);
		return;
	}
	if (!self->params.boostGetAway) {
		return;
	}
	self->control->boost = AIV_TargetInBox(self, g_DistGetAwayBoostAI);
}

/* GET_AWAY_STOP: away from the target while it is within awayDist and in sight, stopped when the line to it is blocked */
static void cAIVehicle_ReactGetAwayStop(cAIVehicle *self) {
	switch (self->internState) {
	case AIVI_STOPPED:
		if (!AIV_TargetBlocked(self)) {
			AIV_AwayFromTarget(self);
		}
		return;
	case AIVI_FINISHING_LINE_LIFE:
		self->control->boost = AIV_TargetInBox(self, g_DistGetAwayBoostAI);
		return;
	case AIVI_FOLLOW_THE_PATH_WITH_SHOOT_CM:
		if (AIV_TargetInBox(self, self->awayDist)) {
			if (!self->params.boostGetAway) {
				self->control->boost = 0;
				return;
			}
			self->control->boost = AIV_TargetInBox(self, g_DistGetAwayBoostAI);
			return;
		}
		if (AIV_TargetBlocked(self)) {
			cAIVehicle_SetInternState(self, AIVI_STOPPED);
		}
		return;
	}
}

/* LOST_PATH and LOST_PATH_CHASE (two copies): FREEZE_LOST when the place of the vehicle is free after 15 s */
static void cAIVehicle_ReactLostPath(cAIVehicle *self) {
	if (!(self->base.base.stateTime > AIV_LOST_TIME) || !AIV_PlaceFree(self)) {
		return;
	}
	cVehicleTraffic_SetOff(AIV_VehTraffic(self->vehicle), 0);
	cAIVehicle_SetStateIntern(self, AIV_FREEZE_LOST);
}

/* LOST_PATH and LOST_PATH_CHASE (two copies): back to the kept state once the vehicle is in the graph again */
static void cAIVehicle_ThinkLostPath(cAIVehicle *self) {
	cSlotController *slot = &self->slotAttack;
	float t;

	if (!cSlotController_Tick(slot, self->base.base.dt) || !cSlotController_FireReset(slot, &t)) {
		return;
	}
	do {
		vec3_t a;
		cEntity *v = self->vehicle;
		float r = Obj_GetRadius(v);
		const vec3_t *p = Entity_GetPos(v, &a);
		if (CNodesWorld_HasPointVis(g_NodesWorld, p, r) && AIV_PlaceFree(self)) {
			cVehicleTraffic_SetOff(AIV_VehTraffic(self->vehicle), 0);
			cAIVehicle_RestoreStateIntern(self, &self->saved);
		}
	} while (cSlotController_FireReset(slot, &t));
}

/* the hook point: tried when the vehicle moves, let go near the goal or out of range; 1 while it is set */
static byte cAIVehicle_TryHook(cAIVehicle *self, int a, int b) {
	vec3_t goal, v;
	cTargetLock *w = &Veh(self->vehicle)->weapons;

	if (!cTargetLock_GetPoint(w)) {
		if (!(Veh(self->vehicle)->controls.acc > 0.0f)) {
			goto done;
		}
		int map = self->params.hookMap;
		if (map != 10 && (map <= 0 || crt_rand() % ((10 - map) * 2) != 0)) {
			goto done;
		}
		const vec3_t *g = cAIVehicleControl_GetGoal(self->control, &goal);
		const vec3_t *p = Entity_GetPos(self->vehicle, &v);
		float dy = p->y - g->y;
		float dx = p->x - g->x;
		float dz = p->z - g->z;
		if (AIV_NEAR_DIST2 > dy * dy + dx * dx + dz * dz || !cAIVehicle_TestHookPoint(self, &goal)) {
			goto done;
		}
		cWeaponManager_LockPoint(&Veh(self->vehicle)->weapons, &goal);
		cAIVehicleShoot_SetMode(self->shoot, 0, 2, 0);
	}
	else {
		const vec3_t *g = cAIVehicleControl_GetGoal(self->control, &goal);
		const vec3_t *p = Entity_GetPos(self->vehicle, &v);
		w = &Veh(self->vehicle)->weapons;
		float dy = p->y - g->y;
		float dx = p->x - g->x;
		float dz = p->z - g->z;
		/* far from the goal the hook stays while its point is in range */
		if (!(AIV_NEAR_DIST2 > dy * dy + dx * dx + dz * dz) &&
				cAIVehicle_InHookRangePos(self, cTargetLock_GetPoint(w))) {
			goto done;
		}
		cWeaponManager_LockPoint(w, NULL);
		cAIVehicleShoot_SetMode(self->shoot, a, b, 1);
	}
done:
	return cTargetLock_GetPoint(&Veh(self->vehicle)->weapons) != NULL;
}

/* one step of the hook slot: let go when release, else the hook tried; 1 ends the slot */
static byte AIV_HookStep(cAIVehicle *self, byte release, int a, int b) {
	if (cTargetLock_GetPoint(&Veh(self->vehicle)->weapons)) {
		if (release) {
			cWeaponManager_LockPoint(&Veh(self->vehicle)->weapons, NULL);
			cAIVehicleShoot_SetMode(self->shoot, a, b, 1);
			return 1;
		}
	}
	else if (release) {
		return 0;
	}
	return cAIVehicle_TryHook(self, a, b);
}

/* the hook slot of a state: the target checked first (not for GO_TO), FACTOR_HOOK_MAP > 0 */
static byte AIV_HookSlot(cAIVehicle *self, byte target, float *t) {
	cSlotController *slot = &self->slotItems;

	if (target && !cAIVehicle_CheckTarget(self)) {
		return 0;
	}
	return self->params.hookMap > 0 && cSlotController_Tick(slot, self->base.base.dt) &&
		cSlotController_FireReset(slot, t);
}

static byte AIV_TargetInSight(cAIVehicle *self) {
	return cAIVehicleControl_GetFlag(self->control) && cAIVehicleShoot_TargetInSight(self->shoot, 0);
}

/* the hook of the states that move: aimed or let go on the hook slot */
static void cAIVehicle_ThinkChase(cAIVehicle *self) {
	float t;

	if (!AIV_HookSlot(self, 1, &t)) {
		return;
	}
	do {
		int in = self->internState;
		if (in == AIVI_FINISHING_LINE_LIFE) {
			if (AIV_HookStep(self, AIV_NearDest(self), 0, 1)) {
				return;
			}
		}
		else if (in == AIVI_CHASE_NOT_SHOOTING) {
			if (AIV_HookStep(self, AIV_TargetInSight(self), 0, 0)) {
				return;
			}
		}
	} while (cSlotController_FireReset(&self->slotItems, &t));
}

static void cAIVehicle_ThinkFighting(cAIVehicle *self) {
	float t;

	if (!AIV_HookSlot(self, 1, &t)) {
		return;
	}
	do {
		int in = self->internState;
		if (in == AIVI_FINISHING_LINE_LIFE || in == AIVI_FINISHING_LINE_AMMO) {
			if (AIV_HookStep(self, AIV_NearDest(self), 0, 1)) {
				return;
			}
		}
		else if (in == AIVI_CHASE_SHOOTING) {
			if (AIV_HookStep(self, AIV_TargetInSight(self), 1, 0)) {
				return;
			}
		}
	} while (cSlotController_FireReset(&self->slotItems, &t));
}

static void cAIVehicle_ThinkGetAway(cAIVehicle *self) {
	float t;

	if (!AIV_HookSlot(self, 1, &t)) {
		return;
	}
	do {
		if (AIV_HookStep(self, AIV_NearDest(self), 0, 1)) {
			return;
		}
	} while (cSlotController_FireReset(&self->slotItems, &t));
}

static void cAIVehicle_ThinkGetAwayStop(cAIVehicle *self) {
	float t;

	if (!AIV_HookSlot(self, 1, &t)) {
		return;
	}
	do {
		int in = self->internState;
		if (in == AIVI_FINISHING_LINE_LIFE || in == AIVI_FOLLOW_THE_PATH_WITH_SHOOT_CM) {
			if (AIV_HookStep(self, AIV_NearDest(self), 0, 1)) {
				return;
			}
		}
	} while (cSlotController_FireReset(&self->slotItems, &t));
}

static void cAIVehicle_ThinkGoTo(cAIVehicle *self) {
	float t;

	if (!AIV_HookSlot(self, 0, &t)) {
		return;
	}
	do {
		if (AIV_HookStep(self, AIV_NearDest(self), 0, 0)) {
			return;
		}
	} while (cSlotController_FireReset(&self->slotItems, &t));
}

/* RACE: on to the finishing line (FINISHING_LINE) after a chase of 15 s, near the goal or when the target is out of
 * hook range; without a target the hook on the way and at random (CONTROL_HOOK_RACE) a chase of a vehicle of the
 * group it can hook */
static void cAIVehicle_ThinkRace(cAIVehicle *self) {
	cSlotController *slot = &self->slotItems;
	int a, kind;
	byte flag;
	float t;

	self->targetTime = self->base.base.dt + self->targetTime;
	cAIVehicleShoot_GetInfo(self->shoot, &a, &kind, &flag);
	if (kind == 2 && flag && (!AIV_Target(self) || !AIV_Target(self)->active)) {
		goto race;
	}
	if (!cSlotController_Tick(slot, self->base.base.dt) || !cSlotController_FireReset(slot, &t)) {
		return;
	}
	do {
		cEntity *target = AIV_Target(self);
		if (!target) {
			if (self->params.hookMap > 0) {
				byte near = AIV_NearDest(self);
				if (cTargetLock_GetPoint(&Veh(self->vehicle)->weapons)) {
					if (near) {
						cWeaponManager_LockPoint(&Veh(self->vehicle)->weapons, NULL);
						cAIVehicleShoot_SetMode(self->shoot, 0, 0, 1);
						return;
					}
					if (cAIVehicle_TryHook(self, 0, 0)) {
						return;
					}
				}
				else if (!near && cAIVehicle_TryHook(self, 0, 0)) {
					return;
				}
			}
			if (0.0f >= Veh(self->vehicle)->controls.acc) {
				return;
			}
			int race = self->params.hookRace;
			if (race == 0 || (race != 10 && crt_rand() % ((10 - race) * 2) != 0)) {
				continue;
			}
			int n = AIGroup_Num(g_VehicleGroup);
			for (int i = 0; i < n; i++) {
				cEntity *e = AIGroup_Get(g_VehicleGroup, i);
				if (e == self->vehicle || !AIMove_CanTarget(self, e) || !cAIVehicle_CanHook(self, e)) {
					continue;
				}
				if (AIV_Target(self) != e) {
					if (AIV_Target(self)) {
						Nto1Node_UnlinkChecked(&self->base.target);
					}
					/* the head of the links to the vehicle */
					Nto1Head_Link(&WLife(e)->targets, &self->base.target);
				}
				float r = Veh(self->vehicle)->wl.prim.radius + Veh(AIV_Target(self))->wl.prim.radius;
				cAIVehicle_SetInternState(self, AIVI_CHASE_SHOOTING, (double)r, (double)(r + r));
				self->targetTime = 0.0f;
				break;
			}
			continue;
		}
		if (self->targetTime > AIV_LOST_TIME || AIV_NearDest(self)) {
			CHECK(self->base.target.data);
			Nto1Node_Unlink(&self->base.target);
			goto race;
		}
		if (!cAIVehicle_CanHook(self, AIV_Target(self))) {
			Nto1Node_UnlinkChecked(&self->base.target);
			goto race;
		}
		if (self->internState == AIVI_CHASE_SHOOTING && cTargetLock_GetEntity(&Veh(self->vehicle)->weapons)) {
			const char *name = AIV_Target(self)->name;
			if (!strcmp(name, cTargetLock_GetEntity(&Veh(self->vehicle)->weapons))) {
				goto race;
			}
		}
	} while (cSlotController_FireReset(slot, &t));
	return;
race:
	cAIVehicle_SetInternState(self, AIVI_FINISHING_LINE, self->base.dest, (double)self->base.dist, self->boost, 20.0,
		20.0, 1.0, self->stopAtGoal);
	self->targetTime = 0.0f;
}

/* CHASE: the life item when the life is low and the target has at least half of it */
static void cAIVehicle_ItemsChase(cAIVehicle *self) {
	int in = self->internState;

	if (in == AIVI_FINISHING_LINE_LIFE) {
		cVehicleEntity *v = Veh(self->vehicle);
		if (v->wl.life < v->maxLife) {
			switch (AIV_RetryLife(self)) {
			case AIV_ITEM_THERE:
			case AIV_ITEM_NEXT:
				return;
			case AIV_ITEM_NONE:
				AIV_Chase(self, AIVI_CHASE_SHOOTING);
				return;
			}
		}
		AIV_Chase(self, AIVI_CHASE_NOT_SHOOTING);
		return;
	}
	if (in != AIVI_CHASE_NOT_SHOOTING || !AIV_NeedLife(self) ||
			(float)Veh(self->vehicle)->wl.life * 0.5f > (float)Veh(AIV_Target(self))->wl.life) {
		return;
	}
	self->goingForLife = 1;
	cEntity *e = AIV_FindLifeItem(self);
	if (!e) {
		self->goingForLife = 0;
		return;
	}
	AIV_GoToLife(self, e);
}

/* FIGHTING: the life item (CHASE_SHOOTING when it is gone or the life is full again), the life item when the life is
 * low, and GET_AWAY (or a hook) after much damage in TimeDamageAI */
static void cAIVehicle_ItemsFighting(cAIVehicle *self) {
	cVehicleEntity *v = Veh(self->vehicle);

	switch (self->internState) {
	case AIVI_FINISHING_LINE_LIFE:
		if (v->wl.life < v->maxLife) {
			int r = AIV_RetryLife(self);
			if (r == AIV_ITEM_THERE || r == AIV_ITEM_NEXT) {
				return;
			}
		}
		AIV_Chase(self, AIVI_CHASE_SHOOTING);
		return;
	case AIVI_FINISHING_LINE_AMMO: {
		if (!AIV_NeedLife(self)) {
			return;
		}
		self->goingForLife = 1;
		cEntity *e = AIV_FindLifeItem(self);
		if (!e) {
			self->goingForLife = 0;
			return;
		}
		AIV_GoToLife(self, e);
		return;
	}
	case AIVI_CHASE_SHOOTING:
		if (AIV_NeedLife(self) && !((float)v->wl.life * 0.5f > (float)Veh(AIV_Target(self))->wl.life)) {
			self->goingForLife = 1;
			if (AIV_FindLifeItem(self)) {
				if (!AIV_HookChance(self, 2)) {
					AIV_GoToLife(self, self->itemLifeEnt);
					return;
				}
			}
			else {
				self->goingForLife = 0;
			}
		}
		v = Veh(self->vehicle);
		if (!((float)(self->lifeMark - v->wl.life) > (float)v->maxLife * self->params.damageGetAway)) {
			return;
		}
		if (AIV_HookChance(self, 1)) {
			return;
		}
		cAIVehicle_SetStateIntern(self, AIV_GET_AWAY, AIV_Target(self));
		return;
	}
}

/* GET_AWAY: the life item (FIGHTING when it is gone), the life item when the life is low */
static void cAIVehicle_ItemsGetAway(cAIVehicle *self) {
	int in = self->internState;

	if (in == AIVI_FINISHING_LINE_LIFE) {
		int r = AIV_RetryLife(self);
		if (r == AIV_ITEM_GOT_LIFE || r == AIV_ITEM_NONE) {
			cEntity *t = AIV_Target(self);
			self->goingForLife = 0;
			cAIVehicle_SetStateIntern(self, AIV_FIGHTING, t);
		}
		return;
	}
	if (in == AIVI_FOLLOW_THE_PATH_WITH_SHOOT_CM) {
		AIV_LifeIfLow(self);
	}
}

/* GET_AWAY_STOP: the same, back to GET_AWAY_STOP with its distance */
static void cAIVehicle_ItemsGetAwayStop(cAIVehicle *self) {
	int in = self->internState;

	if (in == AIVI_FINISHING_LINE_LIFE) {
		int r = AIV_RetryLife(self);
		if (r == AIV_ITEM_GOT_LIFE || r == AIV_ITEM_NONE) {
			double d = self->awayDist;
			self->goingForLife = 0;
			cAIVehicle_SetStateIntern(self, AIV_GET_AWAY_STOP, AIV_Target(self), d);
		}
		return;
	}
	if (in == AIVI_FOLLOW_THE_PATH_WITH_SHOOT_CM) {
		AIV_LifeIfLow(self);
	}
}

/* IN_ROUTE: the same, back to IN_ROUTE */
static void cAIVehicle_ItemsInRoute(cAIVehicle *self) {
	int in = self->internState;

	if (in == AIVI_FINISHING_LINE_LIFE) {
		int r = AIV_RetryLife(self);
		if (r == AIV_ITEM_GOT_LIFE || r == AIV_ITEM_NONE) {
			self->goingForLife = 0;
			cAIVehicle_SetStateIntern(self, AIV_IN_ROUTE, self->base.repos, (double)self->rotA, (double)self->rotB);
		}
		return;
	}
	if (in == AIVI_FOLLOW_THE_PATH) {
		AIV_LifeIfLow(self);
	}
}

/* FIGHTING without a life item: the ammo item when the ammo is low (FINISHING_LINE_AMMO, or a hook first), and after
 * it a life item within 10000 or CHASE_SHOOTING again */
static void cAIVehicle_FightingAfterItem(cAIVehicle *self) {
	if (self->goingForLife) {
		return;
	}
	int in = self->internState;
	if (in == AIVI_FINISHING_LINE_AMMO) {
		if (!(((cItemEntity *)self->itemAmmoBest)->respawn > 0.0f) && !(self->itemAmmoLevel > self->itemAmmoStart)) {
			return;
		}
		self->goingForAmmo = 0;
		self->ammoSearch = 3;
		cVehicleEntity *v = Veh(self->vehicle);
		if (v->wl.life < v->maxLife) {
			cItemDesc desc;
			float dist;
			cString_ctor(&desc.name);
			cEntity *e = cAIVehicle_NearestLife(self, &dist, &desc, NULL);
			self->itemLifeEnt = e;
			if (e && AIV_ITEM_NEAR > dist) {
				AIV_GoToLife(self, e);
				cString_dtor(&desc.name);
				return;
			}
			cString_dtor(&desc.name);
		}
		cAIVehicle_SetInternState(self, AIVI_CHASE_SHOOTING, (double)self->chaseMin, (double)self->chaseMax);
		return;
	}
	if (in != AIVI_CHASE_SHOOTING || cAIVehicleShoot_HasAmmo(self->shoot, self->params.itemAmmo)) {
		return;
	}
	self->goingForAmmo = 1;
	cAIVehicle_FindAmmo(self, NULL);
	if (!self->itemAmmoBest) {
		self->goingForAmmo = 0;
		return;
	}
	if (AIV_HookChance(self, 3)) {
		return;
	}
	cEntity *e = self->itemAmmoBest;
	self->itemAmmoStart = self->itemAmmoLevel;
	AIV_GoToItem(self, AIVI_FINISHING_LINE_AMMO, e);
}

static cGVar GVar_VelReposStopAI;
#define g_VelReposStopAI (GVar_VelReposStopAI.value.f)	/* VelReposStopAI */
static cGVar GVar_MaxDistReposStopAI;
#define g_MaxDistReposStopAI (GVar_MaxDistReposStopAI.value.f)	/* MaxDistReposStopAI */
static cGVar GVar_MinDistReposStopAI;
#define g_MinDistReposStopAI (GVar_MinDistReposStopAI.value.f)	/* MinDistReposStopAI */

/* the repositioning on a traffic track in view, once the vehicle is placed, again when it got too far from the camera
 * (closer when the camera follows a slow ship) */
static void AIV_GraphTrafficRepos(cAIVehicle *self, vec3_t *p) {
	vec3_t tmp;

	*p = *cAIVehicle_TrackPointRandom(self, &tmp, p, self->track, self->node);
}

/* TRAFFIC: a stopped vehicle onto a track near the camera; on the track (FINISHING_LINE_DIRECT) moved again when the
 * camera is too far: behind the camera (out of the cone of view, not just placed) beyond the NotVis distances, else
 * beyond maxDistRepos */
static void cAIVehicle_GraphTraffic(cAIVehicle *self) {
	vec3_t p, tmp, d, dir;

	if (!self->base.repos) {
		return;
	}
	if (self->internState == AIVI_STOPPED) {
		if (!cAIVehicleGroup_ReposReady(g_VehicleGroup) || !cAIVehicle_ReposTraffic(self, &p, self->minDistRepos,
				self->maxDistRepos, self->minDistReposNotVis, self->maxDistReposNotVis, g_MaxDistAroundReposAI)) {
			return;
		}
		AIV_GraphTrafficRepos(self, &p);
		AIV_VehTraffic(self->vehicle)->active = 1;
		AIV_StartTrack(self, 0, &p);
		return;
	}
	if (self->internState != AIVI_FINISHING_LINE_DIRECT) {
		return;
	}
	cCamControl *cam = (cCamControl *)World_FindEntity(cWorld_GetCamName(g_pWorld, 0));
	vec3_t camPos = g_pWorld->camPos[g_pWorld->curCam];
	const vec3_t *pos = Entity_GetPos(self->vehicle, &tmp);
	d.x = pos->x - camPos.x;
	d.y = pos->y - camPos.y;
	d.z = pos->z - camPos.z;
	float dist = (float)__builtin_sqrt((double)(d.y * d.y + d.x * d.x + d.z * d.z));
	int slow = 0;
	if (Entity_IsType(cam, FCAM_CLASSNAME)) {
		cEntity *ship = cCamEntity_GetTarget(&cam->base);
		if (ship) {
			const vec3_t *v = Obj_GetVel(ship, &tmp);
			slow = g_VelReposStopAI > (float)__builtin_sqrt((double)(v->x * v->x + v->y * v->y + v->z * v->z));
		}
	}
	if (slow) {
		self->maxDistRepos = g_MaxDistReposStopAI;
		self->minDistRepos = g_MinDistReposStopAI;
	} else {
		self->maxDistRepos = g_MaxDistReposAI;
		self->minDistRepos = g_MinDistReposAI;
	}
	self->maxDistReposNotVis = g_MaxDistReposNotVisAI;
	self->minDistReposNotVis = g_MinDistReposNotVisAI;
	vec3_t z = { 0.0f, 0.0f, 1.0f };
	Vec3_Rotate(&dir, &z, &cam->cam.rot);
	double ang = Vec3_Angle(&d, &dir);
	if (ang > (double)(cam->cam.fov * AIV_REPOS_HALF * AIV_REPOS_FOV) && !self->base.trafficPlaced) {
		if (!(dist > self->maxDistReposNotVis)) {
			if (!(dist > self->minDistReposNotVis) || AI_IsVisible(self)) {
				return;
			}
		}
	} else {
		if (!(dist > self->maxDistRepos)) {
			return;
		}
		self->base.trafficPlaced = 0;
	}
	CNodesTraffic *t = AIV_Outdoor();
	self->reposBehind = 0;
	Traffic_RemoveVehicle(t, self->track);
	if (!cAIVehicleGroup_ReposReady(g_VehicleGroup) || !cAIVehicle_ReposTraffic(self, &p, self->minDistRepos,
			self->maxDistRepos, self->minDistReposNotVis, self->maxDistReposNotVis, g_MaxDistAroundReposAI)) {
		self->reposBehind = 0;
		Entity_SetActive(self->vehicle, 0);
		cAIVehicle_SetInternState(self, AIVI_STOPPED);
		return;
	}
	AIV_GraphTrafficRepos(self, &p);
	AIV_StartTrack(self, 0, &p);
}

/* TRAFFIC_INDOOR: a stopped vehicle onto a track of a sector in view; on the track (i34: 0 not seen, 1 seen, 2 just
 * placed) off the track once it was out of view for a second */
static void cAIVehicle_GraphTrafficInDoor(cAIVehicle *self) {
	int in = self->internState;

	if (in == AIVI_STOPPED) {
		int type, track, node;
		vec3_t p;
		if (CNodesTrafficInDoor_GetRespawnPoint((CNodesTrafficInDoor *)g_NodesTrafficInDoor, self, &type, &p, &track,
				&node)) {
			cAIVehicle_PlaceOnTrack(self, type, &p, track, node);
		}
		return;
	}
	if (in != AIVI_FINISHING_LINE_DIRECT) {
		return;
	}
	switch (self->base.base.seen) {
	case 0:
		if (AI_IsVisible(self)) {
			self->base.base.seen = 1;
			return;
		}
		if (self->base.base.waitTime > 1.0f) {
			CNodesTraffic *t = AIV_Indoor();
			Traffic_RemoveVehicle(t, self->track);
			cAIVehicle_SetInternState(self, AIVI_STOPPED);
			Entity_SetActive(self->vehicle, 0);
		}
		return;
	case 1:
		if (!AI_IsVisible(self)) {
			self->base.base.seen = 0;
		}
		return;
	case 2:
		if (AI_IsVisible(self)) {
			self->base.base.seen = 1;
			return;
		}
		if (!CNodesTrafficInDoor_SectorHasTrack((CNodesTrafficInDoor *)g_NodesTrafficInDoor, self->track,
				self->trackType)) {
			self->base.base.seen = 0;
		}
		return;
	}
}

void cAIVehicle_WeapListChanged(void *ai) {
	cAIVehicleShoot_WeapListChanged(((cAIVehicle *)ai)->shoot);
}

/* the vtable of cAIVehicle */
static const cAIMoveVtbl cAIVehicle_vftable = {
	.base.Delete = cAIVehicle_Delete,
	.base.Update = cAIVehicle_Update,
	.base.UpdateOff = cAIVehicle_UpdateOff,
	.base.Draw = cAIVehicle_Draw,
	.base.GetChanged = cAI_GetChanged,
	.base.IsVisible = cAI_IsVisible,
	.base.Debugf = cAIVehicle_Debugf,
	.base.VarI = cAIVehicle_VarI,
	.base.VarF = cAIVehicle_VarF,
	.base.VarS = cAIVehicle_VarS,
	.base.VarV = cAIVehicle_VarV,
	.base.Print = cAI_Print,
	.base.GetEventName = cAI_GetEventName,
	.base.OnEvent = cAI_OnEvent,
	.GetTarget = cAIMove_GetTarget,
	.CanJump = cAIMove_CanJump,
	.GetStopped = cAIMove_GetStopped,
	.IsPaused = cAIMove_IsPaused,
	.GetTargetEnt = cAIMove_GetTargetEnt,
	.GetArrived = cAIMove_GetArrived,
	.RandomPoint = cAIMove_RandomPoint,
	.CheckStopped = cAIMove_CheckStopped,
	.Event = cAIVehicle_Event,
	.IsMoving = cAIVehicle_IsMoving,
	.CanTarget = cAIMove_CanTarget,
};

static cGVar GVar_MaxDistReposNotVisAI;
static cGVar GVar_MinDistReposNotVisAI;
static cGVarFunction GVar_OnObjectiveNullAI;
cGVar GVar_MaxDistChaseAI;
cGVar GVar_MinDistReposAI;
cGVar GVar_MinDistChaseAI;
static cGVar GVar_TimeDamageAI;
static cGVarFunction GVar_OnItemTakenAI;
cGVar GVar_HookRange;

/* the dynamic initializers of the globals here, run in the order of src/standalone/crtxc.inc */
INIT_GVAR_INT(Init_GVar_DepthPathRouteOutDoorAI, &GVar_DepthPathRouteOutDoorAI, "DepthPathRouteOutDoorAI", 4, "Depth Path in Route.\n", 6)
INIT_GVAR_FLOAT(Init_GVar_MaxDistReposNotVisAI, &GVar_MaxDistReposNotVisAI, "MaxDistReposNotVisAI", 4, "Distance max. to reposition the agent not visible.\n", 75000.0f)
INIT_GVAR_FLOAT(Init_GVar_MinDistReposNotVisAI, &GVar_MinDistReposNotVisAI, "MinDistReposNotVisAI", 4, "Distance min. to reposition the agent not visible.\n", 37500.0f)
INIT_GVAR_FLOAT(Init_GVar_MaxDistReposStopAI, &GVar_MaxDistReposStopAI, "MaxDistReposStopAI", 4, "Distance max. to reposition the agent when camera stopped.\n", 150000.0f)
INIT_GVAR_FLOAT(Init_GVar_MinDistReposStopAI, &GVar_MinDistReposStopAI, "MinDistReposStopAI", 4, "Distance min. to reposition the agent when camera stopped.\n", 125000.0f)
INIT_GVAR_FLOAT(Init_GVar_MinDistReposAI, &GVar_MinDistReposAI, "MinDistReposAI", 4, "Distance min. to reposition the agent.\n", 75000.0f)
INIT_GVAR_FLOAT(Init_GVar_DistNotGetAwayAI, &GVar_DistNotGetAwayAI, "DistNotGetAwayAI", 4, "Distance not get away from enemy.", 45000.0f)
INIT_GVAR_FLOAT(Init_GVar_DistGetAwayBoostAI, &GVar_DistGetAwayBoostAI, "DistGetAwayBoostAI", 4, "Distance min. get away with boost.", 5000.0f)
INIT_GVAR_FLOAT(Init_GVar_TimeDamageAI, &GVar_TimeDamageAI, "TimeDamageAI", 4, "Time in sec. control damage.", 3.0f)
INIT_GVAR_FLOAT(Init_GVar_MaxDistElevatorAI, &GVar_MaxDistElevatorAI, "MaxDistElevatorAI", 4, "Distance Max. to detect elevator near.\n", 10000.0f)
INIT_GVAR_FLOAT_DATA(Init_GVar_MinDistChaseElevatorAI, &GVar_MinDistChaseElevatorAI, "MinDistChaseElevatorAI", 4, "Distance Min. to objective Chase AI inside hangar.\n", &GVar_MaxDistElevatorAI.value.f)
INIT_GVAR_FLOAT_ADD(Init_GVar_MaxDistChaseElevatorAI, &GVar_MaxDistChaseElevatorAI, "MaxDistChaseElevatorAI", 4, "Distance Max. to objective Chase AI inside hangar.\n", &GVar_MinDistChaseElevatorAI.value.f, &(const float){ 10000.0f })
INIT_GVAR_FLOAT(Init_GVar_MinDistChaseAI, &GVar_MinDistChaseAI, "MinDistChaseAI", 4, "Distance Min. to objective Chase AI.\n", 5000.0f)
INIT_GVAR_FLOAT(Init_GVar_MaxDistChaseAI, &GVar_MaxDistChaseAI, "MaxDistChaseAI", 4, "Distance Max. to objective Chase AI.\n", 15000.0f)
INIT_GVAR_FLOAT(Init_GVar_MaxDistAroundReposAI, &GVar_MaxDistAroundReposAI, "MaxDistAroundReposAI", 4, "Distance max. to reposition the agent around point of reposition.\n", 20000.0f)
INIT_GVAR_FLOAT(Init_GVar_RADIUS_OBJECTIVE_TRAFFIC, &GVar_RADIUS_OBJECTIVE_TRAFFIC, "RADIUS_OBJECTIVE_TRAFFIC", 4, "Radius Points Traffic.\n", 4000.0f)
INIT_GVAR_FLOAT(Init_GVar_RADIUS_OBJECTIVE_TRAFFIC_INDOOR, &GVar_RADIUS_OBJECTIVE_TRAFFIC_INDOOR, "RADIUS_OBJECTIVE_TRAFFIC_INDOOR", 4, "Radius Points Traffic InDoor.\n", 4000.0f)
INIT_GVAR_FLOAT(Init_GVar_VelReposBackAI, &GVar_VelReposBackAI, "VelReposBackAI", 4, "Vel. to repos agent back to camera.\n", 3000.0f)
INIT_GVAR_FLOAT(Init_GVar_VelReposStopAI, &GVar_VelReposStopAI, "VelReposStopAI", 4, "Vel. to repos agent back to camera.\n", 7000.0f)
INIT_GVAR_FLOAT(Init_GVar_VelTrafficLightRotAI, &GVar_VelTrafficLightRotAI, "VelTrafficLightRotAI", 4, "Vel. traffic light to rotate.\n", 5556.0f)
FINI_CALL(Fini_GVar_OnItemTakenAI, &GVar_OnItemTakenAI.func, cPy_Function_dtor)
INIT_GVAR_FUNCTION(Init_GVar_OnItemTakenAI, Fini_GVar_OnItemTakenAI, &GVar_OnItemTakenAI, "OnItemTakenAI", 0, "Execute function in Python to take item", "")
FINI_CALL(Fini_GVar_OnObjectiveNullAI, &GVar_OnObjectiveNullAI.func, cPy_Function_dtor)
INIT_GVAR_FUNCTION(Init_GVar_OnObjectiveNullAI, Fini_GVar_OnObjectiveNullAI, &GVar_OnObjectiveNullAI, "OnObjectiveNullAI", 0, "Execute function in Python if objective is null", "")
INIT_GVAR_FLOAT(Init_GVar_HookRange, &GVar_HookRange, "HookRange", 0, "The maximun range that have the Hook", 20000.0f)
