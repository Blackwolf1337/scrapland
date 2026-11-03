/*
 * Logic/NurseLogic.cpp of the game, the first part: the nurses (cNurses of cLogicControl, the list "Nurse") and
 * their logic cNurseLocalLogic (include/scrap/logic.h). Every 0.73 s the nurse nearest to a dazed character goes to
 * it (state 3, the next one when the nearest is idle out of its relax); a nurse relaxes (0) or walks in alert (1)
 * with the alarm and gets away from a robbery or a hit (2). The SetState of the logic is the second part (rva
 * 0x154970).
 */
#include <string.h>

#include "scrap/chartype.h"
#include "scrap/weapons.h"
#include "scrap/staticinit.h"

/* the states of the nurse */
enum {
	NURSE_RELAX, NURSE_ALERT, NURSE_FLEE, NURSE_ATTACK
};

#define CNURSELOCALLOGIC_VFTABLE (&cNurseLocalLogic_vftable)
#define g_NurseBankerDetect (GVar_NurseBankerDetect.value.f)
#define g_NursePoliceRefuse (GVar_NursePoliceRefuse.value.f)
#define g_NurseEscapeTime (GVar_NurseEscapeTime.value.f)
#define g_NurseRelaxSpeed (GVar_NurseRelaxSpeed.value.f)
#define g_NurseAlertSpeed (GVar_NurseAlertSpeed.value.f)
static const cBaseLocalLogicVtbl cNurseLocalLogic_vftable;
static cGVar GVar_NurseRelaxSpeed;
static cGVar GVar_NursePoliceRefuse;
static cGVar GVar_NurseEscapeTime;
static cGVar GVar_NurseBankerDetect;
static cGVar GVar_NurseAlertSpeed;

/* defined further down or in another file without a header */
static void *Nurses_MakeLogic(cEntity *e);

void *cNurses_ctor(cNurses *self) {
	self->timer = 0.0f;
	return self;
}

void cNurses_Init(cNurses *self) {
	self->list = cWorld_NewEntityList(g_pWorld, "Nurse");
	self->list->func = (int (*)(void))(void *)Nurses_MakeLogic;
}

static void cNurses_Think(cNurses *self) {
	vec3_t t, t2;
	Nto1Node *n = Logic_ListFirst(self->list);

	if (!n) {
		return;
	}
	for (; n; n = Logic_ListNext(n)) {
		cLocalLogic *l = Logic_GetLocalLogic(n->data);
		Logic_Update(l, 0.73f);
	}
	for (cEntity *e = cWorld_GetFirstEntity(g_pWorld); e; e = cEntity_Next(e)) {
		if (!e->active || !Entity_IsType(e, TYEN_WALKCHAR) || !ChType_ActCtrl(e) ||
			!cActCtrl_IsActive(ChType_ActCtrl(e), "Dazed")) {
			continue;
		}
		cEntity *nurse = cEntityList_Nearest(self->list, Entity_GetPos(e, &t), NULL, NULL);
		cBaseLocalLogic *l = nurse->lLogic;
		if (l->state == NURSE_ATTACK) {
			continue;
		}
		if ((l->idle || l->alwaysLock) && strncmp(cActCtrl_GetActionName(ChType_ActCtrl(nurse), 0), "Relax", 5)) {
			cEntity *other = cEntityList_Nearest(self->list, Entity_GetPos(e, &t2), nurse, NULL);
			if (other) {
				cBaseLocalLogic *lo = other->lLogic;
				Logic_SetState(lo, NURSE_ATTACK, e);
				continue;
			}
		}
		Logic_SetState(l, NURSE_ATTACK, e);
	}
}

void cNurses_Update(cNurses *self, float dt) {
	float t = self->timer + dt;

	self->timer = t;
	if (!(0.73f >= t)) {
		self->timer = 0.0f;
		cNurses_Think(self);
	}
}

/* the maker of the logics of the list */
static void *cNurseLocalLogic_ctor(void *mem, cEntity *parent);
static void *Nurses_MakeLogic(cEntity *e) {
	void *m = operator_new(sizeof(cNurseLocalLogic));

	return m ? cNurseLocalLogic_ctor(m, e) : NULL;
}

static byte cNurseLocalLogic_SetState(cLocalLogic *elem, int state, cEntity *target);
static void *cNurseLocalLogic_ctor(void *mem, cEntity *parent) {
	cNurseLocalLogic *self = mem;

	cBaseLocalLogic_ctor(&self->base, parent);
	self->base.vftable = CNURSELOCALLOGIC_VFTABLE;
	self->time = 0.0f;
	cNurseLocalLogic_SetState(&self->base.logic, NURSE_RELAX, NULL);
	return self;
}

static cLocalLogic *cNurseLocalLogic_Delete(cLocalLogic *elem, byte flags) {
	cNurseLocalLogic *self = (cNurseLocalLogic *)elem;
	self->base.vftable = CNURSELOCALLOGIC_VFTABLE;
	cBaseLocalLogic_dtor(&self->base);
	if (flags & 1) {
		operator_delete_sized(self, sizeof(*self));
	}
	return elem;
}

/* back to the relax, the alert with the alarm */
static byte cNurseLocalLogic_Relax(cNurseLocalLogic *self) {
	if (g_pWorld->alarm.on) {
		return Logic_SetState(self, NURSE_ALERT, NULL);
	}
	return Logic_SetState(self, NURSE_RELAX, NULL);
}

/* vf17 */
static void cNurseLocalLogic_Update(cLocalLogic *elem, float dt) {
	cNurseLocalLogic *self = (cNurseLocalLogic *)elem;
	cBaseLocalLogic *b = &self->base;
	cEntity *p = b->parent;

	cBaseLocalLogic_Update(&b->logic, dt);
	WChar(b->parent)->wl.arrow.canUse = !b->busy && !g_pWorld->alarm.on && b->state != NURSE_ATTACK && b->state != NURSE_FLEE;
	if (b->busy) {
		return;
	}
	switch (b->state) {
	case NURSE_RELAX:
		if (g_pWorld->alarm.on) {
			Logic_SetState(self, NURSE_ALERT, NULL);
		}
		return;
	case NURSE_ALERT:
		if (!g_pWorld->alarm.on) {
			Logic_SetState(self, NURSE_RELAX, NULL);
		}
		return;
	case NURSE_FLEE:
		if (b->idle || b->alwaysLock) {
			self->time = g_NurseEscapeTime;
			return;
		}
		self->time = self->time + dt;
		if (self->time > g_NurseEscapeTime) {
			cNurseLocalLogic_Relax(self);
		}
		return;
	case NURSE_ATTACK: {
		cEntity *t = b->target.target;
		if (!t) {
			cNurseLocalLogic_Relax(self);
			return;
		}
		if (!*Nto1Node_TargetName(&WChar(p)->collide)) {
			if (!cActCtrl_IsActive(ChType_ActCtrl(b->target.target), "Dazed")) {
				cNurseLocalLogic_Relax(self);
			}
			self->time = 0.0f;
			return;
		}
		float tm = self->time + dt;
		self->time = tm;
		if (tm > 5.0f) {
			cNurseLocalLogic_Relax(self);
		}
		return;
	}
	}
}

/* vf12: a robbery seen (NurseBankerDetect plus the amount): the escape */
static void cNurseLocalLogic_OnRobbery(cLocalLogic *elem, cEntity *e, float amount) {
	cNurseLocalLogic *self = (cNurseLocalLogic *)elem;
	if (amount > 0.0f && self->base.state != NURSE_ATTACK && Logic_Chance(&self->base, e, g_NurseBankerDetect + amount)) {
		Logic_SetState(self, NURSE_FLEE, e);
	}
}

static inline byte Nurse_Rand101(void) {
	return 101.0f > (float)(Weap_Rand01() * 100.0 + 0.0);
}

/* vf10: the escape from the attacker */
static void cNurseLocalLogic_OnHit(cLocalLogic *elem, cEntity *attacker) {
	cNurseLocalLogic *self = (cNurseLocalLogic *)elem;
	if (!self->base.busy && Nurse_Rand101() && self->base.state != NURSE_ATTACK) {
		Logic_SetState(self, NURSE_FLEE, attacker);
	}
}

/* vf11: the escape from the player */
static void cNurseLocalLogic_OnSeen(cLocalLogic *elem, cEntity *e) {
	cNurseLocalLogic *self = (cNurseLocalLogic *)elem;
	if (!self->base.busy && Nurse_Rand101() && e == ChType_Usr0() && self->base.state != NURSE_ATTACK) {
		Logic_SetState(self, NURSE_FLEE, e);
	}
}

/* vf15: 1 if it refuses the police (NursePoliceRefuse in percent), else the escape from it */
static byte cNurseLocalLogic_GivesMoney(cLocalLogic *elem, cEntity *police) {
	cNurseLocalLogic *self = (cNurseLocalLogic *)elem;
	float refuse = g_NursePoliceRefuse;

	if (!self->base.busy && refuse > (float)(Weap_Rand01() * 100.0 + 0.0)) {
		return 1;
	}
	Logic_SetState(self, NURSE_FLEE, police);
	return 0;
}

/* vf24 */
static void cNurseLocalLogic_RestartState(cLocalLogic *base) {
	cNurseLocalLogic *self = (cNurseLocalLogic *)base;
	Logic_SetState(self, NURSE_RELAX, NULL);
}

/* ---- part 2 ---- */

/* vf19: the AI of the state: the walk around the origin (relaxed or in alert), the escape from target (2) or the
 * walk to the dazed character (3); always 1 */
static byte cNurseLocalLogic_SetState(cLocalLogic *elem, int state, cEntity *target) {
	cNurseLocalLogic *self = (cNurseLocalLogic *)elem;
	cBaseLocalLogic *b = &self->base;
	byte ok = (byte)cBaseLocalLogic_SetState(&b->logic, state, target);
	cAIChar *ai = Entity_GetAI(b->parent);

	if (ok) {
		WChar(b->parent)->wl.arrow.canUse = !b->busy && !g_pWorld->alarm.on && b->state != NURSE_ATTACK && b->state != NURSE_FLEE;
	}
	switch (b->state) {
	case NURSE_RELAX:
		if (ok) {
			AIChar_SetState(ai, AIC_PATROL_ZONE, b->origin, (double)b->actionRadius, (double)g_NurseRelaxSpeed, 1);
		}
		return 1;
	case NURSE_ALERT:
		if (ok) {
			AIChar_SetState(ai, AIC_PATROL_ZONE, b->origin, (double)b->actionRadius, (double)g_NurseAlertSpeed, 0);
		}
		return 1;
	case NURSE_FLEE:
		if (ok) {
			AIChar_SetState(ai, AIC_GET_AWAY, target, 1.0);
		}
		self->time = 0.0f;
		return 1;
	case NURSE_ATTACK:
		if (ok) {
			AIChar_SetState(ai, AIC_CHASE_WITH_ACTION, b->target.target, 1.0);
			self->time = 0.0f;
		}
		return 1;
	}
	return 1;
}

/* the vtable of cNurseLocalLogic */
static const cBaseLocalLogicVtbl cNurseLocalLogic_vftable = {
	.base.Delete = cNurseLocalLogic_Delete,
	.base.GetName = cBaseLocalLogic_GetName,
	.base.AcceptsArrow = cLocalLogic_AcceptsArrowTrue,
	.base.VarI = cBaseLocalLogic_VarI,
	.base.VarF = cBaseLocalLogic_VarF,
	.base.VarS = cBaseLocalLogic_VarS,
	.base.VarV = cBaseLocalLogic_VarV,
	.UpdateBusy = cBaseLocalLogic_UpdateBusy,
	.Refresh = cBaseLocalLogic_Refresh,
	.Listens = cLocalLogic_ListensTrue,
	.OnHit = cNurseLocalLogic_OnHit,
	.OnSeen = cNurseLocalLogic_OnSeen,
	.OnAttack = cNurseLocalLogic_OnRobbery,
	.OnShot = cBaseLocalLogic_OnShot,
	.OnHammer = cLocalLogic_OnHammerNop,
	.GivesMoney = cNurseLocalLogic_GivesMoney,
	.OnAlarm = Logic_SetState2,
	.Update = cNurseLocalLogic_Update,
	.SetOriginFrom = cBaseLocalLogic_SetOriginFrom,
	.SetState = cNurseLocalLogic_SetState,
	.SetIdle = cBaseLocalLogic_SetIdle,
	.SetActionRadius = cBaseLocalLogic_SetActionRadius,
	.SetOrigin = cBaseLocalLogic_SetOrigin,
	.SetOriginAng = cBaseLocalLogic_SetOriginAng,
	.RestartState = cNurseLocalLogic_RestartState,
};

static cGVar GVar_NurseBankerDetect;
static cGVar GVar_NursePoliceRefuse;
static cGVar GVar_NurseEscapeTime;
static cGVar GVar_NurseAlertSpeed;
static cGVar GVar_NurseRelaxSpeed;

/* the dynamic initializers of the globals here, run in the order of src/standalone/crtxc.inc */
INIT_GVAR_FLOAT(Init_GVar_NurseRelaxSpeed, &GVar_NurseRelaxSpeed, "NurseRelaxSpeed", 0, "Nurse speed (Relax)", 0.3f)
INIT_GVAR_FLOAT(Init_GVar_NurseAlertSpeed, &GVar_NurseAlertSpeed, "NurseAlertSpeed", 0, "Nurse speed (Alarm)", 1.0f)
INIT_GVAR_FLOAT(Init_GVar_NurseEscapeTime, &GVar_NurseEscapeTime, "NurseEscapeTime", 0, "Nurse Escape Time", 8.0f)
INIT_GVAR_FLOAT(Init_GVar_NurseBankerDetect, &GVar_NurseBankerDetect, "NurseBankerDetect", 0, "percentage of times when the Nurse escapes from the banker", 30.0f)
INIT_GVAR_FLOAT(Init_GVar_NursePoliceRefuse, &GVar_NursePoliceRefuse, "NursePoliceRefuse", 0, "percentage of times when the Nurse escapes from the police", 50.0f)
