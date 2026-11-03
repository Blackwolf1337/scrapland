/*
 * Logic/ExtPolice.cpp of the game: the police cars of the outdoor maps ("OutCop", include/scrap/logic.h). Every
 * 0.5 s: when the alarm goes on the cars go for the player (attack in its car, chase on foot), when it goes off
 * they relax; a car that sees the player while it attacks it keeps the alarm up; while the cars are out and the
 * alarm is off, cars far from every camera and not drawn are removed. The siren sound of the "Poli" cars plays
 * while they attack. A hit of a cop by the player sets the alarm, by another ship sends the cop after it.
 */
#include <string.h>

#include "scrap/gvar.h"
#include "scrap/chartype.h"
#include "scrap/objentity.h"
#include "scrap/scrapcollision.h"
#include "scrap/scrapmodel.h"
#include "scrap/vehicleentity.h"
#include "scrap/weapons.h"
#include "scrap/staticinit.h"

#define g_ExtPoliceChasing (GVar_DoResetOutPolice.value.i)		/* the cars were sent after the player */
#define g_PoliceWaveEliminated (((cPy_Function *)&GVar_PoliceWaveEliminated.func))	/* no police car left */
static cGVarFunction GVar_PoliceWaveEliminated;
static cGVar GVar_DoResetOutPolice;

/* defined further down or in another file without a header */
static void ExtPolice_OnDamage(cAbstractFile *params);
static void ExtPolice_OnDeath(cAbstractFile *params);

cGVar GVar_MaxDistReposAI;

#define g_MaxDistReposAI (GVar_MaxDistReposAI.value.f)

cExtPolice *cExtPolice_ctor(cExtPolice *self) {
	self->list = NULL;
	g_ExtPoliceChasing = 0;
	self->alarm = 0;
	self->usr = NULL;
	self->timer = 0.0f;
	self->retarget = 5.0f;
	self->raised = -1;
	cPy_Function_SetFunc(g_PoliceWaveEliminated, str_Empty);
	return self;
}

/* the nearest cop within TrafficFightingMaxDist of e that the line test finds goes for e */
cEntity *ExtPolice_CallCop(cEntity *e) {
	vec3_t p, t1, t2;
	cEntity *best = NULL;

	cEntityListData *list = (cEntityListData *)cWorld_GetList(g_pWorld, "OutCop");
	if (!list) {
		return NULL;
	}
	Entity_GetPos(e, &p);
	float bestD = 3.4028234663852886e+38f;
	float maxD = g_TrafficFightingMaxDist * g_TrafficFightingMaxDist;
	for (Nto1Node *n = Logic_ListFirst(list); n; n = Logic_ListNext(n)) {
		cEntity *c = n->data;
		CHECK(Entity_IsType(c, TYEN_CAR) && "wle->IsType(TYEN_VEHICLE)");
		const vec3_t *cp = Entity_GetPos(c, &t1);
		float dy = p.y - cp->y, dx = p.x - cp->x, dz = p.z - cp->z;
		float d = dy * dy + dx * dx + dz * dz;
		if (maxD > d && bestD > d && Engine_TestLine(Entity_GetPos(c, &t2), &p, NULL, NULL)) {
			bestD = d;
			best = c;
		}
	}
	if (best) {
		if (!best->active) {
			Entity_SetActive(best, 1);
		}
		cAIVehicle *ai = Entity_GetAI(best);
		PoliceAI_RandomParams(ai);
		cAIVehicle_SetState(ai, AIV_STATE_ATTACK, e);
	}
	return best;
}

/* the cop name hit (died: killed) by attacker */
static void ExtPolice_CopDamage(const char *name, const char *attacker, byte died) {
	cEntity *cop = World_FindEntity(name);

	if (!cop || !Entity_IsType(cop, TYEN_CAR)) {
		Con_Warnf("WARNING : CopDamage() '%s' invalid\n", name);
		return;
	}
	cAIVehicle *ai = Entity_GetAI(cop);
	if (!ai) {
		Con_Warnf("WARNING : CopDamage() '%s' AI invalid\n", name);
		return;
	}
	cEntity *a = World_FindEntity(attacker);
	if (!a) {
		return;
	}
	if (a == cWorld_GetUsrEntity(g_pWorld, 0)) {
		Alarm_Set(&g_pWorld->alarm, 1.0f);
		if (died && strstr(((cVehicleEntity *)cop)->typeName, "Poli")) {
			SaveVar_Inc("Stats.OutPolice", 1);
		}
		return;
	}
	if (!g_pWorld->alarm.on) {
		if (cAIVehicle_GetState(ai) != 0) {
			return;
		}
		if (!cop->active) {
			Entity_SetActive(cop, 1);
		}
		cAIVehicle_SetState(ai, AIV_STATE_ATTACK, a);
		return;
	}
	if (!cEntity_GetListName(a) || strcmp(cEntity_GetListName(a), "Friends")) {
		return;
	}
	if (cAIVehicle_GetState(ai) == AIV_STATE_ATTACK && AIMove_GetTargetEnt(ai) == cWorld_GetUsrEntity(g_pWorld,
		0)) {
		cAIVehicle_SetState(ai, AIV_STATE_ATTACK, a);
	}
}

/* the cars after the player: attack in its car, else chase */
static void ExtPolice_Hunt(cAIVehicle *ai, cEntity *usr) {
	if (Entity_IsType(usr, TYEN_CAR)) {
		cAIVehicle_SetState(ai, AIV_STATE_ATTACK, usr);
		return;
	}
	cAIVehicle_SetState(ai, AIV_STATE_CHASE, usr, (double)g_AIChaseDefault1, (double)g_AIChaseDefault2);
}

static void ExtPolice_Relax(cEntity *e, cAIVehicle *ai) {
	cAIVehicle_SetState(ai, 3, WLife(e)->arrow.canMission == 0, 5.0, 2.0);
}

/* the police taunt when the alarm was on */
static void ExtPolice_Taunt(cExtPolice *self) {
	if (self->alarm) {
		int r = crt_rand();
		Taunts_Play(str_Empty, *g_PoliceTauntRace, r % 3 + 10);
	}
}

static void cExtPolice_Think(cExtPolice *self) {
	vec3_t tmp;
	Nto1Node *n = Logic_ListFirst(self->list);
	byte alarm = g_pWorld->alarm.on;

	if (alarm && !self->alarm && (byte)Talk_IsShown()) {
		alarm = 0;
		Alarm_Set(&g_pWorld->alarm, 1.0f);
		self->raised = 1;
	} else {
		self->raised = -1;
	}
	cEntity *usr = ChType_Usr0();
	if (!usr) {
		return;
	}
	self->retarget = self->retarget - 0.5f;
	while (n) {
		cEntity *e = n->data;
		CHECK(Entity_IsType(e, TYEN_CAR) && "wle->IsType(TYEN_VEHICLE)");
		cAIVehicle *ai = Entity_GetAI(e);
		CHECK(ai && "ai");
		n = Logic_ListNext(n);
		if (g_ExtPoliceChasing && !alarm) {
			float d = cWorld_GetMinCamDistSq(g_pWorld, Entity_GetPos(e, &tmp));
			if (d > g_MaxDistReposAI * g_MaxDistReposAI * 9.0f * 0.0625f &&
				!CGameModel_IsVisible(Obj_GetModel(e))) {
				cEntity_Del(e);
				continue;
			}
		} else if (alarm) {
			if (cAIVehicle_ControlFlag(ai) && AIMove_GetTargetEnt(ai) == usr) {
				Alarm_Set(&g_pWorld->alarm, 1.0f);
				self->raised = 1;
			}
		}
		if (strstr(((cVehicleEntity *)e)->typeName, "Poli")) {
			cSndItem *siren = PtrArray_Get(&Veh(e)->wl.obj.snd, 0x1f);
			if (siren) {
				if (!alarm && cAIVehicle_GetState(ai) != AIV_STATE_ATTACK) {
					siren->volume = 0.0f;
					siren->pitch = 0.0f;
				} else {
					siren->volume = siren->vols[0];
					siren->pitch = siren->pitches[0];
				}
			}
		}
		if (alarm != self->alarm) {
			if (alarm) {
				if (cAIVehicle_GetState(ai) == AIV_STATE_ATTACK) {
					PoliceAI_RestoreParams(ai);
				}
				WLife(e)->arrow.tgType = 0;
			} else {
				WLife(e)->arrow.tgType = 3;
				ExtPolice_Relax(e, ai);
				g_ExtPoliceChasing = 1;
				ExtPolice_Taunt(self);
				continue;
			}
		} else if (usr != self->usr) {
			if (!alarm) {
				ExtPolice_Relax(e, ai);
				ExtPolice_Taunt(self);
				continue;
			}
		} else {
			if (!alarm) {
				if (cAIVehicle_GetState(ai) == 0) {
					PoliceAI_RestoreParams(ai);
					ExtPolice_Relax(e, ai);
				}
			} else if (0.0f >= self->retarget) {
				int st = cAIVehicle_GetState(ai);
				if ((st != AIV_STATE_ATTACK && cAIVehicle_GetState(ai) != AIV_STATE_CHASE) ||
					AIMove_GetTargetEnt(ai) != usr) {
					ExtPolice_Hunt(ai, usr);
				}
			}
			continue;
		}
		if (!e->active) {
			Entity_SetActive(e, 1);
		}
		ExtPolice_Hunt(ai, usr);
	}
	self->alarm = alarm;
	self->usr = usr;
	int *num = self->list->numElems;
	if (num && *num == 0) {
		cPy_Function_Execute(g_PoliceWaveEliminated, 1);
		self->alarm = 0;
		self->usr = NULL;
		g_ExtPoliceChasing = 0;
	}
	if (0.0f >= self->retarget) {
		self->retarget = 5.0f;
	}
}

void cExtPolice_Init(cExtPolice *self) {
	self->list = cWorld_NewEntityList(g_pWorld, "OutCop");
	cPy_Function_SetNative(&self->list->onDamage, (nativefunc_t)ExtPolice_OnDamage);
	cPy_Function_SetNative(&self->list->onDeath, (nativefunc_t)ExtPolice_OnDeath);
}

/* a cop attacks or chases e */
byte ExtPolice_IsChased(cEntity *e) {
	cEntityListData *list = (cEntityListData *)cWorld_GetList(g_pWorld, "OutCop");

	if (!list) {
		return 0;
	}
	for (Nto1Node *n = Logic_ListFirst(list); n; n = Logic_ListNext(n)) {
		cEntity *c = n->data;
		if (!c || !Entity_IsType(c, TYEN_CAR)) {
			continue;
		}
		cAIVehicle *ai = Entity_GetAI(c);
		if (ai && AIMove_GetTargetEnt(ai) == e) {
			return 1;
		}
	}
	return 0;
}

/* the natives of the list: a cop hit, a cop killed */
static void ExtPolice_ReadCopDamage(cAbstractFile *params, byte died) {
	char name[0x80], attacker[0x80];

	cAbstractFile_ReadString(params, name, 0x80);
	cAbstractFile_ReadString(params, attacker, 0x80);
	ExtPolice_CopDamage(name, attacker, died);
}

static void ExtPolice_OnDamage(cAbstractFile *params) {
	ExtPolice_ReadCopDamage(params, 0);
}

static void ExtPolice_OnDeath(cAbstractFile *params) {
	ExtPolice_ReadCopDamage(params, 1);
}

void cExtPolice_Update(cExtPolice *self, float dt) {
	if (!g_IsMapOutDoor) {
		return;
	}
	float t = self->timer + dt;
	self->timer = t;
	if (!(0.5f >= t)) {
		self->timer = 0.0f;
		cExtPolice_Think(self);
	}
}

static cGVarFunction GVar_PoliceWaveEliminated;
static cGVar GVar_DoResetOutPolice;
cGVar GVar_TrafficFightingMaxDist;

/* the dynamic initializers of the globals here, run in the order of src/standalone/crtxc.inc */
INIT_GVAR_FLOAT(Init_GVar_MaxDistReposAI, &GVar_MaxDistReposAI, "MaxDistReposAI", 4, "Distance max. to reposition the agent.\n", 100000.0f)
FINI_CALL(Fini_GVar_PoliceWaveEliminated, &GVar_PoliceWaveEliminated.func, cPy_Function_dtor)
INIT_GVAR_FUNCTION(Init_GVar_PoliceWaveEliminated, Fini_GVar_PoliceWaveEliminated, &GVar_PoliceWaveEliminated, "PoliceWaveEliminated", 0, "Called if no police active in game.", "")
INIT_GVAR_INT(Init_GVar_DoResetOutPolice, &GVar_DoResetOutPolice, "DoResetOutPolice", 0, "if true, the police will be reset.", 0)
INIT_GVAR_FLOAT(Init_GVar_TrafficFightingMaxDist, &GVar_TrafficFightingMaxDist, "TrafficFightingMaxDist", 0, "Traffic fighting max dist", 50000.0f)
