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

#define NURSES_MAKELOGIC 0x154440
#define NURSE_VT(self) ((self)->base.vftable)

void *__fastcall cNurses_ctor(cNurses *self) {
	self->timer = 0.0f;
	return self;
}

void __fastcall cNurses_Init(cNurses *self) {
	self->list = cWorld_NewEntityList(g_pWorld, "Nurse");
	self->list->func = (int (*)(void))Game_Ptr(NURSES_MAKELOGIC);
}

void __fastcall cNurses_Think(cNurses *self) {
	vec3_t t, t2;
	Nto1Node *n = Logic_ListFirst(self->list);

	if (!n) {
		return;
	}
	for (; n; n = Logic_ListNext(n)) {
		cLocalLogic *l = Logic_GetLocalLogic(n->data);
		l->vftable->Update(l, 0.73f);
	}
	for (cEntity *e = cWorld_GetFirstEntity(g_pWorld); e; e = cEntity_Next(e)) {
		if (!e->flag_4c || !e->vftable->IsType(e, TYEN_WALKCHAR) || !ChType_ActCtrl(e) ||
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
				lo->vftable->SetState(lo, 3, e);
				continue;
			}
		}
		l->vftable->SetState(l, 3, e);
	}
}

void __thiscall cNurses_Update(cNurses *self, float dt) {
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

void *__thiscall cNurseLocalLogic_ctor(void *mem, cEntity *parent) {
	cNurseLocalLogic *self = mem;

	cBaseLocalLogic_ctor(&self->base, parent);
	NURSE_VT(self) = CNURSELOCALLOGIC_VFTABLE;
	self->time = 0.0f;
	cNurseLocalLogic_SetState(self, 0, NULL);
	return self;
}

cNurseLocalLogic *__thiscall cNurseLocalLogic_Delete(cNurseLocalLogic *self, byte flags) {
	NURSE_VT(self) = CNURSELOCALLOGIC_VFTABLE;
	cBaseLocalLogic_dtor(&self->base);
	if (flags & 1) {
		operator_delete_sized(self, sizeof(*self));
	}
	return self;
}

/* back to the relax, the alert with the alarm */
byte __fastcall cNurseLocalLogic_Relax(cNurseLocalLogic *self) {
	if (g_pWorld->alarmOn) {
		return NURSE_VT(self)->SetState(self, 1, NULL);
	}
	return NURSE_VT(self)->SetState(self, 0, NULL);
}

/* vf17 */
void __thiscall cNurseLocalLogic_Update(cNurseLocalLogic *self, float dt) {
	cBaseLocalLogic *b = &self->base;
	cEntity *p = b->parent;

	cBaseLocalLogic_Update(b, dt);
	WChar_B381(b->parent) = !b->busy && !g_pWorld->alarmOn && b->state != 3 && b->state != 2;
	if (b->busy) {
		return;
	}
	switch (b->state) {
	case 0:
		if (g_pWorld->alarmOn) {
			NURSE_VT(self)->SetState(self, 1, NULL);
		}
		return;
	case 1:
		if (!g_pWorld->alarmOn) {
			NURSE_VT(self)->SetState(self, 0, NULL);
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
		if (!*Nto1Node_TargetName(WChar_Link(p))) {
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
void __thiscall cNurseLocalLogic_OnRobbery(cNurseLocalLogic *self, cEntity *e, float amount) {
	if (amount > 0.0f && self->base.state != 3 && Logic_Chance(&self->base, e, g_NurseBankerDetect + amount)) {
		NURSE_VT(self)->SetState(self, 2, e);
	}
}

static inline byte Nurse_Rand101(void) {
	return 101.0f > (float)(Weap_Rand01() * 100.0 + 0.0);
}

/* vf10: the escape from the attacker */
void __thiscall cNurseLocalLogic_OnHit(cNurseLocalLogic *self, cEntity *attacker) {
	if (!self->base.busy && Nurse_Rand101() && self->base.state != 3) {
		NURSE_VT(self)->SetState(self, 2, attacker);
	}
}

/* vf11: the escape from the player */
void __thiscall cNurseLocalLogic_OnSeen(cNurseLocalLogic *self, cEntity *e) {
	if (!self->base.busy && Nurse_Rand101() && e == ChType_Usr0() && self->base.state != 3) {
		NURSE_VT(self)->SetState(self, 2, e);
	}
}

/* vf15: 1 if it refuses the police (NursePoliceRefuse in percent), else the escape from it */
byte __thiscall cNurseLocalLogic_GivesMoney(cNurseLocalLogic *self, cEntity *police) {
	float refuse = g_NursePoliceRefuse;

	if (!self->base.busy && refuse > (float)(Weap_Rand01() * 100.0 + 0.0)) {
		return 1;
	}
	NURSE_VT(self)->SetState(self, 2, police);
	return 0;
}

/* vf24 */
byte __thiscall cNurseLocalLogic_RestartState(cNurseLocalLogic *self) {
	return NURSE_VT(self)->SetState(self, 0, NULL);
}

/* ---- part 2 (rva 0x154970) ---- */

/* vf19: the AI of the state: the walk around the origin (relaxed or in alert), the escape from target (2) or the
 * walk to the dazed character (3); always 1 */
byte __thiscall cNurseLocalLogic_SetState(cNurseLocalLogic *self, int state, cEntity *target) {
	cBaseLocalLogic *b = &self->base;
	byte ok = (byte)cBaseLocalLogic_SetState(b, state, target);
	cAIChar *ai = b->parent->vftable->GetAI(b->parent);

	if (ok) {
		WChar_B381(b->parent) = !b->busy && !g_pWorld->alarmOn && b->state != 3 && b->state != 2;
	}
	switch (b->state) {
	case 0:
		if (ok) {
			ai->vftable->SetState(ai, 6, b->origin, (double)b->actionRadius, (double)g_NurseRelaxSpeed, 1);
		}
		return 1;
	case 1:
		if (ok) {
			ai->vftable->SetState(ai, 6, b->origin, (double)b->actionRadius, (double)g_NurseAlertSpeed, 0);
		}
		return 1;
	case 2:
		if (ok) {
			ai->vftable->SetState(ai, 5, target, 1.0);
		}
		self->time = 0.0f;
		return 1;
	case 3:
		if (ok) {
			ai->vftable->SetState(ai, 2, b->target.target, 1.0);
			self->time = 0.0f;
		}
		return 1;
	}
	return 1;
}
