/*
 * AI/AIBishop.cpp of the game: the AI of the bishop (cAIBishop, a cAIChar) that shoots at its target when it is in
 * the cone of its view and the way is free, with the angle between two vectors (Vec3_Angle) the AIs share. See
 * include/scrap/ai.h.
 */
#include "scrap/actions.h"
#include "scrap/ai.h"
#include "scrap/aichar.h"
#include "scrap/fxmisc.h"
#include "scrap/walkcharentity.h"

#define CAIBISHOP_SIZE sizeof(cAIBishop)
#define BISHOP_BLOCK_TIME 7.5f		/* the way to the target is tested again after it */

void *cAIBishop_ctor(void *mem, cEntity *e, int *arg) {
	cAIBishop *self = mem;
	cAIChar *c = &self->base;

	cAIChar_ctor(mem, e, arg);
	c->vftable = CAIBISHOP_VFTABLE;
	cAIChar_NewControl(c, CAICONTROLSHOOT_SIZE, cAIControlBishop_ctor, e, arg);
	self->targetLife = 0;
	self->blocked = 0;
	return self;
}

cAI *cAIBishop_Delete(cAI *base, byte flags) {
	cAIChar *self = (cAIChar *)base;
	self->vftable = CAIBISHOP_VFTABLE;
	cAIChar_dtor(self);
	if (flags & 1) {
		operator_delete_sized(self, CAIBISHOP_SIZE);
	}
	return base;
}

/* v normalized into out (a copy for the zero vector) */
static void Vec3_NormCopy(vec3_t *out, const vec3_t *v) {
	float len = (float)__builtin_sqrt((double)(v->x * v->x + v->y * v->y + v->z * v->z));

	if (len == 0.0f) {
		*out = *v;
		return;
	}
	float inv = 1.0f / len;
	out->x = v->x * inv;
	out->y = v->y * inv;
	out->z = v->z * inv;
}

double Vec3_Angle(const vec3_t *a, const vec3_t *b) {
	vec3_t na, nb;

	Vec3_NormCopy(&na, a);
	Vec3_NormCopy(&nb, b);
	float dot = na.x * nb.x + na.y * nb.y + na.z * nb.z;
	return acos((double)Float_MaxSS(-1.0f, Float_MinSS(1.0f, dot)));
}

static int Ent_Life(cEntity *e) {
	return WChar(e)->wl.life;
}

void cAIBishop_CheckAttack(cAI *elem) {
	cAIBishop *self = (cAIBishop *)elem;
	cAIChar *c = &self->base;
	float f = c->standTime;
	vec3_t a, b, d;

	self->blocked = 0;
	if (1.0f > f) {
		self->targetLife = Ent_Life(c->base.target.target);
	} else if (f > BISHOP_BLOCK_TIME && self->targetLife <= Ent_Life(c->base.target.target)) {
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
	if (!c->vftable->GetDirect2(&c->base.base) || AIChar_Action0(c->ent)->priority > 0x65 || !(dist > c->actMin) ||
		!(c->actMax > dist)) {
		c->standTime = 0.0f;
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
	if (!((double)self->shootAng + half > ang)) {
		c->ctrl->act = 0;
		return;
	}
	if (c->ctrl->act) {
		c->ctrl->act = !(c->actTime > self->shootTime);
		return;
	}
	if ((self->shootFactor == 0 || crt_rand() % self->shootFactor == 0) && !WChar(c->ent)->falling &&
		AIChar_Action0(c->ent)->priority == 0) {
		c->actTime = 0.0f;
		c->ctrl->act = 1;
		return;
	}
	c->ctrl->act = 0;
}

void cAIBishop_SetRanges(cAI *elem) {
	cAIBishop *self = (cAIBishop *)elem;
	cAIChar *c = &self->base;

	if (c->state != 2) {
		cAIChar_SetDists(&c->base.base);
		return;
	}
	c->actMin = 0.0f;
	float mx = self->shootDist;
	c->chaseNear = 400.0f;
	c->chaseFar = 1250.0f;
	c->actMax = mx;
}

void cAIBishop_SetShoot(cAI *elem) {
	cAIBishop *self = (cAIBishop *)elem;
	cEntity *t = self->base.base.target.target;

	if (t->vftable->GetAI(t)) {
		self->shootFactor = g_ShootBishopAIDef;
		self->shootAng = g_OffsetAngShootBishopAIDef;
		self->shootTime = g_TimeShootBishopAIDef;
		self->shootDist = g_DistMaxShootBishopAIDef;
	} else {
		self->shootFactor = g_ShootBishopAI;
		self->shootAng = g_OffsetAngShootBishopAI;
		self->shootTime = g_TimeShootBishopAI;
		self->shootDist = g_DistMaxShootBishopAI;
	}
	self->base.vftable->SetDists(&self->base.base.base);
}

/* the vtable of cAIBishop */
const cAICharVtbl cAIBishop_vftable = {
	.base.base.Delete = cAIBishop_Delete,
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
	.SetDists = cAIBishop_SetRanges,
	.SetDists2 = cAIBishop_SetShoot,
	.BeginChange = cAIChar_BeginChange,
	.SetPauseCheck = cAIChar_SetPauseCheck,
	.SaveState = cAIChar_SaveState,
	.RestoreSaved = cAIChar_RestoreSaved,
	.SetPaused = cAIChar_SetPaused,
	.UpdateAction1 = cAIChar_UpdateAction1,
	.UpdateAction0 = cAIChar_UpdateAction0,
	.UpdateAction = cAI_UpdateActionNop,
	.CheckActDist = cAIBishop_CheckAttack,
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
