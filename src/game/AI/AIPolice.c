/*
 * AI/AIPolice.cpp of the game: the AI of the police (a cAIChar with the move control of cAIChar): the action button
 * within the distances of the action, which follow the radii of the two characters. See include/scrap/ai.h.
 */
#include "scrap/ai.h"
#include "scrap/aichar.h"
#include "scrap/walkcharentity.h"

#define CAIPOLICE_SIZE sizeof(cAIPolice)

void *cAIPolice_ctor(void *mem, cEntity *e, int *arg) {
	cAIPolice *self = mem;

	cAIChar_ctor(mem, e, arg);
	self->vftable = CAIPOLICE_VFTABLE;
	return self;
}

cAI *cAIPolice_Delete(cAI *base, byte flags) {
	cAIPolice *self = (cAIPolice *)base;
	self->vftable = CAIPOLICE_VFTABLE;
	cAIChar_dtor(self);
	if (flags & 1) {
		operator_delete_sized(self, CAIPOLICE_SIZE);
	}
	return base;
}

void cAIPolice_CheckActDist(cAI *base) {
	cAIPolice *self = (cAIPolice *)base;
	float dist = AIChar_TargetDistFlat(self);

	self->ctrl->act = AIChar_Action0(self->ent)->priority <= 0x65 && self->actMax > dist && dist > self->actMin;
}

void cAIPolice_SetDists(cAI *elem) {
	cAIPolice *self = (cAIPolice *)elem;
	float d;

	if (self->state == 2) {
		d = WChar(self->base.target.target)->wl.prim.radius * 1.5f + WChar(self->ent)->wl.prim.radius;
		self->chaseNear = d;
		d = d + 100.0f;
		self->actMin = self->chaseNear;
		self->chaseFar = d;
		self->actMax = self->chaseFar;
		return;
	}
	if (self->state != 3) {
		cAIChar_SetDists(&self->base.base);
		return;
	}
	d = WChar(self->ent)->wl.prim.radius * 3.5f + WChar(self->base.target.target)->wl.prim.radius;
	self->chaseNear = d;
	self->chaseFar = d + 100.0f;
}

/* the vtable of cAIPolice */
const cAICharVtbl cAIPolice_vftable = {
	.base.base.Delete = cAIPolice_Delete,
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
	.SetDists = cAIPolice_SetDists,
	.SetDists2 = cAIChar_SetDists2,
	.BeginChange = cAIChar_BeginChange,
	.SetPauseCheck = cAIChar_SetPauseCheck,
	.SaveState = cAIChar_SaveState,
	.RestoreSaved = cAIChar_RestoreSaved,
	.SetPaused = cAIChar_SetPaused,
	.UpdateAction1 = cAIChar_UpdateAction1,
	.UpdateAction0 = cAIChar_UpdateAction0,
	.UpdateAction = cAI_UpdateActionNop,
	.CheckActDist = cAIPolice_CheckActDist,
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
