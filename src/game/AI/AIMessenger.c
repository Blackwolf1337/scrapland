/*
 * AI/AIMessenger.cpp of the game: the AI of the messenger (cAIMessenger, a cAIChar with cAIControlMessenger). It
 * shoots at a target in the cone of its view while its action lets it walk, holding the button for TimeShoot. See
 * include/scrap/ai.h.
 */
#include "scrap/ai.h"
#include "scrap/aichar.h"
#include "scrap/crt.h"
#include "scrap/fxmisc.h"
#include "scrap/walkcharentity.h"

#define CAIMESSENGER_SIZE sizeof(cAIMessenger)

void *cAIMessenger_ctor(void *mem, cEntity *e, int *arg) {
	cAIChar *self = mem;

	cAIChar_ctor(mem, e, arg);
	self->vftable = CAIMESSENGER_VFTABLE;
	cAIChar_NewControl(self, CAICHARCONTROL_SIZE, cAIControlMessenger_ctor, e, arg);
	self->canTalk = 0;
	return self;
}

cAI *cAIMessenger_Delete(cAI *base, byte flags) {
	cAIChar *self = (cAIChar *)base;
	self->vftable = CAIMESSENGER_VFTABLE;
	cAIChar_dtor(self);
	if (flags & 1) {
		operator_delete_sized(self, CAIMESSENGER_SIZE);
	}
	return base;
}

void cAIMessenger_CheckActDist(cAI *elem) {
	cAIMessenger *self = (cAIMessenger *)elem;
	cAIChar *c = &self->base;
	byte act = 0;
	vec3_t a, b, d;

	cEntity *tgt = c->base.target.target;
	const vec3_t *op = c->ent->vftable->GetPos(c->ent, &a);
	const vec3_t *tp = tgt->vftable->GetPos(tgt, &b);
	d.x = tp->x - op->x;
	d.y = tp->y - op->y;
	d.z = tp->z - op->z;
	float dist = (float)__builtin_sqrt((double)(d.x * d.x + 0.0f + d.z * d.z));
	if (!AIChar_Action0(c->ent)->useFlyDisp || !c->vftable->GetDirect2(&c->base.base) || AIChar_Action0(c->ent)->priority > 0x65 ||
		!(dist > c->actMin) || !(c->actMax > dist)) {
		goto done;
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
	if (!((double)self->shootAng + half > ang)) {
		goto done;
	}
	if (c->ctrl->act) {
		c->ctrl->act = !(c->actTime > self->shootTime);
		return;
	}
	if ((self->shootFactor == 0 || crt_rand() % self->shootFactor == 0) && !WChar(c->ent)->falling &&
		AIChar_Action0(c->ent)->priority == 0) {
		c->actTime = 0.0f;
		act = 1;
	}
done:
	c->ctrl->act = act;
}

void cAIMessenger_SetDists(cAI *elem) {
	cAIMessenger *self = (cAIMessenger *)elem;
	cAIChar *c = &self->base;

	if (c->state == 2) {
		c->actMin = 0.0f;
		c->actMax = self->shootDist;
	} else if (c->state != 3) {
		cAIChar_SetDists(&c->base.base);
		return;
	}
	c->chaseFar = 400.0f;
	c->chaseNear = 129.0f;
}

void cAIMessenger_SetShoot(cAI *elem) {
	cAIMessenger *self = (cAIMessenger *)elem;
	self->shootFactor = g_ShootMessengerAI;
	self->shootAng = g_OffsetAngShootMessengerAI;
	self->shootTime = g_TimeShootMessengerAI;
	self->shootDist = g_DistMaxShootMessengerAI;
	self->base.vftable->SetDists(&self->base.base.base);
}

/* the vtable of cAIMessenger */
const cAICharVtbl cAIMessenger_vftable = {
	.base.base.Delete = cAIMessenger_Delete,
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
	.SetDists = cAIMessenger_SetDists,
	.SetDists2 = cAIMessenger_SetShoot,
	.BeginChange = cAIChar_BeginChange,
	.SetPauseCheck = cAIChar_SetPauseCheck,
	.SaveState = cAIChar_SaveState,
	.RestoreSaved = cAIChar_RestoreSaved,
	.SetPaused = cAIChar_SetPaused,
	.UpdateAction1 = cAIChar_UpdateAction1,
	.UpdateAction0 = cAIChar_UpdateAction0,
	.UpdateAction = cAI_UpdateActionNop,
	.CheckActDist = cAIMessenger_CheckActDist,
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
