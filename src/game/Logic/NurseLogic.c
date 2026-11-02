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

/* defined further down or in another file without a header */
void *Nurses_MakeLogic(cEntity *e);

#define NURSES_MAKELOGIC 0x154440
#define NURSE_VT(self) ((self)->base.vftable)

void *cNurses_ctor(cNurses *self) {
	self->timer = 0.0f;
	return self;
}

void cNurses_Init(cNurses *self) {
	self->list = cWorld_NewEntityList(g_pWorld, "Nurse");
	self->list->func = (int (*)(void))(void *)Nurses_MakeLogic;
}

void cNurses_Think(cNurses *self) {
	vec3_t t, t2;
	Nto1Node *n = Logic_ListFirst(self->list);

	if (!n) {
		return;
	}
	for (; n; n = Logic_ListNext(n)) {
		cLocalLogic *l = Logic_GetLocalLogic(n->data);
		Logic_Vtbl(l)->Update(l, 0.73f);
	}
	for (cEntity *e = cWorld_GetFirstEntity(g_pWorld); e; e = cEntity_Next(e)) {
		if (!e->active || !e->vftable->IsType(e, TYEN_WALKCHAR) || !ChType_ActCtrl(e) ||
			!cActCtrl_IsActive(ChType_ActCtrl(e), "Dazed")) {
			continue;
		}
		cEntity *nurse = cEntityList_Nearest(self->list, e->vftable->GetPos(e, &t), NULL, NULL);
		cBaseLocalLogic *l = nurse->lLogic;
		if (l->state == 3) {
			continue;
		}
		if ((l->idle || l->alwaysLock) && strncmp(cActCtrl_GetActionName(ChType_ActCtrl(nurse), 0), "Relax", 5)) {
			cEntity *other = cEntityList_Nearest(self->list, e->vftable->GetPos(e, &t2), nurse, NULL);
			if (other) {
				cBaseLocalLogic *lo = other->lLogic;
				lo->vftable->SetState(&lo->logic, 3, e);
				continue;
			}
		}
		l->vftable->SetState(&l->logic, 3, e);
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
void *Nurses_MakeLogic(cEntity *e) {
	void *m = operator_new(sizeof(cNurseLocalLogic));

	return m ? cNurseLocalLogic_ctor(m, e) : NULL;
}

void *cNurseLocalLogic_ctor(void *mem, cEntity *parent) {
	cNurseLocalLogic *self = mem;

	cBaseLocalLogic_ctor(&self->base, parent);
	NURSE_VT(self) = CNURSELOCALLOGIC_VFTABLE;
	self->time = 0.0f;
	cNurseLocalLogic_SetState(&self->base.logic, 0, NULL);
	return self;
}

cLocalLogic *cNurseLocalLogic_Delete(cLocalLogic *elem, byte flags) {
	cNurseLocalLogic *self = (cNurseLocalLogic *)elem;
	NURSE_VT(self) = CNURSELOCALLOGIC_VFTABLE;
	cBaseLocalLogic_dtor(&self->base);
	if (flags & 1) {
		operator_delete_sized(self, sizeof(*self));
	}
	return elem;
}

/* back to the relax, the alert with the alarm */
byte cNurseLocalLogic_Relax(cNurseLocalLogic *self) {
	if (g_pWorld->alarm.on) {
		return NURSE_VT(self)->SetState(&self->base.logic, 1, NULL);
	}
	return NURSE_VT(self)->SetState(&self->base.logic, 0, NULL);
}

/* vf17 */
void cNurseLocalLogic_Update(cLocalLogic *elem, float dt) {
	cNurseLocalLogic *self = (cNurseLocalLogic *)elem;
	cBaseLocalLogic *b = &self->base;
	cEntity *p = b->parent;

	cBaseLocalLogic_Update(&b->logic, dt);
	WChar(b->parent)->wl.arrow.canUse = !b->busy && !g_pWorld->alarm.on && b->state != 3 && b->state != 2;
	if (b->busy) {
		return;
	}
	switch (b->state) {
	case 0:
		if (g_pWorld->alarm.on) {
			NURSE_VT(self)->SetState(&self->base.logic, 1, NULL);
		}
		return;
	case 1:
		if (!g_pWorld->alarm.on) {
			NURSE_VT(self)->SetState(&self->base.logic, 0, NULL);
		}
		return;
	case 2:
		if (b->idle || b->alwaysLock) {
			self->time = g_NurseEscapeTime;
			return;
		}
		self->time = self->time + dt;
		if (self->time > g_NurseEscapeTime) {
			cNurseLocalLogic_Relax(self);
		}
		return;
	case 3: {
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
void cNurseLocalLogic_OnRobbery(cLocalLogic *elem, cEntity *e, float amount) {
	cNurseLocalLogic *self = (cNurseLocalLogic *)elem;
	if (amount > 0.0f && self->base.state != 3 && Logic_Chance(&self->base, e, g_NurseBankerDetect + amount)) {
		NURSE_VT(self)->SetState(&self->base.logic, 2, e);
	}
}

static inline byte Nurse_Rand101(void) {
	return 101.0f > (float)(Weap_Rand01() * 100.0 + 0.0);
}

/* vf10: the escape from the attacker */
void cNurseLocalLogic_OnHit(cLocalLogic *elem, cEntity *attacker) {
	cNurseLocalLogic *self = (cNurseLocalLogic *)elem;
	if (!self->base.busy && Nurse_Rand101() && self->base.state != 3) {
		NURSE_VT(self)->SetState(&self->base.logic, 2, attacker);
	}
}

/* vf11: the escape from the player */
void cNurseLocalLogic_OnSeen(cLocalLogic *elem, cEntity *e) {
	cNurseLocalLogic *self = (cNurseLocalLogic *)elem;
	if (!self->base.busy && Nurse_Rand101() && e == ChType_Usr0() && self->base.state != 3) {
		NURSE_VT(self)->SetState(&self->base.logic, 2, e);
	}
}

/* vf15: 1 if it refuses the police (NursePoliceRefuse in percent), else the escape from it */
byte cNurseLocalLogic_GivesMoney(cLocalLogic *elem, cEntity *police) {
	cNurseLocalLogic *self = (cNurseLocalLogic *)elem;
	float refuse = g_NursePoliceRefuse;

	if (!self->base.busy && refuse > (float)(Weap_Rand01() * 100.0 + 0.0)) {
		return 1;
	}
	NURSE_VT(self)->SetState(&self->base.logic, 2, police);
	return 0;
}

/* vf24 */
void cNurseLocalLogic_RestartState(cLocalLogic *base) {
	cNurseLocalLogic *self = (cNurseLocalLogic *)base;
	NURSE_VT(self)->SetState(&self->base.logic, 0, NULL);
}

/* ---- part 2 ---- */

/* vf19: the AI of the state: the walk around the origin (relaxed or in alert), the escape from target (2) or the
 * walk to the dazed character (3); always 1 */
byte cNurseLocalLogic_SetState(cLocalLogic *elem, int state, cEntity *target) {
	cNurseLocalLogic *self = (cNurseLocalLogic *)elem;
	cBaseLocalLogic *b = &self->base;
	byte ok = (byte)cBaseLocalLogic_SetState(&b->logic, state, target);
	cAIChar *ai = b->parent->vftable->GetAI(b->parent);

	if (ok) {
		WChar(b->parent)->wl.arrow.canUse = !b->busy && !g_pWorld->alarm.on && b->state != 3 && b->state != 2;
	}
	switch (b->state) {
	case 0:
		if (ok) {
			ai->vftable->SetState(&ai->base.base, 6, b->origin, (double)b->actionRadius, (double)g_NurseRelaxSpeed, 1);
		}
		return 1;
	case 1:
		if (ok) {
			ai->vftable->SetState(&ai->base.base, 6, b->origin, (double)b->actionRadius, (double)g_NurseAlertSpeed, 0);
		}
		return 1;
	case 2:
		if (ok) {
			ai->vftable->SetState(&ai->base.base, 5, target, 1.0);
		}
		self->time = 0.0f;
		return 1;
	case 3:
		if (ok) {
			ai->vftable->SetState(&ai->base.base, 2, b->target.target, 1.0);
			self->time = 0.0f;
		}
		return 1;
	}
	return 1;
}

/* the vtable of cNurseLocalLogic */
const cBaseLocalLogicVtbl cNurseLocalLogic_vftable = {
	.base.Delete = cNurseLocalLogic_Delete,
	.base.GetName = cBaseLocalLogic_GetName,
	.base.Vf02 = cLocalLogic_Vf02True,
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

cGVar GVar_NurseBankerDetect;

cGVar GVar_NursePoliceRefuse;

cGVar GVar_NurseEscapeTime;

cGVar GVar_NurseAlertSpeed;

cGVar GVar_NurseRelaxSpeed;

