/*
 * The gatekeepers of the game (the eyes in the walls, "GateKeeper", include/scrap/logic.h): never hurt and without
 * money; relaxed (state 0) one faces the player in view to talk, attacked or at an alarm it hides in its shell
 * (state 1) for GateKeeperHiddenTime. The list updates their logics every sqrt(2) s.
 */
#include <string.h>

#include "scrap/chartype.h"
#include "scrap/objentity.h"

/* defined further down or in another file without a header */
cGateKeeperLocalLogic *GateKeepers_MakeLogic(cEntity *e);

cGateKeepers *__fastcall cGateKeepers_ctor(cGateKeepers *self) {
	self->timer = 0.0f;
	return self;
}

byte __thiscall Logic_True1(void *self, int arg) {
	(void)self;
	(void)arg;
	return 1;
}

void __fastcall cGateKeepers_Think(cGateKeepers *self) {
	for (Nto1Node *n = Logic_ListFirst(self->list); n; n = Logic_ListNext(n)) {
		cLocalLogic *l = Logic_GetLocalLogic(n->data);
		l->vftable->Update(l, 1.41421354f);
	}
}

void __fastcall cGateKeepers_Init(cGateKeepers *self) {
	cEntityListData *list = cWorld_NewEntityList(g_pWorld, "GateKeeper");

	self->list = list;
	list->func = (int (*)(void))(void *)GateKeepers_MakeLogic;
}

void __thiscall cGateKeepers_Update(cGateKeepers *self, float dt) {
	float t = self->timer + dt;

	self->timer = t;
	if (!(1.41421354f >= t)) {
		self->timer = 0.0f;
		cGateKeepers_Think(self);
	}
}

/* the logic of a character of the list (cEntityListData.func) */
cGateKeeperLocalLogic *GateKeepers_MakeLogic(cEntity *e) {
	cGateKeeperLocalLogic *l = operator_new(sizeof(*l));

	if (l) {
		return cGateKeeperLocalLogic_ctor(l, e);
	}
	return NULL;
}

cGateKeeperLocalLogic *__thiscall cGateKeeperLocalLogic_ctor(cGateKeeperLocalLogic *self, cEntity *parent) {
	cBaseLocalLogic_ctor(&self->base, parent);
	self->base.vftable = CGATEKEEPERLOCALLOGIC_VFTABLE;
	self->hiddenTime = 0.0f;
	cGateKeeperLocalLogic_SetState(self, 0, NULL);
	return self;
}

void __fastcall cGateKeeperLocalLogic_dtor(cGateKeeperLocalLogic *self) {
	self->base.vftable = CGATEKEEPERLOCALLOGIC_VFTABLE;
	cBaseLocalLogic_dtor(&self->base);
}

cGateKeeperLocalLogic *__thiscall cGateKeeperLocalLogic_Delete(cGateKeeperLocalLogic *self, byte flags) {
	cGateKeeperLocalLogic_dtor(self);
	if (flags & 1) {
		operator_delete_sized(self, sizeof(*self));
	}
	return self;
}

/* the view flag of the character: relaxed and no alarm */
void __thiscall cGateKeeperLocalLogic_UpdateView(cGateKeeperLocalLogic *self) {
	cEntity *p = self->base.parent;

	WChar(p)->wl.arrow.canUse = !self->base.busy && !g_pWorld->alarm.on && self->base.state != 1;
}

/* vf16 of the gatekeepers and the gears: they never run from the police */
void __thiscall cGateKeeperLocalLogic_Vf16(cGateKeeperLocalLogic *self, int arg) {
	(void)self;
	(void)arg;
	Con_Print(0x8f008f, "Ojito de pared intenta huir de un poli pequeño. Esto no debe pasar nunca", 0);
}

/* vf17 */
void __thiscall cGateKeeperLocalLogic_Update(cGateKeeperLocalLogic *self, float dt) {
	vec3_t tmp;
	cBaseLocalLogic *b = &self->base;
	cEntity *p = b->parent;

	if (!WChar(p)->wl.god) {
		WChar(p)->wl.life = 32000;
	}
	WChar(p)->money = 0;
	WChar(p)->moneyReward = 0;
	cBaseLocalLogic_Update(b, dt);
	cGateKeeperLocalLogic_UpdateView(self);
	if (b->busy) {
		return;
	}
	if (b->state == 1) {
		if (self->hiddenTime >= 0.0f) {
			self->hiddenTime = self->hiddenTime - dt;
			return;
		}
		if (!g_pWorld->alarm.on) {
			b->vftable->SetState(self, 0, NULL);
		}
		return;
	}
	if (b->state != 0) {
		return;
	}
	if (!b->idle && !b->alwaysLock) {
		cEntity *u = cWorld_GetUsrEntity(g_pWorld, 0);
		if (u) {
			u = cWorld_GetUsrEntity(g_pWorld, 0);
			if (Logic_InView(p, u->vftable->GetPos(u, &tmp), g_GateKeeperStdFar, g_GateKeeperStdFovY,
				g_GateKeeperStdFovX, 1)) {
				if (cActCtrl_IsActive(ChType_ActCtrl(p), "Relax")) {
					cActCtrl_PlayNew(ChType_ActCtrl(p), "FaceToTalk", 0, 1.0f);
					cEntity_AddLink(cWorld_GetUsrEntity(g_pWorld, 0), &WChar(p)->listen);
				}
				goto alarm;
			}
		}
		if (cActCtrl_IsActive(ChType_ActCtrl(p), "FaceToTalk")) {
			Nto1Node *n = &WChar(p)->listen;
			CHECK(n->data);
			Nto1Node_Unlink(n);
			cActCtrl_PlayNew(ChType_ActCtrl(p), "Relax0", 1, 1.0f);
		} else {
			WChar_Vtbl(p)->SetViewAng(p, Obj_Vtbl(p)->GetAng(p, &tmp));
		}
	}
alarm:
	if (g_pWorld->alarm.on) {
		b->vftable->SetState(self, 1, NULL);
	}
}

/* vf12 (a robbery seen) and vf11: hide unless idle */
void __thiscall cGateKeeperLocalLogic_OnRobbery(cGateKeeperLocalLogic *self, cEntity *e, float amount) {
	(void)e;
	(void)amount;
	if (!self->base.idle && !self->base.alwaysLock) {
		self->base.vftable->SetState(self, 1, NULL);
	}
}

/* vf10 */
void __thiscall cGateKeeperLocalLogic_OnHit(cGateKeeperLocalLogic *self, cEntity *attacker) {
	(void)attacker;
	self->base.vftable->SetState(self, 1, NULL);
}

/* vf14: hidden at once when the action Hide runs */
void __thiscall cGateKeeperLocalLogic_Vf14(cGateKeeperLocalLogic *self, cEntity *e) {
	cEntity *p = self->base.parent;

	(void)e;
	self->base.vftable->SetState(self, 1, NULL);
	if (cActCtrl_IsActive(ChType_ActCtrl(p), "Hide")) {
		cActCtrl_Play(ChType_ActCtrl(p), "Hidden", 1, 1.0f);
	}
}

void __thiscall cGateKeeperLocalLogic_Vf11(cGateKeeperLocalLogic *self, cEntity *e) {
	(void)e;
	if (!self->base.idle && !self->base.alwaysLock) {
		self->base.vftable->SetState(self, 1, NULL);
	}
}

/* vf24: hidden while the alarm is on */
void __thiscall cGateKeeperLocalLogic_RestartState(cGateKeeperLocalLogic *self) {
	self->base.vftable->SetState(self, g_pWorld->alarm.on != 0, NULL);
}

/* vf19: state 1 needs the action Hide */
byte __thiscall cGateKeeperLocalLogic_SetState(cGateKeeperLocalLogic *self, int state, cEntity *target) {
	void *act = NULL;
	cEntity *p = self->base.parent;

	if (state == 1) {
		act = WithLife_Vtbl(p)->GetAction(p, "Hide");
		if (!act) {
			return 0;
		}
	}
	byte ok = (byte)cBaseLocalLogic_SetState(&self->base, state, target);
	if (ok) {
		cGateKeeperLocalLogic_UpdateView(self);
	}
	if (self->base.state == 0) {
		if (ok && !strcmp(cActCtrl_GetDefLoopName(ChType_ActCtrl(p)), "Hidden")) {
			cActCtrl_Play(ChType_ActCtrl(p), "Unhide", 1, 1.0f);
			cActCtrl_SetDefLoop(ChType_ActCtrl(p), "Relax", 1);
		}
		self->hiddenTime = 0.0f;
	} else if (self->base.state == 1) {
		if (ok && strcmp(cActCtrl_GetDefLoopName(ChType_ActCtrl(p)), "Hidden")) {
			cActCtrl_PlayAction(ChType_ActCtrl(p), act, 1, 1, 1.0f);
			cActCtrl_SetDefLoop(ChType_ActCtrl(p), "Hidden", 1);
		}
		self->hiddenTime = g_GateKeeperHiddenTime;
	}
	return 1;
}
