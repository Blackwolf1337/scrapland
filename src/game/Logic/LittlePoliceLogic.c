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
#include "scrap/staticinit.h"

#define CLITTLEPOLICELOCALLOGIC_VFTABLE (&cLittlePoliceLocalLogic_vftable.base)
#define g_LittlePoliceStopAcusingMaxDistFromUser (GVar_LittlePoliceStopAcusingMaxDistFromUser.value.f)
#define g_LittlePoliceMaxDistFar (GVar_LittlePoliceMaxDistFar.value.f)
#define g_LittlePoliceAsksForMoney (GVar_LittlePoliceAsksForMoney.value.i)
#define g_LittlePoliceChasingTime (GVar_LittlePoliceChasingTime.value.f)
#define g_MainMissionStopDist (GVar_MainMissionStopDist.value.f)
static const cGenCharLocalLogicVtbl cLittlePoliceLocalLogic_vftable;
static cGVar GVar_MainMissionStopDist;
static cGVar GVar_LittlePoliceStopAcusingMaxDistFromUser;
static cGVar GVar_LittlePoliceMaxDistFar;
static cGVar GVar_LittlePoliceChasingTime;
static cGVar GVar_LittlePoliceAsksForMoney;

#define LP_BASE(self) (&(self)->base.base)

/* the AI of the character still has its target */
static inline byte LP_AIHasTarget(cAIChar *ai) {
	return AIChar_GetDirect(ai);
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

static void cLittlePoliceLocalLogic_Free(cLocalLogic *elem) {
	cLittlePoliceLocalLogic *self = (cLittlePoliceLocalLogic *)elem;
	cGenCharLocalLogic_dtor(&self->base);
	operator_delete(self);
}

/* the view flag without the alarm (in state 4 only with the bribe), team 2 (0 with the alarm) */
static void cLittlePoliceLocalLogic_UpdateView(cLocalLogic *base) {
	cLittlePoliceLocalLogic *self = (cLittlePoliceLocalLogic *)base;
	cEntity *p = LP_BASE(self)->parent;

	WChar(p)->wl.arrow.canUse = !LP_BASE(self)->busy && !g_pWorld->alarm.on && (self->bribe ||
		LP_BASE(self)->state != GENCHAR_ATTACK);
	WLife(p)->arrow.tgType = g_pWorld->alarm.on ? 0 : 2;
}

/* the money of the player in state 4: the bribe options of the arrow while the talk runs, else the talk option */
static void cLittlePoliceLocalLogic_UpdateBribe(cLittlePoliceLocalLogic *self) {
	char buf[0x100];
	cBaseLocalLogic *b = LP_BASE(self);
	byte old = self->bribe;
	cEntity *p = b->parent;
	byte usrAttack = 0;

	if (!b->busy && b->state == GENCHAR_ATTACK) {
		cEntity *t = b->target.target;
		if (t == ChType_Usr0()) {
			usrAttack = 1;
			if (!cGenCharGroup_TakeAttack((&g_pWorld->logic.genChars))) {
				Logic_RestartState(self);
				return;
			}
		} else {
			cBaseLocalLogic *lt = (cBaseLocalLogic *)Logic_GetLocalLogic(b->target.target);
			cEntity *te = b->target.target;
			if (lt && (((lt->idle || lt->alwaysLock) && WChar_Listen(te) != p) ||
				!WChar(te)->wl.arrow.canUse)) {
				self->base.searchTime = g_GenCharMaxSpecialActionTime;
				Logic_SetState(self, GENCHAR_RELAX, NULL);
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

/* the special attack ends without a target, or when the AI lost it and it is out of view for longer than
 * LittlePoliceChasingTime */
static byte cLittlePoliceLocalLogic_TargetDone(cLocalLogic *elem) {
	cLittlePoliceLocalLogic *self = (cLittlePoliceLocalLogic *)elem;
	vec3_t t;
	cBaseLocalLogic *b = LP_BASE(self);

	if (!b->target.target) {
		return 1;
	}
	cEntity *p = b->parent;
	byte keep;
	if (LP_AIHasTarget(Entity_GetAI(p))) {
		keep = 1;
	} else {
		cEntity *te = b->target.target;
		keep = Logic_InView(p, Entity_GetPos(te, &t), g_GenCharStdFar, g_GenCharStdFovY, g_GenCharStdFovX, 1);
	}
	if (keep) {
		self->base.specialTime = 0.0f;
	}
	if (g_LittlePoliceChasingTime >= self->base.specialTime) {
		keep = 1;
	}
	return keep ^ 1;
}

/* busy too while it fuses or laughs */
static byte cLittlePoliceLocalLogic_UpdateBusy(cLocalLogic *base) {
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

/* with LittlePoliceAsksForMoney at the search time a character of the first or third GenChar list in view
 * else the player in view unless it is a police, gear or sentinel */
static cEntity *cLittlePoliceLocalLogic_FindTarget(cLocalLogic *elem) {
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
	if (Logic_InView(p, Entity_GetPos(usr, &t), g_GenCharStdFar, g_GenCharStdFovY, g_GenCharStdFovX, 1)) {
		e = usr;
	}
	if (!ChType_ActCtrl(usr) || !strncmp(Logic_TypeName(usr), "Police", 6) || !strcmp(Logic_TypeName(usr), "Gear") ||
		!strcmp(Logic_TypeName(usr), "Sentinel")) {
		return NULL;
	}
	return e;
}

static byte g_LittlePoliceArrived;

/* to the first active object with the radar mark 1 (state 10), unless near it already (MainMissionStopDist) */
static byte cLittlePoliceLocalLogic_GoMainMission(cLittlePoliceLocalLogic *self) {
	vec3_t t1, t2;

	self->policeResult = 0;
	cEntity *e = cWorld_GetFirstEntity(g_pWorld);
	while (e && !(e->active && Entity_IsType(e, TYEN_WITH_LIFE) && Obj(e)->radarShow == 1)) {
		e = cEntity_Next(e);
	}
	if (!e) {
		return 0;
	}
	cEntity *p = LP_BASE(self)->parent;
	const vec3_t *pp = Entity_GetPos(p, &t1);
	const vec3_t *ep = Entity_GetPos(e, &t2);
	if (g_MainMissionStopDist * g_MainMissionStopDist > LP_DistSq(ep, pp)) {
		g_LittlePoliceArrived = 0;
	} else {
		Logic_SetState(self, GENCHAR_GET_IT, e);
		if (g_LittlePoliceArrived) {
			self->policeResult = 1;
			return 1;
		}
	}
	Logic_RestartState(self);
	return 0;
}

static void cLittlePoliceLocalLogic_ToAlert(cLocalLogic *base, cAIChar *ai) {
	cLittlePoliceLocalLogic *self = (cLittlePoliceLocalLogic *)base;
	cBaseLocalLogic *b = LP_BASE(self);

	AIChar_SetState(ai, AIC_PATROL_ZONE, b->origin, (double)b->actionRadius, (double)g_GenCharAlertSpeed, 1);
}

static byte cLittlePoliceLocalLogic_VarI(cLocalLogic *elem, const char *name, int *i, byte set) {
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

	if (st == LPOLICE_DISCOVER || st == LPOLICE_CHASE || (alarm && 0.0f >= self->sirenTime)) {
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

static void cLittlePoliceLocalLogic_UpdateLook(cLittlePoliceLocalLogic *self);

static void cLittlePoliceLocalLogic_Update(cLocalLogic *elem, float dt) {
	cLittlePoliceLocalLogic *self = (cLittlePoliceLocalLogic *)elem;
	cBaseLocalLogic *b = LP_BASE(self);
	vec3_t t1, t2;

	cLittlePoliceLocalLogic_UpdateLook(self);
	cLittlePoliceLocalLogic_UpdateBribe(self);
	int st = b->state;
	byte alarm = g_pWorld->alarm.on;
	if (alarm) {
		self->sirenTime = self->sirenTime - dt;
	}
	cEntity *p = b->parent;
	LP_Siren(self, st, alarm, p);
	if (st == GENCHAR_ALERT) {
		if (cActCtrl_IsActive(ChType_ActCtrl(p), "Relax")) {
			cActCtrl_PlayNew(ChType_ActCtrl(p), "AlertCheck", 0, 1.0f);
		} else if (cActCtrl_IsActive(ChType_ActCtrl(p), "AlertCheck")) {
			vec3_t v = WChar(p)->view, a;
			a.x = v.x + 0.0f;
			a.y = v.y + dt;
			a.z = v.z + 0.0f;
			cWalkCharEntity_SetViewAng(WChar(p), &a);
		}
	} else if (st == GENCHAR_ACCUSE || st == LPOLICE_DISCOVER || st == LPOLICE_CHASE) {
		cBaseLocalLogic_Update(&b->logic, dt);
		GenChar_UpdateView(self);
		cEntity *usr = ChType_Usr0();
		if (!usr || b->busy) {
			return;
		}
		byte accuse = g_pWorld->alarm.sameChar && b->target.target;
		cAIChar *ai = Entity_GetAI(b->parent);
		switch (b->state) {
		case GENCHAR_ACCUSE: {
			if (AIChar_GetState(ai) != AIC_CHASE_WITHOUT_ACTION) {
				if (!accuse) {
					Logic_SetState(self, GENCHAR_ALERT, NULL);
					return;
				}
				if (!Logic_InView(p, Entity_GetPos(usr, &t1), g_LittlePoliceMaxDistFar, g_GenCharStdFovY,
					g_GenCharStdFovX, 1)) {
					AIChar_SetState(ai, AIC_CHASE_WITHOUT_ACTION, b->target.target, 1.0);
					return;
				}
				cGenCharLocalLogic_Discover(&self->base);
				cGenCharGroup_RaiseLevel((&g_pWorld->logic.genChars), 0);
				Alarm_Set(&g_pWorld->alarm, 1.0f);
				AICtrl_GearAttack(&g_pWorld->logic.aiCtrl, usr, 0);
				return;
			}
			if (!alarm) {
				Logic_RestartState(self);
				self->lostTime = 0.0f;
				return;
			}
			vec3_t o = b->origin;
			if (LP_DistSq(Entity_GetPos(p, &t1), &o) > b->actionRadius * b->actionRadius) {
				goto alert;
			}
			self->lostTime = self->lostTime + dt;
			const vec3_t *up = Entity_GetPos(usr, &t1);
			const vec3_t *pp = Entity_GetPos(p, &t2);
			if (g_GenCharStdFar * g_GenCharStdFar >= LP_DistSq(pp, up) &&
				Logic_InView(p, Entity_GetPos(usr, &t1), g_GenCharStdFar, g_GenCharStdFovY, g_GenCharStdFovX, 1)) {
				if (!accuse) {
					goto alert;
				}
				AIChar_SetState(ai, AIC_ORIENT_DYNAMIC_OBJECTIVE, b->target.target, 1);
				self->lostTime = 0.0f;
				return;
			}
			if (!accuse || self->lostTime > g_LittlePoliceChasingTime) {
				goto alert;
			}
			return;
alert:
			Logic_SetState(self, GENCHAR_ALERT, NULL);
			self->lostTime = 0.0f;
			return;
		}
		case LPOLICE_DISCOVER: {
			cEntity *te = b->target.target;
			cLocalLogic *lt;
			if (!te || !(lt = Logic_GetLocalLogic(te))) {
				Logic_RestartState(self);
				return;
			}
			if (!CGameModel_IsVisible(Obj_GetModel(p))) {
				const vec3_t *up = Entity_GetPos(usr, &t1);
				if (LP_DistSq(Entity_GetPos(p, &t2), up) >
					g_LittlePoliceStopAcusingMaxDistFromUser * g_LittlePoliceStopAcusingMaxDistFromUser) {
					Logic_RestartState(self);
					Logic_RestartState(lt);
					return;
				}
			}
			const vec3_t *tp = Entity_GetPos(te, &t1);
			const vec3_t *pp = Entity_GetPos(p, &t2);
			float far = g_LittlePoliceMaxDistFar;
			float dy = pp->y - tp->y, dx = pp->x - tp->x, dz = pp->z - tp->z;
			if (far * far >= dy * dy + dx * dx + dz * dz &&
				Logic_InView(p, Entity_GetPos(te, &t1), far, g_GenCharStdFovY, g_GenCharStdFovX, 1)) {
				cGenCharLocalLogic_Discover(&self->base);
				Logic_OnAlarm(lt, p);
				return;
			}
			Logic_SetState(self, LPOLICE_CHASE, te);
			Logic_OnAlarm(lt, p);
			return;
		}
		case LPOLICE_CHASE: {
			cEntity *te = b->target.target;
			cLocalLogic *lt;
			if (!te || !(lt = Logic_GetLocalLogic(te))) {
				Logic_RestartState(self);
				return;
			}
			if (!CGameModel_IsVisible(Obj_GetModel(p))) {
				const vec3_t *up = Entity_GetPos(usr, &t1);
				if (LP_DistSq(Entity_GetPos(p, &t2), up) >
					g_LittlePoliceStopAcusingMaxDistFromUser * g_LittlePoliceStopAcusingMaxDistFromUser) {
					Logic_RestartState(self);
					Logic_RestartState(lt);
					return;
				}
			}
			vec3_t o = b->origin;
			if (LP_DistSq(Entity_GetPos(p, &t1), &o) > b->actionRadius * b->actionRadius) {
				cActCtrl_Play(ChType_ActCtrl(p), "ActionBad", 1, 1.0f);
				cActCtrl_SetDefLoop(ChType_ActCtrl(p), "Relax", 1);
				Logic_RestartState(self);
				Logic_RestartState(lt);
				return;
			}
			if (Logic_InView(p, Entity_GetPos(te, &t1), g_GenCharStdFar, g_GenCharStdFovY, g_GenCharStdFovX, 1)) {
				Logic_SetState(self, LPOLICE_DISCOVER, te);
			}
			Logic_SetState(lt, GENCHAR_GETAWAY, p);
			return;
		}
		}
		return;
	} else if (st == GENCHAR_ATTACK) {
		self->base.specialTime = self->base.specialTime + dt;
	}
	cGenCharLocalLogic_Update(&self->base.base.logic, dt);
}

/* the alarm when the player hits it */
static void cLittlePoliceLocalLogic_OnHit(cLocalLogic *elem, cEntity *attacker) {
	cLittlePoliceLocalLogic *self = (cLittlePoliceLocalLogic *)elem;
	if (attacker == ChType_Usr0()) {
		Alarm_Set(&g_pWorld->alarm, 1.0f);
	}
	cGenCharLocalLogic_OnHit(&self->base.base.logic, attacker);
}

/* the player seen: the alarm and the accuse */
static void cLittlePoliceLocalLogic_OnSeen(cLocalLogic *base, cEntity *e) {
	cLittlePoliceLocalLogic *self = (cLittlePoliceLocalLogic *)base;
	if (LP_BASE(self)->busy) {
		return;
	}
	if (e == ChType_Usr0()) {
		Alarm_Set(&g_pWorld->alarm, 1.0f);
		Logic_SetState(self, GENCHAR_ACCUSE, e);
	}
}

/* the same, returns 0 */
static byte cLittlePoliceLocalLogic_GivesMoney(cLocalLogic *base, cEntity *e) {
	cLittlePoliceLocalLogic *self = (cLittlePoliceLocalLogic *)base;
	cLittlePoliceLocalLogic_OnSeen(&self->base.base.logic, e);
	return 0;
}

/* the view of the AI; not guaranteed it sees the player in its far view: the alarm and the accuse */
static void cLittlePoliceLocalLogic_UpdateLook(cLittlePoliceLocalLogic *self) {
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
		if (te && b->state == GENCHAR_ACCUSE && te == cWorld_GetUsrEntity(g_pWorld, 0)) {
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
	if (usr && Logic_InView(p, Entity_GetPos(usr, &t), g_LittlePoliceMaxDistFar, g_GenCharStdFovY,
		g_GenCharStdFovX, 1)) {
		Alarm_Set(&g_pWorld->alarm, 1.0f);
		Logic_SetState(self, GENCHAR_ACCUSE, usr);
	}
}

/* state 10, the walk to the player that asks for money: the special attack when the AI is near it, back to
 * the default state after 5 s at more than 2000 */
static void cLittlePoliceLocalLogic_UpdateGetIt(cLocalLogic *base, cAIChar *ai, float dt) {
	cLittlePoliceLocalLogic *self = (cLittlePoliceLocalLogic *)base;
	vec3_t t1, t2;
	cBaseLocalLogic *b = LP_BASE(self);

	if (b->idle || b->alwaysLock) {
		return;
	}
	cEntity *usr = ChType_Usr0();
	if (usr && Entity_IsType(usr, TYEN_WALKCHAR)) {
		cEntity *p = b->parent;
		const vec3_t *up = Entity_GetPos(usr, &t1);
		const vec3_t *pp = Entity_GetPos(p, &t2);
		float d = (float)__builtin_sqrt((double)LP_DistSq(pp, up));
		if (!(d > 2000.0f)) {
			if (AIChar_GetState(ai) == AIC_STATIC) {
				AIChar_SetState(ai, AIC_ORIENT_DYNAMIC_OBJECTIVE, usr, 1);
				return;
			}
			if (AIChar_GetState(ai) != AIC_ORIENT_DYNAMIC_OBJECTIVE) {
				self->lostTime = 5.0f;
				return;
			}
			if (g_EnergyBarActive != 1 || g_SpecialActionActive != 2) {
				Logic_SetState(self, GENCHAR_RELAX, usr);
				return;
			}
			if (!(500.0f > d) || !ChType_ActCtrl(usr) || !strcmp(Logic_TypeName(usr), "Police")) {
				return;
			}
			Logic_SetState(self, GENCHAR_ATTACK, usr);
			return;
		}
		float lt = self->lostTime - dt;
		self->lostTime = lt;
		if (!(0.0f >= lt)) {
			return;
		}
	}
	Logic_RestartState(self);
}

/* the AI walks to e and stops at MainMissionStopDist */
static void cLittlePoliceLocalLogic_GoTo(cLocalLogic *base, cAIChar *ai, cEntity *e) {
	cLittlePoliceLocalLogic *self = (cLittlePoliceLocalLogic *)base;
	vec3_t t1, t2, t3, d, dir;
	cEntity *p = LP_BASE(self)->parent;

	self->lostTime = 5.0f;
	const vec3_t *pp = Entity_GetPos(p, &t1);
	const vec3_t *ep = Entity_GetPos(e, &t2);
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
	const vec3_t *tp = Entity_GetPos(e, &t3);
	AIChar_SetState(ai, AIC_GO_TO, *tp, dir, (double)g_MainMissionStopDist, 1.0);
	g_LittlePoliceArrived = AIChar_GetState(ai) == AIC_GO_TO;
}

/* a change of the idle restarts the search time (not in state 4) */
static void cLittlePoliceLocalLogic_SetIdle(cLocalLogic *elem, byte idle) {
	cLittlePoliceLocalLogic *self = (cLittlePoliceLocalLogic *)elem;
	cBaseLocalLogic *b = LP_BASE(self);
	byte cur = b->idle || b->alwaysLock;

	if (idle != cur && b->state != GENCHAR_ATTACK) {
		self->base.searchTime = g_GenCharMaxSpecialActionTime;
	}
	cBaseLocalLogic_SetIdle(&b->logic, idle);
}

/* 8 the AI chases the target in alert, 7 the AI takes it */
static byte cLittlePoliceLocalLogic_SetState(cLocalLogic *elem, int state, cEntity *target) {
	cLittlePoliceLocalLogic *self = (cLittlePoliceLocalLogic *)elem;
	cBaseLocalLogic *b = LP_BASE(self);

	if (state == LPOLICE_CHASE) {
		if ((byte)cBaseLocalLogic_SetState(&b->logic, LPOLICE_CHASE, target)) {
			GenChar_UpdateView(self);
			cAIChar *ai = Entity_GetAI(b->parent);
			AIChar_SetState(ai, AIC_CHASE_WITHOUT_ACTION, target, (double)g_GenCharAlertSpeed);
		}
		return 1;
	}
	if (state == LPOLICE_DISCOVER) {
		if ((byte)cBaseLocalLogic_SetState(&b->logic, LPOLICE_DISCOVER, target)) {
			GenChar_UpdateView(self);
			cAIChar *ai = Entity_GetAI(b->parent);
			AIChar_SetState(ai, AIC_ORIENT_DYNAMIC_OBJECTIVE, target, 1);
			cGenCharLocalLogic_Discover(&self->base);
		}
		return 1;
	}
	return cGenCharLocalLogic_SetState(&self->base.base.logic, state, target);
}

/* the vtable of cLittlePoliceLocalLogic */
static const cGenCharLocalLogicVtbl cLittlePoliceLocalLogic_vftable = {
	.base.base.Free = cLittlePoliceLocalLogic_Free,
	.base.base.GetName = cBaseLocalLogic_GetName,
	.base.base.AcceptsArrow = cLocalLogic_AcceptsArrowTrue,
	.base.base.VarI = cLittlePoliceLocalLogic_VarI,
	.base.base.VarF = cBaseLocalLogic_VarF,
	.base.base.VarS = cGenCharLocalLogic_VarS,
	.base.base.VarV = cBaseLocalLogic_VarV,
	.base.UpdateBusy = cLittlePoliceLocalLogic_UpdateBusy,
	.base.Refresh = cBaseLocalLogic_Refresh,
	.base.Listens = cLocalLogic_ListensTrue,
	.base.OnHit = cLittlePoliceLocalLogic_OnHit,
	.base.OnSeen = cLittlePoliceLocalLogic_OnSeen,
	.base.OnAttack = cGenCharLocalLogic_OnRobbery,
	.base.OnShot = cBaseLocalLogic_OnShot,
	.base.OnHammer = cLocalLogic_OnHammerNop,
	.base.GivesMoney = cLittlePoliceLocalLogic_GivesMoney,
	.base.OnAlarm = cGenCharLocalLogic_OnAlarm,
	.base.Update = cLittlePoliceLocalLogic_Update,
	.base.SetOriginFrom = cBaseLocalLogic_SetOriginFrom,
	.base.SetState = cLittlePoliceLocalLogic_SetState,
	.base.SetIdle = cLittlePoliceLocalLogic_SetIdle,
	.base.SetActionRadius = cBaseLocalLogic_SetActionRadius,
	.base.SetOrigin = cBaseLocalLogic_SetOrigin,
	.base.SetOriginAng = cBaseLocalLogic_SetOriginAng,
	.base.RestartState = cGenCharLocalLogic_RestartState,
	.CanAttack = cGenCharLocalLogic_CanAttack,
	.UpdateView = cLittlePoliceLocalLogic_UpdateView,
	.ToRelax = cGenCharLocalLogic_ToRelax,
	.ToAlert = cLittlePoliceLocalLogic_ToAlert,
	.Accuse = Logic_SetState2,
	.FindTarget = cLittlePoliceLocalLogic_FindTarget,
	.TargetDone = cLittlePoliceLocalLogic_TargetDone,
	.Work = cLocalLogic_WorkNop,
	.BreakWork = cLocalLogic_BreakWorkNop,
	.GoTo = cLittlePoliceLocalLogic_GoTo,
	.UpdateGetIt = cLittlePoliceLocalLogic_UpdateGetIt,
};

cGVar GVar_GenCharStdFovX;
cGVar GVar_GenCharStdFar;
cGVar GVar_GenCharStdFovY;
static cGVar GVar_LittlePoliceStopAcusingMaxDistFromUser;
static cGVar GVar_LittlePoliceMaxDistFar;
static cGVar GVar_LittlePoliceAsksForMoney;
static cGVar GVar_LittlePoliceChasingTime;
static cGVar GVar_MainMissionStopDist;

/* the dynamic initializers of the globals here, run in the order of src/standalone/crtxc.inc */
INIT_GVAR_FLOAT(Init_GVar_GenCharStdFovX, &GVar_GenCharStdFovX, "GenCharStdFovX", 0, "Standard GenChar FieldOfViewX", 1.5707964f)
INIT_GVAR_FLOAT(Init_GVar_GenCharStdFovY, &GVar_GenCharStdFovY, "GenCharStdFovY", 0, "Standard GenChar FieldOfViewY", 1.5707964f)
INIT_GVAR_FLOAT(Init_GVar_GenCharStdFar, &GVar_GenCharStdFar, "GenCharStdFar", 0, "Standard GenChar Distance of View", 800.0f)
INIT_GVAR_INT(Init_GVar_LittlePoliceAsksForMoney, &GVar_LittlePoliceAsksForMoney, "LittlePoliceAsksForMoney", 0, "Little police asks for money", 1)
INIT_GVAR_FLOAT(Init_GVar_LittlePoliceMaxDistFar, &GVar_LittlePoliceMaxDistFar, "LittlePoliceMaxDistFar", 0, "Little police max distance of view acusing", 1500.0f)
INIT_GVAR_FLOAT(Init_GVar_LittlePoliceStopAcusingMaxDistFromUser, &GVar_LittlePoliceStopAcusingMaxDistFromUser, "LittlePoliceStopAcusingMaxDistFromUser", 0, "Little police stop acusing max dist from user", 4000.0f)
INIT_GVAR_FLOAT(Init_GVar_LittlePoliceChasingTime, &GVar_LittlePoliceChasingTime, "LittlePoliceChasingTime", 0, "Little police chasing time", 3.0f)
INIT_GVAR_FLOAT(Init_GVar_MainMissionStopDist, &GVar_MainMissionStopDist, "MainMissionStopDist", 0, "Main mission stop dist", 1500.0f)
