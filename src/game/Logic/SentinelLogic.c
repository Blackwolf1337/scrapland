/*
 * Logic/SentinelLogic.cpp of the game: the sentinels, the eyes of the indoor police (cSentinelLocalLogic,
 * include/scrap/logic.h; the list "Sentinel" of the AI control). States: 0 relax and 1 search (the walk around the
 * origin), 2 accuse (the player: the alarm and the gears attack), 3 inspect and 4 suspect (follow the player that
 * is not of the type of the alarm), 5 watch a place the AI control sent it to, 6 accuse a character (the gears
 * attack it while the sentinel sees it or the player is near). The view of the sentinel is SentinelStdFar /
 * SentinelStdFovY / SentinelStdFovX.
 */
#include <string.h>

#include "scrap/chartype.h"
#include "scrap/logic.h"
#include "scrap/objentity.h"
#include "scrap/scrap.h"
#include "scrap/scrapmodel.h"
#include "scrap/weapons.h"
#include "scrap/world.h"

#define CSENTINELLOCALLOGIC_VFTABLE GAME_VFTABLE(cSentinelLocalLogic_vftable, 0x3c156c)
#define g_SentinelInspectTime GAME_GLOBAL_IN(float, GVar_SentinelInspectTime, 16, 0x44d9d0)
#define g_SentinelSuspectTime GAME_GLOBAL_IN(float, GVar_SentinelSuspectTime, 16, 0x44d9e8)
#define g_SentinelBankerNPCDetect GAME_GLOBAL_IN(float, GVar_SentinelBankerNPCDetect, 16, 0x44da00)
#define g_SentinelGotoSpeed GAME_GLOBAL_IN(float, GVar_SentinelGotoSpeed, 16, 0x44da18)
#define g_SentinelAcuseTime GAME_GLOBAL_IN(float, GVar_SentinelAcuseTime, 16, 0x44da30)
#define g_SentinelGotoTime GAME_GLOBAL_IN(float, GVar_SentinelGotoTime, 16, 0x44da48)
#define g_SentinelAcuseSpeed GAME_GLOBAL_IN(float, GVar_SentinelAcuseSpeed, 16, 0x44da60)
#define g_SentinelSearchSpeed GAME_GLOBAL_IN(float, GVar_SentinelSearchSpeed, 16, 0x44da78)
#define g_SentinelSuspectSpeed GAME_GLOBAL_IN(float, GVar_SentinelSuspectSpeed, 16, 0x44da90)
#define g_SentinelGotoRadius GAME_GLOBAL_IN(float, GVar_SentinelGotoRadius, 16, 0x44dac0)
#define g_SentinelStdFar GAME_GLOBAL_IN(float, GVar_SentinelStdFar, 16, 0x44dad8)
#define g_SentinelStdFovY GAME_GLOBAL_IN(float, GVar_SentinelStdFovY, 16, 0x44daf0)
#define g_SentinelInspectSpeed GAME_GLOBAL_IN(float, GVar_SentinelInspectSpeed, 16, 0x44db08)
#define g_SentinelBankerDetect GAME_GLOBAL_IN(float, GVar_SentinelBankerDetect, 16, 0x44db20)
#define g_SentinelRelaxSpeed GAME_GLOBAL_IN(float, GVar_SentinelRelaxSpeed, 16, 0x44db50)
#define g_SentinelStdFovX GAME_GLOBAL_IN(float, GVar_SentinelStdFovX, 16, 0x44db68)

#define SENT_VT(self) ((self)->base.vftable)

void *__thiscall cSentinelLocalLogic_ctor(void *mem, cEntity *parent) {
	cSentinelLocalLogic *self = mem;

	cBaseLocalLogic_ctor(&self->base, parent);
	SENT_VT(self) = CSENTINELLOCALLOGIC_VFTABLE;
	cSentinelLocalLogic_SetState(self, 0, NULL);
	self->seen = 0;
	return self;
}

void __fastcall cSentinelLocalLogic_dtor(cSentinelLocalLogic *self) {
	SENT_VT(self) = CSENTINELLOCALLOGIC_VFTABLE;
	cBaseLocalLogic_dtor(&self->base);
}

cSentinelLocalLogic *__thiscall cSentinelLocalLogic_Delete(cSentinelLocalLogic *self, byte flags) {
	SENT_VT(self) = CSENTINELLOCALLOGIC_VFTABLE;
	cBaseLocalLogic_dtor(&self->base);
	if (flags & 1) {
		operator_delete_sized(self, sizeof(*self));
	}
	return self;
}

/* vf16: a sentinel never runs from the alarm */
void __thiscall cSentinelLocalLogic_OnAlarm(cSentinelLocalLogic *self, cEntity *e) {
	(void)self;
	(void)e;
	Con_Print(0x8f008f, "El centinela intenta huir de un poli pequeño. Esto no debe pasar", 0);
}

/* vf25: the player was in view at the last think */
byte __thiscall cSentinelLocalLogic_Seen(cSentinelLocalLogic *self) {
	return self->seen;
}

void __thiscall cSentinelLocalLogic_Watch(void *sentinel, const vec3_t *pos) {
	cSentinelLocalLogic *self = sentinel;
	cBaseLocalLogic *b = &self->base;
	vec3_t tmp;
	int st = b->state;

	if (st != 1 && st != 0 && st != 5) {
		return;
	}
	cAIChar *ai = b->parent->vftable->GetAI(b->parent);
	cBaseLocalLogic_SetState(b, 5, NULL);
	if (ai->vftable->GetState(ai) != 4) {
		const vec3_t *pp = b->parent->vftable->GetPos(b->parent, &tmp);
		vec3_t dir;
		dir.z = pos->z - pp->z;
		dir.x = pos->x - pp->x;
		dir.y = 0.0f;
		ai->vftable->SetState(ai, 4, *pos, dir, (double)(WChar_Radius(b->parent) + g_SentinelGotoRadius),
			(double)g_SentinelGotoSpeed);
	}
	self->time = g_SentinelGotoTime;
}

/* vf26: the view of the player (unless quiet) and the state */
void __thiscall cSentinelLocalLogic_Think(cSentinelLocalLogic *self, float dt, byte quiet) {
	cBaseLocalLogic *b = &self->base;
	cEntity *p = b->parent;
	vec3_t tmp;

	self->seen = 0;
	cEntity *usr = ChType_Usr0();
	cBaseLocalLogic_Update(b, dt);
	Entity_Team(p) = g_pWorld->alarm.on ? 0 : 2;
	if (b->busy) {
		WChar_ViewMode(p) = 0;
		WChar_B381(p) = 0;
		return;
	}
	if (0.0f >= self->time || !b->target.target || quiet) {
		int st = b->state;
		if (st != 1 && st != 0 && st != 5 && !b->idle && !b->alwaysLock) {
			SENT_VT(self)->SetState(self, g_pWorld->alarm.on ? 1 : 0, NULL);
		}
	}
	int st = b->state;
	if (st == 1 || st == 0 || st == 5) {
		self->time = self->time - dt;
	} else {
		cAIChar *ai = p->vftable->GetAI(p);
		if (!Logic_CharAIVtbl(ai)->Vf94(ai)) {
			self->time = self->time - dt;
		}
	}
	if (!quiet && usr && Logic_InView(p, usr->vftable->GetPos(usr, &tmp), g_SentinelStdFar, g_SentinelStdFovY,
			g_SentinelStdFovX, 1)) {
		cAlarm *a = &g_pWorld->alarm;
		self->seen = 1;
		if (a->notGuaranteed) {
			Alarm_Set(&g_pWorld->alarm, 1.0f);
			SENT_VT(self)->SetState(self, 2, usr);
		} else if (a->sameChar) {
			SENT_VT(self)->SetState(self, 2, usr);
		} else if (a->notDefChar) {
			if (g_pWorld->alarm.on) {
				SENT_VT(self)->SetState(self, 4, usr);
			} else if (b->state != 6) {
				SENT_VT(self)->SetState(self, 3, usr);
			}
		}
	}
	cSentinelLocalLogic_UpdateState(self, p, dt);
}

/* vf12: a robbery seen: the player is accused at SentinelBankerDetect percent (plus the amount), a character goes
 * to state 6 at SentinelBankerNPCDetect */
void __thiscall cSentinelLocalLogic_OnRobbery(cSentinelLocalLogic *self, cEntity *e, float amount) {
	if (self->base.busy) {
		return;
	}
	if (e == ChType_Usr0()) {
		if (amount > 0.0f && g_SentinelBankerDetect + amount >= (float)(Weap_Rand01() * 100.0 + 0.0)) {
			SENT_VT(self)->SetState(self, 2, e);
			Alarm_Set(&g_pWorld->alarm, 1.0f);
		}
		return;
	}
	if (g_SentinelBankerNPCDetect + amount >= (float)(Weap_Rand01() * 100.0 + 0.0)) {
		SENT_VT(self)->SetState(self, 6, e);
	}
}

/* vf13: the player is accused with the alarm, another character goes to state 6 */
void __thiscall cSentinelLocalLogic_OnAttack(cSentinelLocalLogic *self, cEntity *e) {
	if (self->base.busy) {
		return;
	}
	if (e == ChType_Usr0()) {
		Alarm_Set(&g_pWorld->alarm, 1.0f);
		SENT_VT(self)->SetState(self, 2, e);
		return;
	}
	SENT_VT(self)->SetState(self, 6, e);
}

/* vf10 and vf11: the player is accused with the alarm */
void __thiscall cSentinelLocalLogic_OnUsr(cSentinelLocalLogic *self, cEntity *e) {
	if (self->base.busy) {
		return;
	}
	if (e == ChType_Usr0()) {
		Alarm_Set(&g_pWorld->alarm, 1.0f);
		SENT_VT(self)->SetState(self, 2, e);
	}
}

/* vf15: the police asks for money: accused (the alarm for the player); always 0 */
byte __thiscall cSentinelLocalLogic_GivesMoney(cSentinelLocalLogic *self, cEntity *e) {
	if (!self->base.busy) {
		if (e == ChType_Usr0()) {
			Alarm_Set(&g_pWorld->alarm, 1.0f);
		}
		SENT_VT(self)->SetState(self, 2, e);
	}
	return 0;
}

/* a relax action of the sentinel looking at the player, unless the other one runs */
static void Sentinel_Relax(cEntity *s) {
	if (strncmp(cActCtrl_GetActionName(ChType_ActCtrl(s), 0), "Relax", 5)) {
		return;
	}
	if ((float)(Weap_Rand01() + 0.0) > 0.5f) {
		if (!cActCtrl_IsActive(ChType_ActCtrl(s), "Relax1")) {
			cActCtrl_PlayNew(ChType_ActCtrl(s), "Relax2", 0, 1.0f);
		}
	} else if (!cActCtrl_IsActive(ChType_ActCtrl(s), "Relax2")) {
		cActCtrl_PlayNew(ChType_ActCtrl(s), "Relax1", 0, 1.0f);
	}
}

/* the view and the look of the character p (the parent) by the state; dt is not used */
void __thiscall cSentinelLocalLogic_UpdateState(cSentinelLocalLogic *self, cEntity *p, float dt) {
	cBaseLocalLogic *b = &self->base;
	cAIChar *ai = b->parent->vftable->GetAI(b->parent);
	cEntity *s = b->parent;
	cAlarm *a = &g_pWorld->alarm;
	vec3_t tmp;

	(void)dt;
	WChar_ViewFovY(p) = g_SentinelStdFovY;
	WChar_ViewFar(p) = g_SentinelStdFar;
	WChar_B381(p) = !g_pWorld->alarm.on && !a->notDefChar;
	switch (b->state) {
	case 0:
	case 1:
		WChar_ViewMode(p) = a->sameChar ? 2 : 1;
		return;
	case 2:
		if (!b->target.target || !g_pWorld->alarm.on) {
			cSentinelLocalLogic_Return(self);
		}
		if (!self->seen) {
			WChar_ViewMode(p) = 2;
			return;
		}
		WChar_ViewMode(p) = 3;
		AICtrl_RaiseLevel(g_pWorld->aiCtrl, 1);
		Alarm_Set(&g_pWorld->alarm, 1.0f);
		if (cWorld_GetUsrEntity(g_pWorld, 0)) {
			AICtrl_GearAttack(g_pWorld->aiCtrl, cWorld_GetUsrEntity(g_pWorld, 0), 0);
		}
		return;
	case 3:
	case 4: {
		cEntity *t = b->target.target;
		if (!t) {
			break;
		}
		if (a->sameChar) {
			SENT_VT(self)->SetState(self, 2, t);
			return;
		}
		if (!self->seen) {
			WChar_ViewMode(p) = 1;
			return;
		}
		Sentinel_Relax(s);
		AICtrl_RaiseLevel(g_pWorld->aiCtrl, 1);
		WChar_ViewMode(p) = 2;
		return;
	}
	case 5:
		WChar_B381(p) = 0;
		WChar_ViewMode(p) = a->sameChar ? 2 : 1;
		if (ai->vftable->GetState(ai)) {
			self->time = g_SentinelGotoTime;
			return;
		}
		if (0.0f < self->time) {
			ai->vftable->SetState(ai, 0);
			cActCtrl_PlayNew(ChType_ActCtrl(s), "Relax0", 0, 1.0f);
			WChar(s)->control.hRot = 0.5f;
			return;
		}
		WChar(s)->control.hRot = 0.0f;
		break;
	case 6:
		if (!g_pWorld->alarm.on && b->target.target && ai->vftable->GetState(ai)) {
			if (!CGameModel_IsVisible(Obj_Vtbl(p)->GetModel(p))) {
				float d = cWorld_GetMinCamDistSq(g_pWorld, p->vftable->GetPos(p, &tmp));
				float r = g_SentinelAbortyNPCAttDist;
				if (d > r * r) {
					break;
				}
			}
			if (Logic_CharAIVtbl(ai)->Vf94(ai)) {
				self->time = g_SentinelAcuseTime;
			}
			WChar_B381(p) = 0;
			WChar_ViewMode(p) = 0;
			AICtrl_GearAttack(g_pWorld->aiCtrl, b->target.target, 0);
			cLocalLogic *l = Logic_GetLocalLogic(b->target.target);
			if (l) {
				l->vftable->OnSeen(l, p);
			}
			return;
		}
		break;
	default:
		CHECK(0);
		return;
	}
	cSentinelLocalLogic_Return(self);
}

/* vf24 */
byte __thiscall cSentinelLocalLogic_RestartState(cSentinelLocalLogic *self) {
	return SENT_VT(self)->SetState(self, 0, NULL);
}

void __fastcall cSentinelLocalLogic_Return(void *sentinel) {
	cSentinelLocalLogic *self = sentinel;

	if (self->base.idle || self->base.alwaysLock) {
		return;
	}
	if (g_pWorld->alarm.on) {
		SENT_VT(self)->SetState(self, 1, NULL);
		return;
	}
	SENT_VT(self)->SetState(self, 0, NULL);
}

/* vf19: the AI of the state; the result of the base */
byte __thiscall cSentinelLocalLogic_SetState(cSentinelLocalLogic *self, int state, cEntity *target) {
	cBaseLocalLogic *b = &self->base;
	vec3_t tmp;
	cAIChar *ai = b->parent->vftable->GetAI(b->parent);
	byte ok = (byte)cBaseLocalLogic_SetState(b, state, target);

	switch (b->state) {
	case 0:
		if (ok) {
			ai->vftable->SetState(ai, 6, b->origin, (double)b->actionRadius, (double)g_SentinelRelaxSpeed, 1);
		}
		self->time = 0.0f;
		return ok;
	case 1:
		if (ok) {
			ai->vftable->SetState(ai, 6, b->origin, (double)b->actionRadius, (double)g_SentinelSearchSpeed, 1);
		}
		self->time = 0.0f;
		return ok;
	case 2:
		if (ok && target) {
			ai->vftable->SetState(ai, 2, target, (double)g_SentinelAcuseSpeed);
		}
		self->time = g_SentinelAcuseTime;
		return ok;
	case 3:
		if (ok && target) {
			ai->vftable->SetState(ai, 3, target, (double)g_SentinelInspectSpeed);
		}
		self->time = g_SentinelInspectTime;
		return ok;
	case 4:
		if (ok && target) {
			ai->vftable->SetState(ai, 3, target, (double)g_SentinelSuspectSpeed);
		}
		self->time = g_SentinelSuspectTime;
		return ok;
	case 5:
		if (!ok) {
			return ok;
		}
		if (!target) {
			cSentinelLocalLogic_Return(self);
			return ok;
		}
		cSentinelLocalLogic_Watch(self, target->vftable->GetPos(target, &tmp));
		return ok;
	case 6:
		if (ok && target) {
			ai->vftable->SetState(ai, 2, target, (double)g_SentinelAcuseSpeed);
			self->time = g_SentinelAcuseTime;
		}
		return ok;
	}
	return ok;
}
