/*
 * AI/AIGear.cpp of the game: the AI of the gear (cAIGear, a cAIChar with cAIControlGear). It shoots at its target
 * like the bishop (AIBishop.c) when the target is in the cone of its view and the way is free, and presses the
 * action button when it is blocked by a paused AI it ran into. See include/scrap/ai.h.
 */
#include <string.h>

#include "scrap/ai.h"
#include "scrap/aichar.h"
#include "scrap/crt.h"
#include "scrap/fxmisc.h"
#include "scrap/walkcharentity.h"
#include "scrap/staticinit.h"

#define CAIGEAR_SIZE sizeof(cAIGear)
#define GEAR_BLOCK_TIME 7.5f		/* the way to the target is tested again after it */

void *cAIGear_ctor(void *mem, cEntity *e, int *arg) {
	cAIGear *self = mem;
	cAIChar *c = &self->base;

	cAIChar_ctor(mem, e, arg);
	c->vftable = CAIGEAR_VFTABLE;
	cAIChar_NewControl(c, CAICONTROLSHOOT_SIZE, cAIControlGear_ctor, e, arg);
	self->targetLife = 0;
	self->blocked = 0;
	c->canTalk = 0;
	return self;
}

cAI *cAIGear_Delete(cAI *base, byte flags) {
	cAIChar *self = (cAIChar *)base;
	self->vftable = CAIGEAR_VFTABLE;
	cAIChar_dtor(self);
	if (flags & 1) {
		operator_delete_sized(self, CAIGEAR_SIZE);
	}
	return base;
}

void cAIGear_UpdateAction(cAI *elem) {
	cAIGear *self = (cAIGear *)elem;
	cAIChar *c = &self->base;

	if (c->vftable->CanRepos(&c->base.base, c->state) || !AICC_Vtbl(c->control)->IsBlocked(c->control)) {
		return;
	}
	const char *name = Nto1Node_TargetName(&WChar(c->ent)->collide);
	if (!strcmp(name, "") || !strcmp(name, "GateKeeper") || !strcmp(name, "Sebastian")) {
		return;
	}
	cEntity *o = World_FindEntity(name);
	if (o && o->vftable->GetAI(o)) {
		cAIChar *ai = o->vftable->GetAI(o);
		if (ai->vftable->base.IsPaused(&ai->base.base)) {
			c->ctrl->act = 1;
		}
	}
}

void cAIGear_CheckActDist(cAI *elem) {
	cAIGear *self = (cAIGear *)elem;
	cAIChar *c = &self->base;
	float f = c->standTime;
	vec3_t a, b, d;

	self->blocked = 0;
	if (1.0f > f) {
		self->targetLife = WChar(c->base.target.target)->wl.life;
	} else if (f > GEAR_BLOCK_TIME && self->targetLife <= WChar(c->base.target.target)->wl.life) {
		/* hit since the last check: is the way to the target free */
		cEntity *tgt = c->base.target.target;
		cAICharControl *ctl = c->control;
		cEntity *own = c->ent;
		float r = Obj_Vtbl(own)->GetRadius(own);
		const vec3_t *tp = tgt->vftable->GetPos(tgt, &b);
		const vec3_t *op = own->vftable->GetPos(own, &a);
		if (!AICC_Vtbl(ctl)->TestPath(ctl, op, tp, r)) {
			self->blocked = 1;
			c->ctrl->act = 0;
			return;
		}
		self->blocked = 0;
		c->standTime = 0.0f;
	}
	cEntity *tgt = c->base.target.target;
	const vec3_t *op = c->ent->vftable->GetPos(c->ent, &a);
	const vec3_t *tp = tgt->vftable->GetPos(tgt, &b);
	d.y = tp->y - op->y;
	d.x = tp->x - op->x;
	d.z = tp->z - op->z;
	float dist = (float)__builtin_sqrt((double)(d.x * d.x + 0.0f + d.z * d.z));
	if (!c->vftable->GetDirect2(&c->base.base) || !(dist > c->actMin) || !(c->actMax > dist)) {
		c->standTime = 0.0f;
		c->ctrl->act = 0;
		return;
	}
	if (AIChar_Action0(c->ent)->priority > 0x65) {
		c->ctrl->act = 0;
		return;
	}
	/* the half angle of the target at its distance, the angle of the view (down z) to it */
	cEntity *t = c->base.target.target;
	float tr = Obj_Vtbl(t)->GetRadius(t);
	double half = atan2((double)tr, (double)dist);
	vec3_t fwd = { 0.0f, 0.0f, -1.0f };
	vec3_t view = WChar(c->ent)->view;
	vec3_t dir;
	Vec3_Rotate(&dir, &fwd, &view);
	double ang = Vec3_Angle(&dir, &d);
	if (!((double)self->shootAng + half > ang) || (self->shootFactor != 0 && crt_rand() % self->shootFactor != 0)) {
		c->ctrl->act = 0;
		return;
	}
	c->ctrl->act = 1;
}

void cAIGear_SetDists(cAI *elem) {
	cAIGear *self = (cAIGear *)elem;
	cAIChar *c = &self->base;

	if (c->state == 2) {
		c->chaseFar = 1250.0f;
		c->actMin = 0.0f;
		c->actMax = 10000.0f;
		c->chaseNear = 400.0f;
		return;
	}
	if (c->state != 3) {
		cAIChar_SetDists(&c->base.base);
		return;
	}
	c->chaseFar = 1000.0f;
	c->chaseNear = 400.0f;
}

void cAIGear_SetShoot(cAI *elem) {
	cAIGear *self = (cAIGear *)elem;
	cEntity *t = self->base.base.target.target;

	if (t->vftable->GetAI(t)) {
		self->shootFactor = g_ShootGearAIDef;
		self->shootAng = g_OffsetAngShootGearAIDef;
	} else {
		self->shootFactor = g_ShootGearAI;
		self->shootAng = g_OffsetAngShootGearAI;
	}
	self->base.vftable->SetDists(&self->base.base.base);
}

byte cAIGear_IsBlocked(cAIGear *self) {
	return self->blocked;
}

/* the vtable of cAIGear */
const cAICharVtbl cAIGear_vftable = {
	.base.base.Delete = cAIGear_Delete,
	.base.base.Update = cAIChar_Update,
	.base.base.UpdateOff = cAIChar_UpdateOff,
	.base.base.Draw = cAIChar_Draw,
	.base.base.GetChanged = cAI_GetChanged,
	.base.base.IsVisible = cAI_IsVisible,
	.base.base.Debugf = cAIChar_Debugf,
	.base.base.VarI = cAIChar_VarI,
	.base.base.VarF = cAIChar_VarF,
	.base.base.VarS = cAIChar_VarS,
	.base.base.VarV = cAIChar_VarV,
	.base.base.Print = cAI_Print,
	.base.base.GetEventName = cAI_GetEventName,
	.base.base.OnEvent = cAI_OnEvent,
	.base.GetTarget = cAIMove_GetTarget,
	.base.CanJump = cAIMove_CanJump,
	.base.GetStopped = cAIMove_GetStopped,
	.base.IsPaused = cAIChar_IsPaused,
	.base.GetTargetEnt = cAIMove_GetTargetEnt,
	.base.GetArrived = cAIMove_GetArrived,
	.base.RandomPoint = cAIMove_RandomPoint,
	.base.CheckStopped = cAIMove_CheckStopped,
	.base.Event = cAIChar_Event,
	.base.IsMoving = cAIChar_IsMoving,
	.base.CanTarget = cAIMove_CanTarget,
	.CheckParams = cAIChar_CheckParams,
	.ChangeState = cAIChar_ChangeState,
	.SetState = cAIChar_SetState,
	.RestoreState = cAIChar_RestoreState,
	.SetInternStateV = cAIChar_SetInternStateV,
	.SetInternState = cAIChar_SetInternState,
	.GetState = cAIChar_GetState,
	.GetInternState = cAIChar_GetInternState,
	.StateName = cAIChar_StateName,
	.GetStateName = cAIChar_GetStateName,
	.InternStateName = cAIChar_InternStateName,
	.GetInternStateName = cAIChar_GetInternStateName,
	.GetDirect = cAIChar_GetDirect,
	.GetDirect2 = cAIChar_GetDirect2,
	.GetControl = cAIChar_GetControl,
	.GetLast = cAIChar_GetLast,
	.SaveTo = cAIChar_SaveTo,
	.SetRepos = cAIChar_SetRepos,
	.GetRepos = cAIChar_GetRepos,
	.IsTalking = cAIChar_IsTalking,
	.GetCollide = cAIChar_GetCollide,
	.React = cAIChar_React,
	.CheckAction = cAIChar_CheckAction,
	.GetChanged2 = cAIChar_GetChanged,
	.CanTalk = cAIChar_CanTalk,
	.SetStateIntern = cAIChar_SetStateIntern,
	.RestoreStateIntern = cAIChar_RestoreStateIntern,
	.ChangeStateMore = cAIChar_ChangeStateMore,
	.IsGroupState = cAIChar_IsGroupState,
	.Repos = cAIChar_Repos,
	.AfterRepos = cAIChar_AfterRepos,
	.SetDists = cAIGear_SetDists,
	.SetDists2 = cAIGear_SetShoot,
	.BeginChange = cAIChar_BeginChange,
	.SetPauseCheck = cAIChar_SetPauseCheck,
	.SaveState = cAIChar_SaveState,
	.RestoreSaved = cAIChar_RestoreSaved,
	.SetPaused = cAIChar_SetPaused,
	.UpdateAction1 = cAIChar_UpdateAction1,
	.UpdateAction0 = cAIChar_UpdateAction0,
	.UpdateAction = cAIGear_UpdateAction,
	.CheckActDist = cAIGear_CheckActDist,
	.SetAction = cAIChar_SetAction,
	.UpdateMove = cAIChar_UpdateMove,
	.EventDefault = cAIChar_EventDefault,
	.EventInRoute = cAIChar_EventInRoute,
	.EventPatrol = cAIChar_EventPatrol,
	.EventGoTo = cAIChar_EventGoTo,
	.EventGetAway = cAIChar_EventGetAway,
	.EventChase = cAI_EventChaseFalse,
	.EventChaseBack = cAIChar_EventChaseBack,
	.EventChaseNoAction = cAI_EventChaseNoActionFalse,
	.EventLostPath = cAIChar_EventLostPath,
	.Resume = cAIChar_Resume,
	.UpdateThink = cAIChar_UpdateThink,
	.CheckPause = cAIChar_CheckPause,
	.CheckRepos = cAIChar_CheckRepos,
	.CanRepos = cAIChar_CanRepos,
	.Think = cAIChar_Think,
	.ThinkInRoute = cAIChar_ThinkInRoute,
	.ThinkGetAway = cAI_ThinkGetAwayNop,
	.ThinkChase = cAI_ThinkChaseNop,
	.ThinkChaseBack = cAI_ThinkChaseBackNop,
	.ThinkChaseNoAction = cAI_ThinkChaseNoActionNop,
	.ThinkLostPath = cAIChar_ThinkLostPath,
	.ThinkLostPathChase = cAIChar_ThinkLostPathChase,
	.ReactLostPath = cAIChar_ReactLostPath,
	.ReactLostPathChase = cAIChar_ReactLostPath,
	.ReactPatrol = cAIChar_ReactPatrol,
	.ReactInRoute = cAIChar_ReactInRoute,
	.ThinkGoTo = cAI_ThinkGoToNop,
	.ThinkPatrol = cAIChar_ThinkPatrol,
	.CheckTalk = cAIChar_CheckTalk,
	.Facing = cAIChar_Facing,
};

cGVar GVar_ShootGearAI;
cGVar GVar_OffsetAngShootGearAI;

/* the dynamic initializers of the globals here, run in the order of src/standalone/crtxc.inc */
INIT_GVAR_INT(Init_GVar_ShootGearAI, &GVar_ShootGearAI, "ShootGearAI", 4, "Factor shoot Gear.\n", 3)
INIT_GVAR_FLOAT(Init_GVar_OffsetAngShootGearAI, &GVar_OffsetAngShootGearAI, "OffsetAngShootGearAI", 4, "Offset angle to shoot the Gear.\n", 0.19634955f)
