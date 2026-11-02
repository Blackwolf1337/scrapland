/*
 * Logic/LittlePoliceLogic.cpp of the game: cLittlePoliceLocalLogic (include/scrap/logic.h), the police of the
 * indoor maps, a GenChar logic. It plays its siren with the alarm, looks for the player (accuse: state 2, the AI
 * chases with state 3), asks characters for money when LittlePoliceAsksForMoney (the special attack, state 4, with
 * the bribe options of the arrow when it is the player), takes a character it accuses to the origin (states 7 and
 * 8) and walks to the main mission for the script ("GotoMainMission").
 */
#include <string.h>

#include "scrap/chartype.h"
#include "scrap/scrapmodel.h"
#include "scrap/weapons.h"


static inline const cLittlePoliceLocalLogicVtbl *LP_VT(const void *l) {
	return *(const cLittlePoliceLocalLogicVtbl *const *)l;
}
#define LP_BASE(self) (&(self)->base.base)

/* the AI of the character still has its target */
static inline byte LP_AIHasTarget(cAIChar *ai) {
	return ai->vftable->GetDirect(ai);
}

static inline float LP_DistSq(const vec3_t *a, const vec3_t *b) {
	float dy = a->y - b->y, dx = a->x - b->x, dz = a->z - b->z;
	return dy * dy + dx * dx + dz * dz;
}

void *cLittlePoliceLocalLogic_ctor(void *mem, cEntity *parent) {
	cLittlePoliceLocalLogic *self = mem;

	cGenCharLocalLogic_ctor(&self->base, parent, 0);
	self->base.base.vftable = CLITTLEPOLICELOCALLOGIC_VFTABLE;
	self->lostTime = 0.0f;
	float t = (float)(Weap_Rand01() * (double)0.2f + 0.0);
	self->policeResult = 0;
	self->bribe = 0;
	self->sirenTime = t;
	return self;
}

cLocalLogic *cLittlePoliceLocalLogic_Delete(cLocalLogic *elem, byte flags) {
	cLittlePoliceLocalLogic *self = (cLittlePoliceLocalLogic *)elem;
	self->base.base.vftable = CLITTLEPOLICELOCALLOGIC_VFTABLE;
	cGenCharLocalLogic_dtor(&self->base);
	if (flags & 1) {
		operator_delete_sized(self, sizeof(*self));
	}
	return elem;
}

/* vf26: the view flag without the alarm (in state 4 only with the bribe), team 2 (0 with the alarm) */
void cLittlePoliceLocalLogic_UpdateView(cLocalLogic *base) {
	cLittlePoliceLocalLogic *self = (cLittlePoliceLocalLogic *)base;
	cEntity *p = LP_BASE(self)->parent;

	WChar(p)->wl.arrow.canUse = !LP_BASE(self)->busy && !g_pWorld->alarm.on && (self->bribe ||
		LP_BASE(self)->state != 4);
	WLife(p)->arrow.tgType = g_pWorld->alarm.on ? 0 : 2;
}

/* the money of the player in state 4: the bribe options of the arrow while the talk runs, else the talk option */
void cLittlePoliceLocalLogic_UpdateBribe(cLittlePoliceLocalLogic *self) {
	char buf[0x100];
	cBaseLocalLogic *b = LP_BASE(self);
	byte old = self->bribe;
	cEntity *p = b->parent;
	byte usrAttack = 0;

	if (!b->busy && b->state == 4) {
		cEntity *t = b->target.target;
		if (t == ChType_Usr0()) {
			usrAttack = 1;
			if (!cGenCharGroup_TakeAttack((&g_pWorld->logic.genChars))) {
				Logic_Vtbl(self)->RestartState(&self->base.base.logic);
				return;
			}
		} else {
			cBaseLocalLogic *lt = (cBaseLocalLogic *)Logic_GetLocalLogic(b->target.target);
			cEntity *te = b->target.target;
			if (lt && (((lt->idle || lt->alwaysLock) && WChar_Listen(te) != p) ||
				!WChar(te)->wl.arrow.canUse)) {
				self->base.searchTime = g_GenCharMaxSpecialActionTime;
				Logic_Vtbl(self)->SetState(&self->base.base.logic, 0, NULL);
				return;
			}
		}
	}
	byte action = cActCtrl_IsActive(ChType_ActCtrl(p), "ActionNPC");
	byte talk = !strncmp(cActCtrl_GetActionName(ChType_ActCtrl(p), 0), "Talk", 4) ||
		cActCtrl_IsActive(ChType_ActCtrl(p), "FaceToTalk") || cActCtrl_IsActive(ChType_ActCtrl(p), "TalkListenNPC");
	self->bribe = usrAttack && (action || talk);
	if (usrAttack) {
		cGenCharGroup_RaiseLevel((&g_pWorld->logic.genChars), 0);
	}
	if (self->bribe == old) {
		return;
	}
	cArrowIndicator *arrow = &WLife(p)->arrow;
	if (self->bribe) {
		cArrowIndicator_ClearOpts(arrow);
		Str_PrintfQuiet(buf, sizeof(buf), "GiveWhy:%s", Lang_GetStr("Generics_PoliceBribeGiveWhyOpt"));
		cArrowIndicator_AddOpt(arrow, buf);
		memcpy(buf, "GiveYes:", 9);
		int amount = g_PoliceStealAmount;
		Str_PrintfQuiet(buf + 8, sizeof(buf) - 8, Lang_GetStr("Generics_PoliceBribeGiveYesOpt"), amount);
		cArrowIndicator_AddOpt(arrow, buf);
		Str_PrintfQuiet(buf, sizeof(buf), "GiveNo:%s", Lang_GetStr("Generics_PoliceBribeGiveNoOpt"));
	} else {
		cArrowIndicator_ClearOpts(arrow);
		Str_PrintfQuiet(buf, sizeof(buf), "Talk:%s", Lang_GetStr("TalkOpt"));
	}
	cArrowIndicator_AddOpt(arrow, buf);
}

/* vf31: the special attack ends without a target, or when the AI lost it and it is out of view for longer than
 * LittlePoliceChasingTime */
byte cLittlePoliceLocalLogic_TargetDone(cLocalLogic *elem) {
	cLittlePoliceLocalLogic *self = (cLittlePoliceLocalLogic *)elem;
	vec3_t t;
	cBaseLocalLogic *b = LP_BASE(self);

	if (!b->target.target) {
		return 1;
	}
	cEntity *p = b->parent;
	byte keep;
	if (LP_AIHasTarget(p->vftable->GetAI(p))) {
		keep = 1;
	} else {
		cEntity *te = b->target.target;
		keep = Logic_InView(p, te->vftable->GetPos(te, &t), g_GenCharStdFar, g_GenCharStdFovY, g_GenCharStdFovX, 1);
	}
	if (keep) {
		self->base.specialTime = 0.0f;
	}
	if (g_LittlePoliceChasingTime >= self->base.specialTime) {
		keep = 1;
	}
	return keep ^ 1;
}

/* vf07: busy too while it fuses or laughs */
byte cLittlePoliceLocalLogic_UpdateBusy(cLocalLogic *base) {
	cLittlePoliceLocalLogic *self = (cLittlePoliceLocalLogic *)base;
	cBaseLocalLogic *b = LP_BASE(self);

	b->dazed = 0;
	if (!strcmp(cActCtrl_GetActionName(ChType_ActCtrl(b->parent), 0), "Fusion") ||
		!strcmp(cActCtrl_GetActionName(ChType_ActCtrl(b->parent), 0), "Laugh")) {
		b->busy = 1;
		return 1;
	}
	return cBaseLocalLogic_UpdateBusy(&b->logic);
}

/* vf30: with LittlePoliceAsksForMoney at the search time a character of the first or third GenChar list in view,
 * else the player in view unless it is a police, gear or sentinel */
cEntity *cLittlePoliceLocalLogic_FindTarget(cLocalLogic *elem) {
	cLittlePoliceLocalLogic *self = (cLittlePoliceLocalLogic *)elem;
	vec3_t t;

	if (!g_LittlePoliceAsksForMoney || !(0.0f > self->base.searchTime)) {
		return NULL;
	}
	self->base.searchTime = g_GenCharMinSpecialActionTime * 0.5f;
	cEntity *usr = cWorld_GetUsrEntity(g_pWorld, 0);
	if (!usr) {
		return NULL;
	}
	cEntity *p = LP_BASE(self)->parent;
	cEntity *e = Logic_FindInView(p, g_pWorld->logic.genChars.genChars, g_GenCharStdFar, g_GenCharStdFovY,
		g_GenCharStdFovX, 1, NULL);
	if (e) {
		return e;
	}
	e = Logic_FindInView(p, g_pWorld->logic.genChars.bishops, g_GenCharStdFar, g_GenCharStdFovY, g_GenCharStdFovX, 1,
		NULL);
	if (e) {
		return e;
	}
	if (Logic_InView(p, usr->vftable->GetPos(usr, &t), g_GenCharStdFar, g_GenCharStdFovY, g_GenCharStdFovX, 1)) {
		e = usr;
	}
	if (!ChType_ActCtrl(usr) || !strncmp(Logic_TypeName(usr), "Police", 6) || !strcmp(Logic_TypeName(usr), "Gear") ||
		!strcmp(Logic_TypeName(usr), "Sentinel")) {
		return NULL;
	}
	return e;
}

/* to the first active object with the radar mark 1 (state 10), unless near it already (MainMissionStopDist) */
byte cLittlePoliceLocalLogic_GoMainMission(cLittlePoliceLocalLogic *self) {
	vec3_t t1, t2;

	self->policeResult = 0;
	cEntity *e = cWorld_GetFirstEntity(g_pWorld);
	while (e && !(e->active && e->vftable->IsType(e, TYEN_WITH_LIFE) && Obj(e)->radarShow == 1)) {
		e = cEntity_Next(e);
	}
	if (!e) {
		return 0;
	}
	cEntity *p = LP_BASE(self)->parent;
	const vec3_t *pp = p->vftable->GetPos(p, &t1);
	const vec3_t *ep = e->vftable->GetPos(e, &t2);
	if (g_MainMissionStopDist * g_MainMissionStopDist > LP_DistSq(ep, pp)) {
		g_LittlePoliceArrived = 0;
	} else {
		Logic_Vtbl(self)->SetState(&self->base.base.logic, GENCHAR_GET_IT, e);
		if (g_LittlePoliceArrived) {
			self->policeResult = 1;
			return 1;
		}
	}
	Logic_Vtbl(self)->RestartState(&self->base.base.logic);
	return 0;
}

/* vf28 */
void cLittlePoliceLocalLogic_ToAlert(cLocalLogic *base, cAIChar *ai) {
	cLittlePoliceLocalLogic *self = (cLittlePoliceLocalLogic *)base;
	cBaseLocalLogic *b = LP_BASE(self);

	ai->vftable->SetState(ai, 6, b->origin, (double)b->actionRadius, (double)g_GenCharAlertSpeed, 1);
}

/* vf03 */
byte cLittlePoliceLocalLogic_VarI(cLocalLogic *elem, const char *name, int *i, byte set) {
	cLittlePoliceLocalLogic *self = (cLittlePoliceLocalLogic *)elem;
	if (!strcmp(name, "PoliceResult")) {
		if (!set) {
			*i = self->policeResult;
			return 1;
		}
		self->policeResult = *i;
		return 1;
	}
	if (set && *i == 1 && !strcmp(name, "GotoMainMission")) {
		cLittlePoliceLocalLogic_GoMainMission(self);
		return 1;
	}
	return cGenCharLocalLogic_VarI(&self->base.base.logic, name, i, set);
}

/* the sirens: with the alarm or in the states 7 and 8 */
static void LP_Siren(cLittlePoliceLocalLogic *self, int st, byte alarm, cEntity *p) {
	void *ctrl;

	if (st == 7 || st == 8 || (alarm && 0.0f >= self->sirenTime)) {
		if (!*cActCtrl_GetActionName(ChType_ActCtrl(p), 1) ||
			(strcmp(cActCtrl_GetActionName(ChType_ActCtrl(p), 1), "SirenInit") &&
			strcmp(cActCtrl_GetActionName(ChType_ActCtrl(p), 1), "Siren"))) {
			ctrl = ChType_ActCtrl(p);
			cActCtrl_Play(ctrl, "SirenInit", 0, 1.0f);
			self->sirenTime = (float)(Weap_Rand01() * (double)0.4f + 0.0);
		}
	} else if (*cActCtrl_GetActionName(ChType_ActCtrl(p), 1) &&
		strcmp(cActCtrl_GetActionName(ChType_ActCtrl(p), 1), "SirenEnd")) {
		ctrl = ChType_ActCtrl(p);
		cActCtrl_Play(ctrl, "SirenEnd", 0, 1.0f);
	}
}

/* vf17 */
void cLittlePoliceLocalLogic_Update(cLocalLogic *elem, float dt) {
	cLittlePoliceLocalLogic *self = (cLittlePoliceLocalLogic *)elem;
	cBaseLocalLogic *b = LP_BASE(self);
	vec3_t t1, t2;

	LP_VT(self)->UpdateLook(&self->base.base.logic);
	cLittlePoliceLocalLogic_UpdateBribe(self);
	int st = b->state;
	byte alarm = g_pWorld->alarm.on;
	if (alarm) {
		self->sirenTime = self->sirenTime - dt;
	}
	cEntity *p = b->parent;
	LP_Siren(self, st, alarm, p);
	if (st == 1) {
		if (cActCtrl_IsActive(ChType_ActCtrl(p), "Relax")) {
			cActCtrl_PlayNew(ChType_ActCtrl(p), "AlertCheck", 0, 1.0f);
		} else if (cActCtrl_IsActive(ChType_ActCtrl(p), "AlertCheck")) {
			vec3_t v = WChar(p)->view, a;
			a.x = v.x + 0.0f;
			a.y = v.y + dt;
			a.z = v.z + 0.0f;
			WChar_Vtbl(p)->SetViewAng(p, &a);
		}
	} else if (st == 2 || st == 7 || st == 8) {
		cBaseLocalLogic_Update(&b->logic, dt);
		LP_VT(self)->base.UpdateView(&self->base.base.logic);
		cEntity *usr = ChType_Usr0();
		if (!usr || b->busy) {
			return;
		}
		byte accuse = g_pWorld->alarm.sameChar && b->target.target;
		cAIChar *ai = b->parent->vftable->GetAI(b->parent);
		switch (b->state) {
		case 2: {
			if (ai->vftable->GetState(ai) != 3) {
				if (!accuse) {
					Logic_Vtbl(self)->SetState(&self->base.base.logic, 1, NULL);
					return;
				}
				if (!Logic_InView(p, usr->vftable->GetPos(usr, &t1), g_LittlePoliceMaxDistFar, g_GenCharStdFovY,
					g_GenCharStdFovX, 1)) {
					ai->vftable->SetState(ai, 3, b->target.target, 1.0);
					return;
				}
				cGenCharLocalLogic_Discover(&self->base);
				cGenCharGroup_RaiseLevel((&g_pWorld->logic.genChars), 0);
				Alarm_Set(&g_pWorld->alarm, 1.0f);
				AICtrl_GearAttack(&g_pWorld->logic.aiCtrl, usr, 0);
				return;
			}
			if (!alarm) {
				Logic_Vtbl(self)->RestartState(&self->base.base.logic);
				self->lostTime = 0.0f;
				return;
			}
			vec3_t o = b->origin;
			if (LP_DistSq(p->vftable->GetPos(p, &t1), &o) > b->actionRadius * b->actionRadius) {
				goto alert;
			}
			self->lostTime = self->lostTime + dt;
			const vec3_t *up = usr->vftable->GetPos(usr, &t1);
			const vec3_t *pp = p->vftable->GetPos(p, &t2);
			if (g_GenCharStdFar * g_GenCharStdFar >= LP_DistSq(pp, up) &&
				Logic_InView(p, usr->vftable->GetPos(usr, &t1), g_GenCharStdFar, g_GenCharStdFovY, g_GenCharStdFovX, 1)) {
				if (!accuse) {
					goto alert;
				}
				ai->vftable->SetState(ai, 8, b->target.target, 1);
				self->lostTime = 0.0f;
				return;
			}
			if (!accuse || self->lostTime > g_LittlePoliceChasingTime) {
				goto alert;
			}
			return;
alert:
			Logic_Vtbl(self)->SetState(&self->base.base.logic, 1, NULL);
			self->lostTime = 0.0f;
			return;
		}
		case 7: {
			cEntity *te = b->target.target;
			cLocalLogic *lt;
			if (!te || !(lt = Logic_GetLocalLogic(te))) {
				Logic_Vtbl(self)->RestartState(&self->base.base.logic);
				return;
			}
			if (!CGameModel_IsVisible(Obj_Vtbl(p)->GetModel(p))) {
				const vec3_t *up = usr->vftable->GetPos(usr, &t1);
				if (LP_DistSq(p->vftable->GetPos(p, &t2), up) >
					g_LittlePoliceStopAcusingMaxDistFromUser * g_LittlePoliceStopAcusingMaxDistFromUser) {
					Logic_Vtbl(self)->RestartState(&self->base.base.logic);
					Logic_Vtbl(lt)->RestartState(lt);
					return;
				}
			}
			const vec3_t *tp = te->vftable->GetPos(te, &t1);
			const vec3_t *pp = p->vftable->GetPos(p, &t2);
			float far = g_LittlePoliceMaxDistFar;
			float dy = pp->y - tp->y, dx = pp->x - tp->x, dz = pp->z - tp->z;
			if (far * far >= dy * dy + dx * dx + dz * dz &&
				Logic_InView(p, te->vftable->GetPos(te, &t1), far, g_GenCharStdFovY, g_GenCharStdFovX, 1)) {
				cGenCharLocalLogic_Discover(&self->base);
				Logic_Vtbl(lt)->OnAlarm(lt, p);
				return;
			}
			Logic_Vtbl(self)->SetState(&self->base.base.logic, 8, te);
			Logic_Vtbl(lt)->OnAlarm(lt, p);
			return;
		}
		case 8: {
			cEntity *te = b->target.target;
			cLocalLogic *lt;
			if (!te || !(lt = Logic_GetLocalLogic(te))) {
				Logic_Vtbl(self)->RestartState(&self->base.base.logic);
				return;
			}
			if (!CGameModel_IsVisible(Obj_Vtbl(p)->GetModel(p))) {
				const vec3_t *up = usr->vftable->GetPos(usr, &t1);
				if (LP_DistSq(p->vftable->GetPos(p, &t2), up) >
					g_LittlePoliceStopAcusingMaxDistFromUser * g_LittlePoliceStopAcusingMaxDistFromUser) {
					Logic_Vtbl(self)->RestartState(&self->base.base.logic);
					Logic_Vtbl(lt)->RestartState(lt);
					return;
				}
			}
			vec3_t o = b->origin;
			if (LP_DistSq(p->vftable->GetPos(p, &t1), &o) > b->actionRadius * b->actionRadius) {
				cActCtrl_Play(ChType_ActCtrl(p), "ActionBad", 1, 1.0f);
				cActCtrl_SetDefLoop(ChType_ActCtrl(p), "Relax", 1);
				Logic_Vtbl(self)->RestartState(&self->base.base.logic);
				Logic_Vtbl(lt)->RestartState(lt);
				return;
			}
			if (Logic_InView(p, te->vftable->GetPos(te, &t1), g_GenCharStdFar, g_GenCharStdFovY, g_GenCharStdFovX, 1)) {
				Logic_Vtbl(self)->SetState(&self->base.base.logic, 7, te);
			}
			Logic_Vtbl(lt)->SetState(lt, 3, p);
			return;
		}
		}
		return;
	} else if (st == 4) {
		self->base.specialTime = self->base.specialTime + dt;
	}
	cGenCharLocalLogic_Update(&self->base.base.logic, dt);
}

/* vf10: the alarm when the player hits it */
void cLittlePoliceLocalLogic_OnHit(cLocalLogic *elem, cEntity *attacker) {
	cLittlePoliceLocalLogic *self = (cLittlePoliceLocalLogic *)elem;
	if (attacker == ChType_Usr0()) {
		Alarm_Set(&g_pWorld->alarm, 1.0f);
	}
	cGenCharLocalLogic_OnHit(&self->base.base.logic, attacker);
}

/* vf11: the player seen: the alarm and the accuse */
void cLittlePoliceLocalLogic_OnSeen(cLocalLogic *base, cEntity *e) {
	cLittlePoliceLocalLogic *self = (cLittlePoliceLocalLogic *)base;
	if (LP_BASE(self)->busy) {
		return;
	}
	if (e == ChType_Usr0()) {
		Alarm_Set(&g_pWorld->alarm, 1.0f);
		Logic_Vtbl(self)->SetState(&self->base.base.logic, 2, e);
	}
}

/* vf15: the same, returns 0 */
byte cLittlePoliceLocalLogic_GivesMoney(cLocalLogic *base, cEntity *e) {
	cLittlePoliceLocalLogic *self = (cLittlePoliceLocalLogic *)base;
	cLittlePoliceLocalLogic_OnSeen(&self->base.base.logic, e);
	return 0;
}

/* vf36: the view of the AI; not guaranteed it sees the player in its far view: the alarm and the accuse */
void cLittlePoliceLocalLogic_UpdateLook(cLocalLogic *base) {
	cLittlePoliceLocalLogic *self = (cLittlePoliceLocalLogic *)base;
	vec3_t t;
	cBaseLocalLogic *b = LP_BASE(self);
	cEntity *p = b->parent;

	if (b->busy) {
		WLife(p)->arrow.coneStatus = 0;
		return;
	}
	cEntity *usr = ChType_Usr0();
	WLife(p)->arrow.coneFov = g_GenCharStdFovY;
	WLife(p)->arrow.coneDist = g_LittlePoliceMaxDistFar;
	if (g_pWorld->alarm.on) {
		cEntity *te = b->target.target;
		if (te && b->state == 2 && te == cWorld_GetUsrEntity(g_pWorld, 0)) {
			WLife(p)->arrow.coneStatus = 3;
		} else {
			WLife(p)->arrow.coneStatus = 2;
		}
		return;
	}
	if (!g_pWorld->alarm.notGuaranteed) {
		WLife(p)->arrow.coneStatus = 0;
		return;
	}
	WLife(p)->arrow.coneStatus = 1;
	if (usr && Logic_InView(p, usr->vftable->GetPos(usr, &t), g_LittlePoliceMaxDistFar, g_GenCharStdFovY,
		g_GenCharStdFovX, 1)) {
		Alarm_Set(&g_pWorld->alarm, 1.0f);
		Logic_Vtbl(self)->SetState(&self->base.base.logic, 2, usr);
	}
}

/* vf35: state 10, the walk to the player that asks for money: the special attack when the AI is near it, back to
 * the default state after 5 s at more than 2000 */
void cLittlePoliceLocalLogic_UpdateGetIt(cLocalLogic *base, cAIChar *ai, float dt) {
	cLittlePoliceLocalLogic *self = (cLittlePoliceLocalLogic *)base;
	vec3_t t1, t2;
	cBaseLocalLogic *b = LP_BASE(self);

	if (b->idle || b->alwaysLock) {
		return;
	}
	cEntity *usr = ChType_Usr0();
	if (usr && usr->vftable->IsType(usr, TYEN_WALKCHAR)) {
		cEntity *p = b->parent;
		const vec3_t *up = usr->vftable->GetPos(usr, &t1);
		const vec3_t *pp = p->vftable->GetPos(p, &t2);
		float d = (float)__builtin_sqrt((double)LP_DistSq(pp, up));
		if (!(d > 2000.0f)) {
			if (ai->vftable->GetState(ai) == 0) {
				ai->vftable->SetState(ai, 8, usr, 1);
				return;
			}
			if (ai->vftable->GetState(ai) != 8) {
				self->lostTime = 5.0f;
				return;
			}
			if (g_EnergyBarActive != 1 || g_SpecialActionActive != 2) {
				Logic_Vtbl(self)->SetState(&self->base.base.logic, 0, usr);
				return;
			}
			if (!(500.0f > d) || !ChType_ActCtrl(usr) || !strcmp(Logic_TypeName(usr), "Police")) {
				return;
			}
			Logic_Vtbl(self)->SetState(&self->base.base.logic, 4, usr);
			return;
		}
		float lt = self->lostTime - dt;
		self->lostTime = lt;
		if (!(0.0f >= lt)) {
			return;
		}
	}
	Logic_Vtbl(self)->RestartState(&self->base.base.logic);
}

/* vf34: the AI walks to e and stops at MainMissionStopDist */
void cLittlePoliceLocalLogic_GoTo(cLocalLogic *base, cAIChar *ai, cEntity *e) {
	cLittlePoliceLocalLogic *self = (cLittlePoliceLocalLogic *)base;
	vec3_t t1, t2, t3, d, dir;
	cEntity *p = LP_BASE(self)->parent;

	self->lostTime = 5.0f;
	const vec3_t *pp = p->vftable->GetPos(p, &t1);
	const vec3_t *ep = e->vftable->GetPos(e, &t2);
	d.y = ep->y - pp->y;
	d.x = ep->x - pp->x;
	d.z = ep->z - pp->z;
	float len = (float)__builtin_sqrt((double)(d.y * d.y + d.x * d.x + d.z * d.z));
	if (len == 0.0f) {
		dir = d;
	} else {
		float inv = 1.0f / len;
		dir.x = inv * d.x;
		dir.y = inv * d.y;
		dir.z = d.z * inv;
	}
	const vec3_t *tp = e->vftable->GetPos(e, &t3);
	ai->vftable->SetState(ai, 4, *tp, dir, (double)g_MainMissionStopDist, 1.0);
	g_LittlePoliceArrived = ai->vftable->GetState(ai) == 4;
}

/* vf20: a change of the idle restarts the search time (not in state 4) */
void cLittlePoliceLocalLogic_SetIdle(cLocalLogic *elem, byte idle) {
	cLittlePoliceLocalLogic *self = (cLittlePoliceLocalLogic *)elem;
	cBaseLocalLogic *b = LP_BASE(self);
	byte cur = b->idle || b->alwaysLock;

	if (idle != cur && b->state != 4) {
		self->base.searchTime = g_GenCharMaxSpecialActionTime;
	}
	cBaseLocalLogic_SetIdle(&b->logic, idle);
}

/* vf19: 8 the AI chases the target in alert, 7 the AI takes it */
byte cLittlePoliceLocalLogic_SetState(cLocalLogic *elem, int state, cEntity *target) {
	cLittlePoliceLocalLogic *self = (cLittlePoliceLocalLogic *)elem;
	cBaseLocalLogic *b = LP_BASE(self);

	if (state == 8) {
		if ((byte)cBaseLocalLogic_SetState(&b->logic, 8, target)) {
			LP_VT(self)->base.UpdateView(&self->base.base.logic);
			cAIChar *ai = b->parent->vftable->GetAI(b->parent);
			ai->vftable->SetState(ai, 3, target, (double)g_GenCharAlertSpeed);
		}
		return 1;
	}
	if (state == 7) {
		if ((byte)cBaseLocalLogic_SetState(&b->logic, 7, target)) {
			LP_VT(self)->base.UpdateView(&self->base.base.logic);
			cAIChar *ai = b->parent->vftable->GetAI(b->parent);
			ai->vftable->SetState(ai, 8, target, 1);
			cGenCharLocalLogic_Discover(&self->base);
		}
		return 1;
	}
	return cGenCharLocalLogic_SetState(&self->base.base.logic, state, target);
}

/* the vtable of cLittlePoliceLocalLogic */
const cLittlePoliceLocalLogicVtbl cLittlePoliceLocalLogic_vftable = {
	.base.base.base.Delete = cLittlePoliceLocalLogic_Delete,
	.base.base.base.GetName = cBaseLocalLogic_GetName,
	.base.base.base.Vf02 = cLocalLogic_Vf02True,
	.base.base.base.VarI = cLittlePoliceLocalLogic_VarI,
	.base.base.base.VarF = cBaseLocalLogic_VarF,
	.base.base.base.VarS = cGenCharLocalLogic_VarS,
	.base.base.base.VarV = cBaseLocalLogic_VarV,
	.base.base.UpdateBusy = cLittlePoliceLocalLogic_UpdateBusy,
	.base.base.Refresh = cBaseLocalLogic_Refresh,
	.base.base.Listens = cLocalLogic_ListensTrue,
	.base.base.OnHit = cLittlePoliceLocalLogic_OnHit,
	.base.base.OnSeen = cLittlePoliceLocalLogic_OnSeen,
	.base.base.OnAttack = cGenCharLocalLogic_OnRobbery,
	.base.base.OnShot = cBaseLocalLogic_OnShot,
	.base.base.OnHammer = cLocalLogic_OnHammerNop,
	.base.base.GivesMoney = cLittlePoliceLocalLogic_GivesMoney,
	.base.base.OnAlarm = cGenCharLocalLogic_OnAlarm,
	.base.base.Update = cLittlePoliceLocalLogic_Update,
	.base.base.SetOriginFrom = cBaseLocalLogic_SetOriginFrom,
	.base.base.SetState = cLittlePoliceLocalLogic_SetState,
	.base.base.SetIdle = cLittlePoliceLocalLogic_SetIdle,
	.base.base.SetActionRadius = cBaseLocalLogic_SetActionRadius,
	.base.base.SetOrigin = cBaseLocalLogic_SetOrigin,
	.base.base.SetOriginAng = cBaseLocalLogic_SetOriginAng,
	.base.base.RestartState = cGenCharLocalLogic_RestartState,
	.base.CanAttack = cGenCharLocalLogic_CanAttack,
	.base.UpdateView = cLittlePoliceLocalLogic_UpdateView,
	.base.ToRelax = cGenCharLocalLogic_ToRelax,
	.base.ToAlert = cLittlePoliceLocalLogic_ToAlert,
	.base.Accuse = Logic_SetState2,
	.base.FindTarget = cLittlePoliceLocalLogic_FindTarget,
	.base.TargetDone = cLittlePoliceLocalLogic_TargetDone,
	.base.Work = cLocalLogic_WorkNop,
	.base.Vf33 = cLocalLogic_Vf33Nop,
	.base.GoTo = cLittlePoliceLocalLogic_GoTo,
	.base.UpdateGetIt = cLittlePoliceLocalLogic_UpdateGetIt,
	.UpdateLook = cLittlePoliceLocalLogic_UpdateLook,
};
