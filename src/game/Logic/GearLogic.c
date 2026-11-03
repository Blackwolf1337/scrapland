/*
 * Logic/GearLogic.cpp of the game: the gears (cGearLocalLogic, include/scrap/logic.h), the police robots. They
 * patrol (relax), stay at their origin or escort the default target; the AI control calls them after a target
 * (state 1), a gear that sees the player with the alarm on, of a type that is not guaranteed, or hit attacks it
 * (state 2) and plays its siren; a target lost for GearLostTargetTime sends it back to its default state.
 */
#include <string.h>

#include "scrap/gvar.h"
#include "scrap/chartype.h"
#include "scrap/objentity.h"
#include "scrap/scrapcollision.h"
#include "scrap/weapons.h"
#include "scrap/staticinit.h"

#define CGEARLOCALLOGIC_VFTABLE (&cGearLocalLogic_vftable)
static const cBaseLocalLogicVtbl cGearLocalLogic_vftable;

cGVar GVar_GearStdFar;

#define g_GearStdFar (GVar_GearStdFar.value.f)
static cGVar GVar_GearStdFovY;
#define g_GearStdFovY (GVar_GearStdFovY.value.f)
static cGVar GVar_GearStdFovX;
#define g_GearStdFovX (GVar_GearStdFovX.value.f)
static cGVar GVar_GearLostTargetTime;
#define g_GearLostTargetTime (GVar_GearLostTargetTime.value.f)
static cGVar GVar_GearRelaxSpeed;
#define g_GearRelaxSpeed (GVar_GearRelaxSpeed.value.f)
static cGVar GVar_GearRobberyDetection;
#define g_GearRobberyDetection (GVar_GearRobberyDetection.value.f)
static cGVar GVar_Gear2PoliceAlways;
#define g_Gear2PoliceAlways (GVar_Gear2PoliceAlways.value.i)
static cGVar GVar_GearChaseSpeed;
#define g_GearChaseSpeed (GVar_GearChaseSpeed.value.f)
static cGVar GVar_GearAttackSpeed;
#define g_GearAttackSpeed (GVar_GearAttackSpeed.value.f)
static cGVar GVar_GearAttackNPCTime;
#define g_GearAttackNPCTime (GVar_GearAttackNPCTime.value.f)
static cGVar GVar_GearScortSpeed;
#define g_GearScortSpeed (GVar_GearScortSpeed.value.f)

int Vec3_Differ(const float *a, const float *b);

/* the AI of the character still has its target */
static inline byte Gear_AIHasTarget(void *ai) {
	cAIChar *a = ai;

	return AIChar_GetDirect(a);
}

static inline cAIChar *Gear_AI(cGearLocalLogic *self) {
	cEntity *p = self->base.parent;

	return Entity_GetAI(p);
}

cGearLocalLogic *cGearLocalLogic_ctor(cGearLocalLogic *self, cEntity *parent) {
	cBaseLocalLogic_ctor(&self->base, parent);
	self->base.vftable = CGEARLOCALLOGIC_VFTABLE;
	self->lostTime = 0.0f;
	self->called = 0;
	self->defaultState = 0;
	cString_ctorStr(&self->defaultTarget, str_Empty);
	self->base.state = 0;
	cBaseLocalLogic_SetIdle(&self->base.logic, 0);
	self->sirenTime = (float)(Weap_Rand01() * (double)0.2f + 0.0);
	return self;
}

static void cGearLocalLogic_dtor(cGearLocalLogic *self) {
	self->base.vftable = CGEARLOCALLOGIC_VFTABLE;
	cString_dtor(&self->defaultTarget);
	cBaseLocalLogic_dtor(&self->base);
}

static cLocalLogic *cGearLocalLogic_Delete(cLocalLogic *base, byte flags) {
	cGearLocalLogic *self = (cGearLocalLogic *)base;
	cGearLocalLogic_dtor(self);
	if (flags & 1) {
		operator_delete_sized(self, sizeof(*self));
	}
	return base;
}

/* the patrol walk ends in Relax */
static void cGearLocalLogic_Relax(cGearLocalLogic *self) {
	cEntity *p = self->base.parent;

	if (!strncmp(cActCtrl_GetActionName(ChType_ActCtrl(p), 0), "Patrol", 6)) {
		cActCtrl_PlayNew(ChType_ActCtrl(p), "Relax", 1, 1.0f);
	}
}

/* vf16 */
static void cGearLocalLogic_Vf16(cLocalLogic *base, cEntity *arg) {
	cGearLocalLogic *self = (cGearLocalLogic *)base;
	(void)self;
	(void)arg;
	Con_Print(0x8f008f, "El Gear intenta huir de un poli pequeño. Esto no debe pasar nunca", 0);
}

static void cGearLocalLogic_SetDefaultState(cGearLocalLogic *self, int v) {
	CHECK((v == GEAR_STAY_IN_PLACE) || (v == GEAR_RELAX) || (v == GEAR_SCORT));
	self->defaultState = v;
}

/* vf03 */
static byte cGearLocalLogic_VarI(cLocalLogic *elem, const char *name, int *i, byte set) {
	cGearLocalLogic *self = (cGearLocalLogic *)elem;
	if (strcmp(name, "DefaultState")) {
		return cBaseLocalLogic_VarI(&self->base.logic, name, i, set);
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

	Logic_SetState(self, self->defaultState, t);
}

/* the view of the AI and the actions of the state */
static void cGearLocalLogic_UpdateAnim(cGearLocalLogic *self, cEntity *p, float dt) {
	cBaseLocalLogic *b = &self->base;
	byte alarm = g_pWorld->alarm.on;

	(void)dt;
	WLife(p)->arrow.coneFov = g_GearStdFovY;
	WLife(p)->arrow.coneDist = g_GearStdFar;
	if (g_pWorld->alarm.on) {
		cEntity *t = b->target.target;
		if (t && b->state == 2 && t == cWorld_GetUsrEntity(g_pWorld, 0)) {
			WLife(p)->arrow.coneStatus = 3;
		} else {
			WLife(p)->arrow.coneStatus = 2;
		}
	} else {
		WLife(p)->arrow.coneStatus = g_pWorld->alarm.notGuaranteed != 0;
	}
	switch (b->state) {
	case 0:
		self->called = 0;
		WChar(p)->wl.arrow.canUse = alarm == 0;
		if (!b->idle && !b->alwaysLock && !cActCtrl_IsActive(ChType_ActCtrl(p), "Patrol")) {
			cActCtrl_PlayNew(ChType_ActCtrl(p), "Patrol0", 0, 1.0f);
		}
		break;
	case 1:
	case 2:
		WChar(p)->wl.arrow.canUse = 0;
		cGearLocalLogic_Relax(self);
		break;
	case 3:
		self->called = 0;
		WChar(p)->wl.arrow.canUse = alarm == 0;
		cGearLocalLogic_Relax(self);
		break;
	case 4:
		self->called = 0;
		WChar(p)->wl.arrow.canUse = alarm == 0;
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
static void cGearLocalLogic_Update(cLocalLogic *elem, float dt) {
	cGearLocalLogic *self = (cGearLocalLogic *)elem;
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
	cBaseLocalLogic_Update(&b->logic, dt);
	WChar(p)->wl.arrow.canUse = 0;
	WLife(p)->arrow.tgType = alarm ? 0 : 2;
	if (b->busy) {
		WLife(p)->arrow.coneStatus = 0;
		return;
	}
	cEntity *usr = ChType_Usr0();
	if (!usr) {
		return;
	}
	byte same = g_pWorld->alarm.sameChar;
	byte seen = Logic_InView(p, Entity_GetPos(usr, &tp), g_GearStdFar, g_GearStdFovY, g_GearStdFovX, 1);
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
		Logic_SetState(self, 2, usr);
		goto check;
	case 3: {
		Entity_GetPos(b->parent, &tp);
		vec3_t o = b->origin;
		if (!locked && alarm && same && seen) {
			Logic_SetState(self, 2, usr);
			goto check;
		}
		if ((byte)Vec3_Differ(&tp.x, &o.x) && !locked) {
			cAIChar *ai = Gear_AI(self);
			if (AIChar_GetState(ai) != AIC_GO_TO) {
				float dy = tp.y - o.y, dx = tp.x - o.x, dz = tp.z - o.z;
				if (dy * dy + dx * dx + dz * dz > 1000000.0f) {
					ai = Gear_AI(self);
					AIChar_SetState(ai, AIC_GO_TO, b->origin, b->originAng, 100.0, (double)g_GearRelaxSpeed);
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
			Logic_SetState(self, 0, NULL);
			goto check;
		}
		if (!alarm && t == cWorld_GetUsrEntity(g_pWorld, 0)) {
			goto deflt;
		}
		if (Gear_AIHasTarget(Gear_AI(self))) {
			self->lostTime = 0.0f;
			if (!self->called) {
				Logic_SetState(self, 2, usr);
			} else if (b->target.target) {
				Logic_SetState(self, 2, b->target.target);
			}
			goto check;
		}
		float lt = dt * 0.5f + self->lostTime;
		self->lostTime = lt;
		if (lt > g_GearLostTargetTime) {
			if (!alarm) {
				goto deflt;
			}
			Logic_SetState(self, 0, NULL);
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
			const vec3_t *gp = Entity_GetPos(b->parent, &tp);
			if (Engine_TestLine(Entity_GetPos(usr, &op), gp, NULL, NULL)) {
				goto check;
			}
			Alarm_SetOn(&g_pWorld->alarm, 1);
			Logic_SetState(self, 2, usr);
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
				Logic_SetState(self, 0, NULL);
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
		Logic_SetState(self, 2, usr);
	}
anim:
	cGearLocalLogic_UpdateAnim(self, p, dt);
}

/* vf12: a robbery seen, with the chance GearRobberyDetection plus the amount */
static void cGearLocalLogic_OnRobbery(cLocalLogic *elem, cEntity *e, float amount) {
	cGearLocalLogic *self = (cGearLocalLogic *)elem;
	if (amount > 0.0f && Logic_Chance(&self->base, e, g_GearRobberyDetection + amount)) {
		if (e == cWorld_GetUsrEntity(g_pWorld, 0)) {
			Alarm_Set(&g_pWorld->alarm, 1.0f);
		}
		Logic_SetState(self, 2, e);
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
static void cGearLocalLogic_OnHit(cLocalLogic *elem, cEntity *attacker) {
	cGearLocalLogic *self = (cGearLocalLogic *)elem;
	if (!Gear_React(self, attacker)) {
		return;
	}
	if (self->base.state != 2 || self->base.target.target != cWorld_GetUsrEntity(g_pWorld, 0)) {
		Logic_SetState(self, 2, attacker);
	}
}

/* vf14 and vf11 */
static void cGearLocalLogic_Vf14(cLocalLogic *base, cEntity *e) {
	cGearLocalLogic *self = (cGearLocalLogic *)base;
	if (Gear_React(self, e)) {
		Logic_SetState(self, 2, e);
	}
}

/* vf15: the same, returns 0 */
static byte cGearLocalLogic_Vf15(cLocalLogic *base, cEntity *e) {
	cGearLocalLogic *self = (cGearLocalLogic *)base;
	if (Gear_React(self, e)) {
		Logic_SetState(self, 2, e);
	}
	return 0;
}

/* vf18: the gear becomes a police, during an alarm only with Gear2PoliceAlways */
static void cGearLocalLogic_ToPolice(cLocalLogic *base, cEntity *e) {
	cGearLocalLogic *self = (cGearLocalLogic *)base;
	(void)self;
	if (!g_Gear2PoliceAlways && g_pWorld->alarm.on) {
		return;
	}
	AICtrl_GearToPolice(&g_pWorld->logic.aiCtrl, e);
}

/* the AI control sends the gear after e; 0 if it attacks e already and its AI has it */
byte cGearLocalLogic_Call(cGearLocalLogic *self, cEntity *e) {
	self->called = 1;
	if (self->base.state == 2 && e == self->base.target.target && Gear_AIHasTarget(Gear_AI(self))) {
		return 0;
	}
	return Logic_SetState(self, 1, e);
}

/* vf24 */
static void cGearLocalLogic_RestartState(cLocalLogic *base) {
	cGearLocalLogic *self = (cGearLocalLogic *)base;
	Gear_ToDefault(self);
}

/* vf19: the AI state of the logic state with the gear speeds */
static byte cGearLocalLogic_SetState(cLocalLogic *elem, int state, cEntity *target) {
	cGearLocalLogic *self = (cGearLocalLogic *)elem;
	cBaseLocalLogic *b = &self->base;
	cAIChar *ai = Gear_AI(self);
	byte ok = (byte)cBaseLocalLogic_SetState(&b->logic, state, target);
	cEntity *usr = ChType_Usr0();

	switch (b->state) {
	case 0:
		if (ok) {
			AIChar_SetState(ai, AIC_PATROL_ZONE, b->origin, (double)b->actionRadius, (double)g_GearRelaxSpeed);
		}
		break;
	case 1:
		if (ok) {
			AIChar_SetState(ai, AIC_CHASE_WITH_ACTION, target, (double)(target == usr ? g_GearChaseSpeed : 1.0f));
		}
		self->lostTime = 0.0f;
		break;
	case 2:
		if (ok) {
			AIChar_SetState(ai, AIC_CHASE_WITH_ACTION, target, (double)(target == usr ? g_GearAttackSpeed : 1.0f));
			if (target != cWorld_GetUsrEntity(g_pWorld, 0)) {
				self->lostTime = g_GearLostTargetTime - g_GearAttackNPCTime;
			}
		}
		break;
	case 3:
		if (ok) {
			AIChar_SetState(ai, AIC_GO_TO, b->origin, b->originAng, 100.0, (double)g_GearRelaxSpeed);
		}
		break;
	case 4:
		if (ok) {
			if (target) {
				AIChar_SetState(ai, AIC_CHASE_WITHOUT_ACTION, target, (double)g_GearScortSpeed);
			} else {
				Logic_SetState(self, 0, NULL);
			}
		}
		break;
	}
	return ok;
}

/* vf05 */
static byte cGearLocalLogic_VarS(cLocalLogic *elem, const char *name, char **s, byte set) {
	cGearLocalLogic *self = (cGearLocalLogic *)elem;
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

/* the vtable of cGearLocalLogic */
static const cBaseLocalLogicVtbl cGearLocalLogic_vftable = {
	.base.Delete = cGearLocalLogic_Delete,
	.base.GetName = cBaseLocalLogic_GetName,
	.base.Vf02 = cLocalLogic_Vf02True,
	.base.VarI = cGearLocalLogic_VarI,
	.base.VarF = cBaseLocalLogic_VarF,
	.base.VarS = cGearLocalLogic_VarS,
	.base.VarV = cBaseLocalLogic_VarV,
	.UpdateBusy = cBaseLocalLogic_UpdateBusy,
	.Refresh = cBaseLocalLogic_Refresh,
	.Listens = cLocalLogic_ListensFalse,
	.OnHit = cGearLocalLogic_OnHit,
	.OnSeen = cGearLocalLogic_Vf14,
	.OnAttack = cGearLocalLogic_OnRobbery,
	.OnShot = cBaseLocalLogic_OnShot,
	.OnHammer = cGearLocalLogic_Vf14,
	.GivesMoney = cGearLocalLogic_Vf15,
	.OnAlarm = cGearLocalLogic_Vf16,
	.Update = cGearLocalLogic_Update,
	.SetOriginFrom = cGearLocalLogic_ToPolice,
	.SetState = cGearLocalLogic_SetState,
	.SetIdle = cBaseLocalLogic_SetIdle,
	.SetActionRadius = cBaseLocalLogic_SetActionRadius,
	.SetOrigin = cBaseLocalLogic_SetOrigin,
	.SetOriginAng = cBaseLocalLogic_SetOriginAng,
	.RestartState = cGearLocalLogic_RestartState,
};

/* the dynamic initializers of the globals here, run in the order of src/standalone/crtxc.inc */
INIT_GVAR_FLOAT(Init_GVar_GearStdFovX, &GVar_GearStdFovX, "GearStdFovX", 0, "Standard Gear FieldOfView", 1.5707964f)
INIT_GVAR_FLOAT(Init_GVar_GearStdFovY, &GVar_GearStdFovY, "GearStdFovY", 0, "Standard Gear FieldOfView", 1.5707964f)
INIT_GVAR_FLOAT(Init_GVar_GearStdFar, &GVar_GearStdFar, "GearStdFar", 0, "Standard Gear Distance of View", 1500.0f)
INIT_GVAR_FLOAT(Init_GVar_GearRelaxSpeed, &GVar_GearRelaxSpeed, "GearRelaxSpeed", 0, "Gear speed (Relax)", 0.5f)
INIT_GVAR_FLOAT(Init_GVar_GearChaseSpeed, &GVar_GearChaseSpeed, "GearChaseSpeed", 0, "Gear speed (Chase)", 0.75f)
INIT_GVAR_FLOAT(Init_GVar_GearAttackSpeed, &GVar_GearAttackSpeed, "GearAttackSpeed", 0, "Gear speed (Attack)", 0.25f)
INIT_GVAR_FLOAT(Init_GVar_GearScortSpeed, &GVar_GearScortSpeed, "GearScortSpeed", 0, "Gear speed (Scort)", 0.5f)
INIT_GVAR_FLOAT(Init_GVar_GearLostTargetTime, &GVar_GearLostTargetTime, "GearLostTargetTime", 0, "Time since the target got lost", 5.0f)
INIT_GVAR_FLOAT(Init_GVar_GearAttackNPCTime, &GVar_GearAttackNPCTime, "GearAttackNPCTime", 0, "Time attacking an npc character", 10.0f)
INIT_GVAR_FLOAT(Init_GVar_GearRobberyDetection, &GVar_GearRobberyDetection, "GearRobberyDetection", 0, "Gear robery detection percentage", 30.0f)
INIT_GVAR_INT(Init_GVar_Gear2PoliceAlways, &GVar_Gear2PoliceAlways, "Gear2PoliceAlways", 0, "if 0 Gears will become polices only if no alarm", 1)
