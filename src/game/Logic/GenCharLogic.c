/*
 * Logic/GenCharLogic.cpp of the game: cGenCharLocalLogic (include/scrap/logic.h), the logic of the generic
 * characters. A character relaxes, stays or chases (its default state); one of the suspect types that sees the
 * player of its own type, or of any type with the alarm on, accuses it (state 2: the sentinels watch, the gears
 * attack); a hit makes it get away (3) or attack (4, the special attack of the type against the target vf30 finds,
 * only some character types). The constructor and the small slots are part 2 of the logic, the rest from rva
 * 0x14c4f0 part 3.
 */
#include <string.h>

#include "scrap/chartype.h"
#include "scrap/weapons.h"

cGenCharLocalLogic *__thiscall cGenCharLocalLogic_ctor(cGenCharLocalLogic *self, cEntity *parent, int noRestart) {
	cBaseLocalLogic_ctor(&self->base, parent);
	self->base.vftable = Game_Ptr(CGENCHARLOCALLOGIC_VFTABLE);
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
	cString_Set(&self->defaultTarget, LOGIC_EMPTY);
	self->alarmState = 3;
	if (!(byte)noRestart) {
		cGenCharLocalLogic_RestartState(self);
	}
	return self;
}

void __fastcall cGenCharLocalLogic_dtor(cGenCharLocalLogic *self) {
	self->base.vftable = Game_Ptr(CGENCHARLOCALLOGIC_VFTABLE);
	cString_dtor(&self->defaultTarget);
	cBaseLocalLogic_dtor(&self->base);
}

cGenCharLocalLogic *__thiscall cGenCharLocalLogic_Delete(cGenCharLocalLogic *self, byte flags) {
	cGenCharLocalLogic_dtor(self);
	if (flags & 1) {
		operator_delete_sized(self, sizeof(*self));
	}
	return self;
}

/* vf26: no view flag for a player of the own suspect type, else relaxed and no alarm; team 3 */
void __thiscall cGenCharLocalLogic_UpdateView(cGenCharLocalLogic *self) {
	cBaseLocalLogic *b = &self->base;
	cEntity *p = b->parent;
	const char *type = Logic_TypeName(p);
	cEntity *usr = ChType_Usr0();

	if (usr && usr->vftable->IsType(usr, TYEN_WALKCHAR) && Logic_IsSuspectType(type) &&
		!strcmp(type, Logic_TypeName(usr))) {
		WChar_B381(p) = 0;
	} else {
		int st = b->state;
		WChar_B381(p) = !b->busy && !g_pWorld->alarmOn && st != 4 && st != 3 && st != 5 && st != 10;
	}
	WChar_ViewMode(p) = 0;
	Entity_Team(p) = 3;
}

/* e can discover the player: it has the action Discover, Berto and Betty only when the player is not of the type
 * UsrDefChar */
byte __thiscall cGenCharLocalLogic_CanDiscover(cGenCharLocalLogic *self, cEntity *e) {
	(void)self;
	if (!ChType_Vtbl(e)->FindAction(e, "Discover")) {
		return 0;
	}
	const char *type = Logic_TypeName(e);
	if (strcmp(type, "Berto") && strcmp(type, "Betty")) {
		return 1;
	}
	return cString_CmpStr(g_UsrDefChar, g_pWorld->alarmChars) != 0;
}

void __thiscall cGenCharLocalLogic_SetB58(cGenCharLocalLogic *self, byte v) {
	self->canAttack = v;
}

/* vf16: the state of the alarm against e */
void __thiscall cGenCharLocalLogic_OnAlarm(cGenCharLocalLogic *self, cEntity *e) {
	self->base.vftable->SetState(self, self->alarmState, e);
}

int __thiscall cGenCharLocalLogic_GetI50(cGenCharLocalLogic *self) {
	return self->defaultState;
}

char *__thiscall cGenCharLocalLogic_GetS54(cGenCharLocalLogic *self) {
	return self->defaultTarget;
}

/* vf28 and vf27: the AI walks the zone around the origin, in alert or relaxed */
void __thiscall cGenCharLocalLogic_ToAlert(cGenCharLocalLogic *self, cAIChar *ai) {
	cBaseLocalLogic *b = &self->base;

	ai->vftable->SetState(ai, 6, b->origin, (double)b->actionRadius, (double)g_GenCharAlertSpeed, 0);
}

void __thiscall cGenCharLocalLogic_ToRelax(cGenCharLocalLogic *self, cAIChar *ai) {
	cBaseLocalLogic *b = &self->base;

	ai->vftable->SetState(ai, 6, b->origin, (double)b->actionRadius, (double)g_GenCharRelaxSpeed, 1);
}

/* ---- part 3 (rva 0x14c4f0) ---- */

#define GENCHAR_VT(self) ((self)->base.vftable)

static inline void GenChar_SetDefaultState(cGenCharLocalLogic *self, int ds) {
	CHECK((ds == GENCHAR_STAY) || (ds == GENCHAR_RELAX) || (ds == GENCHAR_CHASE) || (ds == GENCHAR_GET_IT));
	self->defaultState = ds;
}

/* vf03 */
byte __thiscall cGenCharLocalLogic_VarI(cGenCharLocalLogic *self, const char *name, int *i, byte set) {
	if (strcmp(name, "DefaultState")) {
		return cBaseLocalLogic_VarI(&self->base, name, i, set);
	}
	if (set) {
		GenChar_SetDefaultState(self, *i);
		return 1;
	}
	*i = self->defaultState;
	return 1;
}

byte Logic_IsSuspectType(const char *type) {
	if (Alarm_IsLocked(g_pWorld->alarm)) {
		return 0;
	}
	return !strcmp(type, "PoliceBoss") || !strcmp(type, "Betty") || !strcmp(type, "Berto") || !strcmp(type, "Mayor");
}

void __thiscall Logic_SetState2(cBaseLocalLogic *self, cEntity *e) {
	self->vftable->SetState(self, GENCHAR_ACCUSE, e);
}

void __thiscall Logic_Nop2(void *self, int a, int b) {
	(void)self;
	(void)a;
	(void)b;
}

byte __thiscall Logic_True2(void *self, int a, int b) {
	(void)self;
	(void)a;
	(void)b;
	return 1;
}

/* the random reaction of the hit slots: rand() 0..100 below 101, always */
static inline byte GenChar_Rand101(void) {
	return 101.0f > (float)(Weap_Rand01() * 100.0 + 0.0);
}

/* slot 0x80 of the AI of a character */
static inline int GenChar_AISubState(cAIChar *ai) {
	return (*(int(__thiscall **)(cAIChar *))((byte *)ai->vftable + 0x80))(ai);
}

/* vf17 */
void __thiscall cGenCharLocalLogic_Update(cGenCharLocalLogic *self, float dt) {
	cBaseLocalLogic *b = &self->base;
	vec3_t t;

	self->searchTime = self->searchTime - dt;
	cBaseLocalLogic_Update(b, dt);
	GENCHAR_VT(self)->UpdateView(self);
	float ag = self->accuseAgain - dt;
	self->accuseAgain = ag;
	if (0.0f >= ag) {
		self->accuseAgain = 0.0f;
	}
	if (b->busy) {
		GENCHAR_VT(self)->Vf33(self, 1);
		return;
	}
	cEntity *usr = ChType_Usr0();
	if (!usr || !usr->vftable->IsType(usr, TYEN_WALKCHAR)) {
		return;
	}
	cEntity *p = b->parent;
	const char *type = Logic_TypeName(p);
	const char *usrType = Logic_TypeName(usr);
	byte discover = cGenCharLocalLogic_CanDiscover(self, p) && self->accuseAgain == 0.0f;
	byte alarm = g_pWorld->alarmOn && !self->ignoreAlarm;
	byte sameChar = World_Alarm()->sameChar;
	byte seen = Logic_InView(p, usr->vftable->GetPos(usr, &t), g_GenCharStdFar, g_GenCharStdFovY, g_GenCharStdFovX, 1);
	byte suspect = Logic_IsSuspectType(type);
	byte same = !strcmp(type, usrType) && suspect;
	cAIChar *ai = b->parent->vftable->GetAI(b->parent);
	if (suspect && !Alarm_IsLocked(g_pWorld->alarm)) {
		WChar_ViewFovY(p) = g_GenCharStdFovY;
		WChar_ViewFar(p) = g_GenCharStdFar;
		WChar_ViewMode(p) = same ? 2 : 0;
		Entity_Team(p) = same ? (g_pWorld->alarmOn ? 0 : 2) : 3;
	}
	int st = b->state;
	if (st != GENCHAR_ACCUSE && seen && discover) {
		if (!sameChar || !alarm) {
			if (!same) {
				goto states;
			}
			Alarm_Set(g_pWorld->alarm, 1.0f);
		}
		GENCHAR_VT(self)->Accuse(self, usr);
	}
states:
	if (st == GENCHAR_RELAX || st == GENCHAR_STAY || st == 7 || st == 8) {
		if (alarm) {
			if (!self->ignoreAlarm) {
				GENCHAR_VT(self)->SetState(self, GENCHAR_ALERT, NULL);
			}
		} else if (((cObjEntity *)p)->move.active && (b->idle || b->alwaysLock)) {
			cObjMove_SetTime(&((cObjEntity *)p)->move, 0.0f);
			GENCHAR_VT(self)->Vf33(self, 0);
		} else if (!b->idle && !b->alwaysLock) {
			cEntity *e;
			if (GENCHAR_VT(self)->CanAttack(self, p) && (e = GENCHAR_VT(self)->FindTarget(self)) != NULL) {
				if (e->vftable->IsType(e, TYEN_WITH_LIFE) && !Logic_RadarShow(e) &&
					Logic_InView(p, usr->vftable->GetPos(usr, &t), 3000.0f, 6.2831855f, 6.2831855f, 1)) {
					GENCHAR_VT(self)->SetState(self, GENCHAR_ATTACK, e);
					self->specialOn = 1;
				}
			} else if (b->state != GENCHAR_ATTACK) {
				GENCHAR_VT(self)->Work(self, dt);
			}
		}
	}
	switch (b->state) {
	case GENCHAR_ALERT:
		if (!alarm) {
			GENCHAR_VT(self)->RestartState(self);
		}
		return;
	case GENCHAR_ACCUSE:
		if (!b->target.target) {
			GENCHAR_VT(self)->RestartState(self);
			return;
		}
		cGenCharLocalLogic_Discover(self);
		cGenCharGroup_RaiseLevel(World_GenCharGroup(), 0);
		if (b->target.target && sameChar) {
			if (seen) {
				self->time = g_GenCharAcuseTime;
				AICtrl_SendSentinelToWatch(g_pWorld->aiCtrl, usr->vftable->GetPos(usr, &t));
				AICtrl_GearAttack(g_pWorld->aiCtrl, usr, 1);
				return;
			}
			if (self->time >= 0.0f) {
				self->time = self->time - dt;
				return;
			}
		}
		GENCHAR_VT(self)->SetState(self, GENCHAR_ALERT, NULL);
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
				GENCHAR_VT(self)->RestartState(self);
				return;
			}
			if (!self->ignoreAlarm) {
				GENCHAR_VT(self)->SetState(self, GENCHAR_ALERT, NULL);
			}
		}
		return;
	}
	case GENCHAR_ATTACK:
		if (!alarm || self->alarmState == GENCHAR_ATTACK) {
			if (!b->target.target || GENCHAR_VT(self)->TargetDone(self)) {
				GENCHAR_VT(self)->RestartState(self);
				cGenCharLocalLogic_ResetSpecial(self);
			}
			return;
		}
		if (!self->ignoreAlarm) {
			GENCHAR_VT(self)->SetState(self, GENCHAR_ALERT, NULL);
		}
		cGenCharLocalLogic_ResetSpecial(self);
		return;
	case GENCHAR_STAY:
		if (ai->vftable->GetState(ai) != 4 && !cAIChar_TestFlags(self)) {
			vec3_t o = b->origin;
			const vec3_t *pp = b->parent->vftable->GetPos(b->parent, &t);
			float dy = pp->y - o.y, dx = pp->x - o.x, dz = pp->z - o.z;
			if (dy * dy + dx * dx + dz * dz > 40000.0f) {
				GENCHAR_VT(self)->SetIdle(self, 1);
				GENCHAR_VT(self)->SetState(self, GENCHAR_STAY, NULL);
			}
		}
		return;
	case GENCHAR_GET_IT:
		GENCHAR_VT(self)->UpdateGetIt(self, ai, dt);
		return;
	}
}

/* vf12: a robbery seen, with the chance GenCharRobberyDetection plus the amount */
void __thiscall cGenCharLocalLogic_OnRobbery(cGenCharLocalLogic *self, cEntity *e, float amount) {
	if (amount > 0.0f && Logic_Chance(&self->base, e, g_GenCharRobberyDetection + amount)) {
		GENCHAR_VT(self)->SetState(self, self->alarmState, e);
		cGenCharLocalLogic_ResetSpecial(self);
	}
}

/* vf10: the state of the alarm against the attacker, no accuse for a while */
void __thiscall cGenCharLocalLogic_OnHit(cGenCharLocalLogic *self, cEntity *attacker) {
	if (self->base.busy || !GenChar_Rand101()) {
		return;
	}
	self->accuseAgain = g_GenCharAcuseAgainTime;
	GENCHAR_VT(self)->SetState(self, self->alarmState, attacker);
	cGenCharLocalLogic_ResetSpecial(self);
}

/* vf11: the same, not while accusing */
void __thiscall cGenCharLocalLogic_OnSeen(cGenCharLocalLogic *self, cEntity *e) {
	if (self->base.state == GENCHAR_ACCUSE || self->base.busy || !GenChar_Rand101()) {
		return;
	}
	GENCHAR_VT(self)->SetState(self, self->alarmState, e);
	cGenCharLocalLogic_ResetSpecial(self);
}

/* vf15: 1 if it refuses the police (GenCharPoliceRefuse in percent), else the state of the alarm against it */
byte __thiscall cGenCharLocalLogic_GivesMoney(cGenCharLocalLogic *self, cEntity *police) {
	float refuse = g_GenCharPoliceRefuse;

	if (!self->base.busy && refuse > (float)(Weap_Rand01() * 100.0 + 0.0)) {
		return 1;
	}
	GENCHAR_VT(self)->SetState(self, self->alarmState, police);
	return 0;
}

/* vf35: state 10 ends when the AI arrived */
void __thiscall cGenCharLocalLogic_UpdateGetIt(cGenCharLocalLogic *self, cAIChar *ai, float dt) {
	(void)dt;
	if (ai->vftable->GetState(ai) != 4) {
		GENCHAR_VT(self)->RestartState(self);
	}
}

void __fastcall cGenCharLocalLogic_ResetSpecial(cGenCharLocalLogic *self) {
	self->specialOn = 0;
	self->specialTime = 0.0f;
	self->searchTime = Rand_FloatRange(g_GenCharMinSpecialActionTime, g_GenCharMaxSpecialActionTime);
}

void __fastcall cGenCharLocalLogic_Discover(cGenCharLocalLogic *self) {
	cEntity *p = self->base.parent;

	if (WChar_ControlF4f8(p) == 0.0f && WChar_ControlF4fc(p) == 0.0f &&
		!cActCtrl_IsActive(ChType_ActCtrl(p), "Discover")) {
		cActCtrl_PlayNew(ChType_ActCtrl(p), "DiscoverInit", 0, 1.0f);
		cActCtrl_SetDefLoop(ChType_ActCtrl(p), "Discover", 0);
	}
}

/* vf24: the alert with the alarm (unless IgnoreAlarm), else the default state with its target; OnDefStateCallback
 * (name) when that state was taken */
byte __thiscall cGenCharLocalLogic_RestartState(cGenCharLocalLogic *self) {
	cGenCharLocalLogic_ResetSpecial(self);
	if (g_pWorld->alarmOn && !self->ignoreAlarm) {
		return GENCHAR_VT(self)->SetState(self, GENCHAR_ALERT, NULL);
	}
	cEntity *t = World_FindEntity(self->defaultTarget);
	byte ok = GENCHAR_VT(self)->SetState(self, self->defaultState, t);
	if (ok && (byte)cPy_Function_IsValid(g_OnDefStateCallback)) {
		cPy_Function_AddParamS(g_OnDefStateCallback, self->base.parent->name);
		cPy_Function_Execute(g_OnDefStateCallback, 0);
	}
	return ok;
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

/* vf34: the AI walks to e, faced from the character to it */
void __thiscall cGenCharLocalLogic_GoTo(cGenCharLocalLogic *self, cAIChar *ai, cEntity *e) {
	vec3_t t1, t2, t3, dir;
	cEntity *p = self->base.parent;
	const vec3_t *pp = p->vftable->GetPos(p, &t1);
	const vec3_t *ep = e->vftable->GetPos(e, &t2);

	GenChar_Dir(&dir, pp, ep);
	const vec3_t *tp = e->vftable->GetPos(e, &t3);
	ai->vftable->SetState(ai, 4, *tp, dir, 70.0, 1.0);
}

/* vf19: the AI state of the logic state; always 1 */
byte __thiscall cGenCharLocalLogic_SetState(cGenCharLocalLogic *self, int state, cEntity *target) {
	cBaseLocalLogic *b = &self->base;
	byte ok;

	if (state == GENCHAR_GETAWAY && b->state == GENCHAR_GETAWAY && !b->idle && !b->alwaysLock) {
		ok = 0;
	} else {
		ok = (byte)cBaseLocalLogic_SetState(b, state, target);
	}
	cAIChar *ai = b->parent->vftable->GetAI(b->parent);
	cEntity *p = b->parent;
	if (ok) {
		GENCHAR_VT(self)->UpdateView(self);
	}
	switch (b->state) {
	case GENCHAR_RELAX:
		if (ok) {
			GENCHAR_VT(self)->ToRelax(self, ai);
		}
		break;
	case GENCHAR_ALERT:
		if (ok) {
			GENCHAR_VT(self)->ToAlert(self, ai);
		}
		break;
	case GENCHAR_ACCUSE:
		if (ok && World_Alarm()->sameChar) {
			self->time = g_GenCharAcuseTime;
			ai->vftable->SetState(ai, 8, target, 1);
			if (ChType_Vtbl(p)->FindAction(p, "Discover")) {
				cGenCharLocalLogic_Discover(self);
				cGenCharGroup_RaiseLevel(World_GenCharGroup(), 0);
			}
		}
		break;
	case GENCHAR_GETAWAY:
		if (ok) {
			ai->vftable->SetState(ai, 5, target, (double)g_GenCharEscapeSpeed);
		}
		self->time = 0.0f;
		break;
	case GENCHAR_ATTACK:
		if (ok) {
			if (!strcmp(Logic_TypeName(p), "Mayor")) {
				cAIChar *a2 = b->parent->vftable->GetAI(b->parent);
				a2->vftable->SetState(a2, 2, target, (double)g_GenCharAttackSpeed);
			} else {
				float speed = target == cWorld_GetUsrEntity(g_pWorld, 0) ? g_GenCharAttackSpeed : 1.0f;
				ai->vftable->SetState(ai, 2, target, (double)speed);
			}
		}
		break;
	case GENCHAR_CHASE:
		if (ok) {
			if (0.0f > g_GenCharSpecialChaseSpeed) {
				ai->vftable->SetState(ai, 3, target, (double)g_GenCharAlertSpeed);
			} else {
				ai->vftable->SetState(ai, 3, target, (double)g_GenCharSpecialChaseSpeed);
			}
		}
		break;
	case GENCHAR_STAY:
		if (ok) {
			ai->vftable->SetState(ai, 4, b->origin, b->originAng, 70.0, 1.0);
		}
		break;
	case GENCHAR_GET_IT:
		if (ok && target) {
			GENCHAR_VT(self)->GoTo(self, ai, target);
		}
		break;
	}
	return 1;
}

void __thiscall cGenCharLocalLogic_SetDefaultState(cGenCharLocalLogic *self, int ds) {
	GenChar_SetDefaultState(self, ds);
}

void __thiscall cGenCharLocalLogic_SetDefaultTarget(cGenCharLocalLogic *self, const char *name) {
	cString_Set(&self->defaultTarget, name);
}

/* vf05 */
byte __thiscall cGenCharLocalLogic_VarS(cGenCharLocalLogic *self, const char *name, char **s, byte set) {
	if (strcmp(name, "DefaultTarget")) {
		return cBaseLocalLogic_VarS(&self->base, name, s, set);
	}
	if (set) {
		cString_Set(&self->defaultTarget, *s);
		return 1;
	}
	*s = self->defaultTarget;
	return 1;
}

/* vf25: the special attack of e is allowed (energy bar and special actions on, GenCharAttackActive), e is not
 * idle, near a camera (GenCharAttackLimit), not on the radar and its AI not in sub state 12 */
byte __thiscall cGenCharLocalLogic_CanAttack(cGenCharLocalLogic *self, cEntity *e) {
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
	float d = cWorld_GetMinCamDistSq(g_pWorld, e->vftable->GetPos(e, &t));
	if (!(g_GenCharAttackLimit * g_GenCharAttackLimit > d) || Logic_RadarShow(e) || !e->vftable->GetAI(e)) {
		return 0;
	}
	return GenChar_AISubState(e->vftable->GetAI(e)) != 12;
}
