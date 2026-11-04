/*
 * Logic/GenCharLogic.cpp of the game: cGenCharLocalLogic (include/scrap/logic.h), the logic of the generic
 * characters. A character relaxes, stays or chases (its default state); one of the suspect types that sees the
 * player of its own type, or of any type with the alarm on, accuses it (state 2: the sentinels watch, the gears
 * attack); a hit makes it get away (3) or attack (4, the special attack of the type against the target finds
 * only some character types). The constructor and the small slots are part 2 of the logic, the rest from rva
 * 0x14c4f0 part 3.
 */
#include <string.h>

#include "scrap/chartype.h"
#include "scrap/weapons.h"
#include "scrap/staticinit.h"

#define CGENCHARLOCALLOGIC_VFTABLE (&cGenCharLocalLogic_vftable.base)
#define g_OnDefStateCallback (((cPy_Function *)&GVar_OnDefStateCallback.func))	/* (name) after RestartState */
#define g_GenCharAcuseAgainTime (GVar_GenCharAcuseAgainTime.value.f)
#define g_GenCharRobberyDetection (GVar_GenCharRobberyDetection.value.f)
#define g_GenCharAcuseTime (GVar_GenCharAcuseTime.value.f)
#define g_GenCharSpecialChaseSpeed (GVar_GenCharSpecialChaseSpeed.value.f)
#define g_GenCharAttackSpeed (GVar_GenCharAttackSpeed.value.f)
#define g_GenCharGetAwayTime (GVar_GenCharGetAwayTime.value.f)
#define g_GenCharAttackLimit (GVar_GenCharAttackLimit.value.f)
#define g_GenCharPoliceRefuse (GVar_GenCharPoliceRefuse.value.f)
static const cGenCharLocalLogicVtbl cGenCharLocalLogic_vftable;
static cGVarFunction GVar_OnDefStateCallback;
static cGVar GVar_GenCharSpecialChaseSpeed;
static cGVar GVar_GenCharRobberyDetection;
static cGVar GVar_GenCharPoliceRefuse;
static cGVar GVar_GenCharGetAwayTime;
static cGVar GVar_GenCharAttackSpeed;
static cGVar GVar_GenCharAttackLimit;
static cGVar GVar_GenCharAcuseTime;
static cGVar GVar_GenCharAcuseAgainTime;

cGenCharLocalLogic *cGenCharLocalLogic_ctor(cGenCharLocalLogic *self, cEntity *parent, int noRestart) {
	cBaseLocalLogic_ctor(&self->base, parent);
	self->base.vftable = CGENCHARLOCALLOGIC_VFTABLE;
	self->time = 0.0f;
	self->accuseAgain = 0.0f;
	cString_ctor(&self->defaultTarget);
	self->canAttack = 1;
	self->specialOn = 0;
	self->ignoreAlarm = 0;
	self->specialTime = 0.0f;
	float t = Rand_FloatRange(g_GenCharMinSpecialActionTime, g_GenCharMaxSpecialActionTime);
	self->defaultState = 0;
	self->searchTime = t;
	cString_Set(&self->defaultTarget, str_Empty);
	self->alarmState = 3;
	if (!(byte)noRestart) {
		cGenCharLocalLogic_RestartState(&self->base.logic);
	}
	return self;
}

void cGenCharLocalLogic_dtor(cGenCharLocalLogic *self) {
	cString_dtor(&self->defaultTarget);
	cBaseLocalLogic_dtor(&self->base);
}

static void cGenCharLocalLogic_Free(cLocalLogic *base) {
	cGenCharLocalLogic *self = (cGenCharLocalLogic *)base;
	cGenCharLocalLogic_dtor(self);
	operator_delete(self);
}

/* no view flag for a player of the own suspect type, else relaxed and no alarm; team 3 */
static byte Logic_IsSuspectType(const char *type);
void cGenCharLocalLogic_UpdateView(cLocalLogic *elem) {
	cGenCharLocalLogic *self = (cGenCharLocalLogic *)elem;
	cBaseLocalLogic *b = &self->base;
	cEntity *p = b->parent;
	const char *type = Logic_TypeName(p);
	cEntity *usr = ChType_Usr0();

	if (usr && Entity_IsType(usr, TYEN_WALKCHAR) && Logic_IsSuspectType(type) &&
		!strcmp(type, Logic_TypeName(usr))) {
		WChar(p)->wl.arrow.canUse = 0;
	} else {
		int st = b->state;
		WChar(p)->wl.arrow.canUse = !b->busy && !g_pWorld->alarm.on && st != 4 && st != 3 && st != 5 &&
			st != 10;
	}
	WLife(p)->arrow.coneStatus = 0;
	WLife(p)->arrow.tgType = 3;
}

/* e can discover the player: it has the action Discover, Berto and Betty only when the player is not of the type
 * UsrDefChar */
static byte cGenCharLocalLogic_CanDiscover(cGenCharLocalLogic *self, cEntity *e) {
	(void)self;
	if (!WithLife_GetAction(e, "Discover")) {
		return 0;
	}
	const char *type = Logic_TypeName(e);
	if (strcmp(type, "Berto") && strcmp(type, "Betty")) {
		return 1;
	}
	return cString_CmpStr(g_UsrDefChar, g_pWorld->alarm.chars) != 0;
}

/* the state of the alarm against e */
void cGenCharLocalLogic_OnAlarm(cLocalLogic *elem, cEntity *e) {
	cGenCharLocalLogic *self = (cGenCharLocalLogic *)elem;
	Logic_SetState(self, self->alarmState, e);
}

/* and: the AI walks the zone around the origin, in alert or relaxed */
void cGenCharLocalLogic_ToAlert(cLocalLogic *elem, cAIChar *ai) {
	cGenCharLocalLogic *self = (cGenCharLocalLogic *)elem;
	cBaseLocalLogic *b = &self->base;

	AIChar_SetState(ai, AIC_PATROL_ZONE, b->origin, (double)b->actionRadius, (double)g_GenCharAlertSpeed, 0);
}

void cGenCharLocalLogic_ToRelax(cLocalLogic *elem, cAIChar *ai) {
	cGenCharLocalLogic *self = (cGenCharLocalLogic *)elem;
	cBaseLocalLogic *b = &self->base;

	AIChar_SetState(ai, AIC_PATROL_ZONE, b->origin, (double)b->actionRadius, (double)g_GenCharRelaxSpeed, 1);
}

/* ---- part 3 ---- */

static inline void GenChar_SetDefaultState(cGenCharLocalLogic *self, int ds) {
	CHECK((ds == GENCHAR_STAY) || (ds == GENCHAR_RELAX) || (ds == GENCHAR_CHASE) || (ds == GENCHAR_GET_IT));
	self->defaultState = ds;
}

byte cGenCharLocalLogic_VarI(cLocalLogic *elem, const char *name, int *i, byte set) {
	cGenCharLocalLogic *self = (cGenCharLocalLogic *)elem;
	if (strcmp(name, "DefaultState")) {
		return cBaseLocalLogic_VarI(&self->base.logic, name, i, set);
	}
	if (set) {
		GenChar_SetDefaultState(self, *i);
		return 1;
	}
	*i = self->defaultState;
	return 1;
}

/* the type of the player draws the attention of a generic character */
static byte Logic_IsSuspectType(const char *type) {
	if (Alarm_IsLocked(&g_pWorld->alarm)) {
		return 0;
	}
	return !strcmp(type, "PoliceBoss") || !strcmp(type, "Betty") || !strcmp(type, "Berto") || !strcmp(type, "Mayor");
}

void Logic_SetState2(cLocalLogic *base, cEntity *e) {
	cBaseLocalLogic *self = (cBaseLocalLogic *)base;
	Logic_SetState(self, GENCHAR_ACCUSE, e);
}

/* the random reaction of the hit slots: rand() 0..100 below 101, always */
static inline byte GenChar_Rand101(void) {
	return 101.0f > (float)(Weap_Rand01() * 100.0 + 0.0);
}

static inline int GenChar_AISubState(cAIChar *ai) {
	return AIChar_GetInternState(ai);
}

static void cGenCharLocalLogic_ResetSpecial(cGenCharLocalLogic *self);
void cGenCharLocalLogic_Update(cLocalLogic *elem, float dt) {
	cGenCharLocalLogic *self = (cGenCharLocalLogic *)elem;
	cBaseLocalLogic *b = &self->base;
	vec3_t t;

	self->searchTime = self->searchTime - dt;
	cBaseLocalLogic_Update(&b->logic, dt);
	GenChar_UpdateView(self);
	float ag = self->accuseAgain - dt;
	self->accuseAgain = ag;
	if (0.0f >= ag) {
		self->accuseAgain = 0.0f;
	}
	if (b->busy) {
		GenChar_BreakWork(self, 1);
		return;
	}
	cEntity *usr = ChType_Usr0();
	if (!usr || !Entity_IsType(usr, TYEN_WALKCHAR)) {
		return;
	}
	cEntity *p = b->parent;
	const char *type = Logic_TypeName(p);
	const char *usrType = Logic_TypeName(usr);
	byte discover = cGenCharLocalLogic_CanDiscover(self, p) && self->accuseAgain == 0.0f;
	byte alarm = g_pWorld->alarm.on && !self->ignoreAlarm;
	byte sameChar = g_pWorld->alarm.sameChar;
	byte seen = Logic_InView(p, Entity_GetPos(usr, &t), g_GenCharStdFar, g_GenCharStdFovY, g_GenCharStdFovX, 1);
	byte suspect = Logic_IsSuspectType(type);
	byte same = !strcmp(type, usrType) && suspect;
	cAIChar *ai = Entity_GetAI(b->parent);
	if (suspect && !Alarm_IsLocked(&g_pWorld->alarm)) {
		WLife(p)->arrow.coneFov = g_GenCharStdFovY;
		WLife(p)->arrow.coneDist = g_GenCharStdFar;
		WLife(p)->arrow.coneStatus = same ? 2 : 0;
		WLife(p)->arrow.tgType = same ? (g_pWorld->alarm.on ? 0 : 2) : 3;
	}
	int st = b->state;
	if (st != GENCHAR_ACCUSE && seen && discover) {
		if (!sameChar || !alarm) {
			if (!same) {
				goto states;
			}
			Alarm_Set(&g_pWorld->alarm, 1.0f);
		}
		GenChar_Accuse(self, usr);
	}
states:
	if (st == GENCHAR_RELAX || st == GENCHAR_STAY || st == 7 || st == 8) {
		if (alarm) {
			if (!self->ignoreAlarm) {
				Logic_SetState(self, GENCHAR_ALERT, NULL);
			}
		} else if (Obj(p)->move.active && (b->idle || b->alwaysLock)) {
			cObjMove_SetTime(&Obj(p)->move, 0.0f);
			GenChar_BreakWork(self, 0);
		} else if (!b->idle && !b->alwaysLock) {
			cEntity *e;
			if (GenChar_CanAttack(self, p) && (e = GenChar_FindTarget(self)) != NULL) {
				if (Entity_IsType(e, TYEN_WITH_LIFE) && !Obj(e)->radarShow &&
					Logic_InView(p, Entity_GetPos(usr, &t), 3000.0f, 6.2831855f, 6.2831855f, 1)) {
					Logic_SetState(self, GENCHAR_ATTACK, e);
					self->specialOn = 1;
				}
			} else if (b->state != GENCHAR_ATTACK) {
				GenChar_Work(self, dt);
			}
		}
	}
	switch (b->state) {
	case GENCHAR_ALERT:
		if (!alarm) {
			Logic_RestartState(self);
		}
		return;
	case GENCHAR_ACCUSE:
		if (!b->target.target) {
			Logic_RestartState(self);
			return;
		}
		cGenCharLocalLogic_Discover(self);
		cGenCharGroup_RaiseLevel((&g_pWorld->logic.genChars), 0);
		if (b->target.target && sameChar) {
			if (seen) {
				self->time = g_GenCharAcuseTime;
				AICtrl_SendSentinelToWatch(&g_pWorld->logic.aiCtrl, Entity_GetPos(usr, &t));
				AICtrl_GearAttack(&g_pWorld->logic.aiCtrl, usr, 1);
				return;
			}
			if (self->time >= 0.0f) {
				self->time = self->time - dt;
				return;
			}
		}
		Logic_SetState(self, GENCHAR_ALERT, NULL);
		return;
	case GENCHAR_GETAWAY: {
		if (cAIChar_TestFlags(self)) {
			self->time = g_GenCharGetAwayTime;
			return;
		}
		float tm = self->time + dt;
		self->time = tm;
		if (tm > g_GenCharGetAwayTime) {
			self->time = 0.0f;
			if (!alarm) {
				Logic_RestartState(self);
				return;
			}
			if (!self->ignoreAlarm) {
				Logic_SetState(self, GENCHAR_ALERT, NULL);
			}
		}
		return;
	}
	case GENCHAR_ATTACK:
		if (!alarm || self->alarmState == GENCHAR_ATTACK) {
			if (!b->target.target || GenChar_TargetDone(self)) {
				Logic_RestartState(self);
				cGenCharLocalLogic_ResetSpecial(self);
			}
			return;
		}
		if (!self->ignoreAlarm) {
			Logic_SetState(self, GENCHAR_ALERT, NULL);
		}
		cGenCharLocalLogic_ResetSpecial(self);
		return;
	case GENCHAR_STAY:
		if (AIChar_GetState(ai) != AIC_GO_TO && !cAIChar_TestFlags(self)) {
			vec3_t o = b->origin;
			const vec3_t *pp = Entity_GetPos(b->parent, &t);
			float dy = pp->y - o.y, dx = pp->x - o.x, dz = pp->z - o.z;
			if (dy * dy + dx * dx + dz * dz > 40000.0f) {
				Logic_SetIdle(self, 1);
				Logic_SetState(self, GENCHAR_STAY, NULL);
			}
		}
		return;
	case GENCHAR_GET_IT:
		GenChar_UpdateGetIt(self, ai, dt);
		return;
	}
}

/* a robbery seen, with the chance GenCharRobberyDetection plus the amount */
void cGenCharLocalLogic_OnRobbery(cLocalLogic *elem, cEntity *e, float amount) {
	cGenCharLocalLogic *self = (cGenCharLocalLogic *)elem;
	if (amount > 0.0f && Logic_Chance(&self->base, e, g_GenCharRobberyDetection + amount)) {
		Logic_SetState(self, self->alarmState, e);
		cGenCharLocalLogic_ResetSpecial(self);
	}
}

/* the state of the alarm against the attacker, no accuse for a while */
void cGenCharLocalLogic_OnHit(cLocalLogic *elem, cEntity *attacker) {
	cGenCharLocalLogic *self = (cGenCharLocalLogic *)elem;
	if (self->base.busy || !GenChar_Rand101()) {
		return;
	}
	self->accuseAgain = g_GenCharAcuseAgainTime;
	Logic_SetState(self, self->alarmState, attacker);
	cGenCharLocalLogic_ResetSpecial(self);
}

/* the same, not while accusing */
void cGenCharLocalLogic_OnSeen(cLocalLogic *elem, cEntity *e) {
	cGenCharLocalLogic *self = (cGenCharLocalLogic *)elem;
	if (self->base.state == GENCHAR_ACCUSE || self->base.busy || !GenChar_Rand101()) {
		return;
	}
	Logic_SetState(self, self->alarmState, e);
	cGenCharLocalLogic_ResetSpecial(self);
}

/* 1 if it refuses the police (GenCharPoliceRefuse in percent), else the state of the alarm against it */
byte cGenCharLocalLogic_GivesMoney(cLocalLogic *elem, cEntity *police) {
	cGenCharLocalLogic *self = (cGenCharLocalLogic *)elem;
	float refuse = g_GenCharPoliceRefuse;

	if (!self->base.busy && refuse > (float)(Weap_Rand01() * 100.0 + 0.0)) {
		return 1;
	}
	Logic_SetState(self, self->alarmState, police);
	return 0;
}

/* state 10 ends when the AI arrived */
void cGenCharLocalLogic_UpdateGetIt(cLocalLogic *base, cAIChar *ai, float dt) {
	cGenCharLocalLogic *self = (cGenCharLocalLogic *)base;
	(void)dt;
	if (AIChar_GetState(ai) != AIC_GO_TO) {
		Logic_RestartState(self);
	}
}

/* the special action over, the next target search in a random time */
static void cGenCharLocalLogic_ResetSpecial(cGenCharLocalLogic *self) {
	self->specialOn = 0;
	self->specialTime = 0.0f;
	self->searchTime = Rand_FloatRange(g_GenCharMinSpecialActionTime, g_GenCharMaxSpecialActionTime);
}

void cGenCharLocalLogic_Discover(cGenCharLocalLogic *self) {
	cEntity *p = self->base.parent;

	if (WChar(p)->control.walk == 0.0f && WChar(p)->control.strafe == 0.0f &&
		!cActCtrl_IsActive(ChType_ActCtrl(p), "Discover")) {
		cActCtrl_PlayNew(ChType_ActCtrl(p), "DiscoverInit", 0, 1.0f);
		cActCtrl_SetDefLoop(ChType_ActCtrl(p), "Discover", 0);
	}
}

/* the alert with the alarm (unless IgnoreAlarm), else the default state with its target; OnDefStateCallback
 * (name) when that state was taken */
void cGenCharLocalLogic_RestartState(cLocalLogic *elem) {
	cGenCharLocalLogic *self = (cGenCharLocalLogic *)elem;
	cGenCharLocalLogic_ResetSpecial(self);
	if (g_pWorld->alarm.on && !self->ignoreAlarm) {
		Logic_SetState(self, GENCHAR_ALERT, NULL);
		return;
	}
	cEntity *t = World_FindEntity(self->defaultTarget);
	byte ok = Logic_SetState(self, self->defaultState, t);
	if (ok && (byte)cPy_Function_IsValid(g_OnDefStateCallback)) {
		cPy_Function_AddParamS(g_OnDefStateCallback, self->base.parent->name);
		cPy_Function_Execute(g_OnDefStateCallback, 0);
	}
}

/* the direction from a to b, the difference itself when it has no length */
static void GenChar_Dir(vec3_t *dir, const vec3_t *a, const vec3_t *b) {
	vec3_t d;

	d.y = b->y - a->y;
	d.x = b->x - a->x;
	d.z = b->z - a->z;
	float len = (float)__builtin_sqrt((double)(d.y * d.y + d.x * d.x + d.z * d.z));
	if (len == 0.0f) {
		*dir = d;
		return;
	}
	float inv = 1.0f / len;
	dir->x = inv * d.x;
	dir->y = inv * d.y;
	dir->z = d.z * inv;
}

/* the AI walks to e, faced from the character to it */
void cGenCharLocalLogic_GoTo(cLocalLogic *elem, cAIChar *ai, cEntity *e) {
	cGenCharLocalLogic *self = (cGenCharLocalLogic *)elem;
	vec3_t t1, t2, t3, dir;
	cEntity *p = self->base.parent;
	const vec3_t *pp = Entity_GetPos(p, &t1);
	const vec3_t *ep = Entity_GetPos(e, &t2);

	GenChar_Dir(&dir, pp, ep);
	const vec3_t *tp = Entity_GetPos(e, &t3);
	AIChar_SetState(ai, AIC_GO_TO, *tp, dir, 70.0, 1.0);
}

/* the AI state of the logic state; always 1 */
byte cGenCharLocalLogic_SetState(cLocalLogic *elem, int state, cEntity *target) {
	cGenCharLocalLogic *self = (cGenCharLocalLogic *)elem;
	cBaseLocalLogic *b = &self->base;
	byte ok;

	if (state == GENCHAR_GETAWAY && b->state == GENCHAR_GETAWAY && !b->idle && !b->alwaysLock) {
		ok = 0;
	} else {
		ok = (byte)cBaseLocalLogic_SetState(&b->logic, state, target);
	}
	cAIChar *ai = Entity_GetAI(b->parent);
	cEntity *p = b->parent;
	if (ok) {
		GenChar_UpdateView(self);
	}
	switch (b->state) {
	case GENCHAR_RELAX:
		if (ok) {
			GenChar_ToRelax(self, ai);
		}
		break;
	case GENCHAR_ALERT:
		if (ok) {
			GenChar_ToAlert(self, ai);
		}
		break;
	case GENCHAR_ACCUSE:
		if (ok && g_pWorld->alarm.sameChar) {
			self->time = g_GenCharAcuseTime;
			AIChar_SetState(ai, AIC_ORIENT_DYNAMIC_OBJECTIVE, target, 1);
			if (WithLife_GetAction(p, "Discover")) {
				cGenCharLocalLogic_Discover(self);
				cGenCharGroup_RaiseLevel((&g_pWorld->logic.genChars), 0);
			}
		}
		break;
	case GENCHAR_GETAWAY:
		if (ok) {
			AIChar_SetState(ai, AIC_GET_AWAY, target, (double)g_GenCharEscapeSpeed);
		}
		self->time = 0.0f;
		break;
	case GENCHAR_ATTACK:
		if (ok) {
			if (!strcmp(Logic_TypeName(p), "Mayor")) {
				cAIChar *a2 = Entity_GetAI(b->parent);
				AIChar_SetState(a2, AIC_CHASE_WITH_ACTION, target, (double)g_GenCharAttackSpeed);
			} else {
				float speed = target == cWorld_GetUsrEntity(g_pWorld, 0) ? g_GenCharAttackSpeed : 1.0f;
				AIChar_SetState(ai, AIC_CHASE_WITH_ACTION, target, (double)speed);
			}
		}
		break;
	case GENCHAR_CHASE:
		if (ok) {
			if (0.0f > g_GenCharSpecialChaseSpeed) {
				AIChar_SetState(ai, AIC_CHASE_WITHOUT_ACTION, target, (double)g_GenCharAlertSpeed);
			} else {
				AIChar_SetState(ai, AIC_CHASE_WITHOUT_ACTION, target, (double)g_GenCharSpecialChaseSpeed);
			}
		}
		break;
	case GENCHAR_STAY:
		if (ok) {
			AIChar_SetState(ai, AIC_GO_TO, b->origin, b->originAng, 70.0, 1.0);
		}
		break;
	case GENCHAR_GET_IT:
		if (ok && target) {
			GenChar_GoTo(self, ai, target);
		}
		break;
	}
	return 1;
}

byte cGenCharLocalLogic_VarS(cLocalLogic *elem, const char *name, char **s, byte set) {
	cGenCharLocalLogic *self = (cGenCharLocalLogic *)elem;
	if (strcmp(name, "DefaultTarget")) {
		return cBaseLocalLogic_VarS(&self->base.logic, name, s, set);
	}
	if (set) {
		cString_Set(&self->defaultTarget, *s);
		return 1;
	}
	*s = self->defaultTarget;
	return 1;
}

/* the special attack of e is allowed (energy bar and special actions on, GenCharAttackActive), e is not
 * idle, near a camera (GenCharAttackLimit), not on the radar and its AI not in sub state 12 */
byte cGenCharLocalLogic_CanAttack(cLocalLogic *base, cEntity *e) {
	cGenCharLocalLogic *self = (cGenCharLocalLogic *)base;
	vec3_t t;
	cBaseLocalLogic *l = (cBaseLocalLogic *)Logic_GetLocalLogic(e);

	if (g_EnergyBarActive != 1 || g_SpecialActionActive != 2 || g_GenCharAttackActive != 1) {
		return 0;
	}
	if (l && (l->idle || l->alwaysLock)) {
		return 0;
	}
	if (!self->canAttack) {
		return 0;
	}
	float d = cWorld_GetMinCamDistSq(g_pWorld, Entity_GetPos(e, &t));
	if (!(g_GenCharAttackLimit * g_GenCharAttackLimit > d) || Obj(e)->radarShow || !Entity_GetAI(e)) {
		return 0;
	}
	return GenChar_AISubState(Entity_GetAI(e)) != 12;
}

/* the vtable of cGenCharLocalLogic */
static const cGenCharLocalLogicVtbl cGenCharLocalLogic_vftable = {
	.base.base.Free = cGenCharLocalLogic_Free,
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
	.base.Update = cGenCharLocalLogic_Update,
	.base.SetOriginFrom = cBaseLocalLogic_SetOriginFrom,
	.base.SetState = cGenCharLocalLogic_SetState,
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
	.FindTarget = (void *)crt_purecall,
	.TargetDone = (void *)crt_purecall,
	.Work = cLocalLogic_WorkNop,
	.BreakWork = cLocalLogic_BreakWorkNop,
	.GoTo = cGenCharLocalLogic_GoTo,
	.UpdateGetIt = cGenCharLocalLogic_UpdateGetIt,
};

static cGVarFunction GVar_OnDefStateCallback;
static cGVar GVar_GenCharAcuseAgainTime;
static cGVar GVar_GenCharRobberyDetection;
cGVar GVar_GenCharAlertSpeed;
static cGVar GVar_GenCharAcuseTime;
static cGVar GVar_GenCharSpecialChaseSpeed;
static cGVar GVar_GenCharAttackSpeed;
static cGVar GVar_GenCharGetAwayTime;
static cGVar GVar_GenCharAttackLimit;
static cGVar GVar_GenCharPoliceRefuse;
cGVar GVar_GenCharEscapeSpeed;
cGVar GVar_GenCharMaxSpecialActionTime;
cGVar GVar_GenCharMinSpecialActionTime;

/* the dynamic initializers of the globals here, run in the order of src/standalone/crtxc.inc */
INIT_GVAR_FLOAT(Init_GVar_GenCharAlertSpeed, &GVar_GenCharAlertSpeed, "GenCharAlertSpeed", 0, "GenChar speed (Alert)", 1.0f)
INIT_GVAR_FLOAT(Init_GVar_GenCharEscapeSpeed, &GVar_GenCharEscapeSpeed, "GenCharEscapeSpeed", 0, "GenChar speed (Escape)", 0.85f)
INIT_GVAR_FLOAT(Init_GVar_GenCharAttackSpeed, &GVar_GenCharAttackSpeed, "GenCharAttackSpeed", 0, "GenChar speed (Attack)", 0.8f)
INIT_GVAR_FLOAT(Init_GVar_GenCharMinSpecialActionTime, &GVar_GenCharMinSpecialActionTime, "GenCharMinSpecialActionTime", 0, "GenChar min special action time", 6.0f)
INIT_GVAR_FLOAT(Init_GVar_GenCharMaxSpecialActionTime, &GVar_GenCharMaxSpecialActionTime, "GenCharMaxSpecialActionTime", 0, "GenChar max special action time", 12.0f)
INIT_GVAR_FLOAT(Init_GVar_GenCharGetAwayTime, &GVar_GenCharGetAwayTime, "GenCharGetAwayTime", 0, "GenChar get away time", 4.0f)
INIT_GVAR_FLOAT(Init_GVar_GenCharRobberyDetection, &GVar_GenCharRobberyDetection, "GenCharRobberyDetection", 0, "GenChar robery detection percentage", 20.0f)
INIT_GVAR_FLOAT(Init_GVar_GenCharAttackLimit, &GVar_GenCharAttackLimit, "GenCharAttackLimit", 0, "Beyond this distance the generic logic does not attack randomly!", 2000.0f)
INIT_GVAR_FLOAT(Init_GVar_GenCharAcuseTime, &GVar_GenCharAcuseTime, "GenCharAcuseTime", 0, "Minimal time pointing the main character", 2.0f)
INIT_GVAR_FLOAT(Init_GVar_GenCharAcuseAgainTime, &GVar_GenCharAcuseAgainTime, "GenCharAcuseAgainTime", 0, "Time to acuse again after having received damage from the Gear (being acusing the main char)", 4.0f)
INIT_GVAR_FLOAT(Init_GVar_GenCharPoliceRefuse, &GVar_GenCharPoliceRefuse, "GenCharPoliceRefuse", 0, "Percentage of times when the generic char escapes from the police", 50.0f)
INIT_GVAR_FLOAT(Init_GVar_GenCharSpecialChaseSpeed, &GVar_GenCharSpecialChaseSpeed, "GenCharSpecialChaseSpeed", 0, "Gen char special chase speed", -1.0f)
FINI_CALL(Fini_GVar_OnDefStateCallback, &GVar_OnDefStateCallback.func, cPy_Function_dtor)
INIT_GVAR_FUNCTION(Init_GVar_OnDefStateCallback, Fini_GVar_OnDefStateCallback, &GVar_OnDefStateCallback, "OnDefStateCallback", 0, "Python function to be called when the NPC is forced to have the default state", "")
