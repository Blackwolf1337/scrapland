/*
 * Logic/MayorLogic.cpp of the game: cMayorLocalLogic (include/scrap/logic.h), the mayor (a GenChar logic). Its
 * special attack goes for the nearest dazed character in its view within MayorBlabliblaRadius; the character that
 * talks to it (the listener) makes its AI listen (state 9) during the special action.
 */
#include <string.h>

#include "scrap/chartype.h"
#include "scrap/players.h"

#define CMAYORLOCALLOGIC_VFTABLE (&cMayorLocalLogic_vftable.base)
static const cGenCharLocalLogicVtbl cMayorLocalLogic_vftable;

/* defined further down or in another file without a header */
static void Mayor_SphereFunc(cAbstractFile *params);

#define MAYOR_VT(self) ((self)->base.base.vftable)

void *cMayorLocalLogic_ctor(void *mem, cEntity *parent) {
	cMayorLocalLogic *self = mem;

	cGenCharLocalLogic_ctor(&self->base, parent, 0);
	MAYOR_VT(self) = CMAYORLOCALLOGIC_VFTABLE;
	self->listener[0] = 0;
	return self;
}

static void cMayorLocalLogic_Free(cLocalLogic *elem) {
	cMayorLocalLogic *self = (cMayorLocalLogic *)elem;
	MAYOR_VT(self) = CMAYORLOCALLOGIC_VFTABLE;
	cGenCharLocalLogic_dtor(&self->base);
	operator_delete(self);
}

/* the special action is over after GenCharSpecialActionTime */
static byte cMayorLocalLogic_TargetDone(cLocalLogic *elem) {
	cMayorLocalLogic *self = (cMayorLocalLogic *)elem;
	return self->base.specialTime > g_GenCharSpecialActionTime;
}

static cEntity *g_MayorTarget;
/* the search of the sphere native: the mayor, the nearest dazed character in its view and the square distance */
static cEntity *g_MayorSearchEnt;
static float g_MayorTargetDist = 3.4028235e+38f;

/* at the search time the nearest dazed character in view in the sphere of MayorBlabliblaRadius */
static cEntity *cMayorLocalLogic_FindTarget(cLocalLogic *elem) {
	cMayorLocalLogic *self = (cMayorLocalLogic *)elem;
	vec3_t t;

	if (!(0.0f > self->base.searchTime)) {
		return NULL;
	}
	g_MayorTargetDist = 3.4028235e38f;
	g_MayorTarget = NULL;
	g_MayorSearchEnt = self->base.base.parent;
	cPy_Function_SetNative(&g_ChTypeSphereFunc, (nativefunc_t)Mayor_SphereFunc);
	float radius = g_MayorBlabliblaRadius;
	cEntity *m = g_MayorSearchEnt;
	cWorld_SphereCallF(g_pWorld, 0x10, Entity_GetPos(m, &t), radius, &g_ChTypeSphereFunc, 1);
	return g_MayorTarget;
}

/* and: the AI walks in alert or relaxed */
static void cMayorLocalLogic_ToAlert(cLocalLogic *base, cAIChar *ai) {
	cMayorLocalLogic *self = (cMayorLocalLogic *)base;
	(void)self;
	AIChar_SetState(ai, AIC_IN_ROUTE, (double)g_GenCharAlertSpeed, 0);
}

static void cMayorLocalLogic_ToRelax(cLocalLogic *base, cAIChar *ai) {
	cMayorLocalLogic *self = (cMayorLocalLogic *)base;
	(void)self;
	AIChar_SetState(ai, AIC_IN_ROUTE, (double)g_GenCharRelaxSpeed, 0);
}

/* during the special action the AI listens to the listener once */
static void cMayorLocalLogic_Update(cLocalLogic *elem, float dt) {
	cMayorLocalLogic *self = (cMayorLocalLogic *)elem;
	if (self->base.specialOn) {
		self->base.specialTime = dt + self->base.specialTime;
		if (self->listener[0]) {
			cEntity *p = self->base.base.parent;
			cAIChar *ai = Entity_GetAI(p);
			cEntity *e = World_FindEntity(self->listener);
			if (e) {
				AIChar_SetState(ai, AIC_ORIENT_DYNAMIC_OBJECTIVE_WITH_ACTION, e, 0);
			}
			self->listener[0] = 0;
		}
	}
	cGenCharLocalLogic_Update(&self->base.base.logic, dt);
}

/* the C function of the sphere: a dazed character (not the player, not the mayor) in the view of the mayor, the
 * nearest is kept */
static void Mayor_SphereFunc(cAbstractFile *params) {
	char name[0x80];
	vec3_t pos, t;

	cAbstractFile_ReadString(params, name, sizeof(name));
	cEntity *e = World_FindEntity(name);
	if (!e) {
		return;
	}
	cEntity *usr = Players_Nearest(&e->pos);
	if (!usr || usr == e || g_MayorSearchEnt == e) {
		return;
	}
	Entity_GetPos(e, &pos);
	if (!WithLife_GetAction(e, "Dazed") ||
		!Logic_InView(g_MayorSearchEnt, &pos, g_GenCharStdFar, g_GenCharStdFovY, g_GenCharStdFovX, 1)) {
		return;
	}
	cEntity *m = g_MayorSearchEnt;
	const vec3_t *mp = Entity_GetPos(m, &t);
	float dy = pos.y - mp->y, dx = pos.x - mp->x, dz = pos.z - mp->z;
	float d = dy * dy + dx * dx + dz * dz;
	if (g_MayorTargetDist > d) {
		g_MayorTargetDist = d;
		g_MayorTarget = e;
	}
}

void cMayorLocalLogic_SetListener(cLocalLogic *l, cEntity *e) {
	cMayorLocalLogic *self = (cMayorLocalLogic *)l;

	if (e) {
		Str_Copy(self->listener, e->name, sizeof(self->listener));
		return;
	}
	self->listener[0] = 0;
}

/* the vtable of cMayorLocalLogic */
static const cGenCharLocalLogicVtbl cMayorLocalLogic_vftable = {
	.base.base.Free = cMayorLocalLogic_Free,
	.base.base.GetName = cBaseLocalLogic_GetName,
	.base.base.AcceptsArrow = cLocalLogic_AcceptsArrowTrue,
	.base.base.VarI = cGenCharLocalLogic_VarI,
	.base.base.VarF = cBaseLocalLogic_VarF,
	.base.base.VarS = cGenCharLocalLogic_VarS,
	.base.base.VarV = cBaseLocalLogic_VarV,
	.base.UpdateBusy = cBaseLocalLogic_UpdateBusy,
	.base.Refresh = cBaseLocalLogic_Refresh,
	.base.Listens = cLocalLogic_ListensTrue,
	.base.OnHit = cGenCharLocalLogic_OnHit,
	.base.OnSeen = cGenCharLocalLogic_OnSeen,
	.base.OnAttack = cGenCharLocalLogic_OnRobbery,
	.base.OnShot = cBaseLocalLogic_OnShot,
	.base.OnHammer = cLocalLogic_OnHammerNop,
	.base.GivesMoney = cGenCharLocalLogic_GivesMoney,
	.base.OnAlarm = cGenCharLocalLogic_OnAlarm,
	.base.Update = cMayorLocalLogic_Update,
	.base.SetOriginFrom = cBaseLocalLogic_SetOriginFrom,
	.base.SetState = cGenCharLocalLogic_SetState,
	.base.SetIdle = cBaseLocalLogic_SetIdle,
	.base.SetActionRadius = cBaseLocalLogic_SetActionRadius,
	.base.SetOrigin = cBaseLocalLogic_SetOrigin,
	.base.SetOriginAng = cBaseLocalLogic_SetOriginAng,
	.base.RestartState = cGenCharLocalLogic_RestartState,
	.CanAttack = cGenCharLocalLogic_CanAttack,
	.UpdateView = cGenCharLocalLogic_UpdateView,
	.ToRelax = cMayorLocalLogic_ToRelax,
	.ToAlert = cMayorLocalLogic_ToAlert,
	.Accuse = Logic_SetState2,
	.FindTarget = cMayorLocalLogic_FindTarget,
	.TargetDone = cMayorLocalLogic_TargetDone,
	.Work = cLocalLogic_WorkNop,
	.BreakWork = cLocalLogic_BreakWorkNop,
	.GoTo = cGenCharLocalLogic_GoTo,
	.UpdateGetIt = cGenCharLocalLogic_UpdateGetIt,
};
