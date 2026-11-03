/*
 * Logic/SentinelLogic.cpp of the game: the sentinels, the eyes of the indoor police (cSentinelLocalLogic,
 * include/scrap/logic.h; the list "Sentinel" of the AI control). States: 0 relax and 1 search (the walk around the
 * origin), 2 accuse (the player: the alarm and the gears attack), 3 inspect and 4 suspect (follow the player that
 * is not of the type of the alarm), 5 watch a place the AI control sent it to, 6 accuse a character (the gears
 * attack it while the sentinel sees it or the player is near). The view of the sentinel is SentinelStdFar /
 * SentinelStdFovY / SentinelStdFovX.
 */
#include <string.h>

#include "scrap/gvar.h"
#include "scrap/chartype.h"
#include "scrap/logic.h"
#include "scrap/objentity.h"
#include "scrap/scrap.h"
#include "scrap/scrapmodel.h"
#include "scrap/weapons.h"
#include "scrap/world.h"
#include "scrap/staticinit.h"

static const cSentinelLocalLogicVtbl cSentinelLocalLogic_vftable;
#define CSENTINELLOCALLOGIC_VFTABLE (&cSentinelLocalLogic_vftable.base)
static cGVar GVar_SentinelInspectTime;
#define g_SentinelInspectTime (GVar_SentinelInspectTime.value.f)
static cGVar GVar_SentinelSuspectTime;
#define g_SentinelSuspectTime (GVar_SentinelSuspectTime.value.f)
static cGVar GVar_SentinelBankerNPCDetect;
#define g_SentinelBankerNPCDetect (GVar_SentinelBankerNPCDetect.value.f)
static cGVar GVar_SentinelGotoSpeed;
#define g_SentinelGotoSpeed (GVar_SentinelGotoSpeed.value.f)
static cGVar GVar_SentinelAcuseTime;
#define g_SentinelAcuseTime (GVar_SentinelAcuseTime.value.f)
static cGVar GVar_SentinelGotoTime;
#define g_SentinelGotoTime (GVar_SentinelGotoTime.value.f)
static cGVar GVar_SentinelAcuseSpeed;
#define g_SentinelAcuseSpeed (GVar_SentinelAcuseSpeed.value.f)
static cGVar GVar_SentinelSearchSpeed;
#define g_SentinelSearchSpeed (GVar_SentinelSearchSpeed.value.f)
static cGVar GVar_SentinelSuspectSpeed;
#define g_SentinelSuspectSpeed (GVar_SentinelSuspectSpeed.value.f)
static cGVar GVar_SentinelGotoRadius;
#define g_SentinelGotoRadius (GVar_SentinelGotoRadius.value.f)
extern cGVar GVar_SentinelStdFar;
#define g_SentinelStdFar (GVar_SentinelStdFar.value.f)
extern cGVar GVar_SentinelStdFovY;
#define g_SentinelStdFovY (GVar_SentinelStdFovY.value.f)
static cGVar GVar_SentinelInspectSpeed;
#define g_SentinelInspectSpeed (GVar_SentinelInspectSpeed.value.f)
static cGVar GVar_SentinelBankerDetect;
#define g_SentinelBankerDetect (GVar_SentinelBankerDetect.value.f)
static cGVar GVar_SentinelRelaxSpeed;
#define g_SentinelRelaxSpeed (GVar_SentinelRelaxSpeed.value.f)
extern cGVar GVar_SentinelStdFovX;
#define g_SentinelStdFovX (GVar_SentinelStdFovX.value.f)

static byte cSentinelLocalLogic_SetState(cLocalLogic *elem, int state, cEntity *target);
void *cSentinelLocalLogic_ctor(void *mem, cEntity *parent) {
	cSentinelLocalLogic *self = mem;

	cBaseLocalLogic_ctor(&self->base, parent);
	self->base.vftable = CSENTINELLOCALLOGIC_VFTABLE;
	cSentinelLocalLogic_SetState(&self->base.logic, 0, NULL);
	self->seen = 0;
	return self;
}

static cLocalLogic *cSentinelLocalLogic_Delete(cLocalLogic *elem, byte flags) {
	cSentinelLocalLogic *self = (cSentinelLocalLogic *)elem;
	self->base.vftable = CSENTINELLOCALLOGIC_VFTABLE;
	cBaseLocalLogic_dtor(&self->base);
	if (flags & 1) {
		operator_delete_sized(self, sizeof(*self));
	}
	return elem;
}

/* vf16: a sentinel never runs from the alarm */
static void cSentinelLocalLogic_OnAlarm(cLocalLogic *base, cEntity *e) {
	cSentinelLocalLogic *self = (cSentinelLocalLogic *)base;
	(void)self;
	(void)e;
	Con_Print(0x8f008f, "El centinela intenta huir de un poli pequeño. Esto no debe pasar", 0);
}

/* vf25: the player was in view at the last think */
static byte cSentinelLocalLogic_Seen(cLocalLogic *base) {
	cSentinelLocalLogic *self = (cSentinelLocalLogic *)base;
	return self->seen;
}

void cSentinelLocalLogic_Watch(void *sentinel, const vec3_t *pos) {
	cSentinelLocalLogic *self = sentinel;
	cBaseLocalLogic *b = &self->base;
	vec3_t tmp;
	int st = b->state;

	if (st != 1 && st != 0 && st != 5) {
		return;
	}
	cAIChar *ai = Entity_GetAI(b->parent);
	cBaseLocalLogic_SetState(&b->logic, 5, NULL);
	if (AIChar_GetState(ai) != 4) {
		const vec3_t *pp = Entity_GetPos(b->parent, &tmp);
		vec3_t dir;
		dir.z = pos->z - pp->z;
		dir.x = pos->x - pp->x;
		dir.y = 0.0f;
		AIChar_SetState(ai, 4, *pos, dir, (double)(WLife(b->parent)->prim.radius + g_SentinelGotoRadius),
			(double)g_SentinelGotoSpeed);
	}
	self->time = g_SentinelGotoTime;
}

/* vf26: the view of the player (unless quiet) and the state */
static void cSentinelLocalLogic_UpdateState(cSentinelLocalLogic *self, cEntity *p, float dt);
static void cSentinelLocalLogic_Think(cLocalLogic *elem, float dt, byte quiet) {
	cSentinelLocalLogic *self = (cSentinelLocalLogic *)elem;
	cBaseLocalLogic *b = &self->base;
	cEntity *p = b->parent;
	vec3_t tmp;

	self->seen = 0;
	cEntity *usr = ChType_Usr0();
	cBaseLocalLogic_Update(&b->logic, dt);
	WLife(p)->arrow.tgType = g_pWorld->alarm.on ? 0 : 2;
	if (b->busy) {
		WLife(p)->arrow.coneStatus = 0;
		WChar(p)->wl.arrow.canUse = 0;
		return;
	}
	if (0.0f >= self->time || !b->target.target || quiet) {
		int st = b->state;
		if (st != 1 && st != 0 && st != 5 && !b->idle && !b->alwaysLock) {
			Logic_SetState(self, g_pWorld->alarm.on ? 1 : 0, NULL);
		}
	}
	int st = b->state;
	if (st == 1 || st == 0 || st == 5) {
		self->time = self->time - dt;
	} else {
		cAIChar *ai = Entity_GetAI(p);
		if (!AIChar_GetDirect(ai)) {
			self->time = self->time - dt;
		}
	}
	if (!quiet && usr && Logic_InView(p, Entity_GetPos(usr, &tmp), g_SentinelStdFar, g_SentinelStdFovY,
			g_SentinelStdFovX, 1)) {
		cAlarm *a = &g_pWorld->alarm;
		self->seen = 1;
		if (a->notGuaranteed) {
			Alarm_Set(&g_pWorld->alarm, 1.0f);
			Logic_SetState(self, 2, usr);
		} else if (a->sameChar) {
			Logic_SetState(self, 2, usr);
		} else if (a->notDefChar) {
			if (g_pWorld->alarm.on) {
				Logic_SetState(self, 4, usr);
			} else if (b->state != 6) {
				Logic_SetState(self, 3, usr);
			}
		}
	}
	cSentinelLocalLogic_UpdateState(self, p, dt);
}

/* vf12: a robbery seen: the player is accused at SentinelBankerDetect percent (plus the amount), a character goes
 * to state 6 at SentinelBankerNPCDetect */
static void cSentinelLocalLogic_OnRobbery(cLocalLogic *elem, cEntity *e, float amount) {
	cSentinelLocalLogic *self = (cSentinelLocalLogic *)elem;
	if (self->base.busy) {
		return;
	}
	if (e == ChType_Usr0()) {
		if (amount > 0.0f && g_SentinelBankerDetect + amount >= (float)(Weap_Rand01() * 100.0 + 0.0)) {
			Logic_SetState(self, 2, e);
			Alarm_Set(&g_pWorld->alarm, 1.0f);
		}
		return;
	}
	if (g_SentinelBankerNPCDetect + amount >= (float)(Weap_Rand01() * 100.0 + 0.0)) {
		Logic_SetState(self, 6, e);
	}
}

/* vf13: the player is accused with the alarm, another character goes to state 6 */
static void cSentinelLocalLogic_OnAttack(cLocalLogic *elem, cEntity *e) {
	cSentinelLocalLogic *self = (cSentinelLocalLogic *)elem;
	if (self->base.busy) {
		return;
	}
	if (e == ChType_Usr0()) {
		Alarm_Set(&g_pWorld->alarm, 1.0f);
		Logic_SetState(self, 2, e);
		return;
	}
	Logic_SetState(self, 6, e);
}

/* vf10 and vf11: the player is accused with the alarm */
static void cSentinelLocalLogic_OnUsr(cLocalLogic *elem, cEntity *e) {
	cSentinelLocalLogic *self = (cSentinelLocalLogic *)elem;
	if (self->base.busy) {
		return;
	}
	if (e == ChType_Usr0()) {
		Alarm_Set(&g_pWorld->alarm, 1.0f);
		Logic_SetState(self, 2, e);
	}
}

/* vf15: the police asks for money: accused (the alarm for the player); always 0 */
static byte cSentinelLocalLogic_GivesMoney(cLocalLogic *elem, cEntity *e) {
	cSentinelLocalLogic *self = (cSentinelLocalLogic *)elem;
	if (!self->base.busy) {
		if (e == ChType_Usr0()) {
			Alarm_Set(&g_pWorld->alarm, 1.0f);
		}
		Logic_SetState(self, 2, e);
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
static void cSentinelLocalLogic_UpdateState(cSentinelLocalLogic *self, cEntity *p, float dt) {
	cBaseLocalLogic *b = &self->base;
	cAIChar *ai = Entity_GetAI(b->parent);
	cEntity *s = b->parent;
	cAlarm *a = &g_pWorld->alarm;
	vec3_t tmp;

	(void)dt;
	WLife(p)->arrow.coneFov = g_SentinelStdFovY;
	WLife(p)->arrow.coneDist = g_SentinelStdFar;
	WChar(p)->wl.arrow.canUse = !g_pWorld->alarm.on && !a->notDefChar;
	switch (b->state) {
	case 0:
	case 1:
		WLife(p)->arrow.coneStatus = a->sameChar ? 2 : 1;
		return;
	case 2:
		if (!b->target.target || !g_pWorld->alarm.on) {
			cSentinelLocalLogic_Return(self);
		}
		if (!self->seen) {
			WLife(p)->arrow.coneStatus = 2;
			return;
		}
		WLife(p)->arrow.coneStatus = 3;
		AICtrl_RaiseLevel(&g_pWorld->logic.aiCtrl, 1);
		Alarm_Set(&g_pWorld->alarm, 1.0f);
		if (cWorld_GetUsrEntity(g_pWorld, 0)) {
			AICtrl_GearAttack(&g_pWorld->logic.aiCtrl, cWorld_GetUsrEntity(g_pWorld, 0), 0);
		}
		return;
	case 3:
	case 4: {
		cEntity *t = b->target.target;
		if (!t) {
			break;
		}
		if (a->sameChar) {
			Logic_SetState(self, 2, t);
			return;
		}
		if (!self->seen) {
			WLife(p)->arrow.coneStatus = 1;
			return;
		}
		Sentinel_Relax(s);
		AICtrl_RaiseLevel(&g_pWorld->logic.aiCtrl, 1);
		WLife(p)->arrow.coneStatus = 2;
		return;
	}
	case 5:
		WChar(p)->wl.arrow.canUse = 0;
		WLife(p)->arrow.coneStatus = a->sameChar ? 2 : 1;
		if (AIChar_GetState(ai)) {
			self->time = g_SentinelGotoTime;
			return;
		}
		if (0.0f < self->time) {
			AIChar_SetState(ai, 0);
			cActCtrl_PlayNew(ChType_ActCtrl(s), "Relax0", 0, 1.0f);
			WChar(s)->control.hRot = 0.5f;
			return;
		}
		WChar(s)->control.hRot = 0.0f;
		break;
	case 6:
		if (!g_pWorld->alarm.on && b->target.target && AIChar_GetState(ai)) {
			if (!CGameModel_IsVisible(Obj_GetModel(p))) {
				float d = cWorld_GetMinCamDistSq(g_pWorld, Entity_GetPos(p, &tmp));
				float r = g_SentinelAbortyNPCAttDist;
				if (d > r * r) {
					break;
				}
			}
			if (AIChar_GetDirect(ai)) {
				self->time = g_SentinelAcuseTime;
			}
			WChar(p)->wl.arrow.canUse = 0;
			WLife(p)->arrow.coneStatus = 0;
			AICtrl_GearAttack(&g_pWorld->logic.aiCtrl, b->target.target, 0);
			cLocalLogic *l = Logic_GetLocalLogic(b->target.target);
			if (l) {
				Logic_OnSeen(l, p);
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
static void cSentinelLocalLogic_RestartState(cLocalLogic *base) {
	cSentinelLocalLogic *self = (cSentinelLocalLogic *)base;
	Logic_SetState(self, 0, NULL);
}

void cSentinelLocalLogic_Return(void *sentinel) {
	cSentinelLocalLogic *self = sentinel;

	if (self->base.idle || self->base.alwaysLock) {
		return;
	}
	if (g_pWorld->alarm.on) {
		Logic_SetState(self, 1, NULL);
		return;
	}
	Logic_SetState(self, 0, NULL);
}

/* vf19: the AI of the state; the result of the base */
static byte cSentinelLocalLogic_SetState(cLocalLogic *elem, int state, cEntity *target) {
	cSentinelLocalLogic *self = (cSentinelLocalLogic *)elem;
	cBaseLocalLogic *b = &self->base;
	vec3_t tmp;
	cAIChar *ai = Entity_GetAI(b->parent);
	byte ok = (byte)cBaseLocalLogic_SetState(&b->logic, state, target);

	switch (b->state) {
	case 0:
		if (ok) {
			AIChar_SetState(ai, 6, b->origin, (double)b->actionRadius, (double)g_SentinelRelaxSpeed, 1);
		}
		self->time = 0.0f;
		return ok;
	case 1:
		if (ok) {
			AIChar_SetState(ai, 6, b->origin, (double)b->actionRadius, (double)g_SentinelSearchSpeed, 1);
		}
		self->time = 0.0f;
		return ok;
	case 2:
		if (ok && target) {
			AIChar_SetState(ai, 2, target, (double)g_SentinelAcuseSpeed);
		}
		self->time = g_SentinelAcuseTime;
		return ok;
	case 3:
		if (ok && target) {
			AIChar_SetState(ai, 3, target, (double)g_SentinelInspectSpeed);
		}
		self->time = g_SentinelInspectTime;
		return ok;
	case 4:
		if (ok && target) {
			AIChar_SetState(ai, 3, target, (double)g_SentinelSuspectSpeed);
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
		cSentinelLocalLogic_Watch(self, Entity_GetPos(target, &tmp));
		return ok;
	case 6:
		if (ok && target) {
			AIChar_SetState(ai, 2, target, (double)g_SentinelAcuseSpeed);
			self->time = g_SentinelAcuseTime;
		}
		return ok;
	}
	return ok;
}

/* the vtable of cSentinelLocalLogic */
static const cSentinelLocalLogicVtbl cSentinelLocalLogic_vftable = {
	.base.base.Delete = cSentinelLocalLogic_Delete,
	.base.base.GetName = cBaseLocalLogic_GetName,
	.base.base.Vf02 = cLocalLogic_Vf02True,
	.base.base.VarI = cBaseLocalLogic_VarI,
	.base.base.VarF = cBaseLocalLogic_VarF,
	.base.base.VarS = cBaseLocalLogic_VarS,
	.base.base.VarV = cBaseLocalLogic_VarV,
	.base.UpdateBusy = cBaseLocalLogic_UpdateBusy,
	.base.Refresh = cBaseLocalLogic_Refresh,
	.base.Listens = cLocalLogic_ListensTrue,
	.base.OnHit = cSentinelLocalLogic_OnUsr,
	.base.OnSeen = cSentinelLocalLogic_OnUsr,
	.base.OnAttack = cSentinelLocalLogic_OnRobbery,
	.base.OnShot = cSentinelLocalLogic_OnAttack,
	.base.OnHammer = cLocalLogic_OnHammerNop,
	.base.GivesMoney = cSentinelLocalLogic_GivesMoney,
	.base.OnAlarm = cSentinelLocalLogic_OnAlarm,
	.base.Update = cBaseLocalLogic_Update,
	.base.SetOriginFrom = cBaseLocalLogic_SetOriginFrom,
	.base.SetState = cSentinelLocalLogic_SetState,
	.base.SetIdle = cBaseLocalLogic_SetIdle,
	.base.SetActionRadius = cBaseLocalLogic_SetActionRadius,
	.base.SetOrigin = cBaseLocalLogic_SetOrigin,
	.base.SetOriginAng = cBaseLocalLogic_SetOriginAng,
	.base.RestartState = cSentinelLocalLogic_RestartState,
	.Seen = cSentinelLocalLogic_Seen,
	.Think = cSentinelLocalLogic_Think,
};

/* the dynamic initializers of the globals here, run in the order of src/standalone/crtxc.inc */
INIT_GVAR_FLOAT(Init_GVar_SentinelAcuseTime, &GVar_SentinelAcuseTime, "SentinelAcuseTime", 0, "Time following the main after out of viewrange (Acuse)", 3.0f)
INIT_GVAR_FLOAT(Init_GVar_SentinelSuspectTime, &GVar_SentinelSuspectTime, "SentinelSuspectTime", 0, "Time following the main after out of viewrange (Suspect)", 3.0f)
INIT_GVAR_FLOAT(Init_GVar_SentinelInspectTime, &GVar_SentinelInspectTime, "SentinelInspectTime", 0, "Time following the main after out of viewrange (Inspect)", 3.0f)
INIT_GVAR_FLOAT(Init_GVar_SentinelGotoTime, &GVar_SentinelGotoTime, "SentinelGotoTime", 0, "Time following the xtrange viewpoint (Goto)", 5.0f)
INIT_GVAR_FLOAT(Init_GVar_SentinelGotoRadius, &GVar_SentinelGotoRadius, "SentinelGotoRadius", 0, "Radius to the viewpoint (Goto)", 300.0f)
INIT_GVAR_FLOAT(Init_GVar_SentinelRelaxSpeed, &GVar_SentinelRelaxSpeed, "SentinelRelaxSpeed", 0, "Sentinel speed (Relax)", 0.25f)
INIT_GVAR_FLOAT(Init_GVar_SentinelSearchSpeed, &GVar_SentinelSearchSpeed, "SentinelSearchSpeed", 0, "Sentinel speed (Search)", 1.0f)
INIT_GVAR_FLOAT(Init_GVar_SentinelAcuseSpeed, &GVar_SentinelAcuseSpeed, "SentinelAcuseSpeed", 0, "Sentinel speed (Acuse)", 0.6f)
INIT_GVAR_FLOAT(Init_GVar_SentinelSuspectSpeed, &GVar_SentinelSuspectSpeed, "SentinelSuspectSpeed", 0, "Sentinel speed (Suspect)", 0.5f)
INIT_GVAR_FLOAT(Init_GVar_SentinelInspectSpeed, &GVar_SentinelInspectSpeed, "SentinelInspectSpeed", 0, "Sentinel speed (Inspect)", 0.25f)
INIT_GVAR_FLOAT(Init_GVar_SentinelGotoSpeed, &GVar_SentinelGotoSpeed, "SentinelGotoSpeed", 0, "Sentinel speed (Goto order)", 1.0f)
INIT_GVAR_FLOAT(Init_GVar_SentinelBankerDetect, &GVar_SentinelBankerDetect, "SentinelBankerDetect", 0, "percentage of times when the eye acuse the banker", 30.0f)
INIT_GVAR_FLOAT(Init_GVar_SentinelBankerNPCDetect, &GVar_SentinelBankerNPCDetect, "SentinelBankerNPCDetect", 0, "percentage of times when the eye acuse the banker (for npcs)", 50.0f)
