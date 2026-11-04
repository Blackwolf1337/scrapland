/*
 * Logic/BossLogic.cpp of the game: the boss walks (state 0 or 6) for BossWalkingTime, goes to the dummy
 * BossWorkPlace (state 7), moves onto it when the place is free and works there (state 8) for BossWorkingTime
 * (include/scrap/logic.h).
 */
#include "scrap/chartype.h"
#include "scrap/modmove.h"
#include "scrap/objentity.h"
#include "scrap/staticinit.h"

/* the states the boss adds to the GENCHAR_ ones */
enum {
	BOSS_GO_TO_WORK = 7, BOSS_WORK = 8
};

#define CBOSSLOCALLOGIC_VFTABLE (&cBossLocalLogic_vftable.base)
#define g_BossWalkingTime (GVar_BossWalkingTime.value.f)
#define g_BossWorkPlace (GVar_BossWorkPlace.value.s)
#define g_BossWorkingTime (GVar_BossWorkingTime.value.f)
static const cGenCharLocalLogicVtbl cBossLocalLogic_vftable;
static cGVar GVar_BossWorkingTime;
static cGVar GVar_BossWorkPlace;
static cGVar GVar_BossWalkingTime;

cBossLocalLogic *cBossLocalLogic_ctor(cBossLocalLogic *self, cEntity *parent) {
	char name[0x80];
	byte flag;

	cGenCharLocalLogic_ctor(&self->base, parent, 0);
	self->base.base.vftable = CBOSSLOCALLOGIC_VFTABLE;
	self->time = 0.0f;
	DummySearch_Start(g_BossWorkPlace, 0);
	byte found = DummySearch_Next(name, 0x80, &self->workPos, self->workAng, &flag);
	CHECK(found && "No existe el dummy de lugar de trabajo del Boss");
	return self;
}

static void cBossLocalLogic_Free(cLocalLogic *elem) {
	cBossLocalLogic *self = (cBossLocalLogic *)elem;
	cGenCharLocalLogic_dtor(&self->base);
	operator_delete(self);
}

/* vf33 (0x84): at work, the end of the work */
static void cBossLocalLogic_BreakWork(cLocalLogic *elem, byte on) {
	cBossLocalLogic *self = (cBossLocalLogic *)elem;
	if (self->base.base.state != BOSS_WORK || !on) {
		return;
	}
	cEntity *p = self->base.base.parent;
	if (p && Obj(p)->move.active) {
		cObjMove_SetTime(&Obj(p)->move, 0.0f);
	}
	Logic_SetState(self, GENCHAR_RELAX, NULL);
}

static byte g_BossWasIdle;

/* vf17 */
static void cBossLocalLogic_Update(cLocalLogic *elem, float dt) {
	cBossLocalLogic *self = (cBossLocalLogic *)elem;
	cGenCharLocalLogic_Update(&self->base.base.logic, dt);
	int st = self->base.base.state;
	if (st == GENCHAR_RELAX || st == GENCHAR_STAY) {
		self->time = dt + self->time;
		if (self->time >= g_BossWalkingTime) {
			Logic_SetState(self, BOSS_GO_TO_WORK, NULL);
		}
		return;
	}
	if (st == BOSS_GO_TO_WORK) {
		cEntity *p = self->base.base.parent;
		if (!p) {
			return;
		}
		void *ai = Entity_GetAI(p);
		if (!ai || AIChar_GetState(ai) != AIC_STATIC || !AIMove_GetArrived(ai)) {
			return;
		}
		cObjMove *m = &Obj(p)->move;
		m->moveAng.x = 0.0f;
		m->moveAng.y = self->workAng[1];
		m->moveAng.z = 0.0f;
		vec3_t pos = self->workPos;
		if (cWorld_TestSphereEntities(g_pWorld, p, 0x10, &pos, WLife(p)->prim.radius, NULL, 1, 0)) {
			return;
		}
		m->movePos = self->workPos;
		cObjMove_SetTime(m, 0.5f);
		m->fade = 3;
		Logic_SetState(self, BOSS_WORK, NULL);
		return;
	}
	if (st != BOSS_WORK) {
		return;
	}
	cEntity *p = self->base.base.parent;
	if (!self->base.base.idle && !self->base.base.alwaysLock) {
		if (g_BossWasIdle) {
			cObjMove_SetTime(&Obj(p)->move, 0.5f);
			g_BossWasIdle = 0;
		}
	} else {
		g_BossWasIdle = 1;
	}
	self->time = dt + self->time;
	if (self->time >= g_BossWorkingTime) {
		self->time = 0.0f;
		void *ctrl = ChType_ActCtrl(p);
		cActCtrl_Play(ctrl, "WorkEnd", 1, 1.0f);
		cActCtrl_SetDefLoop(ctrl, "Walk", 1);
		Logic_SetState(self, GENCHAR_RELAX, NULL);
	}
}

/* vf19: 7 the way to the work place (AI state 4 with the place and the direction of its angle), 8 the work */
static byte cBossLocalLogic_SetState(cLocalLogic *elem, int state, cEntity *target) {
	cBossLocalLogic *self = (cBossLocalLogic *)elem;
	vec3_t out;

	if (state == BOSS_GO_TO_WORK) {
		if (cBaseLocalLogic_SetState(&self->base.base.logic, BOSS_GO_TO_WORK, target)) {
			cEntity *p = self->base.base.parent;
			cAIChar *ai = Entity_GetAI(p);
			vec3_t rot = { 0.0f, self->workAng[1], 0.0f };
			vec3_t dir = { 0.0f, 0.0f, -1.0f };
			Vec3_Rotate(&out, &dir, &rot);
			AIChar_SetState(ai, AIC_GO_TO, self->workPos, out, 100.0, (double)g_GenCharRelaxSpeed);
		}
		return 1;
	}
	if (state == BOSS_WORK) {
		if (cBaseLocalLogic_SetState(&self->base.base.logic, BOSS_WORK, target)) {
			self->time = 0.0f;
			void *ctrl = ChType_ActCtrl(self->base.base.parent);
			cActCtrl_Play(ctrl, "WorkInit", 1, 1.0f);
			cActCtrl_SetDefLoop(ctrl, "Work", 1);
		}
		return 1;
	}
	cEntity *p = self->base.base.parent;
	if (p && Obj(p)->move.active) {
		cObjMove_SetTime(&Obj(p)->move, 0.0f);
	}
	return cGenCharLocalLogic_SetState(&self->base.base.logic, state, target);
}

/* the vtable of cBossLocalLogic */
static const cGenCharLocalLogicVtbl cBossLocalLogic_vftable = {
	.base.base.Free = cBossLocalLogic_Free,
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
	.base.Update = cBossLocalLogic_Update,
	.base.SetOriginFrom = cBaseLocalLogic_SetOriginFrom,
	.base.SetState = cBossLocalLogic_SetState,
	.base.SetIdle = cBaseLocalLogic_SetIdle,
	.base.SetActionRadius = cBaseLocalLogic_SetActionRadius,
	.base.SetOrigin = cBaseLocalLogic_SetOrigin,
	.base.SetOriginAng = cBaseLocalLogic_SetOriginAng,
	.base.RestartState = cGenCharLocalLogic_RestartState,
	.CanAttack = cGenCharLocalLogic_CanAttack,
	.UpdateView = cGenCharLocalLogic_UpdateView,
	.ToRelax = cGenCharLocalLogic_ToRelax,
	.ToAlert = cGenCharLocalLogic_ToAlert,
	.Accuse = Logic_SetState2,
	.FindTarget = cLocalLogic_FindTargetNull,
	.TargetDone = cLocalLogic_TargetDoneTrue,
	.Work = cLocalLogic_WorkNop,
	.BreakWork = cBossLocalLogic_BreakWork,
	.GoTo = cGenCharLocalLogic_GoTo,
	.UpdateGetIt = cGenCharLocalLogic_UpdateGetIt,
};

static cGVar GVar_BossWalkingTime;
static cGVar GVar_BossWorkPlace;
static cGVar GVar_BossWorkingTime;

/* the dynamic initializers of the globals here, run in the order of src/standalone/crtxc.inc */
FINI_GVAR_STRING(Fini_GVar_BossWorkPlace, &GVar_BossWorkPlace)
INIT_GVAR_STRING(Init_GVar_BossWorkPlace, Fini_GVar_BossWorkPlace, &GVar_BossWorkPlace, "BossWorkPlace", 0, "Boss' work place dummy name", "")
INIT_GVAR_FLOAT(Init_GVar_BossWorkingTime, &GVar_BossWorkingTime, "BossWorkingTime", 0, "Boss working time", 60.0f)
INIT_GVAR_FLOAT(Init_GVar_BossWalkingTime, &GVar_BossWalkingTime, "BossWalkingTime", 0, "Boss walking time", 30.0f)
