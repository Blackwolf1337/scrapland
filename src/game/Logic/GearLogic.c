/*
 * Logic/GearLogic.cpp of the game: the gears (cGearLocalLogic, include/scrap/logic.h), the police robots. They
 * patrol (relax), stay at their origin or escort the default target; the AI control calls them after a target
 * (state 1), a gear that sees the player with the alarm on, of a type that is not guaranteed, or hit attacks it
 * (state 2) and plays its siren; a target lost for GearLostTargetTime sends it back to its default state.
 */
#include <string.h>

#include "scrap/chartype.h"
#include "scrap/objentity.h"
#include "scrap/scrapcollision.h"
#include "scrap/weapons.h"

#define g_GearStdFar GAME_GLOBAL_IN(float, GVar_GearStdFar, 16, 0x44d068)
#define g_GearStdFovY GAME_GLOBAL_IN(float, GVar_GearStdFovY, 16, 0x44d098)
#define g_GearStdFovX GAME_GLOBAL_IN(float, GVar_GearStdFovX, 16, 0x44d080)
#define g_GearLostTargetTime GAME_GLOBAL_IN(float, GVar_GearLostTargetTime, 16, 0x44d038)
#define g_GearRelaxSpeed GAME_GLOBAL_IN(float, GVar_GearRelaxSpeed, 16, 0x44d0c8)
#define g_GearRobberyDetection GAME_GLOBAL_IN(float, GVar_GearRobberyDetection, 16, 0x44d0e0)
#define g_Gear2PoliceAlways GAME_GLOBAL_IN(int, GVar_Gear2PoliceAlways, 16, 0x44d0b0)
#define g_GearChaseSpeed GAME_GLOBAL_IN(float, GVar_GearChaseSpeed, 16, 0x44d0f8)
#define g_GearAttackSpeed GAME_GLOBAL_IN(float, GVar_GearAttackSpeed, 16, 0x44d050)
#define g_GearAttackNPCTime GAME_GLOBAL_IN(float, GVar_GearAttackNPCTime, 16, 0x44d020)
#define g_GearScortSpeed GAME_GLOBAL_IN(float, GVar_GearScortSpeed, 16, 0x44d110)

int Vec3_Differ(const float *a, const float *b);

#define Gear_Vtbl(self) ((self)->base.vftable)

/* the AI of the character still has its target */
static inline byte Gear_AIHasTarget(void *ai) {
	cAIChar *a = ai;

	return a->vftable->GetDirect(a);
}

static inline cAIChar *Gear_AI(cGearLocalLogic *self) {
	cEntity *p = self->base.parent;

	return p->vftable->GetAI(p);
}

cGearLocalLogic *__thiscall cGearLocalLogic_ctor(cGearLocalLogic *self, cEntity *parent) {
	cBaseLocalLogic_ctor(&self->base, parent);
	Gear_Vtbl(self) = CGEARLOCALLOGIC_VFTABLE;
	self->lostTime = 0.0f;
	self->called = 0;
	self->defaultState = 0;
	cString_ctorStr(&self->defaultTarget, LOGIC_EMPTY);
	self->base.state = 0;
	cBaseLocalLogic_SetIdle(&self->base, 0);
	self->sirenTime = (float)(Weap_Rand01() * (double)0.2f + 0.0);
	return self;
}

void __fastcall cGearLocalLogic_dtor(cGearLocalLogic *self) {
	Gear_Vtbl(self) = CGEARLOCALLOGIC_VFTABLE;
	cString_dtor(&self->defaultTarget);
	cBaseLocalLogic_dtor(&self->base);
}

cGearLocalLogic *__thiscall cGearLocalLogic_Delete(cGearLocalLogic *self, byte flags) {
	cGearLocalLogic_dtor(self);
	if (flags & 1) {
		operator_delete_sized(self, sizeof(*self));
	}
	return self;
}

/* the patrol walk ends in Relax */
void __fastcall cGearLocalLogic_Relax(cGearLocalLogic *self) {
	cEntity *p = self->base.parent;

	if (!strncmp(cActCtrl_GetActionName(ChType_ActCtrl(p), 0), "Patrol", 6)) {
		cActCtrl_PlayNew(ChType_ActCtrl(p), "Relax", 1, 1.0f);
	}
}

/* vf16 */
void __thiscall cGearLocalLogic_Vf16(cGearLocalLogic *self, int arg) {
	(void)self;
	(void)arg;
	Con_Print(0x8f008f, GAME_STR(str_3befa0, 0x3befa0), 0);
}

int __thiscall cGearLocalLogic_GetDefaultState(cGearLocalLogic *self) {
	return self->defaultState;
}

char *__thiscall cGearLocalLogic_GetDefaultTarget(cGearLocalLogic *self) {
	return self->defaultTarget;
}

void __thiscall cGearLocalLogic_SetDefaultState(cGearLocalLogic *self, int v) {
	CHECK((v == GEAR_STAY_IN_PLACE) || (v == GEAR_RELAX) || (v == GEAR_SCORT));
	self->defaultState = v;
}

/* vf03 */
byte __thiscall cGearLocalLogic_VarI(cGearLocalLogic *self, const char *name, int *i, byte set) {
	if (strcmp(name, "DefaultState")) {
		return cBaseLocalLogic_VarI(&self->base, name, i, set);
	}
	if (set) {
		cGearLocalLogic_SetDefaultState(self, *i);
		return 1;
	}
	*i = self->defaultState;
	return 1;
}

/* the default state with its target */
static void Gear_ToDefault(cGearLocalLogic *self) {
	cEntity *t = World_FindEntity(self->defaultTarget);

	Gear_Vtbl(self)->SetState(self, self->defaultState, t);
}

/* the view of the AI and the actions of the state */
void __thiscall cGearLocalLogic_UpdateAnim(cGearLocalLogic *self, cEntity *p, float dt) {
	cBaseLocalLogic *b = &self->base;
	byte alarm = g_pWorld->alarm.on;

	(void)dt;
	WChar_ConeFov(p) = g_GearStdFovY;
	WChar_ConeDist(p) = g_GearStdFar;
	if (g_pWorld->alarm.on) {
		cEntity *t = b->target.target;
		if (t && b->state == 2 && t == cWorld_GetUsrEntity(g_pWorld, 0)) {
			WChar_ConeStatus(p) = 3;
		} else {
			WChar_ConeStatus(p) = 2;
		}
	} else {
		WChar_ConeStatus(p) = g_pWorld->alarm.notGuaranteed != 0;
	}
	switch (b->state) {
	case 0:
		self->called = 0;
		WChar_B381(p) = alarm == 0;
		if (!b->idle && !b->alwaysLock && !cActCtrl_IsActive(ChType_ActCtrl(p), "Patrol")) {
			cActCtrl_PlayNew(ChType_ActCtrl(p), "Patrol0", 0, 1.0f);
		}
		break;
	case 1:
	case 2:
		WChar_B381(p) = 0;
		cGearLocalLogic_Relax(self);
		break;
	case 3:
		self->called = 0;
		WChar_B381(p) = alarm == 0;
		cGearLocalLogic_Relax(self);
		break;
	case 4:
		self->called = 0;
		WChar_B381(p) = alarm == 0;
		if (!b->idle && !b->alwaysLock && !cActCtrl_IsActive(ChType_ActCtrl(p), "Patrol")) {
			cActCtrl_PlayNew(ChType_ActCtrl(p), "Patrol0", 0, 1.0f);
		}
		if (!b->target.target) {
			Gear_ToDefault(self);
		}
		break;
	default:
		CHECK(0);
	}
}

/* vf17 */
void __thiscall cGearLocalLogic_Update(cGearLocalLogic *self, float dt) {
	vec3_t tp, op;
	cBaseLocalLogic *b = &self->base;
	byte alarm = g_pWorld->alarm.on;
	cEntity *p = b->parent;

	if (alarm) {
		self->sirenTime = self->sirenTime - dt;
	}
	int st = b->state;
	if (st == 1 || st == 2 || (alarm && 0.0f >= self->sirenTime)) {
		if (!*cActCtrl_GetActionName(ChType_ActCtrl(p), 1) ||
			(strcmp(cActCtrl_GetActionName(ChType_ActCtrl(p), 1), "SirenInit") &&
			strcmp(cActCtrl_GetActionName(ChType_ActCtrl(p), 1), "Siren"))) {
			cActCtrl_Play(ChType_ActCtrl(p), "SirenInit", 0, 1.0f);
			self->sirenTime = (float)(Weap_Rand01() * (double)0.4f + 0.0);
		}
	} else if (*cActCtrl_GetActionName(ChType_ActCtrl(p), 1) &&
		strcmp(cActCtrl_GetActionName(ChType_ActCtrl(p), 1), "SirenEnd")) {
		cActCtrl_Play(ChType_ActCtrl(p), "SirenEnd", 0, 1.0f);
	}
	cBaseLocalLogic_Update(b, dt);
	WChar_B381(p) = 0;
	Entity_Team(p) = alarm ? 0 : 2;
	if (b->busy) {
		WChar_ConeStatus(p) = 0;
		return;
	}
	cEntity *usr = ChType_Usr0();
	if (!usr) {
		return;
	}
	byte same = g_pWorld->alarm.sameChar;
	byte seen = Logic_InView(p, usr->vftable->GetPos(usr, &tp), g_GearStdFar, g_GearStdFovY, g_GearStdFovX, 1);
	byte locked = b->idle || b->alwaysLock;
	switch (st) {
	case 0:
		if (locked) {
			goto check;
		}
		if (!alarm) {
			goto deflt;
		}
		goto attack;
	case 4:
		if (locked || !alarm) {
			goto check;
		}
attack:
		if (!same) {
			goto check;
		}
		if (!seen) {
			goto anim;
		}
		Gear_Vtbl(self)->SetState(self, 2, usr);
		goto check;
	case 3: {
		b->parent->vftable->GetPos(b->parent, &tp);
		vec3_t o = b->origin;
		if (!locked && alarm && same && seen) {
			Gear_Vtbl(self)->SetState(self, 2, usr);
			goto check;
		}
		if ((byte)Vec3_Differ(&tp.x, &o.x) && !locked) {
			cAIChar *ai = Gear_AI(self);
			if (ai->vftable->GetState(ai) != 4) {
				float dy = tp.y - o.y, dx = tp.x - o.x, dz = tp.z - o.z;
				if (dy * dy + dx * dx + dz * dz > 1000000.0f) {
					ai = Gear_AI(self);
					ai->vftable->SetState(ai, 4, b->origin, b->originAng, 100.0, (double)g_GearRelaxSpeed);
				}
			}
		}
		goto check;
	}
	case 1: {
		cEntity *t = b->target.target;
		if (!t) {
			if (!alarm) {
				goto deflt;
			}
			Gear_Vtbl(self)->SetState(self, 0, NULL);
			goto check;
		}
		if (!alarm && t == cWorld_GetUsrEntity(g_pWorld, 0)) {
			goto deflt;
		}
		if (Gear_AIHasTarget(Gear_AI(self))) {
			self->lostTime = 0.0f;
			if (!self->called) {
				Gear_Vtbl(self)->SetState(self, 2, usr);
			} else if (b->target.target) {
				Gear_Vtbl(self)->SetState(self, 2, b->target.target);
			}
			goto check;
		}
		float lt = dt * 0.5f + self->lostTime;
		self->lostTime = lt;
		if (lt > g_GearLostTargetTime) {
			if (!alarm) {
				goto deflt;
			}
			Gear_Vtbl(self)->SetState(self, 0, NULL);
		}
		goto check;
	}
	case 2: {
		if (same && seen && b->target.target == cWorld_GetUsrEntity(g_pWorld, 0)) {
			AICtrl_RaiseLevel(&g_pWorld->logic.aiCtrl, 0);
			Alarm_Set(&g_pWorld->alarm, 1.0f);
		}
		cAIChar *ai = Gear_AI(self);
		if (!b->target.target) {
			if (!alarm) {
				goto deflt;
			}
			const vec3_t *gp = b->parent->vftable->GetPos(b->parent, &tp);
			if (Engine_TestLine(usr->vftable->GetPos(usr, &op), gp, NULL, NULL)) {
				goto check;
			}
			Alarm_SetOn(&g_pWorld->alarm, 1);
			Gear_Vtbl(self)->SetState(self, 2, usr);
			goto check;
		}
		if (!Gear_AIHasTarget(ai)) {
			if (g_pWorld->logic.aiCtrl.attacking > 2) {
				self->lostTime = g_GearLostTargetTime;
			}
			float lt = self->lostTime + dt;
			self->lostTime = lt;
			if (lt > g_GearLostTargetTime) {
				self->lostTime = 0.0f;
				if (!alarm) {
					goto deflt;
				}
				Gear_Vtbl(self)->SetState(self, 0, NULL);
			}
			goto check;
		}
		self->lostTime = 0.0f;
		if (same || b->target.target != cWorld_GetUsrEntity(g_pWorld, 0)) {
			goto check;
		}
		goto deflt;
	}
	default:
		goto check;
	}
deflt:
	Gear_ToDefault(self);
check:
	if (seen && st != 2 && g_pWorld->alarm.notGuaranteed) {
		Alarm_Set(&g_pWorld->alarm, 1.0f);
		Gear_Vtbl(self)->SetState(self, 2, usr);
	}
anim:
	cGearLocalLogic_UpdateAnim(self, p, dt);
}

/* vf12: a robbery seen, with the chance GearRobberyDetection plus the amount */
void __thiscall cGearLocalLogic_OnRobbery(cGearLocalLogic *self, cEntity *e, float amount) {
	if (amount > 0.0f && Logic_Chance(&self->base, e, g_GearRobberyDetection + amount)) {
		if (e == cWorld_GetUsrEntity(g_pWorld, 0)) {
			Alarm_Set(&g_pWorld->alarm, 1.0f);
		}
		Gear_Vtbl(self)->SetState(self, 2, e);
	}
}

/* the random reaction of the hit slots (always: the chance is 101 of 100) */
static byte Gear_React(cGearLocalLogic *self, cEntity *e) {
	if (self->base.busy || !(101.0f > (float)(Weap_Rand01() * 100.0 + 0.0))) {
		return 0;
	}
	if (e == ChType_Usr0()) {
		Alarm_Set(&g_pWorld->alarm, 1.0f);
	}
	return 1;
}

/* vf10: the attacker, unless the gear already attacks the player */
void __thiscall cGearLocalLogic_OnHit(cGearLocalLogic *self, cEntity *attacker) {
	if (!Gear_React(self, attacker)) {
		return;
	}
	if (self->base.state != 2 || self->base.target.target != cWorld_GetUsrEntity(g_pWorld, 0)) {
		Gear_Vtbl(self)->SetState(self, 2, attacker);
	}
}

/* vf14 and vf11 */
void __thiscall cGearLocalLogic_Vf14(cGearLocalLogic *self, cEntity *e) {
	if (Gear_React(self, e)) {
		Gear_Vtbl(self)->SetState(self, 2, e);
	}
}

/* vf15: the same, returns 0 */
byte __thiscall cGearLocalLogic_Vf15(cGearLocalLogic *self, cEntity *e) {
	if (Gear_React(self, e)) {
		Gear_Vtbl(self)->SetState(self, 2, e);
	}
	return 0;
}

/* vf18: the gear becomes a police, during an alarm only with Gear2PoliceAlways */
void __thiscall cGearLocalLogic_ToPolice(cGearLocalLogic *self, cEntity *e) {
	(void)self;
	if (!g_Gear2PoliceAlways && g_pWorld->alarm.on) {
		return;
	}
	AICtrl_GearToPolice(&g_pWorld->logic.aiCtrl, e);
}

/* the AI control sends the gear after e; 0 if it attacks e already and its AI has it */
byte __thiscall cGearLocalLogic_Call(cGearLocalLogic *self, cEntity *e) {
	self->called = 1;
	if (self->base.state == 2 && e == self->base.target.target && Gear_AIHasTarget(Gear_AI(self))) {
		return 0;
	}
	return Gear_Vtbl(self)->SetState(self, 1, e);
}

/* vf24 */
void __thiscall cGearLocalLogic_RestartState(cGearLocalLogic *self) {
	Gear_ToDefault(self);
}

void __thiscall cGearLocalLogic_SetDefaultTarget(cGearLocalLogic *self, const char *name) {
	cString_Set(&self->defaultTarget, name);
}

/* vf19: the AI state of the logic state with the gear speeds */
byte __thiscall cGearLocalLogic_SetState(cGearLocalLogic *self, int state, cEntity *target) {
	cBaseLocalLogic *b = &self->base;
	cAIChar *ai = Gear_AI(self);
	byte ok = (byte)cBaseLocalLogic_SetState(b, state, target);
	cEntity *usr = ChType_Usr0();

	switch (b->state) {
	case 0:
		if (ok) {
			ai->vftable->SetState(ai, 6, b->origin, (double)b->actionRadius, (double)g_GearRelaxSpeed);
		}
		break;
	case 1:
		if (ok) {
			ai->vftable->SetState(ai, 2, target, (double)(target == usr ? g_GearChaseSpeed : 1.0f));
		}
		self->lostTime = 0.0f;
		break;
	case 2:
		if (ok) {
			ai->vftable->SetState(ai, 2, target, (double)(target == usr ? g_GearAttackSpeed : 1.0f));
			if (target != cWorld_GetUsrEntity(g_pWorld, 0)) {
				self->lostTime = g_GearLostTargetTime - g_GearAttackNPCTime;
			}
		}
		break;
	case 3:
		if (ok) {
			ai->vftable->SetState(ai, 4, b->origin, b->originAng, 100.0, (double)g_GearRelaxSpeed);
		}
		break;
	case 4:
		if (ok) {
			if (target) {
				ai->vftable->SetState(ai, 3, target, (double)g_GearScortSpeed);
			} else {
				Gear_Vtbl(self)->SetState(self, 0, NULL);
			}
		}
		break;
	}
	return ok;
}

/* vf05 */
byte __thiscall cGearLocalLogic_VarS(cGearLocalLogic *self, const char *name, char **s, byte set) {
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
